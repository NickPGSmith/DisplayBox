#pragma once

//
// Name:    irreceiver.h
// Purpose: Infrared receiver class.
//
// This program is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 2.1 of the License, or any later version.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
// PARTICULAR PURPOSE.  See the GNU General Public License for more details.
// Do not remove information from this header.
//
// Note: IR pin is active low. 
//
// >>> NEC Protocol <<<
//
// A data frame (when button is pressed) consists of:
//
// Startbit:
//  * 9 ms mark followed by 4.5 ms space (13.5 ms total).
// 32 Data Bits:
//  * Logical '1': 562.5 µs mark + 1.6875 ms space (2.25 ms total).
//  * Logical '0': 562.5 µs mark + 562.5 µs space (1.125 ms total).
// Stop Bit:
//  * 562.5 µs mark.
//
// The 32 bits of data are sent (LSB first) as: CCccAAaa
//  * aa : 8 bit address
//  * AA : 8 bit address (inverted)
//  * cc : 8 bit command
//  * CC : 8 bit command (inverted).
//
// As an extension to the protocol, the two address fields aa and AA
// can be used as a single 16 bit address.
//
// If button is held down, a Repeat Code is sent every 110 ms from the
// start of the previous startbit:
//  * 9 ms startbit mark, followed by 2.25 ms space.
//  * 562.5 µs stop mark.
//
// It was found that the IR remote used generates multiple "presses" and
// requires some "debouncing".
//

#include <stdint.h>

namespace IRR {
  #undef B1
  // Button codes for LED controller remomte (6 rows and 4 cols).
  enum class Button : uint8_t {
        BrtU = 0x00, BrtD = 0x01, Off  = 0x02, On   = 0x03,
        Red  = 0x04, Grn  = 0x05, Blu  = 0x06, Wht  = 0x07,
        LRed = 0x08, LGrn = 0x09, LBlu = 0x0A, Flsh = 0x0B,
        A4   = 0x0C, B4   = 0x0D, C4   = 0x0E, Strb = 0x0F,
        Org  = 0x10, LCyn = 0x11, Pur  = 0x12, Fade = 0x13,
        Yel  = 0x14, Cyn  = 0x15, Mag  = 0x16, Smth = 0x17
  };

  // Button codes for TV tunder controller remomte (7 rows and 4 cols).
  // enum class Button : uint8_t {
  //       A1 = 0x13, B1 = 0x11, C1 = 0x1C, D1 = 0x12,
  //       A2 = 0x01, B2 = 0x02, C2 = 0x03, D2 = 0x0D,
  //       A3 = 0x04, B3 = 0x05, C3 = 0x06, D3 = 0x19,
  //       A4 = 0x07, B4 = 0x08, C4 = 0x09, D4 = 0x10,
  //       A5 = 0x0A, B5 = 0x00, C5 = 0x0B, D5 = 0x0C,
  //       A6 = 0x1B, B6 = 0x1D, C6 = 0x15, D6 = 0x18,
  //       A7 = 0x0E, B7 = 0x1E, C7 = 0x0F, D7 = 0x1A
  // };
}

class IR_Receiver
{
  public:
    enum class State: uint8_t
      { Idle = 0 , StartbitMark, StartbitSpace, BitMark, BitSpace, Repeat } ;
    enum class Error: uint8_t
      { OK = 0, Timeout, BadIdleTransition, BadStartbitMarkLength, BadStartbitSpaceLength,
        BadRepeatTransition, BadBitSpaceTransition, BadBitMarkTransition, BadBitLength,
        AddressMissmatch, CommandMismatch };

    IR_Receiver(uint8_t pin, uint16_t address = 0, bool verify = true, bool debounce = true)
        : address_(address), verify_(verify), state_(State::Idle), errorState_(Error::OK) {
      // Pre-calculate the bitmask (e.g., 1 << 2).
      bitMask_ = digitalPinToBitMask(pin);
      // Pre-calculate the memory address of the PIN register.
      pinRegister_ = portInputRegister(digitalPinToPort(pin));
    };

    void recordEdge();
    bool available() const { return available_; }
    uint32_t getData();
    uint8_t getCommand();
    Error getError();

  private:
    // Timing Helper, to check if a duration is within a a tollerance.
    struct Window {
        uint32_t nominal;
        uint32_t tolerance;
        bool within(uint32_t duration) const {
          return (duration >= (nominal - tolerance)) && (duration <= (nominal + tolerance));
        }
    };

    // Timing windows for the various marks and spaces, with 20% tolerance.
    static constexpr Window StartMark     { 9000, 1800 };
    static constexpr Window StartSpace    { 4500, 900 };
    static constexpr Window BitLong       { 1688, 337 };
    static constexpr Window BitShort      { 563,  112 };
    static constexpr Window RepeatSpace   { 2250, 450 };
    static constexpr Window RepeatPeriod  { 110000, 22000 };

    // Times here are in us.
    static constexpr uint32_t startbitTimeout   = 100000;   // Timeout for word after startbit
    static constexpr uint32_t debounceInterval  = 75000;    // Minimum time between button presses

    uint8_t bitMask_;                 // Bitmask for the pin (e.g., 1 << 2)
    volatile uint8_t* pinRegister_;   // Pointer to PIND, PINB, or PINC
    uint16_t address_;                // Address to filter commands (0 = no filtering)
    bool verify_;                     // True if command word must match inverted word
    bool debounce_;                   // Whether to debounce the button presses
    State state_;                   // State of data frame FSM
    volatile Error errorState_;
    
    uint32_t startbitMarkStart_;
    uint32_t lastStartbitMarkStart_;
    uint32_t startbitSpaceStart_;
    uint32_t bitMarkStart_;
    uint32_t bitSpaceStart_;
    uint32_t lastCmdReceived_;        // For debouncing
    
    uint8_t bitcount_;
    uint32_t tempData_;
    
    volatile bool available_ = false;
    volatile uint32_t data_;          // Received 32 bit data: CCccAAaa
    volatile uint8_t command_;        // Received 8 bit command code cc

    void resetToIdle();               // Reset settings and set to Idle state
    void setAvailable(uint32_t now);  // Set available flag if outside debounce period
};
