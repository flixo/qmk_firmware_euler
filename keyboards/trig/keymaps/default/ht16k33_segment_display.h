// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef HT16K33_SEGMENT_LEFT_I2C_ADDRESS
#    define HT16K33_SEGMENT_LEFT_I2C_ADDRESS (0x73 << 1)
#endif
#ifndef HT16K33_SEGMENT_MIDDLE_I2C_ADDRESS
#    define HT16K33_SEGMENT_MIDDLE_I2C_ADDRESS (0x72 << 1)
#endif
#ifndef HT16K33_SEGMENT_RIGHT_I2C_ADDRESS
#    define HT16K33_SEGMENT_RIGHT_I2C_ADDRESS (0x71 << 1)
#endif

#define HT16K33_SEGMENT_CHAIN_LENGTH 3
#define HT16K33_SEGMENT_CHAIN_CHARS  12

bool ht16k33_segment_display_init(void);
bool ht16k33_segment_display_set_brightness(uint8_t level);
bool ht16k33_segment_display_write_text(const char *text);

bool ht16k33_segment_display_start_scroll(const char *text, uint16_t step_ms);
void ht16k33_segment_display_stop_scroll(void);
void ht16k33_segment_display_tick(void);
bool ht16k33_segment_display_is_scrolling(void);
