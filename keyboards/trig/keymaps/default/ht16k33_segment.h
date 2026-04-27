// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef HT16K33_SEGMENT_I2C_ADDRESS
#    define HT16K33_SEGMENT_I2C_ADDRESS (0x71 << 1)
#endif

// Segment bit names for 14-segment displays.
#define HT16K33_SEG_A  0x0001
#define HT16K33_SEG_B  0x0002
#define HT16K33_SEG_C  0x0004
#define HT16K33_SEG_D  0x0008
#define HT16K33_SEG_E  0x0010
#define HT16K33_SEG_F  0x0020
#define HT16K33_SEG_G1 0x0040
#define HT16K33_SEG_G2 0x0080
#define HT16K33_SEG_H  0x0100
#define HT16K33_SEG_J  0x0200
#define HT16K33_SEG_K  0x0400
#define HT16K33_SEG_L  0x0800
#define HT16K33_SEG_M  0x1000
#define HT16K33_SEG_N  0x2000

// Dot/decimal-point bit used by Adafruit 14-segment backpacks.
#define HT16K33_SEGMENT_DP 0x4000

typedef enum {
    HT16K33_SEGMENT_BLINK_OFF   = 0,
    HT16K33_SEGMENT_BLINK_2HZ   = 1,
    HT16K33_SEGMENT_BLINK_1HZ   = 2,
    HT16K33_SEGMENT_BLINK_HALF  = 3,
} ht16k33_segment_blink_t;

bool ht16k33_segment_init(void);
bool ht16k33_segment_init_address(uint8_t i2c_address);
void ht16k33_segment_set_address(uint8_t i2c_address);
uint8_t ht16k33_segment_get_address(void);
bool ht16k33_segment_set_brightness(uint8_t level);
bool ht16k33_segment_set_blink(ht16k33_segment_blink_t blink);
bool ht16k33_segment_set_display_on(bool on);

void ht16k33_segment_clear(void);
bool ht16k33_segment_set_ram(const uint16_t ram_words[8]);
bool ht16k33_segment_set_digit_raw(uint8_t digit, uint16_t segments, bool dot);
bool ht16k33_segment_set_char(uint8_t digit, char c, bool dot);
bool ht16k33_segment_write_text(const char *text);
bool ht16k33_segment_write(void);
