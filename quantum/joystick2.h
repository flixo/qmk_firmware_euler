// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef JOYSTICK2_AXIS_COUNT
#    define JOYSTICK2_AXIS_COUNT 6
#elif JOYSTICK2_AXIS_COUNT == 0 || JOYSTICK2_AXIS_COUNT > 6
#    error JOYSTICK2_AXIS_COUNT must be between 1 and 6
#endif

#ifndef JOYSTICK2_AXIS_RESOLUTION
#    define JOYSTICK2_AXIS_RESOLUTION 10
#elif JOYSTICK2_AXIS_RESOLUTION < 8 || JOYSTICK2_AXIS_RESOLUTION > 16
#    error JOYSTICK2_AXIS_RESOLUTION must be between 8 and 16
#endif

#define JOYSTICK2_MAX_VALUE ((1L << (JOYSTICK2_AXIS_RESOLUTION - 1)) - 1)

typedef struct {
    int16_t axes[JOYSTICK2_AXIS_COUNT];
    bool    dirty;
} joystick2_t;

extern joystick2_t joystick2_state;

void joystick2_init(void);
void joystick2_task(void);
void joystick2_flush(void);
void joystick2_set_axis(uint8_t axis, int16_t value);