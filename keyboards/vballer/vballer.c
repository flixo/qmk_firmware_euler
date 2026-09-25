// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

void matrix_init_pins(void) {
    gpio_set_pin_input_low(GP2);
    gpio_set_pin_input_low(GP3);
    gpio_set_pin_input_low(GP4);
    gpio_set_pin_input_low(GP5);
    gpio_set_pin_input_low(GP6);
    gpio_set_pin_input_low(GP7);
    gpio_set_pin_input_low(GP8);
    gpio_set_pin_input_high(GP10);
    gpio_set_pin_input_high(GP14);
}

void matrix_read_cols_on_row(matrix_row_t current_matrix[], uint8_t current_row) {
    if (current_row != 0) {
        return;
    }

    current_matrix[0] = (gpio_read_pin(GP2) << 0) | (gpio_read_pin(GP3) << 1) | (gpio_read_pin(GP4) << 2) | (gpio_read_pin(GP5) << 3) | (gpio_read_pin(GP6) << 4) | (gpio_read_pin(GP7) << 5) | (gpio_read_pin(GP8) << 6) | (!gpio_read_pin(GP10) << 7) | (!gpio_read_pin(GP14) << 8);
}