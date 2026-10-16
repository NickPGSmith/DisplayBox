//
// Name:    main.cpp
// Purpose: Display Box main code.
// 
// This program is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 2.1 of the License, or any later version.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
// PARTICULAR PURPOSE.  See the GNU General Public License for more details.
// Do not remove information from this header.
// 
// Control of programmable LEDs in a looped back (zig-zag) string.
//
// Modes of operation:
//  0: staticSingle: unchanging solid colour, user-selectable from a pallette.
//  1: randomSingle: solid colour, fades to a different one randomly from a pallette.
//  2: randomDouble: like randomSingle but left/right ends are different colours.
//
// Control is via an IR remote control, with the following functions:
//  - Power toggle
//  - Brightness adjustment
//  - Colour palette selection
//  - Mode selection:
//    - Static single colour ("Smooth" button)
//    - Random single colour ("Fade" button)
//    - Random double colour ("Strobe" button)
//    - Colour fade interval selection ("Flash" button)
// To aid testing other kinds of IR remotes, the command code is printed to the serial
// console when a button is pressed.
//
// Hardware:
//  Board:          Arduino Nano
//  IR Receiver:    TSOP34338 (38 kHz carrier) active low
//  LEDs:           WS2812B
//
// Libraries:
//  Arduino SDK
//  EEPROM          https://www.arduino.cc/en/Reference/EEPROM 
//  FastLED         https://fastled.io/
//
// NOTE: See here for colour names:
// https://github.com/FastLED/FastLED/wiki/Pixel-reference
//

#include <Arduino.h>
#include <Wire.h>
#define FASTLED_ALLOW_INTERRUPTS 1  // Allow FastLED to run with IR receiver interrupts
#include <FastLED.h>
#include "irreceiver.h"
#include "pins.h"
#include "storage.h"

constexpr char firmwareVersion[]          = "V0.1";
constexpr char firmwareLocation[]         = "https://github.com/NickPGSmith/DisplayBox";
constexpr uint8_t numLEDsPerSegment       = 6;      // Segments are looped back
constexpr uint8_t numLEDSegments          = 4;
constexpr uint8_t numLEDs                 = numLEDSegments * numLEDsPerSegment;
constexpr uint16_t statusOnInterval       = 5;
constexpr uint16_t statusOffInterval      = 10000 - statusOnInterval;
constexpr uint8_t jumpTimer               = 50;
constexpr uint32_t serialSpeed            = 115200;
constexpr uint8_t transitionFactor        = 4; // Power of 2 to slow transitions. 

// 
//  ----------------------------------------------------------------------------
//  File Scope Objects
//  ----------------------------------------------------------------------------
//
namespace {
  CRGB leds[numLEDs];
  IR_Receiver irr(Pins::IRR);
  bool status = false;            // State of onboard LED
  uint32_t prevStatusTimer;       // Previous millis() timer for onboard LED
  uint32_t prevColourTimer;       // Previous millis() timer for colour changes

  bool powerOn;                   // Overall state: on/off
  bool colourTransitioning;       // Whether a colour transition is in progress (for Random modes)
  uint8_t brightness;             // Overall user-controlled brightness level
  uint8_t leftCurrColourIndex;    // Left end of the pallette entry
  uint8_t rightCurrColourIndex;   // Right end of the pallette entry
  uint8_t leftNextColourIndex;    // Left end of the pallette entry
  uint8_t rightNextColourIndex;   // Right end of the pallette entry

  enum class Mode : uint8_t            // Display mode
    { StaticSingle = 0, RandomSingle, RandomDouble };
  Mode mode;
  
  // This array of colours can be selected for display.
  const CRGB colours[] = {
    CRGB::White, CRGB::Red, CRGB::Green, CRGB::Blue,
    CRGB::Cyan,  CRGB::Magenta, CRGB::Yellow,
    CRGB::HotPink, CRGB::LimeGreen, CRGB::LightSkyBlue,
    CRGB::OrangeRed, CRGB::MediumSpringGreen, CRGB::Indigo
  };
  // Time intervals for colour transitions: how long each colour is shown.
  const uint16_t ColourIntervals[] = {
    2000, 5000, 10000, 20000, 50000
  };
  uint8_t ColourIntervalIndex;    // Index into ColourIntervals[] for current interval
  CRGBPalette16 currPalette;      // Palette used for display
  CRGBPalette16 prevPalette;      // Previous palette, used for transitions in Random modes
  CRGBPalette16 nextPalette;      // Next palette used for transitions in Random modes
  uint16_t transitionCounter;     // Progress through colour transition in Random modes
}

//
//  ----------------------------------------------------------------------------
//  Functions
//  ----------------------------------------------------------------------------
//

// Interrupt Service Routine: Process IR receiver event.
void ISR_IRR() {
  irr.recordEdge();
}

// Pick a new random colour, but not the current one.
uint8_t randomColour(uint8_t currentColour) {
  uint8_t newColour;

  do {
    newColour = random8() % (sizeof(colours) / sizeof(colours[0]));
  } while (newColour == currentColour);
  return newColour;
}

