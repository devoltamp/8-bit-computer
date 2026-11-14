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

// 4bit hex decoder for common anode 7-segment display
// byte data[] = {0x01, 0x4f, 0x12, 0x06, 0x4c, 0x24, 0x20, 0x0f, 0x00, 0x04, 0x08, 0x60, 0x31, 0x42, 0x30, 0x38};

// 4bit hex decoder for common cathode 7-segment display
// byte data[] = {0x7e, 0x30, 0x6d, 0x79, 0x33, 0x5b, 0x5f, 0x70, 0x7f, 0x7b, 0x77, 0x1f, 0x4e, 0x3d, 0x4f, 0x47};

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

    // program data bytes
    Serial.print("programming eeprom");

    byte digits[] = {0x7e, 0x30, 0x6d, 0x79, 0x33, 0x5b, 0x5f, 0x70, 0x7f, 0x7b};   // 0 - 9
    // ones
    for (int value = 0; value <= 255; value += 1){
        writeEEPROM(value, digits[value % 10]);
    }
    // tens
    for (int value = 0; value <= 255; value += 1){
        writeEEPROM(value + 256, digits[(value / 10) % 10]);
    }
    // hundreds
    for (int value = 0; value <= 255; value += 1){
        writeEEPROM(value + 512, digits[(value / 100) % 10]);
    }
    // 4th place
    for (int value = 0; value <= 255; value += 1){
        writeEEPROM(value + 768, 0);
    }

    // 2's compliment -- -ve sign
    // onces
    for (int value = -128; value <= 127; value += 1){
        writeEEPROM((byte)value + 1024, digits[abs(value) % 10]);
    }
    // tens
    for (int value = -128; value <= 127; value += 1){
        writeEEPROM((byte)value + 1280, digits[abs(value / 10) % 10]);
    }
    // hundreds
    for (int value = -128; value <= 127; value += 1){
        writeEEPROM((byte)value + 1536, digits[abs(value / 100) % 10]);
    }
    // 4th place
    for (int value = -128; value <= 127; value += 1){
        if (value < 0){
            writeEEPROM((byte)value + 1792, 0x01);      // g - segment
        }
        else {
            writeEEPROM((byte)value + 1792, 0);
        }
    }

    // scrap - test
    // for (int address = 0; address <= 15; address += 1){
    //     writeEEPROM(address, data[address]);
    //     if (address % 64 == 0){
    //         Serial.print(".");
    //     }
    // }
    // Serial.println(" done");
    Serial.println("reading -- eeprom");
    printContents();
}

void loop(){

}
