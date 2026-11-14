/* test code to see, if the rom works or what
 * it'll change the add. on it's own so you can see the output - on 7 segment
 */
#define SHIFT_DATA   2
#define SHIFT_CLK    3
#define SHIFT_LATCH  4
#define EEPROM_D0    5
#define EEPROM_D7    12
#define WRITE_EN     13

#define ERASE_ALL      false   // true = write 0xFF to all 2048 bytes first (very slow in Proteus)
#define STEP_DELAY_MS  1000    // time each digit stays on the display

// 4bit hex decoder for common cathode 7-segment display
byte digits[] = {0x7e, 0x30, 0x6d, 0x79, 0x33, 0x5b, 0x5f, 0x70,
                 0x7f, 0x7b, 0x77, 0x1f, 0x4e, 0x3d, 0x4f, 0x47};

// 4bit hex decoder for common anode 7-segment display (use instead if needed)
// byte digits[] = {0x01, 0x4f, 0x12, 0x06, 0x4c, 0x24, 0x20, 0x0f,
//                  0x00, 0x04, 0x08, 0x60, 0x31, 0x42, 0x30, 0x38};

void setAddress(int address, bool outputEnable) {
  // upper byte: A8-A10 in bits 0-2, bit 7 = /OE (low = outputs enabled)
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, (address >> 8) | (outputEnable ? 0x00 : 0x80));
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address);

  digitalWrite(SHIFT_LATCH, LOW);
  digitalWrite(SHIFT_LATCH, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
}

// Release the data pins so the EEPROM alone drives its outputs
void releaseDataBus() {
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin++) {
    pinMode(pin, INPUT);
  }
}

byte readEEPROM(int address) {
  releaseDataBus();
  setAddress(address, true);
  byte data = 0;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin--) {
    data = (data << 1) + digitalRead(pin);
  }
  return data;
}

void writeEEPROM(int address, byte data) {
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin++) {
    pinMode(pin, OUTPUT);
  }
  setAddress(address, false);
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin++) {
    digitalWrite(pin, data & 1);
    data = data >> 1;
  }
  digitalWrite(WRITE_EN, LOW);
  delayMicroseconds(1);
  digitalWrite(WRITE_EN, HIGH);
  delay(10);
}

void printContents() {
  for (int base = 0; base <= 255; base += 16) {
    byte d[16];
    for (int offset = 0; offset <= 15; offset++) {
      d[offset] = readEEPROM(base + offset);
    }
    char buf[80];
    sprintf(buf, "%03x:   %02x %02x %02x %02x %02x %02x %02x %02x   %02x %02x %02x %02x %02x %02x %02x %02x",
            base, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7],
            d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
    Serial.println(buf);
  }
}

// Read back addresses 0-15 and compare with what we meant to write
bool verifyDigits() {
  bool ok = true;
  for (int address = 0; address <= 15; address++) {
    byte got = readEEPROM(address);
    if (got != digits[address]) {
      char buf[60];
      sprintf(buf, "MISMATCH @%02x: wrote %02x, read %02x", address, digits[address], got);
      Serial.println(buf);
      ok = false;
    }
  }
  return ok;
}

void setup() {
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  digitalWrite(WRITE_EN, HIGH);
  pinMode(WRITE_EN, OUTPUT);
  Serial.begin(57600);

  if (ERASE_ALL) {
    Serial.println("Erasing...");
    for (int address = 0; address <= 2047; address++) {
      writeEEPROM(address, 0xff);
    }
  }

  Serial.println("Programming 16 bytes...");
  for (int address = 0; address <= 15; address++) {
    writeEEPROM(address, digits[address]);
  }

  printContents();

  if (verifyDigits()) {
    Serial.println("VERIFY OK - starting display sweep");
  } else {
    Serial.println("VERIFY FAILED - check wiring / WE / OE / data pins");
  }

  // leave the bus released so only the EEPROM drives the display lines
  releaseDataBus();
}

void loop() {
  // Step through the stored digits forever: 0,1,2,...,F,0,1,...
  for (int address = 0; address <= 15; address++) {
    setAddress(address, true);          // /OE low -> EEPROM drives D0-D7
    Serial.print("addr ");
    Serial.print(address, HEX);
    Serial.print(" -> display should show ");
    Serial.println(address, HEX);
    delay(STEP_DELAY_MS);
  }
}