// Create a palette that blends between two colours.
void make2ColPalette(CRGBPalette16& pal, CRGB colour1, CRGB colour2) {
  for (uint8_t i=0; i<16; ++i) {
      uint8_t blendAmount = map(i, 0, 15, 0, 255);
      pal[i] = blend(colour1, colour2, blendAmount);
  }
}

// Blend a palette towards another, updated out by ammount (0-255).
void blendPalette(const CRGBPalette16& from, const CRGBPalette16& to,
                  CRGBPalette16& out, uint8_t amount) {
  for (int i = 0; i<16; ++i) {
    out[i] = blend(from[i], to[i], amount);
  }
}

// Flash black/white for a short time to indicate a mode change.
void flashWhite(uint8_t times) {
  // Start black.
  fill_solid(leds, numLEDs, CRGB::Black);
  FastLED.show();
  delay(500);

  for (uint8_t i=0; i<times; ++i) {
    // Pulse whhite.
    fill_solid(leds, numLEDs, CRGB::White);
    FastLED.show();
    delay(100);
    
    // Pulse black between white.
    fill_solid(leds, numLEDs, CRGB::Black);
    FastLED.show();

    // Short gap between pulses, or longer after the final pulse.
    if (i < times - 1) {
      delay(100);
    } else {
      delay(500);
    }
  } 
}

// Set mode and colours.
void setMode(Mode newMode, uint8_t colourIndex) {
  mode = newMode;
  leftCurrColourIndex = colourIndex;
  rightCurrColourIndex = colourIndex;
}

// 
//  ----------------------------------------------------------------------------
//  Setup
//  ----------------------------------------------------------------------------
//
void setup() {
  Serial.begin(serialSpeed);
  while (!Serial) ; // Wait for serial port to connect, for native USB
  Serial.print(F("Display Box "));
  Serial.println(firmwareVersion);
  Serial.println(firmwareLocation);
  Serial.print(F("Initialising... "));

  Storage::init();

  // Setup pins.
  pinMode(Pins::IRR, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  // Setup LEDs.
  FastLED.addLeds<WS2812, Pins::LED, GRB>(leds, numLEDs);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 750); // For 750 mA total USB power
  FastLED.setBrightness(0xFF);

  // Show LED Test.
  CRGB testColours[] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::White, CRGB::Black};
  for (auto colour : testColours) {
    fill_solid(leds, numLEDs, colour);
    FastLED.show();
    delay(500);
  }

  powerOn = true;
  colourTransitioning = false;
  mode = static_cast<Mode>(Storage::getMode());
  prevColourTimer = millis();
  ColourIntervalIndex = Storage::getColourIntervalIndex();

  // Set the colour and brightness of the LEDs.
  if (mode == Mode::StaticSingle) {
    leftCurrColourIndex = Storage::getPalletteEntry();
    rightCurrColourIndex = leftCurrColourIndex;
  } else if (mode == Mode::RandomSingle) {
    leftCurrColourIndex = randomColour(0xFF);
    rightCurrColourIndex = leftCurrColourIndex;
  } else if (mode == Mode::RandomDouble) {
    leftCurrColourIndex = randomColour(0xFF);
    rightCurrColourIndex = randomColour(leftCurrColourIndex);
  }
  make2ColPalette(currPalette, colours[leftCurrColourIndex], colours[rightCurrColourIndex]);
  prevPalette = currPalette;
  brightness = Storage::getBrightness();
  FastLED.setBrightness(brightness);
  FastLED.show();

  // Enable Interrupts.
  attachInterrupt(digitalPinToInterrupt(Pins::IRR), ISR_IRR, CHANGE);

  Serial.println(F("Done."));
}

