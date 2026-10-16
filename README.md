# DisplayBox

Control of programmable LEDs in a looped back (zig-zag) string.

##Modes of operation:
 0: staticSingle: unchanging solid colour, user-selectable from a palette.
 1: randomSingle: solid colour, fades to a different one randomly from a palette.
 2: randomDouble: like randomSingle but left/right ends are different colours.

Control is via an IR remote control, with the following functions:
 - Power toggle
 - Brightness adjustment
 - Colour palette selection
 - Mode selection:
   - Static single colour ("Smooth" button)
   - Random single colour ("Fade" button)
   - Random double colour ("Strobe" button)
   - Colour fade interval selection ("Flash" button)

To aid testing other kinds of IR remotes, the command code is printed to the serial
console when a button is pressed.

##Hardware:
 Board:          Arduino Nano
 IR Receiver:    TSOP34338 (38 kHz carrier) active low
 LEDs:           WS2812B

##Libraries:
 Arduino SDK
 EEPROM          https://www.arduino.cc/en/Reference/EEPROM 
 FastLED         https://fastled.io/

NOTE: See here for colour names:
https://github.com/FastLED/FastLED/wiki/Pixel-reference
