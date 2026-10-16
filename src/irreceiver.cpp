//
// Name:    irreceiver.cpp
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
#include <Arduino.h>
#include "irreceiver.h"

//
// Initialise to Idle state
//
void IR_Receiver::resetToIdle() {
  state_ = State::Idle;
  bitcount_ = 0;
  tempData_ = 0;
}

void IR_Receiver::setAvailable(uint32_t now) {
  if (now - lastCmdReceived_ >= debounceInterval) {
    lastCmdReceived_ = now;
    available_ = true;
  }
}

//
// Trigger on pulse edge; to be called from interrupt.
//
void IR_Receiver::recordEdge() {
  uint32_t now = micros();    // micros() records the start of ISR but will not tick
  // Record edge direction of mark (opposite of active low pin input).
  // Direct port access is faster replacement for: bool isFalling = digitalRead(2);
  bool isRising = !(*pinRegister_ & bitMask_);
  
  // Fail previous receives after 100 ms.
  if (state_ != State::Idle && (now - startbitMarkStart_ >= startbitTimeout)) {
    resetToIdle();
    errorState_ = Error::Timeout;
    return;
  }

  // Perform Finite State Machine transitions.
  switch (state_) {
    case State::Idle:
      if (isRising) {
        // A rising edge indicates a Startbit; initialise data.
        state_ = State::StartbitMark;
        startbitMarkStart_ = now;
      } else {
        // A falling edge should be end of stop bit or end of repeat code.
        // Not validating length; nothing to do in either case.
      }
      break;

    // Detect end of Startbit mark, otherwise abort back to Idle.
    case State::StartbitMark:
      if (!isRising && StartMark.within(now - startbitMarkStart_)) {
        state_ = State::StartbitSpace;
        startbitSpaceStart_ = now;
      }
      else {
        resetToIdle();
        errorState_ = Error::BadStartbitMarkLength;
      }
      break;

    // Detect end of Startbit space (start of a data bit, or a Repeat code), otherwise abort to Idle.
    case State::StartbitSpace:
      // Start of a data bit.
      if (isRising && StartSpace.within(now - startbitSpaceStart_)) {
        state_ = State::BitMark;
        bitMarkStart_ = now;
      }
      // Repeat code if space is correct length, and is with time window since last startbit.
      else if (isRising && RepeatSpace.within(now - startbitSpaceStart_) &&
                RepeatPeriod.within(now - lastStartbitMarkStart_)) {
        state_ = State::Repeat;
      }
      else {
        resetToIdle();
        errorState_ = Error::BadStartbitSpaceLength;
      }
      break;

    // Detect end of Repeat code.
    case State::Repeat:
      // Only a falling edge from Repeat should be valid, but check anyway.
      if (!isRising) {
        state_ = State::Idle;
        lastStartbitMarkStart_ = startbitMarkStart_;
        setAvailable(now);
      }
      else {
        resetToIdle();
        errorState_ = Error::BadRepeatTransition;
      }
      break;

    // Detect start of bit space.
    case State::BitMark:
      // Only a falling edge from BitMark should be valid, but check anyway.
      if (!isRising) {
        if (BitShort.within(now - bitMarkStart_)) {
          state_ = State::BitSpace;
          bitSpaceStart_ = now;
        } else {
          state_ = State::Idle;
          errorState_ = Error::BadBitLength;
        }
      }
      else {
        resetToIdle();
        errorState_ = Error::BadBitMarkTransition;
      }
      break;
  
    // Detect end of bit space, and determine if bit is '0' or '1'.
    case State::BitSpace:
      // Only a rising edge from BitSpace should be valid, but check anyway.
      if (isRising) {
        // If bit space is long, record a bit '1'.
        if (BitLong.within(now - bitSpaceStart_)) {
          // bitSet(tempData_, bitcount_);
          tempData_ |= (1UL << bitcount_);
          state_ = State::BitMark;
          bitMarkStart_ = now;
        }
        // If bit space is short, record a bit '0'.
        else if (BitShort.within(now - bitSpaceStart_)) {
          // Bits are already '0'; nothing to do.
          state_ = State::BitMark;
          bitMarkStart_ = now;
        }
        // Bad timing, so abort to Idle.
        else {
          resetToIdle();
          errorState_ = Error::BadBitLength;
          break;
        }

        // If 32 bits received, store data word and finalise states.
        if (++bitcount_ == 32) {
          lastStartbitMarkStart_ = startbitMarkStart_;
          
          // If address filtering enabled, verify address matches, otherwise abort to Idle.
          if (address_) {
            uint16_t address = tempData_ & 0xFFFF;
            if (address != address_) {
              resetToIdle();
              errorState_ = Error::AddressMissmatch;
              return;
            }
          }

          uint8_t command = (tempData_ >> 16) & 0xFF;
          // Verify command and inverted command match, otherwise abort to Idle.
          if (verify_) {
            uint8_t commandInverted = (tempData_ >> 24);
            if ((command ^ commandInverted) != 0xFF) {
              resetToIdle();
              errorState_ = Error::CommandMismatch;
              return;
            }
          }

          // Successful data/command decoding, so store.
          command_ = command;
          data_ = tempData_;
          errorState_ = Error::OK;
          resetToIdle();
          setAvailable(now);

        } else {
          // Have started the next bit.
          state_ = State::BitMark;
        }
      }
      else {
        resetToIdle();
        errorState_ = Error::BadBitSpaceTransition;
      }
      break;
  }
}

//
// Get full 32 bit data payload.
//
uint32_t IR_Receiver::getData() {
  uint32_t data;

  // Protect access from an interrupt update.
  noInterrupts();
  data = data_;
  available_ = false;
  interrupts();
  
  return data;
};

//
// Get 8 bit command code.
//
uint8_t IR_Receiver::getCommand() {
  uint32_t command;

  // Protect access from an interrupt update.
  noInterrupts();
  command = command_;
  available_ = false;
  interrupts();
  
  return command;
}

//
// Get error code.
//
IR_Receiver::Error IR_Receiver::getError()  {
  Error errorState;

  // Protect access from an interrupt update; not strictly necessary for 8 bit value.
  noInterrupts();
  errorState = errorState_;
  interrupts();

  return errorState;
}
