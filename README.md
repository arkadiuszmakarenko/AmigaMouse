# Amiga scroll wheel mouse driver

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
Codes supported by driver:

#define MM_WHEEL_UP 0x0A
#define MM_WHEEL_DOWN 0x09

