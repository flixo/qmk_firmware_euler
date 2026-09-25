// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET
#define RP2040_BOOTLOADER_DOUBLE_TAP_TIMEOUT 500U // milliseconds

#define JOYSTICK_BUTTON_COUNT 16
// Min 0, max 6: X, Y, Z, Rx, Ry, Rz
#define JOYSTICK_AXIS_COUNT 6
// Min 8, max 16
#define JOYSTICK_AXIS_RESOLUTION 10
