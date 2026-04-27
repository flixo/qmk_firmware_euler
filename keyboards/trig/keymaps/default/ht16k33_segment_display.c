// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ht16k33_segment_display.h"
#include "ht16k33_segment.h"
#include <string.h>
#include "timer.h"

static const uint8_t ht16k33_segment_display_addresses[HT16K33_SEGMENT_CHAIN_LENGTH] = {
    HT16K33_SEGMENT_LEFT_I2C_ADDRESS,
    HT16K33_SEGMENT_MIDDLE_I2C_ADDRESS,
    HT16K33_SEGMENT_RIGHT_I2C_ADDRESS,
};

#define HT16K33_SCROLL_MAX_SLOTS 96

static bool     ht16k33_scroll_active;
static uint16_t ht16k33_scroll_step_ms;
static uint32_t ht16k33_scroll_last_tick;
static uint8_t  ht16k33_scroll_offset;
static uint8_t  ht16k33_scroll_total_slots;
static char     ht16k33_scroll_chars[HT16K33_SCROLL_MAX_SLOTS];
static bool     ht16k33_scroll_dots[HT16K33_SCROLL_MAX_SLOTS];

static uint8_t ht16k33_pack_slots(const char *text, char *chars, bool *dots, uint8_t max_slots) {
    if (!text || !chars || !dots || max_slots == 0) {
        return 0;
    }

    uint8_t index = 0;
    for (const char *p = text; *p && index < max_slots; p++) {
        if (*p == '.' && index > 0) {
            dots[index - 1] = true;
            continue;
        }

        chars[index] = *p;
        dots[index]  = false;
        index++;
    }

    return index;
}

static bool ht16k33_render_window(const char *chars, const bool *dots, uint8_t start_slot) {
    for (uint8_t display = 0; display < HT16K33_SEGMENT_CHAIN_LENGTH; display++) {
        ht16k33_segment_set_address(ht16k33_segment_display_addresses[display]);
        ht16k33_segment_clear();

        for (uint8_t digit = 0; digit < 4; digit++) {
            const uint8_t slot = (uint8_t)(start_slot + (display * 4) + digit);
            if (!ht16k33_segment_set_char(digit, chars[slot], dots[slot])) {
                return false;
            }
        }

        if (!ht16k33_segment_write()) {
            return false;
        }
    }

    return true;
}

bool ht16k33_segment_display_init(void) {
    for (uint8_t i = 0; i < HT16K33_SEGMENT_CHAIN_LENGTH; i++) {
        if (!ht16k33_segment_init_address(ht16k33_segment_display_addresses[i])) {
            return false;
        }
    }

    return true;
}

bool ht16k33_segment_display_set_brightness(uint8_t level) {
    for (uint8_t i = 0; i < HT16K33_SEGMENT_CHAIN_LENGTH; i++) {
        ht16k33_segment_set_address(ht16k33_segment_display_addresses[i]);
        if (!ht16k33_segment_set_brightness(level)) {
            return false;
        }
    }

    return true;
}

bool ht16k33_segment_display_write_text(const char *text) {
    if (!text) {
        return false;
    }

    char chars[HT16K33_SEGMENT_CHAIN_CHARS] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    bool dots[HT16K33_SEGMENT_CHAIN_CHARS] = {0};

    ht16k33_pack_slots(text, chars, dots, HT16K33_SEGMENT_CHAIN_CHARS);
    return ht16k33_render_window(chars, dots, 0);
}

bool ht16k33_segment_display_start_scroll(const char *text, uint16_t step_ms) {
    if (!text) {
        return false;
    }

    if (step_ms == 0) {
        step_ms = 1;
    }

    for (uint8_t i = 0; i < HT16K33_SCROLL_MAX_SLOTS; i++) {
        ht16k33_scroll_chars[i] = ' ';
        ht16k33_scroll_dots[i]  = false;
    }

    const uint8_t text_slots = ht16k33_pack_slots(text, &ht16k33_scroll_chars[HT16K33_SEGMENT_CHAIN_CHARS], &ht16k33_scroll_dots[HT16K33_SEGMENT_CHAIN_CHARS], (uint8_t)(HT16K33_SCROLL_MAX_SLOTS - (2 * HT16K33_SEGMENT_CHAIN_CHARS)));
    ht16k33_scroll_total_slots = (uint8_t)(text_slots + (2 * HT16K33_SEGMENT_CHAIN_CHARS));

    if (ht16k33_scroll_total_slots <= HT16K33_SEGMENT_CHAIN_CHARS) {
        ht16k33_scroll_total_slots = HT16K33_SEGMENT_CHAIN_CHARS;
    }

    ht16k33_scroll_step_ms   = step_ms;
    ht16k33_scroll_offset    = 0;
    ht16k33_scroll_last_tick = timer_read32();
    ht16k33_scroll_active    = true;

    return ht16k33_render_window(ht16k33_scroll_chars, ht16k33_scroll_dots, 0);
}

void ht16k33_segment_display_stop_scroll(void) {
    ht16k33_scroll_active = false;
}

bool ht16k33_segment_display_is_scrolling(void) {
    return ht16k33_scroll_active;
}

void ht16k33_segment_display_tick(void) {
    if (!ht16k33_scroll_active) {
        return;
    }

    if (timer_elapsed32(ht16k33_scroll_last_tick) < ht16k33_scroll_step_ms) {
        return;
    }

    ht16k33_scroll_last_tick = timer_read32();

    if (ht16k33_scroll_total_slots <= HT16K33_SEGMENT_CHAIN_CHARS) {
        return;
    }

    const uint8_t max_offset = (uint8_t)(ht16k33_scroll_total_slots - HT16K33_SEGMENT_CHAIN_CHARS);
    ht16k33_scroll_offset++;
    if (ht16k33_scroll_offset > max_offset) {
        ht16k33_scroll_offset = 0;
    }

    (void)ht16k33_render_window(ht16k33_scroll_chars, ht16k33_scroll_dots, ht16k33_scroll_offset);
}
