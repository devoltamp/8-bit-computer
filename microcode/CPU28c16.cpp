// 28C16 (2048 x 8) EEPROM model for Proteus VSM, preloaded with the
// Ben Eater 8-bit computer microcode (16-bit control words split over two bytes).
//
// Layout (matches the Arduino sketch):
//   0x000-0x07F : data[i] >> 8      (upper control byte)
//   0x080-0x0FF : data[i] & 0xFF    (lower control byte)
//   0x100-0x7FF : 0xFF (erased)
//
// Use ONE chip per byte in the schematic: the chip feeding control bits 15-8
// needs A7 = 0, the chip feeding bits 7-0 needs A7 = 1.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>

// Temporary renames avoid collisions between vsm.hpp and windows.h.
#define HWND  HWND_vsm_fix
#define HDC   HDC_vsm_fix
#define POINT POINT_vsm_fix
#include "vsm.hpp"
#undef HWND
#undef HDC
#undef POINT

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------
#define EEPROM_SIZE        2048
#define EEPROM_IMAGE_FILE  "28c16_microcode_16bit.bin"

// 0 = always start from the built-in microcode (recommended: a stale or
//     erased .bin can no longer override it).
// 1 = load the .bin if it exists and looks valid (not blank).
#define LOAD_SAVED_IMAGE   0

// 1 = write the image to disk after every completed write cycle.
#define SAVE_ON_WRITE      0

// 1 = write debug output to %TEMP%\28c16.log
#define DEBUG_LOG          1

// ---------------------------------------------------------------------------
// Debug logging
// ---------------------------------------------------------------------------
static void dbglog(const char *fmt, ...)
{
#if DEBUG_LOG
    char path[MAX_PATH] = {0};
    DWORD n = GetTempPathA(MAX_PATH, path);
    if (n == 0 || n + 10 >= MAX_PATH) return;
    strcat(path, "28c16.log");

    FILE *f = fopen(path, "a");
    if (!f) return;

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fputc('\n', f);
    fclose(f);
#else
    (void)fmt;
#endif
}

// ---------------------------------------------------------------------------
// Pins
// ---------------------------------------------------------------------------
enum PinIndex
{
    PIN_A0, PIN_A1, PIN_A2, PIN_A3, PIN_A4, PIN_A5,
    PIN_A6, PIN_A7, PIN_A8, PIN_A9, PIN_A10,
    PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7,
    PIN_CE, PIN_OE, PIN_WE,
    PIN_COUNT
};

// 1 = high, 0 = low, -1 = floating/undefined
static int logicLevel(STATE s)
{
    if (s == PHI || s == SHI || s == WHI) return 1;
    if (s == PLO || s == SLO || s == WLO) return 0;
    return -1;
}

static int pinLevel(IDSIMPIN *p)
{
    return p ? logicLevel(p->istate()) : -1;
}

// ---------------------------------------------------------------------------
// Microcode (same bit definitions as the Arduino sketch)
// ---------------------------------------------------------------------------
#define HLT 0x8000
#define MI  0x4000
#define RI  0x2000
#define RO  0x1000
#define IO  0x0800
#define II  0x0400
#define AI  0x0200
#define AO  0x0100
#define EO  0x0080
#define SU  0x0040
#define BI  0x0020
#define OI  0x0010
#define CE_ 0x0008   // counter enable (CE is a pin name elsewhere)
#define CO  0x0004
#define J   0x0002

