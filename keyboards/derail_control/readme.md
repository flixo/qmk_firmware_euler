# vballer

A nine-switch RP2040 keyboard with direct GPIO wiring.

* Keyboard Maintainer: Jens Nomtak
* Hardware Supported: RP2040 controller
* Hardware Availability: Custom project

Make example for this keyboard (after setting up your build environment):

    make vballer:default

Flashing example for this keyboard:

    make vballer:default:flash

## Bootloader

Enter the bootloader by holding the key at matrix position `(0,0)` while connecting USB, or by using the RP2040 double-tap reset.
