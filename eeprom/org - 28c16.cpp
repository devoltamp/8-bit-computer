// normal one all 0xFF
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>

// Proteus VSM SDK header (copy from the Proteus installation's INCLUDE folder).
// These temporary renames avoid collisions between vsm.hpp and windows.h.
#define HWND  HWND_vsm_fix
#define HDC   HDC_vsm_fix
#define POINT POINT_vsm_fix
#include "vsm.hpp"
#undef HWND
#undef HDC
#undef POINT

// The file must be a RAW binary image, not Intel HEX or a text hex dump.
// Put it in Proteus' current working directory, or change this to an absolute path.
#define EEPROM_IMAGE_FILE "28c16.bin"

// A 28C16 is 2048 x 8 bits.
#define EEPROM_SIZE 2048

// -----------------------------------------------------------------------------
// Debug logging: %TEMP%\28c16.log
// -----------------------------------------------------------------------------
static void dbglog(const char *fmt, ...)
{
    char path[MAX_PATH] = {0};
    DWORD n = GetTempPathA(MAX_PATH, path);
    if (n == 0 || n >= MAX_PATH) return;

    const size_t used = strlen(path);
    if (used + strlen("28c16.log") + 1 >= MAX_PATH) return;
    strcat(path, "28c16.log");

    FILE *f = fopen(path, "a");
    if (!f) return;

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fputc('\n', f);
    fclose(f);
}

// -----------------------------------------------------------------------------
// Pin indexes. Names here must match the names in the Proteus device definition.
// -----------------------------------------------------------------------------
enum PinIndex
{
    PIN_A0, PIN_A1, PIN_A2, PIN_A3, PIN_A4, PIN_A5,
    PIN_A6, PIN_A7, PIN_A8, PIN_A9, PIN_A10,

    PIN_D0, PIN_D1, PIN_D2, PIN_D3,
    PIN_D4, PIN_D5, PIN_D6, PIN_D7,

    PIN_CE, PIN_OE, PIN_WE,
    PIN_COUNT
};

// Return: 1 = HIGH, 0 = LOW, -1 = floating/undefined/contention/missing.
// Proteus DSIM distinguishes strong, weak and power logic states.
static int logicLevel(STATE s)
{
    if (s == PHI || s == SHI || s == WHI) return 1;
    if (s == PLO || s == SLO || s == WLO) return 0;
    return -1;
}

static int pinLevel(IDSIMPIN *p)
{
    if (!p) return -1;
    return logicLevel(p->istate());
}

static IDSIMPIN *getOptionalPin(IINSTANCE *instance, char *nameList)
{
    // VSM accepts a comma-separated list of alternative pin names.
    return instance ? instance->getdsimpin(nameList, FALSE) : NULL;
}

static void loadImage(BYTE *memory)
{
    memset(memory, 0xFF, EEPROM_SIZE);

    char cwd[MAX_PATH] = {0};
    GetCurrentDirectoryA(MAX_PATH, cwd);

    FILE *f = fopen(EEPROM_IMAGE_FILE, "rb");
    if (!f)
    {
        dbglog("IMAGE: '%s' not found in working directory '%s'. EEPROM initialized to FF.",
               EEPROM_IMAGE_FILE, cwd);
        return;
    }

    const size_t bytesRead = fread(memory, 1, EEPROM_SIZE, f);
    const int readError = ferror(f);
    fclose(f);

    dbglog("IMAGE: loaded %u bytes from '%s' (working directory '%s'), read_error=%d",
           (unsigned)bytesRead, EEPROM_IMAGE_FILE, cwd, readError);

    if (bytesRead < EEPROM_SIZE)
    {
        dbglog("IMAGE: remaining %u bytes stay FF.",
               (unsigned)(EEPROM_SIZE - bytesRead));
    }
}

class EEPROM_28C_Model : public IDSIMMODEL
{
public:
    EEPROM_28C_Model()
        : m_instance(NULL),
          m_writeArmed(FALSE),
          m_prevWeLow(FALSE),
          m_prevCeLow(FALSE),
          m_havePreviousControls(FALSE),
          m_lastCtrl(-1),
          m_haveLastRead(FALSE),
          m_lastReadAddress(0),
          m_lastReadData(0)
    {
        for (int i = 0; i < PIN_COUNT; ++i) pins[i] = NULL;
        for (int i = 0; i < 8; ++i) outputStateKnown[i] = FALSE;
        memset(memory, 0xFF, sizeof(memory));
    }

    INT isdigital(CHAR *pinname) override
    {
        (void)pinname;
        return TRUE;
    }

