// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(KC_ENTER, KC_1, KC_5, KC_ESC, KC_5, KC_RIGHT_CTRL, KC_LEFT_CTRL, KC_RIGHT_SHIFT, KC_LEFT_SHIFT),
};

static uint32_t last_blink_time;

#define JOYSTICK_AXIS_LEG_MS 500
#define JOYSTICK_AXIS_CENTER_PAUSE_MS 500
#define JOYSTICK_AXIS_WAIT_MS 1000
#define JOYSTICK_AXIS_TEST_MS (4 * JOYSTICK_AXIS_LEG_MS + 2 * JOYSTICK_AXIS_CENTER_PAUSE_MS + JOYSTICK_AXIS_WAIT_MS)
#define JOYSTICK_TEST_CYCLE_MS ((JOYSTICK_AXIS_COUNT + JOYSTICK2_AXIS_COUNT) * JOYSTICK_AXIS_TEST_MS)

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

static int16_t joystick_axis_value(uint32_t phase) {
    if (phase < JOYSTICK_AXIS_LEG_MS) {
        return -JOYSTICK_MAX_VALUE + (int32_t)phase * JOYSTICK_MAX_VALUE / JOYSTICK_AXIS_LEG_MS;
    }

    if (phase < JOYSTICK_AXIS_LEG_MS + JOYSTICK_AXIS_CENTER_PAUSE_MS) {
        return 0;
    }

    phase -= JOYSTICK_AXIS_LEG_MS + JOYSTICK_AXIS_CENTER_PAUSE_MS;
    if (phase < JOYSTICK_AXIS_LEG_MS) {
        return (int32_t)phase * JOYSTICK_MAX_VALUE / JOYSTICK_AXIS_LEG_MS;
    }

    phase -= JOYSTICK_AXIS_LEG_MS;
    if (phase < JOYSTICK_AXIS_LEG_MS) {
        return JOYSTICK_MAX_VALUE - (int32_t)phase * JOYSTICK_MAX_VALUE / JOYSTICK_AXIS_LEG_MS;
    }

    phase -= JOYSTICK_AXIS_LEG_MS;
    if (phase < JOYSTICK_AXIS_CENTER_PAUSE_MS) {
        return 0;
    }

    phase -= JOYSTICK_AXIS_CENTER_PAUSE_MS;
    if (phase < JOYSTICK_AXIS_LEG_MS) {
        return -(int32_t)phase * JOYSTICK_MAX_VALUE / JOYSTICK_AXIS_LEG_MS;
    }

    return 0;
}

void matrix_scan_user(void) {
    uint32_t elapsed_ms = timer_read32();
    uint32_t test_phase = elapsed_ms % JOYSTICK_TEST_CYCLE_MS;

    if (timer_elapsed32(last_blink_time) >= 500) {
        gpio_toggle_pin(GP25);
        last_blink_time = elapsed_ms;
    }

    if (test_phase < JOYSTICK_AXIS_COUNT * JOYSTICK_AXIS_TEST_MS) {
        uint8_t  active_axis = test_phase / JOYSTICK_AXIS_TEST_MS;
        uint32_t axis_phase  = test_phase % JOYSTICK_AXIS_TEST_MS;

        for (uint8_t axis = 0; axis < JOYSTICK_AXIS_COUNT; axis++) {
            joystick_set_axis(axis, axis == active_axis ? joystick_axis_value(axis_phase) : 0);
        }

        for (uint8_t axis = 0; axis < JOYSTICK2_AXIS_COUNT; axis++) {
            joystick2_set_axis(axis, 0);
        }
    } else {
        uint32_t joystick2_phase = test_phase - JOYSTICK_AXIS_COUNT * JOYSTICK_AXIS_TEST_MS;
        uint8_t  active_axis     = joystick2_phase / JOYSTICK_AXIS_TEST_MS;
        uint32_t axis_phase      = joystick2_phase % JOYSTICK_AXIS_TEST_MS;

        for (uint8_t axis = 0; axis < JOYSTICK_AXIS_COUNT; axis++) {
            joystick_set_axis(axis, 0);
        }

        for (uint8_t axis = 0; axis < JOYSTICK2_AXIS_COUNT; axis++) {
            joystick2_set_axis(axis, axis == active_axis ? joystick_axis_value(axis_phase) : 0);
        }
    }
}
