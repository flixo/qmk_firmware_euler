// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#include "joystick2.h"

joystick2_t joystick2_state = {
    .axes  = {0},
    .dirty = false,
};

void joystick2_flush(void) {
    if (!joystick2_state.dirty) return;

    void host_joystick2_send(joystick2_t * joystick);
    host_joystick2_send(&joystick2_state);
    joystick2_state.dirty = false;
}

void joystick2_set_axis(uint8_t axis, int16_t value) {
    if (axis >= JOYSTICK2_AXIS_COUNT) return;

    if (value != joystick2_state.axes[axis]) {
        joystick2_state.axes[axis] = value;
        joystick2_state.dirty      = true;
    }
}

void joystick2_init(void) {}

void joystick2_task(void) {
    joystick2_flush();
}