    VOID setup(IINSTANCE *instance, IDSIMCKT *dsim) override
    {
        (void)dsim;
        m_instance = instance;

        // Address pins A0-A10.
        for (int i = 0; i < 11; ++i)
        {
            char name[8];
            sprintf(name, "A%d", i);
            pins[PIN_A0 + i] = instance->getdsimpin(name, TRUE);
        }

        // Data pins. Include common I/O spellings as aliases in case the
        // Proteus device definition uses the real IC names instead of D0-D7.
        for (int i = 0; i < 8; ++i)
        {
            char names[32];
            sprintf(names, "D%d,IO%d,I/O%d", i, i, i);
            pins[PIN_D0 + i] = instance->getdsimpin(names, TRUE);
        }

        // Active-low controls. The overbar on a symbol is often only graphical;
        // nevertheless, accept common alternate pin-name conventions.
        char ceNames[] = "CE,/CE,CE#,CE_N,CEN,!CE,~CE,$CE$";
        char oeNames[] = "OE,/OE,OE#,OE_N,OEN,!OE,~OE,$OE$";
        char weNames[] = "WE,/WE,WE#,WE_N,WEN,!WE,~WE,$WE$";
        pins[PIN_CE] = getOptionalPin(instance, ceNames);
        pins[PIN_OE] = getOptionalPin(instance, oeNames);
        pins[PIN_WE] = getOptionalPin(instance, weNames);

        dbglog("------------------------------------------------------------");
        dbglog("SETUP: instance=%s", instance ? instance->id() : "<null>");
        for (int i = 0; i < 11; ++i)
            dbglog("PIN A%d: %s", i, pins[PIN_A0 + i] ? "FOUND" : "MISSING");
        for (int i = 0; i < 8; ++i)
            dbglog("PIN D%d: %s", i, pins[PIN_D0 + i] ? "FOUND" : "MISSING");
        dbglog("PIN /CE: %s", pins[PIN_CE] ? "FOUND" : "MISSING");
        dbglog("PIN /OE: %s", pins[PIN_OE] ? "FOUND" : "MISSING");
        dbglog("PIN /WE: %s", pins[PIN_WE] ? "FOUND" : "MISSING");

        loadImage(memory);

        m_writeArmed = FALSE;
        m_prevWeLow = FALSE;
        m_prevCeLow = FALSE;
        m_havePreviousControls = FALSE;
        m_lastCtrl = -1;
        m_haveLastRead = FALSE;
        for (int i = 0; i < 8; ++i) outputStateKnown[i] = FALSE;
    }

    VOID runctrl(RUNMODES mode) override
    {
        (void)mode;
    }

    VOID actuate(REALTIME time, ACTIVESTATE newstate) override
    {
        (void)time;
        (void)newstate;
    }

    BOOL indicate(REALTIME time, ACTIVEDATA *newstate) override
    {
        (void)time;
        (void)newstate;
        return FALSE;
    }

    VOID simulate(ABSTIME time, DSIMMODES mode) override
    {
        // DSIMMODES enum members differ between Proteus SDK releases. We do not
        // depend on any named mode constants; the first call synchronizes inputs.
        (void)mode;

        const int ceLevel = pinLevel(pins[PIN_CE]);
        const int oeLevel = pinLevel(pins[PIN_OE]);
        const int weLevel = pinLevel(pins[PIN_WE]);

        // Controls are active-low. A floating/undefined control is NOT treated
        // as asserted; it safely disables the operation.
        const BOOL ceLow  = (ceLevel == 0);
        const BOOL oeLow  = (oeLevel == 0);
        const BOOL oeHigh = (oeLevel == 1);
        const BOOL weLow  = (weLevel == 0);
        const BOOL weHigh = (weLevel == 1);

        BOOL addressValid = FALSE;
        const DWORD address = readAddress(addressValid);

        const int ctrl = (ceLow ? 1 : 0) | (oeLow ? 2 : 0) | (weLow ? 4 : 0);
        if (ctrl != m_lastCtrl)
        {
            dbglog("CTRL: CE=%s OE=%s WE=%s address=0x%03X valid=%d",
                   levelName(ceLevel), levelName(oeLevel), levelName(weLevel),
                   (unsigned)address, addressValid ? 1 : 0);
            m_lastCtrl = ctrl;
        }

        // Real 28C16 read condition: /CE=0, /OE=0, /WE=1.
        const BOOL readEnabled = ceLow && oeLow && weHigh && addressValid;

        // Real 28C16 write condition: /CE=0, /OE=1, /WE=0.
        const BOOL writeSelected = ceLow && oeHigh && weLow;

        // On the first simulation call, synchronize the previous control levels.
        // This prevents power-up switch states from being mistaken for a write edge.
        // It replaces BOOT/SETTLE checks, which are not present in every VSM SDK.
        if (!m_havePreviousControls)
        {
            m_prevWeLow = weLow;
            m_prevCeLow = ceLow;
            m_writeArmed = FALSE;
            m_havePreviousControls = TRUE;
        }
        else
        {
            const BOOL weFalling = (!m_prevWeLow && weLow);
            const BOOL weRising  = (m_prevWeLow && !weLow);
            const BOOL ceFalling = (!m_prevCeLow && ceLow);
            const BOOL ceRising  = (m_prevCeLow && !ceLow);

            // Address is captured when /WE or /CE is asserted LOW.
            if (addressValid && oeHigh &&
                ((weFalling && ceLow) || (ceFalling && weLow)))
            {
                m_latchedAddress = address;
                m_writeArmed = TRUE;
                dbglog("WRITE START: latched address=0x%03X", (unsigned)m_latchedAddress);
            }

            // Commit data when /WE or /CE returns HIGH.
            if (m_writeArmed && (weRising || ceRising))
            {
                if (oeHigh)
                {
                    BYTE data = 0;
                    BYTE invalidMask = 0;
                    if (readDataBus(data, invalidMask))
                    {
                        memory[m_latchedAddress] = data;
                        dbglog("WRITE COMMIT: address=0x%03X data=0x%02X",
                               (unsigned)m_latchedAddress, (unsigned)data);
                    }
                    else
                    {
                        dbglog("WRITE ABORT: data bus has floating/undefined bits; invalid mask=0x%02X",
                               (unsigned)invalidMask);
                    }
                }
                else
                {
                    dbglog("WRITE ABORT: /OE was not HIGH at data-latch edge.");
                }
                m_writeArmed = FALSE;
            }

            // Cancel an incomplete write if the chip is deselected.
            if (m_writeArmed && !ceLow)
            {
                dbglog("WRITE CANCEL: chip deselected before completion.");
                m_writeArmed = FALSE;
            }

            m_prevWeLow = weLow;
            m_prevCeLow = ceLow;
        }

        if (readEnabled)
        {
            const BYTE value = memory[address];
            if (!m_haveLastRead || address != m_lastReadAddress || value != m_lastReadData)
            {
                dbglog("READ: address=0x%03X data=0x%02X",
                       (unsigned)address, (unsigned)value);
                m_lastReadAddress = address;
                m_lastReadData = value;
                m_haveLastRead = TRUE;
            }
            driveDataBus(time, value);
        }
        else
        {
            // Release D0-D7 during writes, deselection, or output-disabled mode.
            releaseDataBus(time);
        }
    }