static const unsigned short microcode[128] =
{
    MI|CO, RO|II|CE_, 0,     0,     0,     0, 0, 0,   // 0000 NOP
    MI|CO, RO|II|CE_, IO|MI, RO|AI, 0,     0, 0, 0,   // 0001 LDA
    MI|CO, RO|II|CE_, IO|MI, RO|BI, EO|AI, 0, 0, 0,   // 0010 ADD
    MI|CO, RO|II|CE_, 0, 0, 0, 0, 0, 0,               // 0011
    MI|CO, RO|II|CE_, 0, 0, 0, 0, 0, 0,               // 0100
    MI|CO, RO|II|CE_, 0, 0, 0, 0, 0, 0,               // 0101
    MI|CO, RO|II|CE_, 0, 0, 0, 0, 0, 0,               // 0110
    MI|CO, RO|II|CE_, 0, 0, 0, 0, 0, 0,               // 0111
    MI|CO, RO|II|CE_, 0, 0, 0, 0, 0, 0,               // 1000
    MI|CO, RO|II|CE_, 0, 0, 0, 0, 0, 0,               // 1001
    MI|CO, RO|II|CE_, 0, 0, 0, 0, 0, 0,               // 1010
    MI|CO, RO|II|CE_, 0, 0, 0, 0, 0, 0,               // 1011
    MI|CO, RO|II|CE_, 0, 0, 0, 0, 0, 0,               // 1100
    MI|CO, RO|II|CE_, 0, 0, 0, 0, 0, 0,               // 1101
    MI|CO, RO|II|CE_, AO|OI, 0, 0, 0, 0, 0,           // 1110 OUT
    MI|CO, RO|II|CE_, HLT,   0, 0, 0, 0, 0,           // 1111 HLT
};

static void buildDefaultImage(BYTE *memory)
{
    memset(memory, 0xFF, EEPROM_SIZE);
    for (int i = 0; i < 128; ++i)
    {
        memory[i]       = (BYTE)(microcode[i] >> 8);
        memory[i + 128] = (BYTE)(microcode[i] & 0xFF);
    }
    dbglog("IMAGE: factory microcode built (0x000-0x0FF), rest 0xFF.");
}

static BOOL saveImage(const BYTE *memory)
{
    FILE *f = fopen(EEPROM_IMAGE_FILE, "wb");
    if (!f) { dbglog("ERROR: cannot open '%s' for writing.", EEPROM_IMAGE_FILE); return FALSE; }
    const size_t n = fwrite(memory, 1, EEPROM_SIZE, f);
    fclose(f);
    return n == EEPROM_SIZE;
}

static void loadImage(BYTE *memory)
{
    buildDefaultImage(memory);

#if LOAD_SAVED_IMAGE
    FILE *f = fopen(EEPROM_IMAGE_FILE, "rb");
    if (!f)
    {
        dbglog("IMAGE: '%s' not found, using factory image.", EEPROM_IMAGE_FILE);
        saveImage(memory);
        return;
    }

    BYTE tmp[EEPROM_SIZE];
    memset(tmp, 0xFF, sizeof(tmp));
    const size_t n = fread(tmp, 1, EEPROM_SIZE, f);
    fclose(f);

    // Reject a blank/erased file so it cannot wipe out the microcode.
    BOOL blank = TRUE;
    for (int i = 0; i < 0x100; ++i) if (tmp[i] != 0xFF) { blank = FALSE; break; }

    if (n == EEPROM_SIZE && !blank)
    {
        memcpy(memory, tmp, EEPROM_SIZE);
        dbglog("IMAGE: loaded %u bytes from '%s'.", (unsigned)n, EEPROM_IMAGE_FILE);
    }
    else
    {
        dbglog("IMAGE: '%s' is short or blank (%u bytes); using factory image.",
               EEPROM_IMAGE_FILE, (unsigned)n);
    }
#endif
}

// ---------------------------------------------------------------------------
// Device model
// ---------------------------------------------------------------------------
class EEPROM_28C_Model : public IDSIMMODEL
{
public:
    EEPROM_28C_Model()
        : m_writeArmed(FALSE), m_prevWeLow(FALSE), m_prevCeLow(FALSE),
          m_havePrev(FALSE), m_latchedAddress(0), m_lastCtrl(-1)
    {
        for (int i = 0; i < PIN_COUNT; ++i) pins[i] = NULL;
        for (int i = 0; i < 8; ++i) { outKnown[i] = FALSE; lastOut[i] = FLT; }
        memset(memory, 0xFF, sizeof(memory));
    }

