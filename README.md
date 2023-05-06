# Amiga scroll wheel mouse driver using Cocolino protocol (e.g. [TankMouse](http://tank-mouse.com))

This branch implements (subset of) Cocolino protocol. TankMouse make use of scroll signals up and down, although driver takes care of other events (WiP).

NOTES
=====

The Middle mouse button is emulated, it does not use the real middle mouse button pin.
This is because this pin is used for communication between the Amiga and a mouse for
scroll wheel etc.


THANKS
======

Source code is based on the code prepared by [nogginthenog](https://github.com/paulroberthill/AmigaPS2Mouse).


HOW IT WORKS?
=============

Mouse'es controller sends code through LSB, RMB lines and two out of four quadrature signals. Mouse driver (runs on host - this driver) synchronises with mouse'es controller at the falling edge of MMB pin.
Codes recognized by driver:

1. MM_WHEEL_UP 0x0A
2. MM_WHEEL_DOWN 0x09
3. MM_MIDDLEMOUSE_UP 0x0D
4. MM_MIDDLEMOUSE_DOWN 0x0E
5. MM_WHEEL_RIGHT 0x06
6. MM_WHEEL_LEFT 0x03
7. MM_FOURTH_UP 0x80
8. MM_FOURTH_DOWN 0x40
9. MM_FIVETH_UP 0x08
10. MM_FIVETH_DOWN 0x04


TROUBLESHOOTING
===============

In case you observed unexpected behavior, please run OS event monitor (e.g. [RawKeys](http://aminet.net/package/dev/moni/Rawkeys)) and verify if the input from mouse matches.
If desired, the direction of wheel mouse can be reversed by the argument option:

* VREV (abrev. V) - up/down
* HREV (abrev. H) - left/right

Lastly the middle mouse button enable option is also available, but does not make any use for TankMouse:
* MMB (abrev. M) - enable the use of middle mouse button (use with care as Tikus locks up)