    VOID callback(ABSTIME time, EVENTID id) override
    {
        (void)time;
        (void)id;
    }

private:
    static const char *levelName(int level)
    {
        if (level == 1) return "HIGH";
        if (level == 0) return "LOW";
        return "FLOAT/UNDEFINED";
    }

    DWORD readAddress(BOOL &valid)
    {
        DWORD address = 0;
        valid = TRUE;

        for (int i = 0; i < 11; ++i)
        {
            const int level = pinLevel(pins[PIN_A0 + i]);
            if (level < 0)
            {
                valid = FALSE;
                dbglog("ADDRESS ERROR: A%d is floating/undefined", i);
                continue;
            }
            if (level == 1) address |= (1u << i);
        }

        return address & 0x07FF;
    }

    BOOL readDataBus(BYTE &data, BYTE &invalidMask)
    {
        data = 0;
        invalidMask = 0;

        for (int i = 0; i < 8; ++i)
        {
            const int level = pinLevel(pins[PIN_D0 + i]);
            if (level < 0)
            {
                invalidMask |= (BYTE)(1u << i);
                continue;
            }
            if (level == 1) data |= (BYTE)(1u << i);
        }

        return (invalidMask == 0);
    }

    void postDataState(ABSTIME time, int bit, STATE state)
    {
        IDSIMPIN *p = pins[PIN_D0 + bit];
        if (!p) return;

        if (!outputStateKnown[bit] || lastOutputState[bit] != state)
        {
            // Use a one-tick delay for all SDK simulation modes. This avoids
            // same-time feedback and does not rely on version-specific enum names.
            p->setstate(time, RELTIME(1), state);
            lastOutputState[bit] = state;
            outputStateKnown[bit] = TRUE;
        }
    }

    void driveDataBus(ABSTIME time, BYTE data)
    {
        for (int i = 0; i < 8; ++i)
        {
            const STATE state = (data & (1u << i)) ? SHI : SLO;
            postDataState(time, i, state);
        }
    }

    void releaseDataBus(ABSTIME time)
    {
        for (int i = 0; i < 8; ++i)
            postDataState(time, i, FLT);
    }

    IINSTANCE *m_instance;
    IDSIMPIN *pins[PIN_COUNT];
    BYTE memory[EEPROM_SIZE];

    BOOL m_writeArmed;
    BOOL m_prevWeLow;
    BOOL m_prevCeLow;
    BOOL m_havePreviousControls;
    DWORD m_latchedAddress;

    int m_lastCtrl;
    BOOL m_haveLastRead;
    DWORD m_lastReadAddress;
    BYTE m_lastReadData;

    STATE lastOutputState[8];
    BOOL outputStateKnown[8];
};

// -----------------------------------------------------------------------------
// Mandatory Proteus VSM exports.
// -----------------------------------------------------------------------------
extern "C" __declspec(dllexport)
IDSIMMODEL *createdsimmodel(CHAR *device, ILICENCESERVER *ils)
{
    (void)device;
    if (ils) ils->authorize(0x00000000);
    return new EEPROM_28C_Model();
}

extern "C" __declspec(dllexport)
VOID deletedsimmodel(IDSIMMODEL *model)
{
    delete static_cast<EEPROM_28C_Model *>(model);
}
