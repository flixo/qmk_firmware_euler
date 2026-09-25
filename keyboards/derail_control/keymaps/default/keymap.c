// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(KC_ENTER, KC_1, KC_5, KC_ESC, KC_5, KC_RIGHT_CTRL, KC_LEFT_CTRL, KC_RIGHT_SHIFT, KC_LEFT_SHIFT),
};

static uint32_t last_blink_time;

#define JOYSTICK_CYCLE_MS 2000
#define JOYSTICK_AXIS_OFFSET_MS 200
#define JOYSTICK2_CYCLE_MS 3000
#define JOYSTICK2_AXIS_OFFSET_MS 350
#define JOYSTICK2_START_OFFSET_MS 1000

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

static int16_t joystick_axis_value(uint32_t elapsed_ms, uint32_t cycle_ms) {
    uint32_t phase = elapsed_ms % cycle_ms;

    if (phase < cycle_ms / 2) {
        return -JOYSTICK_MAX_VALUE + (int32_t)phase * 2 * JOYSTICK_MAX_VALUE / (cycle_ms / 2);
    }

    return JOYSTICK_MAX_VALUE - (int32_t)(phase - cycle_ms / 2) * 2 * JOYSTICK_MAX_VALUE / (cycle_ms / 2);
}

void matrix_scan_user(void) {
    uint32_t elapsed_ms = timer_read32();

    if (timer_elapsed32(last_blink_time) >= 500) {
        gpio_toggle_pin(GP25);
        last_blink_time = elapsed_ms;
    }

    for (uint8_t axis = 0; axis < JOYSTICK_AXIS_COUNT; axis++) {
        joystick_set_axis(axis, joystick_axis_value(elapsed_ms + axis * JOYSTICK_AXIS_OFFSET_MS, JOYSTICK_CYCLE_MS));
    }

    for (uint8_t axis = 0; axis < JOYSTICK2_AXIS_COUNT; axis++) {
        joystick2_set_axis(axis, joystick_axis_value(elapsed_ms + JOYSTICK2_START_OFFSET_MS + axis * JOYSTICK2_AXIS_OFFSET_MS, JOYSTICK2_CYCLE_MS));
    }
}
