# DIY Amiga scroll wheel mouse driver

For Arduino based PS/2 Amiga mouse driver please refer to the original repository: https://github.com/paulroberthill/AmigaPS2Mouse

Altough majority of functionality is taken over from the original design, it does not support PS/2 Arduino based controller, but MSP430 based mouse controller that uses AVAGO sensor. For microcontroller please refer to the external repository: https://github.com/sq7bti/AVAGO/

NOTES
=====

The Middle mouse button is emulated, it does not use the real middle mouse button pin.
This is because this pin is used for communication between the Amiga and a mouse for
scroll wheel etc.


THANKS
======

Source code is based on the code prepared by nogginthenog.
https://github.com/paulroberthill/AmigaPS2Mouse


HOW IT WORKS?
=============

Mouse controller sends code through four quadrature signals. Synchronisation with Amiga driver occurs at the falling edge of MMB pin.

|joy0dat|      | xor  |   | swap | X | Hamm | * |                  |
|------:|-----:|-----:|--:|-----:|--:|-----:|:-:|:-----------------|
|  0x21 | 1001 | 1101 | D | 1110 | E |   3  |   | CODE_MMB_UP      |
|  0x23 | 1011 | 1110 | E | 1101 | D |   3  |   | CODE_MMB_DOWN    |
|  0x20 | 1000 | 1100 | C | 1100 | C |   2  |   | CODE_4TH_DOWN    |
|  0x32 | 1110 | 1011 | B | 1011 | B |   3  |   | CODE_WHEEL_UP    |
|  0x31 | 1101 | 1001 | 9 | 1010 | A |   2  | * | CODE_WHEEL_LEFT  |
|  0x33 | 1111 | 1010 | A | 1001 | 9 |   2  | * | CODE_4TH_UP      |
|  0x12 | 0110 | 0111 | 7 | 0111 | 7 |   3  |   | CODE_WHEEL_DOWN  |
|  0x11 | 0101 | 0101 | 5 | 0110 | 6 |   2  | * | CODE_5TH_UP      |
|  0x13 | 0111 | 0110 | 6 | 0101 | 5 |   2  | * | CODE_WHEEL_RIGHT |
|  0x02 | 0010 | 0011 | 3 | 0011 | 3 |   2  |   | CODE_5TH_DOWN    |

- joy0dat - code read from hardware register (after xor'ing with previously read value)
- xor - xor applied on bits X0 and X1 (Y0 and Y1) - see [elowar](http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0038.html) for details
- swap - position Y0 and Y1 swapped
- Hamm - code hamming distance
- * - problematic codes - potential occurrence when traditional mouse connected.
