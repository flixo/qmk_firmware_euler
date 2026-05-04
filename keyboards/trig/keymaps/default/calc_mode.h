// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include QMK_KEYBOARD_H
#include <stddef.h>

enum custom_keycodes {
    KC_FN = SAFE_RANGE,
    CK_MEM,
    CK_SEL,
    CK_CPY,
    CK_PST,
    CK_CLR,
    CK_MOD,
};

void calc_mode_init(void);
bool calc_mode_is_enabled(void);
void calc_mode_toggle(void);
void calc_mode_refresh_display(void);
void calc_mode_tick(void);
void calc_mode_set_render_suppressed(bool suppressed);
void calc_mode_cycle_angle_mode(void);
bool calc_mode_angle_mode_is_degrees(void);
void calc_mode_set_mem_modifier(bool held);
bool calc_mode_handle_function_key(uint16_t keycode);
char calc_mode_pending_op(void);
char calc_mode_take_repeat_flash_op(void);
uint8_t calc_mode_take_repeat_flash_symbol(void);
bool calc_mode_browse_scientific(int8_t delta);
bool calc_mode_confirm_scientific(void);
uint8_t calc_mode_scientific_matrix_symbol(void);
bool calc_mode_scientific_underline_visible(void);
bool calc_mode_handle_keycode(uint16_t keycode);
bool calc_mode_get_current_value_text(char *out, size_t out_size);
bool calc_mode_set_input_from_text(const char *text);