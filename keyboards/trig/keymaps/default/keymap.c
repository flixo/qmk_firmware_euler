// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "ht16k33_matrix.h"
#include "ht16k33_segment.h"
#include "ht16k33_segment_display.h"
#include "calc_mode.h"
#include "matrix_symbols.h"
#include "timer.h"
#include <stdio.h>

static bool ht16k33_ready;
static bool calc_startup_display_pending;

#define ______ KC_TRANSPARENT
#define MASKED KC_NO

enum layer_names {
    _BL,
    _UL,
    _FL,
    _TEMPLATE,
};

#define REPEAT_OP_FLASH_MS 300

static void segment_display_blank(void) {
    (void)ht16k33_segment_display_write_text("            ");
}

static void matrix_display_tick(bool force) {
    if (!ht16k33_ready) {
        return;
    }

    static int8_t   last_calc_idx    = -1;
    static int8_t   flash_calc_idx   = -1;
    static bool     flash_just_ended = false;
    static uint32_t flash_started_ms = 0;

    if (calc_mode_is_enabled()) {
        const uint8_t sci_idx = calc_mode_scientific_matrix_symbol();
        if (sci_idx != MATRIX_SYMBOL_INVALID) {
            if (force || (int8_t)sci_idx != last_calc_idx) {
                set_matrix_visual(matrix_symbol_get(sci_idx));
                last_calc_idx = (int8_t)sci_idx;
            }
            return;
        }

        const uint8_t flash_symbol = calc_mode_take_repeat_flash_symbol();
        if (flash_symbol != MATRIX_SYMBOL_INVALID) {
            flash_calc_idx   = (int8_t)flash_symbol;
            flash_started_ms = timer_read32();
        }

        const char flash_op = calc_mode_take_repeat_flash_op();
        if (flash_op != 0) {
            const uint8_t flash_idx = matrix_symbol_for_op(flash_op);
            if (flash_idx != MATRIX_SYMBOL_INVALID) {
                flash_calc_idx   = (int8_t)flash_idx;
                flash_started_ms = timer_read32();
            }
        }

        if (flash_calc_idx != -1) {
            if (force || last_calc_idx != flash_calc_idx) {
                set_matrix_visual(matrix_symbol_get((uint8_t)flash_calc_idx));
                last_calc_idx = flash_calc_idx;
            }

            if (timer_elapsed32(flash_started_ms) < REPEAT_OP_FLASH_MS) {
                return;
            }

            flash_calc_idx = -1;
            last_calc_idx  = -1;
            flash_just_ended = true;
        }

        if (flash_just_ended) {
            set_matrix_visual(matrix_symbol_blank());
            last_calc_idx     = MATRIX_SYMBOL_INVALID;
            flash_just_ended  = false;
            return;
        }

        const uint8_t calc_idx = matrix_symbol_for_op(calc_mode_pending_op());
        if (force || (int8_t)calc_idx != last_calc_idx) {
            if (calc_idx == MATRIX_SYMBOL_INVALID) {
                set_matrix_visual(matrix_symbol_blank());
            } else {
                set_matrix_visual(matrix_symbol_get(calc_idx));
            }
            last_calc_idx = (int8_t)calc_idx;
        }
        return;
    }

    if (force || last_calc_idx != -1) {
        flash_calc_idx    = -1;
        flash_just_ended  = false;
        last_calc_idx     = -1;
        set_matrix_visual(matrix_symbol_get(MATRIX_SYMBOL_BASE));
    }
}


