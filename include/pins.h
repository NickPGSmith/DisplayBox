#pragma once

//
// Name:    pins.h
// Purpose: Define pin mappings (numbers are Arduino pins not AtMega328P pins).
//
// This program is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 2.1 of the License, or any later version.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
// PARTICULAR PURPOSE.  See the GNU General Public License for more details.
// Do not remove information from this header.
//

#include <Arduino.h>

namespace Pins {
  constexpr uint8_t IRR =           2;      // D2: Input IR Receiver interrupt (0) (active low)
  constexpr uint8_t LED =           3;      // D3: Output for LEDs
}