// 
//  ----------------------------------------------------------------------------
//  Main Loop
//  ----------------------------------------------------------------------------
//
void loop() {
  // Update delay timer.
  uint32_t currTimer = millis();
  
  // Update onboard LED status, and store current settings in EEPROM.
  if (currTimer - prevStatusTimer >= (status ? statusOnInterval : statusOffInterval)) {
    prevStatusTimer = currTimer;
    status = !status;
    
    if (status) {
      digitalWrite(LED_BUILTIN, HIGH);
      Storage::setBrightness(brightness);
      Storage::setMode(static_cast<uint8_t>(mode));
      Storage::setColourIntervalIndex(ColourIntervalIndex);
      // Only store colour index if not in a random mode.
      if (mode == Mode::StaticSingle) {
        Storage::setPalletteEntry(leftCurrColourIndex);
      }
    } else {
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
  
  if (!powerOn) {
    FastLED.setBrightness(0);
  }  else {
    if (mode != Mode::StaticSingle) {
      // Need to handle palette transitions in Random modes.
      if (!colourTransitioning) {
        // Check for need to start a new transition.
        if (currTimer - prevColourTimer > ColourIntervals[ColourIntervalIndex]) {
          colourTransitioning = true;
          transitionCounter = 0;
          prevPalette = currPalette;
          leftNextColourIndex = randomColour(leftCurrColourIndex);
          rightNextColourIndex = randomColour(rightCurrColourIndex);
          make2ColPalette(nextPalette, colours[leftNextColourIndex],
            (mode == Mode::RandomDouble) ? colours[rightNextColourIndex] : colours[leftNextColourIndex]);
        }
      
      } else {
        // Progress colour transition.
        // nblendPaletteTowardPalette(palette1, palette2, 5);
        blendPalette(prevPalette, nextPalette, currPalette, transitionCounter/transitionFactor);
        ++transitionCounter;
        if (currPalette == nextPalette) {
          // End transition if no more changes are needed.
          colourTransitioning = false;
          prevColourTimer = currTimer;
          leftCurrColourIndex = leftNextColourIndex;
          rightCurrColourIndex = rightNextColourIndex;
        }
      }
    } else {
       // Prepare palette1 as a single colour.
      if (mode == Mode::StaticSingle) {
        make2ColPalette(currPalette, colours[leftCurrColourIndex], colours[rightCurrColourIndex]);
      }
    }

    // Map the LED position to a palette index. Index 240 is the end of the palette with LINEARBLEND.
    for (uint8_t i=0; i<numLEDsPerSegment; ++i) {
      uint8_t paletteIndex = map(i, 0, numLEDsPerSegment - 1, 0, 240);
      CRGB c = ColorFromPalette(currPalette, paletteIndex, brightness, LINEARBLEND);
      for (uint8_t j=0; j<numLEDSegments; ++j) {
        //leds[i +  j * numLEDsPerSegment] = c;
        uint8_t posInSegment = (j % 2 == 0) ? i : (numLEDsPerSegment - 1 - i);
        leds[posInSegment + j * numLEDsPerSegment] = c;
      }
    }
    FastLED.setBrightness(brightness);
  }
  FastLED.show();

  // Process IR receiver commands.
  if (irr.available()) {
    uint8_t command = irr.getCommand();
    Serial.println(command, HEX);

    // If power is on or command is power on, process command.
    if (powerOn || command == static_cast<uint8_t>(IRR::Button::On)) {
      switch (static_cast<IRR::Button>(command)) {
        case IRR::Button::BrtU:
          if (brightness <= 0xFF - 0x10) {
            brightness += 0x10;
          }
          break;
      
        case IRR::Button::BrtD:
          if (brightness >= 0x10) {
            brightness -= 0x10;
          }
          break;
        
        case IRR::Button::Off:
          powerOn = false;
          break;
      
        case IRR::Button::On:
          powerOn = true;
          break;

        case IRR::Button::Red:
          setMode(Mode::StaticSingle, 1);
          break;

        case IRR::Button::Grn:
          setMode(Mode::StaticSingle, 2);
          break;

        case IRR::Button::Blu:
          setMode(Mode::StaticSingle, 3);
          break;

        case IRR::Button::Wht:
          setMode(Mode::StaticSingle, 0);
          break;
        
        case IRR::Button::LRed:
          setMode(Mode::StaticSingle, 7);
          break;

        case IRR::Button::LGrn:
          setMode(Mode::StaticSingle, 8);
          break;

        case IRR::Button::LBlu:
          setMode(Mode::StaticSingle, 9);
          break;  
        
        case IRR::Button::Flsh:
          // Cycle through colour fade intervals.
          ColourIntervalIndex = (ColourIntervalIndex + 1) % (sizeof(ColourIntervals) / sizeof(ColourIntervals[0]));
          flashWhite(ColourIntervalIndex);
          break;
        
        case IRR::Button::A4:
          // TODO
          break;

        case IRR::Button::B4:
          // TODO
          break;

        case IRR::Button::C4:
          // TODO
          break;

        case IRR::Button::Strb:
          mode = Mode::RandomDouble;
          prevColourTimer = millis() - ColourIntervals[ColourIntervalIndex];
          break;
  
        case IRR::Button::Org:
          setMode(Mode::StaticSingle, 10);
          break;

        case IRR::Button::LCyn:
          setMode(Mode::StaticSingle, 11);
          break;

        case IRR::Button::Pur:
          setMode(Mode::StaticSingle, 12);
          break;
          
        case IRR::Button::Fade:
          mode = Mode::RandomSingle;
          rightCurrColourIndex = leftCurrColourIndex;
          prevColourTimer = millis() - ColourIntervals[ColourIntervalIndex];
          break;
  
        case IRR::Button::Yel:
          setMode(Mode::StaticSingle, 6);
          break;

        case IRR::Button::Cyn:
          setMode(Mode::StaticSingle, 4);
          break;

        case IRR::Button::Mag:
          setMode(Mode::StaticSingle, 5);
          break;

        case IRR::Button::Smth:
          // Step up colour on repeated presses.
          if (mode == Mode::StaticSingle) {
            if (leftCurrColourIndex < (sizeof(colours) / sizeof(colours[0]) - 1)) {
              ++leftCurrColourIndex;
            } else {
              leftCurrColourIndex = 0;
            }
            rightCurrColourIndex = leftCurrColourIndex;
          } else {
            mode = Mode::StaticSingle;
          }
          break; 
      }
    }
  }
}
