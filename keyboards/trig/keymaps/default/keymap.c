// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "ht16k33_matrix.h"
#include "ht16k33_segment.h"
#include "ht16k33_segment_display.h"
#include "calc_mode.h"
#include "matrix_symbols.h"
#include "raw_hid.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>

static bool ht16k33_ready;
static bool calc_startup_display_pending;
static bool ck_mod_held;
static bool ck_mod_interrupted;
static uint16_t ck_mod_pressed_at;
static bool host_time_valid;
static uint64_t host_time_base_secs;
static uint32_t host_time_base_ms;
static uint32_t host_time_last_render_secs;

#define ______ KC_TRANSPARENT
#define MASKED KC_NO

enum layer_names {
    _BL,
    _UL,
    _FL,
    _TEMPLATE,
};

#define REPEAT_OP_FLASH_MS 300
#define HOST_TIME_TAG_LEN 4
#define HOST_TIME_PAYLOAD_LEN 12
#define HOST_TIME_TZ_OFFSET_SECS (2 * 3600)
#define RAW_HID_PACKET_LEN 32

static void matrix_display_tick(bool force);

static const uint8_t TAG_TIME[4]      = {'t', 'i', 'm', 'e'};
static const uint8_t TAG_PASTE_REQ[4] = {'p', 'a', 's', 't'};
static const uint8_t TAG_COPY_VAL[4]  = {'c', 'o', 'p', 'y'};
static const uint8_t TAG_INPUT_VAL[4] = {'i', 'n', 'p', 't'};

static int16_t raw_hid_find_tag(const uint8_t *data, uint8_t length, const uint8_t tag[4]) {
    if (!data || !tag || length < 4) {
        return -1;
    }

    for (uint8_t i = 0; i + 4 <= length; i++) {
        if (data[i] == tag[0] && data[i + 1] == tag[1] && data[i + 2] == tag[2] && data[i + 3] == tag[3]) {
            return (int16_t)i;
        }
    }

    return -1;
}

static void raw_hid_send_packet(const uint8_t tag[4], const char *payload) {
    uint8_t packet[RAW_HID_PACKET_LEN] = {0};
    memcpy(packet, tag, 4);

    if (payload && payload[0] != '\0') {
        const uint8_t max_copy = (uint8_t)(RAW_HID_PACKET_LEN - 5);
        size_t len = strlen(payload);
        if (len > max_copy) {
            len = max_copy;
        }
        memcpy(&packet[4], payload, len);
        packet[4 + len] = '\0';
    }

    raw_hid_send(packet, RAW_HID_PACKET_LEN);
}

static bool is_leap_year(uint16_t year) {
    return (year % 4U == 0U && year % 100U != 0U) || (year % 400U == 0U);
}

static uint8_t days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }

    return days_per_month[month - 1U];
}

static void unix_to_local_datetime(uint64_t unix_secs, uint16_t *year_out, uint8_t *month_out, uint8_t *day_out, uint8_t *hour_out, uint8_t *min_out) {
    uint64_t local_secs = unix_secs + (uint64_t)HOST_TIME_TZ_OFFSET_SECS;
    uint32_t day_secs   = (uint32_t)(local_secs % 86400ULL);
    uint32_t days       = (uint32_t)(local_secs / 86400ULL);

    *hour_out = (uint8_t)(day_secs / 3600U);
    *min_out  = (uint8_t)((day_secs % 3600U) / 60U);

    uint16_t year = 1970;
    while (true) {
        const uint16_t days_this_year = is_leap_year(year) ? 366U : 365U;
        if (days < days_this_year) {
            break;
        }
        days -= days_this_year;
        year++;
    }

    uint8_t month = 1;
    while (true) {
        const uint8_t dim = days_in_month(year, month);
        if (days < dim) {
            break;
        }
        days -= dim;
        month++;
    }

    *year_out  = year;
    *month_out = month;
    *day_out   = (uint8_t)(days + 1U);
}

static bool host_time_try_parse(const uint8_t *data, uint8_t length, uint64_t *unix_secs_out) {
    if (!data || !unix_secs_out || length < HOST_TIME_PAYLOAD_LEN) {
        return false;
    }

    // Be tolerant to report-id and transport wrappers: locate the tag
    // anywhere in the payload and parse the 8 bytes that follow.
    const int16_t tag_offset = raw_hid_find_tag(data, length, TAG_TIME);

    if (tag_offset < 0) {
        return false;
    }

    uint64_t unix_secs = 0;
    for (uint8_t i = 0; i < 8; i++) {
        unix_secs |= ((uint64_t)data[(uint8_t)tag_offset + HOST_TIME_TAG_LEN + i]) << (8U * i);
    }

    *unix_secs_out = unix_secs;
    return true;
}

static uint64_t host_time_current_secs(void) {
    if (!host_time_valid) {
        return 0;
    }

    return host_time_base_secs + (uint64_t)(timer_elapsed32(host_time_base_ms) / 1000U);
}

