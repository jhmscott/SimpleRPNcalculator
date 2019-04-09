# SimpleRPNcalculator

When I satarted 2nd year electrical engineering, I was required to buy a new graphing calculator. 
It was the first I had ever owned to support RPN(Reverse Polish Notation). I fell in love with it right away.
After learning about stacks in data structures and algorithms, I immediatly thought "I can do that."

At the core of this project is the 3V Pro-Trinket from adafruit. this bored is based on the familair ATMEGA328P and is combatible with most of the available arduino libraries. The PCD8544 was used as a display, as it allows for enough lines to display a portion of the stack, and is very affordable for its size. However, the code was written to make it easy to port over to any display of your choosing.

This projects relies on the following libraries:
1. The Arduino Keypad library v3.1.1 - by Mark Stanley and Alexander Brevig - found here: https://github.com/Chris--A/Keypad
2. The Adafruit GFX library v1.4.8 - by Limor Fried - found here: https://github.com/adafruit/Adafruit-GFX-Library
3. the Adafruit PCD8544 Library v1.0.0 - by Limor Fried - found here: https://github.com/adafruit/Adafruit-PCD8544-Nokia-5110-LCD-library

Additionally I used the builitn Arduino SPI and EEPROM libraries
