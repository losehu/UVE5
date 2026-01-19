#include <EEPROM.h>
#define EEPROM_START_CHECK 500
#define EEPROM_START_LEVEL EEPROM_START_CHECK + 2
#define EEPROM_START_SCORE EEPROM_START_LEVEL + 1

byte toplevel = 1;
uint32_t topscore = 0;

static void ensureEEPROM() {
  static bool inited = false;
  if (inited) {
    return;
  }
  // ESP32 EEPROM emulation requires an explicit begin(size).
  // We use a small fixed size that covers the addresses used by this sketch.
  EEPROM.begin(1024);
  inited = true;
}

static void eepromUpdateByte(int addr, byte value) {
  if (EEPROM.read(addr) != value) {
    EEPROM.write(addr, value);
  }
}

void saveEEPROM() {
  ensureEEPROM();
  eepromUpdateByte(EEPROM_START_LEVEL, toplevel);
  EEPROM.put(EEPROM_START_SCORE, topscore);
  EEPROM.commit();
}

void initEEPROM() {
  ensureEEPROM();
  byte c1 = EEPROM.read(EEPROM_START_CHECK);
  byte c2 = EEPROM.read(EEPROM_START_CHECK + 1);

  if (c1 == 0x20 && c2 == 0x82) {
    toplevel = EEPROM.read(EEPROM_START_LEVEL);
    EEPROM.get(EEPROM_START_SCORE, topscore);

  } else {
    eepromUpdateByte(EEPROM_START_CHECK, 0x20);
    eepromUpdateByte(EEPROM_START_CHECK + 1, 0x82);
    saveEEPROM();
  }
}