    INT isdigital(CHAR *pinname) override { (void)pinname; return TRUE; }

    VOID setup(IINSTANCE *instance, IDSIMCKT *dsim) override
    {
        (void)dsim;

        for (int i = 0; i < 11; ++i)
        {
            char name[8];
            sprintf(name, "A%d", i);
            pins[PIN_A0 + i] = instance->getdsimpin(name, TRUE);
        }
        for (int i = 0; i < 8; ++i)
        {
            char names[32];
            sprintf(names, "D%d,IO%d,I/O%d", i, i, i);
            pins[PIN_D0 + i] = instance->getdsimpin(names, TRUE);
        }

        char ceNames[] = "CE,/CE,CE#,CE_N,CEN,!CE,~CE,$CE$";
        char oeNames[] = "OE,/OE,OE#,OE_N,OEN,!OE,~OE,$OE$";
        char weNames[] = "WE,/WE,WE#,WE_N,WEN,!WE,~WE,$WE$";
        pins[PIN_CE] = instance->getdsimpin(ceNames, FALSE);
        pins[PIN_OE] = instance->getdsimpin(oeNames, FALSE);
        pins[PIN_WE] = instance->getdsimpin(weNames, FALSE);

        dbglog("------------------------------------------------------------");
        dbglog("SETUP: instance=%s", instance->id());
        for (int i = 0; i < 11; ++i)
            if (!pins[PIN_A0 + i]) dbglog("PIN A%d: MISSING", i);
        for (int i = 0; i < 8; ++i)
            if (!pins[PIN_D0 + i]) dbglog("PIN D%d: MISSING", i);
        dbglog("PIN CE: %s  OE: %s  WE: %s",
               pins[PIN_CE] ? "ok" : "MISSING",
               pins[PIN_OE] ? "ok" : "MISSING",
               pins[PIN_WE] ? "ok" : "MISSING");

        loadImage(memory);
    }

    VOID runctrl(RUNMODES mode) override { (void)mode; }
    VOID actuate(REALTIME time, ACTIVESTATE newstate) override { (void)time; (void)newstate; }
    BOOL indicate(REALTIME time, ACTIVEDATA *newstate) override { (void)time; (void)newstate; return FALSE; }
    VOID callback(ABSTIME time, EVENTID id) override { (void)time; (void)id; }

