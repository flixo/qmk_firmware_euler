// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "ht16k33_matrix.h"
#include "ht16k33_segment.h"
#include "ht16k33_segment_display.h"
#include "timer.h"

static bool ht16k33_ready;

static void ht16k33_matrix_symbol_test_tick(bool force) {
    if (!ht16k33_ready) {
        return;
    }

    static uint32_t last_update_ms = 0;
    static uint8_t  symbol_index   = 0;

    static const char *symbols[] = {
        "⚪⚪⚪⚪⚪⚪⚪⚪"
        "⚪⚪⚪⚪⚪⚪⚪⚪"
        "⚪⚪⚪⚪⚪⚪⚪⚪"
        "⚪⚪⚪⚪⚪⚪⚪⚪"
        "⚪⚪⚪⚪⚪⚪⚪⚪"
        "⚪⚪⚪⚪⚪⚪⚪⚪"
        "⚪⚪⚪⚪⚪⚪⚪⚪"
        "⚪⚪⚪⚪⚪⚪⚪⚪",

        // Addision
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚪⚫⚫⚫⚫"
        "⚫⚫⚫⚪⚫⚫⚫⚫"
        "⚫⚪⚪⚪⚪⚪⚫⚫"
        "⚫⚫⚫⚪⚫⚫⚫⚫"
        "⚫⚫⚫⚪⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫",

        // Multiply
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚪⚫⚫⚫⚪⚫⚫"
        "⚫⚫⚪⚫⚪⚫⚫⚫"
        "⚫⚫⚫⚪⚫⚫⚫⚫"
        "⚫⚫⚪⚫⚪⚫⚫⚫"
        "⚫⚪⚫⚫⚫⚪⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫",

        // Divide
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚪⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚪⚪⚪⚪⚪⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚪⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫",

        // Subtract
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚪⚪⚪⚪⚪⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫",

        // Power of
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚪⚫⚫⚫⚫"
        "⚫⚫⚪⚫⚪⚫⚫⚫"
        "⚫⚪⚫⚫⚫⚪⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫",

        // Square root
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚪⚪⚪⚪⚫"
        "⚪⚫⚫⚪⚫⚫⚫⚫"
        "⚫⚪⚫⚪⚫⚫⚫⚫"
        "⚫⚫⚪⚪⚫⚫⚫⚫"
        "⚫⚫⚫⚪⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫",

        // Pi
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚪⚪⚪⚪⚪⚫⚫"
        "⚫⚫⚪⚫⚪⚫⚫⚫"
        "⚫⚫⚪⚫⚪⚫⚫⚫"
        "⚫⚫⚪⚫⚫⚪⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫",

        // Sine
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚪⚫⚫⚫⚫"
        "⚫⚪⚫⚪⚫⚪⚫⚫"
        "⚪⚫⚫⚪⚫⚫⚪⚫"
        "⚪⚫⚫⚪⚫⚫⚪⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫",

        // Cosine
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚪⚪⚪⚫⚫⚫"
        "⚫⚪⚫⚫⚫⚪⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚪⚪⚪⚪⚪⚪⚪⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫",

        // Secant
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚪⚪⚪⚫⚪⚫"
        "⚫⚪⚫⚫⚫⚪⚫⚫"
        "⚪⚫⚫⚫⚪⚫⚪⚫"
        "⚪⚫⚫⚪⚫⚫⚪⚫"
        "⚫⚫⚪⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫",

        // Cosecant
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚪⚫⚪⚪⚪⚫⚫⚫"
        "⚫⚪⚫⚫⚫⚪⚫⚫"
        "⚪⚫⚪⚫⚫⚫⚪⚫"
        "⚪⚫⚫⚪⚫⚫⚪⚫"
        "⚫⚫⚫⚫⚪⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫",

        // Tangent
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚪⚪⚪⚪⚪⚪⚪⚫"
        "⚫⚫⚪⚪⚪⚫⚫⚫"
        "⚫⚪⚫⚫⚫⚪⚫⚫"
        "⚪⚫⚫⚫⚫⚫⚪⚫"
        "⚪⚫⚫⚫⚫⚫⚪⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫",

        // Cotangent
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚪⚫⚫⚫⚫⚫⚪⚫"
        "⚪⚫⚫⚫⚫⚫⚪⚫"
        "⚫⚪⚫⚫⚫⚪⚫⚫"
        "⚫⚫⚪⚪⚪⚫⚫⚫"
        "⚪⚪⚪⚪⚪⚪⚪⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫",

        // Cotangent
        "⚫⚫⚫⚫⚫⚫⚫⚫"
        "⚫⚪⚫⚫⚫⚫⚫⚫"
        "⚫⚪⚪⚫⚫⚫⚫⚫"
        "⚫⚪⚫⚪⚫⚫⚫⚫"
        "⚫⚪⚫⚫⚪⚫⚫⚫"
        "⚫⚪⚫⚫⚫⚪⚫⚫"
        "⚫⚪⚪⚪⚪⚪⚪⚫"
        "⚫⚫⚫⚫⚫⚫⚫⚫",
    };

    if (force) {
        symbol_index   = 0;
        last_update_ms = timer_read32();
        set_matrix_visual(symbols[symbol_index]);
        return;
    }

    if (timer_elapsed32(last_update_ms) < 1000) {
        return;
    }

    last_update_ms = timer_read32();
    symbol_index++;
    if (symbol_index >= (sizeof(symbols) / sizeof(symbols[0]))) {
        symbol_index = 0;
    }

    set_matrix_visual(symbols[symbol_index]);
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    /*
     * ┌───┬───┬───┬───┐
     * │ 7 │ 8 │ 9 │   │
     * ├───┼───┼───┤ + │
     * │ 4 │ 5 │ 6 │   │
     * ├───┼───┼───┼───┤
     * │ 1 │ 2 │ 3 │   │
     * ├───┴───┼───┤Ent│
     * │   0   │ . │   │
     * └───────┴───┴───┘
     */
    [0] = LAYOUT_numpad_4x4(
        KC_P7,   KC_P8,   KC_P9,   KC_PPLS,
        KC_P4,   KC_P5,   KC_P6,
        KC_P1,   KC_P2,   KC_P3,   KC_PENT,
        KC_P0,            KC_PDOT
    )
};


void keyboard_post_init_user(void) {
    rgblight_enable_noeeprom();
    rgblight_mode_noeeprom(RGBLIGHT_MODE_RAINBOW_MOOD);

    if (ht16k33_segment_display_init()) {
        ht16k33_segment_display_set_brightness(15);
        ht16k33_segment_display_start_scroll("LOREM IPSUM DOLOR SIT AMET.", 200);
    }

    ht16k33_ready = ht16k33_matrix_init();

    if (ht16k33_ready) {
        ht16k33_matrix_set_rotation(HT16K33_ROTATION_180);
        ht16k33_matrix_set_brightness(15);
        ht16k33_matrix_symbol_test_tick(true);
    }
}

void matrix_scan_user(void) {
    ht16k33_segment_display_tick();
    ht16k33_matrix_symbol_test_tick(false);
}