static void segment_display_time_tick(bool force) {
    if (!host_time_valid) {
        (void)force;
        (void)ht16k33_segment_display_write_text(" WAIT TIME ");
        return;
    }

    const uint64_t unix_secs = host_time_current_secs();
    const uint32_t render_key = (uint32_t)(unix_secs / 60ULL);
    if (!force && host_time_last_render_secs == render_key) {
        return;
    }

    static const char *month_names[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    uint16_t year = 0;
    uint8_t month = 1;
    uint8_t day = 1;
    uint8_t hour = 0;
    uint8_t min = 0;
    unix_to_local_datetime(unix_secs, &year, &month, &day, &hour, &min);

    char text[16];
    (void)snprintf(text, sizeof(text), "%s %u  %02u.%02u", month_names[month - 1U], (unsigned int)day, (unsigned int)hour, (unsigned int)min);
    (void)ht16k33_segment_display_write_text(text);
    host_time_last_render_secs = render_key;
}

void raw_hid_receive(uint8_t *data, uint8_t length) {
    const int16_t input_tag_offset = raw_hid_find_tag(data, length, TAG_INPUT_VAL);
    if (input_tag_offset >= 0) {
        const uint8_t payload_offset = (uint8_t)input_tag_offset + 4;
        if (payload_offset < length) {
            char input_text[28];
            uint8_t out_len = 0;
            for (uint8_t i = payload_offset; i < length && out_len < sizeof(input_text) - 1; i++) {
                if (data[i] == '\0') {
                    break;
                }
                input_text[out_len++] = (char)data[i];
            }
            input_text[out_len] = '\0';

            if (out_len > 0) {
                if (!calc_mode_is_enabled()) {
                    calc_mode_toggle();
                }

                if (calc_mode_set_input_from_text(input_text)) {
                    matrix_display_tick(true);
                }
            }
        }
        return;
    }

    uint64_t unix_secs = 0;
    if (!host_time_try_parse(data, length, &unix_secs)) {
        return;
    }

    host_time_valid = true;
    host_time_base_secs = unix_secs;
    host_time_base_ms = timer_read32();
    host_time_last_render_secs = UINT32_MAX;

    if (!calc_mode_is_enabled()) {
        segment_display_time_tick(true);
    }
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
        set_matrix_visual(matrix_symbol_blank());
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
        /**/    KC_P1,    KC_P2,    KC_P3,   KC_PENT,                         KC_PSLS,    KC_F3,   CK_CPY,   CK_MOD,       /**/
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
        host_time_last_render_secs = UINT32_MAX;
        if (!calc_mode_is_enabled()) {
            segment_display_time_tick(true);
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
    if (!calc_mode_is_enabled()) {
        segment_display_time_tick(false);
    }
    matrix_display_tick(false);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (ck_mod_held && keycode != CK_MOD && record->event.pressed) {
        ck_mod_interrupted = true;
    }

    if (keycode == CK_MEM) {
        calc_mode_set_mem_modifier(record->event.pressed);
        return false;
    }

    if (keycode == CK_PST && record->event.pressed) {
        if (!calc_mode_is_enabled()) {
            calc_mode_toggle();
            calc_mode_refresh_display();
            matrix_display_tick(true);
        }

        raw_hid_send_packet(TAG_PASTE_REQ, NULL);
        return false;
    }

    if (keycode == CK_CPY && record->event.pressed) {
        if (!calc_mode_is_enabled()) {
            calc_mode_toggle();
            calc_mode_refresh_display();
            matrix_display_tick(true);
        }

        char value_text[24];
        if (calc_mode_get_current_value_text(value_text, sizeof(value_text))) {
            raw_hid_send_packet(TAG_COPY_VAL, value_text);
        }
        return false;
    }

    if (keycode == CK_MOD) {
        if (record->event.pressed) {
            ck_mod_held = true;
            ck_mod_interrupted = false;
            ck_mod_pressed_at = timer_read();
            layer_on(_FL);
            return false;
        }

        layer_off(_FL);

        const bool was_tap = !ck_mod_interrupted && (timer_elapsed(ck_mod_pressed_at) < TAPPING_TERM);
        ck_mod_held = false;

        if (was_tap) {
            calc_mode_toggle();
            if (calc_mode_is_enabled()) {
                calc_mode_refresh_display();
                matrix_display_tick(true);
            } else {
                segment_display_time_tick(true);
                matrix_display_tick(true);                
            }
        }
        return false;
    }

    if (!record->event.pressed) {
        return true;
    }

    if (calc_mode_is_enabled() && calc_mode_handle_function_key(keycode)) {
        return false;
    }

    if (calc_mode_is_enabled() && calc_mode_handle_keycode(keycode)) {
        return false;
    }

    switch (keycode) {
        case CK_CLR:
            return false;
        default:
            return true;
    }
}

#ifdef ENCODER_ENABLE
bool encoder_update_user(uint8_t index, bool clockwise) {
    // Some encoders emit two transitions per detent. Collapse them to one
    // action while keeping the first step after direction changes responsive.
    static int8_t last_dir = 0;
    static bool half_step_phase = false;

    if (index != 0) {
        return false;
    }

    const int8_t dir = clockwise ? 1 : -1;
    if (dir != last_dir) {
        last_dir = dir;
        half_step_phase = false;
    }

    half_step_phase = !half_step_phase;
    if (!half_step_phase) {
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