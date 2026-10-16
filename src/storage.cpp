//
// Name:    storage.cpp
// Purpose: Storage class for data and settings in EEPROM.
//
// This program is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 2.1 of the License, or any later version.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
// PARTICULAR PURPOSE.  See the GNU General Public License for more details.
// Do not remove information from this header.
//
// Uninitialised EEPROM has all bits set to 1 (0xFF).
// Writes to EEPROM are slow and have limited write cycles (~100,000), but EEPROM
// library only writes changed data.
//

#include <stdint.h>
#include <EEPROM.h>
#include "storage.h"

namespace {
  constexpr uint8_t defaultBrightness = 0x9F;
  constexpr uint8_t defaultMode = 0;
  constexpr uint8_t defaultPalletteEntry = 0;
  constexpr uint8_t defaultColourIntervalIndex = 0;

  // EEPROM initialisation marker: can be anything other than 0xFF.
  constexpr uint8_t initialisedVal = 0;

  // Addresses of storage locations in EEPROM
  constexpr uint16_t addrInitialised = 1;
  constexpr uint16_t addrBrightness = addrInitialised + sizeof(uint8_t);
  constexpr uint16_t addrMode = addrBrightness + sizeof(uint8_t);
  constexpr uint16_t addrPalletteEntry = addrMode + sizeof(uint8_t);
  constexpr uint16_t addrColourIntervalIndex = addrPalletteEntry + sizeof(uint8_t);
}

//
// Check whether EEPROM has been initialised.
//
void Storage::init() {
  if (EEPROM.read(addrInitialised) != initialisedVal)
    Storage::resetToDefaults();
}

//
// Reset all settings to their default values.
//
void Storage::resetToDefaults() {
  EEPROM.put(addrInitialised, initialisedVal);
  Storage::setBrightness(defaultBrightness);
  Storage::setMode(defaultMode);
  Storage::setPalletteEntry(defaultPalletteEntry);
  Storage::setColourIntervalIndex(defaultColourIntervalIndex);
}

//
// Get and set brightness.
//
uint8_t Storage::getBrightness() {
  uint8_t value;
  
  EEPROM.get(addrBrightness, value);
  return value;
};
  
void Storage::setBrightness(uint8_t value) {
  EEPROM.put(addrBrightness, value);
};

//
// Get and set mode.
//
uint8_t Storage::getMode() {
  uint8_t value;
  
  EEPROM.get(addrMode, value);
  return value;
};
  
void Storage::setMode(uint8_t value) {
  EEPROM.put(addrMode, value);
};

//
// Get and set pallette entry.
//
uint8_t Storage::getPalletteEntry() {
  uint8_t value;
  
  EEPROM.get(addrPalletteEntry, value);
  return value;
};
  
void Storage::setPalletteEntry(uint8_t value) {
  EEPROM.put(addrPalletteEntry, value);
};

//
// Get and set colour interval index.
//
uint8_t Storage::getColourIntervalIndex() {
  uint8_t value;
  
  EEPROM.get(addrColourIntervalIndex, value);
  return value;
};
  
void Storage::setColourIntervalIndex(uint8_t value) {
  EEPROM.put(addrColourIntervalIndex, value);
};