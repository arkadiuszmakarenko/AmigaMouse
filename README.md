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

Kris Adcock for his PS/2-to-Quadrature-mouse project.
http://danceswithferrets.org/geekblog/?m=201702
PS2_to_Amiga.ino is based heavily on his PS2_to_Quadrature_Mouse.ino
I used an interrupt based PS/2 library instead if Kris's.

PS2Utils Arduino library by Roger Tawa (Apache License 2.0)
https://github.com/rogerta/PS2Utils
Designed for a PS/2 keyboard but works perfectly with a mouse.  Interrupt driven.

digitalWriteFast by Nickson Yap

Members of English Amiga Board for help
http://eab.abime.net/showthread.php?p=1178490



OUTSTANDING ISSUES
==================

Mouse controller sends code through four quadrature signals. Synchronisation with Amiga driver occurs at the falling edge of MMB pin.

