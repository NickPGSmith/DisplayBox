#pragma once

//
// Name:    storage.h
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

#include <stdint.h>

namespace Storage
{  
  void init();
  void resetToDefaults();

  uint8_t getBrightness();
  void setBrightness(uint8_t value);

  uint8_t getMode();
  void setMode(uint8_t value);

  uint8_t getPalletteEntry();
  void setPalletteEntry(uint8_t value);

  uint8_t getColourIntervalIndex();
  void setColourIntervalIndex(uint8_t value);
};