    VOID simulate(ABSTIME time, DSIMMODES mode) override
    {
        (void)mode;

        const int ceLevel = pinLevel(pins[PIN_CE]);
        const int oeLevel = pinLevel(pins[PIN_OE]);
        const int weLevel = pinLevel(pins[PIN_WE]);

        const BOOL ceLow  = (ceLevel == 0);
        const BOOL oeLow  = (oeLevel == 0);
        const BOOL oeHigh = (oeLevel == 1);
        const BOOL weLow  = (weLevel == 0);
        const BOOL weHigh = (weLevel == 1);

        BOOL addressValid = FALSE;
        const DWORD address = readAddress(addressValid);

        const int ctrl = (ceLow ? 1 : 0) | (oeLow ? 2 : 0) | (weLow ? 4 : 0)
                       | (addressValid ? 8 : 0);
        if (ctrl != m_lastCtrl)
        {
            dbglog("CTRL: CE=%s OE=%s WE=%s addr=0x%03X valid=%d",
                   levelName(ceLevel), levelName(oeLevel), levelName(weLevel),
                   (unsigned)address, addressValid ? 1 : 0);
            m_lastCtrl = ctrl;
        }

        const BOOL readEnabled = ceLow && oeLow && weHigh && addressValid;

        // ---- write handling ----
        if (!m_havePrev)
        {
            m_prevWeLow = weLow;
            m_prevCeLow = ceLow;
            m_havePrev = TRUE;
        }
        else
        {
            const BOOL weFall = (!m_prevWeLow && weLow);
            const BOOL weRise = (m_prevWeLow && !weLow);
            const BOOL ceFall = (!m_prevCeLow && ceLow);
            const BOOL ceRise = (m_prevCeLow && !ceLow);

            if (addressValid && oeHigh &&
                ((weFall && ceLow) || (ceFall && weLow)))
            {
                m_latchedAddress = address;
                m_writeArmed = TRUE;
            }

            if (m_writeArmed && (weRise || ceRise))
            {
                BYTE data = 0, bad = 0;
                if (oeHigh && readDataBus(data, bad))
                {
                    memory[m_latchedAddress] = data;
                    dbglog("WRITE: [0x%03X] = 0x%02X", (unsigned)m_latchedAddress, (unsigned)data);
#if SAVE_ON_WRITE
                    saveImage(memory);
#endif
                }
                else
                {
                    dbglog("WRITE ABORT (oeHigh=%d, bad bits=0x%02X)", oeHigh ? 1 : 0, (unsigned)bad);
                }
                m_writeArmed = FALSE;
            }

            if (m_writeArmed && !ceLow) m_writeArmed = FALSE;

            m_prevWeLow = weLow;
            m_prevCeLow = ceLow;
        }

        // ---- read handling ----
        if (readEnabled) driveDataBus(time, memory[address]);
        else             releaseDataBus(time);
    }

private:
    static const char *levelName(int l)
    {
        return l == 1 ? "HIGH" : (l == 0 ? "LOW" : "FLOAT");
    }

    DWORD readAddress(BOOL &valid)
    {
        DWORD a = 0;
        valid = TRUE;
        for (int i = 0; i < 11; ++i)
        {
            const int l = pinLevel(pins[PIN_A0 + i]);
            if (l < 0) { valid = FALSE; continue; }
            if (l == 1) a |= (1u << i);
        }
        return a & 0x07FF;
    }

    BOOL readDataBus(BYTE &data, BYTE &bad)
    {
        data = 0; bad = 0;
        for (int i = 0; i < 8; ++i)
        {
            const int l = pinLevel(pins[PIN_D0 + i]);
            if (l < 0) { bad |= (BYTE)(1u << i); continue; }
            if (l == 1) data |= (BYTE)(1u << i);
        }
        return bad == 0;
    }

    void postData(ABSTIME time, int bit, STATE s)
    {
        IDSIMPIN *p = pins[PIN_D0 + bit];
        if (!p) return;
        if (!outKnown[bit] || lastOut[bit] != s)
        {
            p->setstate(time, RELTIME(1), s);
            lastOut[bit] = s;
            outKnown[bit] = TRUE;
        }
    }

    void driveDataBus(ABSTIME time, BYTE data)
    {
        for (int i = 0; i < 8; ++i)
            postData(time, i, (data & (1u << i)) ? SHI : SLO);
    }

    void releaseDataBus(ABSTIME time)
    {
        for (int i = 0; i < 8; ++i)
            postData(time, i, FLT);
    }

    IDSIMPIN *pins[PIN_COUNT];
    BYTE      memory[EEPROM_SIZE];

    BOOL  m_writeArmed, m_prevWeLow, m_prevCeLow, m_havePrev;
    DWORD m_latchedAddress;
    int   m_lastCtrl;

    STATE lastOut[8];
    BOOL  outKnown[8];
};

// ---------------------------------------------------------------------------
// Mandatory VSM exports
// ---------------------------------------------------------------------------
extern "C" __declspec(dllexport)
IDSIMMODEL *createdsimmodel(CHAR *device, ILICENCESERVER *ils)
{
    (void)device; (void)ils;
    return new EEPROM_28C_Model();
}

extern "C" __declspec(dllexport)
VOID deletedsimmodel(IDSIMMODEL *model)
{
    delete static_cast<EEPROM_28C_Model *>(model);
}
