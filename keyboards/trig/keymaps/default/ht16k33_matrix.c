// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "i2c_master.h"
#include "ht16k33_matrix.h"
#include <string.h>

#define HT16K33_CMD_SYSTEM_SETUP      0x20
#define HT16K33_CMD_SYSTEM_OSCILLATOR 0x01
#define HT16K33_CMD_DISPLAY_SETUP     0x80
#define HT16K33_CMD_DISPLAY_ON        0x01
#define HT16K33_CMD_BLINK_OFF         0x00
#define HT16K33_CMD_DIMMING_SETUP     0xE0
#define HT16K33_BRIGHTNESS_MEDIUM     0x08

static uint8_t ht16k33_i2c_addr;
static ht16k33_rotation_t ht16k33_rotation = HT16K33_ROTATION_0;

static bool ht16k33_write_cmd(uint8_t cmd) {
    return i2c_transmit(ht16k33_i2c_addr, &cmd, 1, 100) == I2C_STATUS_SUCCESS;
}

static bool ht16k33_write_display_rows(const uint8_t rows[8]) {
    uint8_t buffer[17] = {0};
    buffer[0]          = 0x00;

    for (uint8_t row = 0; row < 8; row++) {
        buffer[1 + (row * 2)] = rows[row];
    }

    return i2c_transmit(ht16k33_i2c_addr, buffer, sizeof(buffer), 100) == I2C_STATUS_SUCCESS;
}

static uint8_t map_row_logical_to_hw(uint8_t logical_row) {
    // Board wiring maps logical col N to hardware col (N+1) mod 8.
    // Rotate right by 1 so logical bit0 stays on visible column 0.
    return (uint8_t)((logical_row >> 1) | (logical_row << 7));
}

static bool consume_token(const char **cursor, const char *token) {
    size_t len = strlen(token);
    if (strncmp(*cursor, token, len) == 0) {
        *cursor += len;
        return true;
    }
    return false;
}

static void rotate_rows(const uint8_t src[8], uint8_t dst[8]) {
    memset(dst, 0, 8);

    for (uint8_t r = 0; r < 8; r++) {
        for (uint8_t c = 0; c < 8; c++) {
            if (((src[r] >> c) & 1u) == 0u) {
                continue;
            }

            uint8_t dr = r;
            uint8_t dc = c;

            switch (ht16k33_rotation) {
                case HT16K33_ROTATION_90:
                    dr = c;
                    dc = (uint8_t)(7 - r);
                    break;
                case HT16K33_ROTATION_180:
                    dr = (uint8_t)(7 - r);
                    dc = (uint8_t)(7 - c);
                    break;
                case HT16K33_ROTATION_270:
                    dr = (uint8_t)(7 - c);
                    dc = r;
                    break;
                case HT16K33_ROTATION_0:
                default:
                    break;
            }

            dst[dr] |= (uint8_t)(1u << dc);
        }
    }
}

bool ht16k33_matrix_set_brightness(uint8_t level) {
    if (level > 0x0F) {
        level = 0x0F;
    }

    return ht16k33_write_cmd((uint8_t)(HT16K33_CMD_DIMMING_SETUP | level));
}

bool ht16k33_matrix_set_rotation(ht16k33_rotation_t rotation) {
    if (rotation > HT16K33_ROTATION_270) {
        return false;
    }

    ht16k33_rotation = rotation;
    return true;
}

bool ht16k33_matrix_set_rows(const uint8_t rows[8]) {
    uint8_t rotated[8] = {0};
    uint8_t mapped[8] = {0};

    rotate_rows(rows, rotated);

    for (uint8_t row = 0; row < 8; row++) {
        mapped[row] = map_row_logical_to_hw(rotated[row]);
    }

    return ht16k33_write_display_rows(mapped);
}

bool ht16k33_matrix_set_visual(const char *visual) {
    if (!visual) {
        return false;
    }

    uint8_t rows[8] = {0};
    uint8_t row     = 0;
    uint8_t col     = 0;
    const char *p   = visual;

    while (*p) {
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',' || *p == '|') {
            p++;
            continue;
        }

        bool is_on = false;
        if (consume_token(&p, "⚪")) {
            is_on = true;
        } else if (consume_token(&p, "⚫")) {
            // OFF pixel
        } else {
            return false;
        }

        if (row >= 8 || col >= 8) {
            return false;
        }

        if (is_on) {
            rows[row] |= (uint8_t)(1u << col);
        }

        col++;
        if (col == 8) {
            col = 0;
            row++;
        }
    }

    if (row != 8 || col != 0) {
        return false;
    }

    return ht16k33_matrix_set_rows(rows);
}

bool ht16k33_matrix_show_all_on(void) {
    uint8_t buffer[17] = {0};
    buffer[0]          = 0x00;

    for (uint8_t i = 0; i < 16; i++) {
        buffer[1 + i] = 0xFF;
    }

    return i2c_transmit(ht16k33_i2c_addr, buffer, sizeof(buffer), 100) == I2C_STATUS_SUCCESS;
}

bool ht16k33_matrix_init(void) {
    i2c_init();

    ht16k33_i2c_addr = HT16K33_I2C_ADDRESS;

    if (!ht16k33_write_cmd(HT16K33_CMD_SYSTEM_SETUP | HT16K33_CMD_SYSTEM_OSCILLATOR)) {
        return false;
    }

    if (!ht16k33_write_cmd(HT16K33_CMD_DISPLAY_SETUP | HT16K33_CMD_DISPLAY_ON | HT16K33_CMD_BLINK_OFF)) {
        return false;
    }

    if (!ht16k33_matrix_set_brightness(HT16K33_BRIGHTNESS_MEDIUM)) {
        return false;
    }

    uint8_t rows[8] = {0};
    return ht16k33_write_display_rows(rows);
}
