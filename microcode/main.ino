/* arduino code for eeprom -- ben eater version */
#define SHIFT_DATA 2
#define SHIFT_CLK 3
#define SHIFT_LATCH 4
#define EEPROM_D0 5
#define EEPROM_D7 12
#define WRITE_EN 13

void setAddress(int address, bool outputEnable){
    shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address >> 8 | (outputEnable ? 0x00 : 0x80));
    shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address);

    /* gotta toggle the latch */
    digitalWrite(SHIFT_LATCH, LOW);
    digitalWrite(SHIFT_LATCH, HIGH);
    digitalWrite(SHIFT_LATCH, LOW);
}

byte readEEPROM(int address){
    /* setting up the pin mode for read */
    for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin += 1){
        pinMode(pin, INPUT);
    }
    setAddress(address, /* outputEnable */ true);
    byte data = 0;
    for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin -= 1){
        data = (data << 1) + digitalRead(pin);
    }
    return data;
}

void writeEEPROM(int address, byte data){
    /* setting up the pin mode for write */
    for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin += 1){
        pinMode(pin, OUTPUT);
    }

    setAddress(address, /* outputEnable */ false);
    /* sep. bits and loading  */
    for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin += 1){
        digitalWrite(pin, data & 1);
        data = data >> 1;
    }
    digitalWrite(WRITE_EN, LOW);
    delayMicroseconds(1);
    digitalWrite(WRITE_EN, HIGH);
    delay(10);
    // delay(5);
}

void printContents(){
    // first 255 add. in the blocks of 16 -- kinda jump
    // if, you want more than 255, change that particular
    for (int base = 0; base <= 255; base += 16){
        byte data[16];
        for (int offset = 0; offset <= 15; offset += 1){
            data[offset] = readEEPROM(base + offset);
        }

        char buf[80];
        sprintf(buf, "%03x:    %02x %02x %02x %02x %02x %02x %02x %02x    %02x %02x %02x %02x %02x %02x %02x %02x",
            base, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9], data[10],
            data[11], data[12], data[13], data[14], data[15]);
        Serial.println(buf);
    }
}

#define HLT 0b1000000000000000
#define MI  0b0100000000000000
#define RI  0b0010000000000000
#define RO  0b0001000000000000
#define IO  0b0000100000000000
#define II  0b0000010000000000
#define AI  0b0000001000000000
#define AO  0b0000000100000000
#define EO  0b0000000010000000
#define SU  0b0000000001000000
#define BI  0b0000000000100000
#define OI  0b0000000000010000
#define CE  0b0000000000001000
#define CO  0b0000000000000100
#define J   0b0000000000000010

uint16_t data[] = {
    MI|CO, RO|II|CE, 0, 0, 0, 0, 0, 0,              // 0000 - NOP
    MI|CO, RO|II|CE, IO|MI, RO|AI, 0, 0, 0, 0,      // 0001 - LDA
    MI|CO, RO|II|CE, IO|MI, RO|BI, EO|AI, 0, 0, 0,  // 0010 - ADD
    MI|CO, RO|II|CE, 0, 0, 0, 0, 0, 0,              // 0011
    MI|CO, RO|II|CE, 0, 0, 0, 0, 0, 0,              // 0100
    MI|CO, RO|II|CE, 0, 0, 0, 0, 0, 0,              // 0101
    MI|CO, RO|II|CE, 0, 0, 0, 0, 0, 0,              // 0110
    MI|CO, RO|II|CE, 0, 0, 0, 0, 0, 0,              // 0111
    MI|CO, RO|II|CE, 0, 0, 0, 0, 0, 0,              // 1000
    MI|CO, RO|II|CE, 0, 0, 0, 0, 0, 0,              // 1001
    MI|CO, RO|II|CE, 0, 0, 0, 0, 0, 0,              // 1010
    MI|CO, RO|II|CE, 0, 0, 0, 0, 0, 0,              // 1011
    MI|CO, RO|II|CE, 0, 0, 0, 0, 0, 0,              // 1100
    MI|CO, RO|II|CE, 0, 0, 0, 0, 0, 0,              // 1101
    MI|CO, RO|II|CE, AO|OI, 0, 0, 0, 0, 0,          // 1110 - OUT
    MI|CO, RO|II|CE, HLT, 0, 0, 0, 0, 0,            // 1111
};

void setup(){
    pinMode(SHIFT_DATA, OUTPUT);
    pinMode(SHIFT_CLK, OUTPUT);
    pinMode(SHIFT_LATCH, OUTPUT);
    digitalWrite(WRITE_EN, HIGH);
    pinMode(WRITE_EN, OUTPUT);
    Serial.begin(57600);

    // erase entire eeprom
    // it's gonna take time in the simulation
    for (int address = 0; address <= 2047; address += 1){
        writeEEPROM(address, 0xff);
    }

    /* programming data bytes */
    Serial.print("programming eeprom");
    for (int address = 0; address < sizeof(data)/sizeof(data[0]); address += 1){
        writeEEPROM(address, data[address] >> 8);

        if (address % 64 == 0){
            Serial.print(".");
        }
    }

    for (int address = 0; address < sizeof(data)/sizeof(data[0]); address += 1){
        writeEEPROM(address + 128, data[address]);

        if (address % 64 == 0){
            Serial.print(".");
        }
    }

    Serial.print(" done");

    Serial.println("reading -- eeprom");
    printContents();
}

void loop(){

}
