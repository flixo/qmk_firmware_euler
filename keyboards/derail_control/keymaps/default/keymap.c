// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(KC_ENTER, KC_1, KC_5, KC_ESC, KC_5, KC_RIGHT_CTRL, KC_LEFT_CTRL, KC_RIGHT_SHIFT, KC_LEFT_SHIFT),
};

static uint32_t last_blink_time;

#define JOYSTICK_CYCLE_MS 2000
#define JOYSTICK_AXIS_OFFSET_MS 200

joystick_config_t joystick_axes[JOYSTICK_AXIS_COUNT] = {
    JOYSTICK_AXIS_VIRTUAL, // x
    JOYSTICK_AXIS_VIRTUAL, // y
    JOYSTICK_AXIS_VIRTUAL, // z
    JOYSTICK_AXIS_VIRTUAL, // Rx
    JOYSTICK_AXIS_VIRTUAL, // Ry
    JOYSTICK_AXIS_VIRTUAL  // Rz
};


void keyboard_post_init_user(void) {
    gpio_set_pin_output(GP25);
    gpio_write_pin_low(GP25);
    last_blink_time = timer_read32();
}

static int16_t joystick_axis_value(uint32_t elapsed_ms) {
    uint32_t phase = elapsed_ms % JOYSTICK_CYCLE_MS;

    if (phase < JOYSTICK_CYCLE_MS / 2) {
        return -JOYSTICK_MAX_VALUE + (int32_t)phase * 2 * JOYSTICK_MAX_VALUE / (JOYSTICK_CYCLE_MS / 2);
    }

    return JOYSTICK_MAX_VALUE - (int32_t)(phase - JOYSTICK_CYCLE_MS / 2) * 2 * JOYSTICK_MAX_VALUE / (JOYSTICK_CYCLE_MS / 2);
}

void matrix_scan_user(void) {
    uint32_t elapsed_ms = timer_read32();

    if (timer_elapsed32(last_blink_time) >= 500) {
        gpio_toggle_pin(GP25);
        last_blink_time = elapsed_ms;
    }

    for (uint8_t axis = 0; axis < JOYSTICK_AXIS_COUNT; axis++) {
        joystick_set_axis(axis, joystick_axis_value(elapsed_ms + axis * JOYSTICK_AXIS_OFFSET_MS));
    }
}