const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BL] = LAYOUT_numpad_4x4(
        /**|  ┌────────┐┌────────┐┌────────┐┌────────┐                       ┌────────┐┌────────┐┌──────────────────┐     |**/
        /**/    KC_P7,    KC_P8,    KC_P9,   KC_PPLS,                          KC_PMNS,   KC_F1,                           /**/
        /**|  └────────┘└────────┘└────────┘│        │                       └────────┘└────────┘│                  │     |**/
        /**|  ┌────────┐┌────────┐┌────────┐│        │                       ┌────────┐┌────────┐│                  │     |**/
        /**/    KC_P4,    KC_P5,    KC_P6,                                    KC_PAST,    KC_F2,        CK_SEL,            /**/
        /**|  └────────┘└────────┘└────────┘└────────┘                       └────────┘└────────┘└──────────────────┘     |**/
        /**|  ┌────────┐┌────────┐┌────────┐┌────────┐                       ┌────────┐┌────────┐┌────────┐┌────────┐     |**/
        /**/    KC_P1,    KC_P2,    KC_P3,   KC_PENT,                         KC_PSLS,    KC_F3,   CK_CPY, LT(_FL, CK_MOD),       /**/
        /**|  └────────┘└────────┘└────────┘│        │                       └────────┘└────────┘└────────┘└────────┘     |**/
        /**|  ┌──────────────────┐┌────────┐│        │                       ┌────────┐┌────────┐┌────────┐┌────────┐     |**/
        /**/          KC_P0,       KC_PDOT,                                    KC_DEL,   CK_MEM,   CK_PST,   CK_CLR   /**/
        /**|  └──────────────────┘└────────┘└────────┘                       └────────┘└────────┘└────────┘└────────┘     |**/
    ),

    [_FL] = LAYOUT_numpad_4x4(
        /**|  ┌────────┐┌────────┐┌────────┐┌────────┐                       ┌────────┐┌────────┐┌──────────────────┐     |**/
        /**/    ______,   ______,   ______,   ______,                          ______,   ______,                          /**/
        /**|  └────────┘└────────┘└────────┘│        │                       └────────┘└────────┘│                  │     |**/
        /**|  ┌────────┐┌────────┐┌────────┐│        │                       ┌────────┐┌────────┐│                  │     |**/
        /**/    ______,   ______,   ______,                                    ______,   ______,       QK_BOOT,           /**/
        /**|  └────────┘└────────┘└────────┘└────────┘                       └────────┘└────────┘└──────────────────┘     |**/
        /**|  ┌────────┐┌────────┐┌────────┐┌────────┐                       ┌────────┐┌────────┐┌────────┐┌────────┐     |**/
        /**/    ______,   ______,   ______,   ______,                          ______,   ______,   ______,   ______,      /**/
        /**|  └────────┘└────────┘└────────┘│        │                       └────────┘└────────┘└────────┘└────────┘     |**/
        /**|  ┌──────────────────┐┌────────┐│        │                       ┌────────┐┌────────┐┌────────┐┌────────┐     |**/
        /**/         ______,        ______,                                    ______,   ______,   ______,   ______       /**/
        /**|  └──────────────────┘└────────┘└────────┘                       └────────┘└────────┘└────────┘└────────┘     |**/
    )
};


void keyboard_post_init_user(void) {
    rgblight_enable_noeeprom();
    rgblight_mode_noeeprom(RGBLIGHT_MODE_RAINBOW_MOOD);

    if (ht16k33_segment_display_init()) {
        ht16k33_segment_display_set_brightness(15);
        ht16k33_segment_display_stop_scroll();
        calc_mode_init();
        calc_startup_display_pending = true;
        if (!calc_mode_is_enabled()) {
            segment_display_blank();
        }
    }

    ht16k33_ready = ht16k33_matrix_init();

    if (ht16k33_ready) {
        ht16k33_matrix_set_rotation(HT16K33_ROTATION_180);
        ht16k33_matrix_set_brightness(15);
        matrix_display_tick(true);
    }
}

void matrix_scan_user(void) {
    if (calc_startup_display_pending && calc_mode_is_enabled()) {
        calc_mode_refresh_display();
        calc_startup_display_pending = false;
    }

    calc_mode_tick();
    matrix_display_tick(false);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (keycode == CK_MEM) {
        calc_mode_set_mem_modifier(record->event.pressed);
        return false;
    }

    if (!record->event.pressed) {
        return true;
    }

    if (calc_mode_is_enabled() && calc_mode_handle_function_key(keycode)) {
        return false;
    }

    if (keycode == CK_MOD) {
        calc_mode_toggle();
        if (calc_mode_is_enabled()) {
            calc_mode_refresh_display();
            matrix_display_tick(true);
        } else {
            segment_display_blank();
            matrix_display_tick(true);
        }
        return false;
    }

    if (calc_mode_is_enabled() && calc_mode_handle_keycode(keycode)) {
        return false;
    }

    switch (keycode) {
        case CK_CPY:
        case CK_PST:
        case CK_CLR:
            return false;
        default:
            return true;
    }
}

#ifdef ENCODER_ENABLE
bool encoder_update_user(uint8_t index, bool clockwise) {
    if (index != 0) {
        return false;
    }

    if (calc_mode_is_enabled()) {
        (void)calc_mode_browse_scientific(clockwise ? 1 : -1);
        matrix_display_tick(true);
        return false;
    }
    
    tap_code(clockwise ? KC_VOLU : KC_VOLD);
    return false;
}
#endif