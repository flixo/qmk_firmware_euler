// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#include "calc_mode.h"
#include "ht16k33_segment_display.h"
#include "matrix_symbols.h"
#include <float.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "timer.h"
#include <limits.h>

/*
 * Calculator mode engine for the trig keymap.
 *
 * Architecture overview
 * =====================
 * This module is the single source of truth for calculator runtime state.
 * Input handlers mutate state, render helpers project state to display text,
 * and the periodic tick updates timed/animated state (blink phases, status).
 *
 * State-machine model
 * ===================
 * Core compute states:
 * - Idle accumulator: no active numeric entry, accumulator is current value.
 * - Input active: digit/decimal entry is being built in calc_input_value and
 *   mirrored in calc_input_text to preserve exact typed representation.
 * - Pending op: calc_pending_op holds a waiting binary operation.
 * - Error state: calc_error gates most input until clear/reset.
 *
 * Overlay sub-states (can coexist with core compute states):
 * - Memory preview: blinks selected bank value until confirm/cancel.
 * - Status message: short-lived text (for example STORED Mx).
 * - Scientific browse: encoder-selected function with blink phase.
 * - Scientific label: left-side function label while browsing/selection.
 *
 * Input interaction
 * =================
 * - process_record_user in keymap.c forwards keys into this module.
 * - Numeric and operator keys update input/accumulator/pending-op state.
 * - F1/F2/F3 + CK_MEM drive memory store/preview/load flows.
 * - Encoder updates scientific browse index via calc_mode_browse_scientific().
 * - CK_SEL confirms scientific selection and executes mapped behavior.
 * - CK_CLR/KC_DEL can cancel overlays or perform full reset depending state.
 *
 * Display interaction
 * ===================
 * Segment display (this module owns content):
 * - calc_render_display() applies strict priority:
 *   status > error > memory preview > typed input > accumulator/result.
 * - Values are formatted for a 12-slot field with dot-aware packing.
 * - Scientific labels overlay the left side while value remains right-aligned.
 *
 * Matrix display (owned by keymap.c, informed by this module):
 * - keymap.c queries calc_mode_pending_op() for operator icons.
 * - keymap.c queries calc_mode_scientific_matrix_symbol() for browse icon.
 * - calc_mode_tick() maintains blink phase used by both segment label and
 *   matrix scientific icon so they stay visually synchronized.
 */

#define CALC_DISPLAY_DECIMALS 6
#define CALC_DISPLAY_SCALE    1000000.0
#define CALC_MEMORY_BANKS      3
#define CALC_MEM_BLINK_MS      120
#define CALC_SCI_BLINK_MS      440
#define CALC_STATUS_MSG_MS     500

/* Core calculator runtime state. */
static bool    calc_mode_enabled;
static bool    calc_error;
static bool    calc_error_overflow;
static bool    calc_input_active;
static bool    calc_input_decimal;
static double  calc_input_value;
static double  calc_accumulator;
static double  calc_decimal_place;
static char    calc_pending_op;
static char    calc_last_eval_op;
static double  calc_last_eval_rhs;
static bool    calc_last_eval_valid;
static char    calc_repeat_flash_op;

/* Textual input path keeps typed decimals/sign intact for display. */
static char    calc_input_text[32];
static uint8_t calc_input_text_len;

/* Memory-bank workflow state (F1/F2/F3 + CK_MEM modifier). */
static double  calc_memory_banks[CALC_MEMORY_BANKS];
static bool    calc_mem_modifier_held;
static bool    calc_mem_preview_active;
static bool    calc_mem_preview_visible;
static uint8_t calc_mem_preview_bank;
static uint32_t calc_mem_preview_started_ms;

/* Short transient status messages (for example STORED Mx). */
static bool    calc_status_active;
static uint32_t calc_status_started_ms;
static char    calc_status_text[16];

/* Scientific function browse/select state used by encoder + CK_SEL. */
static bool    calc_sci_active;
static bool    calc_sci_browsing;
static bool    calc_sci_visible;
static uint8_t calc_sci_index;
static uint32_t calc_sci_blink_started_ms;
static bool    calc_sci_label_active;
static bool    calc_last_sci_unary_valid;
static uint8_t calc_last_sci_unary_index;
static uint8_t calc_repeat_flash_symbol;

static void calc_format_value(double value, char *out, size_t out_size);
static void calc_input_text_clear(void);
static void calc_input_rebuild_from_text(void);
static double calc_current_value(void);
static bool calc_value_overflowed(double value);
static double calc_round_for_display(double value);
static uint8_t calc_pack_slots_local(const char *text, char *chars, bool *dots, uint8_t max_slots);
static void calc_slots_to_text(const char *chars, const bool *dots, uint8_t slots, char *out, size_t out_size);
static void calc_clear_scientific_selection(void);

/* Scientific label text shown while browsing a function. */
static const char *calc_scientific_label(uint8_t sci_index) {
    switch (sci_index) {
        case MATRIX_SCI_POWER:
            return "POW";
        case MATRIX_SCI_SQRT:
            return "SQRT";
        case MATRIX_SCI_PI:
            return "PI";
        case MATRIX_SCI_SIN:
            return "SIN";
        case MATRIX_SCI_COS:
            return "COS";
        case MATRIX_SCI_SEC:
            return "SEC";
        case MATRIX_SCI_CSC:
            return "CSC";
        case MATRIX_SCI_TAN:
            return "TAN";
        case MATRIX_SCI_COT:
            return "COT";
        case MATRIX_SCI_FACT:
            return "FACT";
        case MATRIX_SCI_HYPOT:
            return "HYP";
        default:
            return "";
    }
}

/*
 * Overlays a fixed left label and right-aligned value inside a 12-slot display.
 * Dots are attached to their preceding value slots to match HT16K33 packing.
 */
static void calc_overlay_left_label(const char *label, const char *value_text, char *out, size_t out_size) {
    if (!label || !value_text || !out || out_size == 0) {
        return;
    }

    char render_chars[12] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    bool render_dots[12]  = {0};

    char value_chars[12] = {' '};
    bool value_dots[12]  = {0};
    const uint8_t value_slots = calc_pack_slots_local(value_text, value_chars, value_dots, 12);
    const uint8_t start_slot = (value_slots < 12) ? (uint8_t)(12 - value_slots) : 0;

    for (uint8_t i = 0; i < value_slots; i++) {
        const uint8_t dst = (uint8_t)(start_slot + i);
        if (dst >= 12) {
            break;
        }
        render_chars[dst] = value_chars[i];
        render_dots[dst]  = value_dots[i];
    }

    // Overlay the label after value placement so value alignment stays stable.
    uint8_t label_len = 0;
    for (; label_len < 12 && label[label_len] != '\0'; label_len++) {
        render_chars[label_len] = label[label_len];
        render_dots[label_len]  = false;
    }

    // Always leave two visual spaces after the label.
    for (uint8_t i = 0; i < 2; i++) {
        const uint8_t dst = (uint8_t)(label_len + i);
        if (dst >= 12) {
            break;
        }
        render_chars[dst] = ' ';
        render_dots[dst]  = false;
    }

    calc_slots_to_text(render_chars, render_dots, 12, out, out_size);
}

/* Commits a computed result and returns to neutral entry state. */
static void calc_commit_result(double result) {
    calc_accumulator   = calc_round_for_display(result);
    calc_input_active  = false;
    calc_input_decimal = false;
    calc_input_value   = 0.0;
    calc_decimal_place = 0.1;
    calc_pending_op    = 0;
    calc_last_eval_valid = false;
    calc_input_text_clear();
}

/* Marks a domain/invalid math error (non-overflow). */
static bool calc_set_math_error(void) {
    calc_error          = true;
    calc_error_overflow = false;
    calc_last_eval_valid = false;
    return false;
}

/* Applies scientific functions that immediately evaluate current value. */
static bool calc_apply_unary_scientific(uint8_t sci_index) {
    const double input = calc_current_value();
    double       result = 0.0;

    switch (sci_index) {
        case MATRIX_SCI_SQRT:
            if (input < 0.0) {
                return calc_set_math_error();
            }
            result = sqrt(input);
            break;
        case MATRIX_SCI_SIN:
            result = sin(input);
            break;
        case MATRIX_SCI_COS:
            result = cos(input);
            break;
        case MATRIX_SCI_SEC: {
            const double c = cos(input);
            if (fabs(c) < 0.000000000001) {
                return calc_set_math_error();
            }
            result = 1.0 / c;
            break;
        }
        case MATRIX_SCI_CSC: {
            const double s = sin(input);
            if (fabs(s) < 0.000000000001) {
                return calc_set_math_error();
            }
            result = 1.0 / s;
            break;
        }
        case MATRIX_SCI_TAN:
            result = tan(input);
            break;
        case MATRIX_SCI_COT: {
            const double s = sin(input);
            if (fabs(s) < 0.000000000001) {
                return calc_set_math_error();
            }
            result = cos(input) / s;
            break;
        }
        case MATRIX_SCI_FACT: {
            // Factorial is defined here for non-negative integers only.
            if (input < 0.0) {
                return calc_set_math_error();
            }

            const double n_rounded = floor(input + 0.5);
            if (fabs(input - n_rounded) > 0.000000001) {
                return calc_set_math_error();
            }

            const uint16_t n = (uint16_t)n_rounded;
            if (n > 170U) {
                calc_error          = true;
                calc_error_overflow = true;
                return false;
            }

            result = 1.0;
            for (uint16_t i = 2; i <= n; i++) {
                result *= (double)i;
            }
            break;
        }
        default:
            return false;
    }

    if (calc_value_overflowed(result)) {
        calc_error          = true;
        calc_error_overflow = true;
        return false;
    }

    calc_commit_result(result);
    calc_last_sci_unary_valid = true;
    calc_last_sci_unary_index = sci_index;
    return true;
}

/* Arms a two-operand scientific operation and waits for rhs input. */
static bool calc_prepare_binary_scientific(uint8_t sci_index) {
    const double lhs = calc_current_value();

    if (calc_value_overflowed(lhs)) {
        calc_error          = true;
        calc_error_overflow = true;
        return false;
    }

    calc_accumulator   = calc_round_for_display(lhs);
    calc_input_active  = false;
    calc_input_decimal = false;
    calc_input_value   = 0.0;
    calc_decimal_place = 0.1;
    calc_input_text_clear();
    calc_last_eval_valid = false;
    calc_last_sci_unary_valid = false;

    if (sci_index == MATRIX_SCI_POWER) {
        calc_pending_op = '^';
        return true;
    }

    if (sci_index == MATRIX_SCI_HYPOT) {
        calc_pending_op = 'H';
        return true;
    }

    return false;
}

/* PI acts as a shorthand constant entry. */
static bool calc_insert_pi_value(void) {
    static const char pi_text[] = "3.141592653589793";
    const size_t pi_len = strlen(pi_text);

    if (pi_len >= sizeof(calc_input_text)) {
        return false;
    }

    memcpy(calc_input_text, pi_text, pi_len + 1);
    calc_input_text_len = (uint8_t)pi_len;
    calc_input_rebuild_from_text();

    calc_error = false;
    calc_error_overflow = false;
    calc_last_eval_valid = false;
    calc_last_sci_unary_valid = false;
    return true;
}

static void calc_clear_scientific_selection(void) {
    calc_sci_active = false;
    calc_sci_browsing = false;
    calc_sci_visible = false;
    calc_sci_label_active = false;
}

/* Packs display text into character slots where '.' attaches to previous slot. */
static uint8_t calc_pack_slots_local(const char *text, char *chars, bool *dots, uint8_t max_slots) {
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

/* Rebuilds render string from packed slots and dot flags. */
static void calc_slots_to_text(const char *chars, const bool *dots, uint8_t slots, char *out, size_t out_size) {
    if (!chars || !dots || !out || out_size == 0) {
        return;
    }

    size_t pos = 0;
    for (uint8_t i = 0; i < slots && pos < out_size - 1; i++) {
        out[pos++] = chars[i];
        if (dots[i] && pos < out_size - 1) {
            out[pos++] = '.';
        }
    }
    out[pos] = '\0';
}

/* Memory preview shows Mx label and blinks bank value for load confirmation. */
static void calc_render_memory_preview_display(void) {
    char preview_chars[12] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    bool preview_dots[12]  = {0};

    preview_chars[0] = 'M';
    preview_chars[1] = (char)('1' + calc_mem_preview_bank);
    preview_chars[2] = ' ';

    if (calc_mem_preview_visible) {
        char value[16];
        calc_format_value(calc_memory_banks[calc_mem_preview_bank], value, sizeof(value));

        char value_chars[12] = {' '};
        bool value_dots[12]  = {0};
        const uint8_t value_slots = calc_pack_slots_local(value, value_chars, value_dots, 12);

        uint8_t start_slot = (value_slots < 12) ? (uint8_t)(12 - value_slots) : 0;
        if (start_slot < 3) {
            start_slot = 3;
        }

        for (uint8_t i = 0; i < value_slots; i++) {
            const uint8_t dst = (uint8_t)(start_slot + i);
            if (dst >= 12) {
                break;
            }
            preview_chars[dst] = value_chars[i];
            preview_dots[dst]  = value_dots[i];
        }
    }

    char text[32];
    calc_slots_to_text(preview_chars, preview_dots, 12, text, sizeof(text));
    (void)ht16k33_segment_display_write_text(text);
}

static int8_t calc_bank_index_from_keycode(uint16_t keycode) {
    switch (keycode) {
        case KC_F1:
            return 0;
        case KC_F2:
            return 1;
        case KC_F3:
            return 2;
        default:
            return -1;
    }
}

static bool calc_sci_is_constant(uint8_t sci_index) {
    return sci_index == MATRIX_SCI_PI;
}

/* Wraps encoder browse index over the scientific function table. */
static uint8_t calc_wrap_sci_index(int16_t index) {
    while (index < 0) {
        index += MATRIX_SCI_COUNT;
    }
    while (index >= MATRIX_SCI_COUNT) {
        index -= MATRIX_SCI_COUNT;
    }
    return (uint8_t)index;
}

static double calc_current_value(void) {
    return calc_input_active ? calc_input_value : calc_accumulator;
}

static bool calc_value_overflowed(double value) {
    return (value != value) || (value > DBL_MAX) || (value < -DBL_MAX);
}

/* Rounds with fixed scaling while avoiding int64 overflow near extremes. */
static double calc_round_for_display(double value) {
    const double safe_limit = (double)INT64_MAX / CALC_DISPLAY_SCALE;
    if (value > safe_limit || value < -safe_limit) {
        // Too large for fixed-scale rounding without int64 overflow.
        return value;
    }

    const double scaled = value * CALC_DISPLAY_SCALE;
    const double rounded = (scaled >= 0.0) ? (double)((int64_t)(scaled + 0.5)) : (double)((int64_t)(scaled - 0.5));
    return rounded / CALC_DISPLAY_SCALE;
}

static uint8_t calc_u64_digits(uint64_t value) {
    uint8_t digits = 1;
    while (value >= 10) {
        value /= 10;
        digits++;
    }
    return digits;
}

static uint8_t calc_u16_digits(uint16_t value) {
    uint8_t digits = 1;
    while (value >= 10) {
        value /= 10;
        digits++;
    }
    return digits;
}

static size_t calc_u64_to_str(uint64_t value, char *out, size_t out_size) {
    if (out_size == 0) {
        return 0;
    }

    char tmp[21];
    size_t n = 0;

    do {
        tmp[n++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value > 0 && n < sizeof(tmp));

    size_t copy = (n < (out_size - 1)) ? n : (out_size - 1);
    for (size_t i = 0; i < copy; i++) {
        out[i] = tmp[n - 1 - i];
    }
    out[copy] = '\0';
    return copy;
}

static uint8_t calc_display_slot_count(const char *text) {
    if (!text) {
        return 0;
    }

    uint8_t slots = 0;
    for (const char *p = text; *p; p++) {
        if (*p == '.' && slots > 0) {
            continue;
        }
        slots++;
    }

    return slots;
}

/* Right-aligns packed display text into 12-slot segment field. */
static void calc_right_align_for_segment(const char *value, char *out, size_t out_size) {
    if (!value || !out || out_size == 0) {
        return;
    }

    const uint8_t slot_count = calc_display_slot_count(value);
    const uint8_t pad_slots  = (slot_count < 12) ? (uint8_t)(12 - slot_count) : 0;

    size_t pos = 0;
    while (pos < pad_slots && pos < out_size - 1) {
        out[pos++] = ' ';
    }

    for (const char *p = value; *p && pos < out_size - 1; p++) {
        out[pos++] = *p;
    }

    out[pos] = '\0';
}

/* Uses compact scientific notation for very large/small magnitudes. */
static bool calc_should_use_scientific(double value) {
    if (value == 0.0) {
        return false;
    }

    if (value < 0.0) {
        value = -value;
    }

    return (value >= 10000000000.0) || (value < 0.0001);
}

/* Scientific formatter constrained to fit 12-slot display budget. */
static void calc_format_scientific(double value, char *out, size_t out_size) {
    bool negative = false;
    if (value < 0.0) {
        negative = true;
        value    = -value;
    }

    int16_t exponent = 0;
    while (value >= 10.0 && exponent < 999) {
        value /= 10.0;
        exponent++;
    }
    while (value > 0.0 && value < 1.0 && exponent > -999) {
        value *= 10.0;
        exponent--;
    }

    const uint8_t lead_digit = (uint8_t)value;
    double        frac       = value - (double)lead_digit;

    const uint16_t exp_abs    = (exponent < 0) ? (uint16_t)(-exponent) : (uint16_t)exponent;
    const uint8_t  exp_digits = calc_u16_digits(exp_abs);
    const uint8_t  reserved   = (negative ? 1 : 0) + 1 + 1 + 1 + 1 + exp_digits;

    uint8_t max_frac = 0;
    if (reserved < 12) {
        max_frac = (uint8_t)(12 - reserved);
    }
    if (max_frac > 6) {
        max_frac = 6;
    }

    uint8_t frac_digits[6];
    uint8_t frac_count = 0;
    for (uint8_t i = 0; i < max_frac; i++) {
        frac *= 10.0;
        uint8_t digit = (uint8_t)frac;
        if (digit > 9) {
            digit = 9;
        }
        frac_digits[frac_count++] = digit;
        frac -= (double)digit;
        if (frac < 0.0) {
            frac = 0.0;
        }
    }

    while (frac_count > 0 && frac_digits[frac_count - 1] == 0) {
        frac_count--;
    }

    char   sci[32];
    size_t pos = 0;

    if (negative && pos < sizeof(sci) - 1) {
        sci[pos++] = '-';
    }

    sci[pos++] = (char)('0' + (lead_digit % 10));

    if (frac_count > 0 && pos < sizeof(sci) - 1) {
        sci[pos++] = '.';
        for (uint8_t i = 0; i < frac_count && pos < sizeof(sci) - 1; i++) {
            sci[pos++] = (char)('0' + frac_digits[i]);
        }
    }

    if (pos < sizeof(sci) - 1) {
        sci[pos++] = 'E';
    }
    if (pos < sizeof(sci) - 1) {
        sci[pos++] = (exponent < 0) ? '-' : '+';
    }

    pos += calc_u64_to_str((uint64_t)exp_abs, &sci[pos], sizeof(sci) - pos);
    sci[pos] = '\0';

    size_t len = strlen(sci);
    if (len > 12) {
        len = 12;
    }

    size_t copy = len;
    if (copy > out_size - 1) {
        copy = out_size - 1;
    }
    memcpy(out, sci, copy);
    out[copy] = '\0';
}

/* Full mode reset used by clear/toggle and mode init. */
static void calc_reset_all(void) {
    calc_error         = false;
    calc_error_overflow = false;
    calc_input_active  = false;
    calc_input_decimal = false;
    calc_input_value   = 0.0;
    calc_accumulator   = 0.0;
    calc_decimal_place = 0.1;
    calc_pending_op    = 0;
    calc_last_eval_op    = 0;
    calc_last_eval_rhs   = 0.0;
    calc_last_eval_valid = false;
    calc_repeat_flash_op = 0;
    calc_repeat_flash_symbol = MATRIX_SYMBOL_INVALID;
    calc_input_text[0]   = '\0';
    calc_input_text_len  = 0;
    calc_mem_preview_active = false;
    calc_mem_preview_visible = false;
    calc_mem_preview_bank = 0;
    calc_mem_preview_started_ms = 0;
    calc_status_active = false;
    calc_status_started_ms = 0;
    calc_status_text[0] = '\0';
    calc_clear_scientific_selection();
    calc_sci_index = MATRIX_SCI_POWER;
    calc_sci_blink_started_ms = 0;
    calc_last_sci_unary_valid = false;
    calc_last_sci_unary_index = MATRIX_SCI_SQRT;
}

static void calc_input_text_clear(void) {
    calc_input_text[0]  = '\0';
    calc_input_text_len = 0;
}

/* Rebuilds numeric input state from the editable input text buffer. */
static void calc_input_rebuild_from_text(void) {
    if (calc_input_text_len == 0) {
        calc_input_active  = false;
        calc_input_decimal = false;
        calc_input_value   = 0.0;
        calc_decimal_place = 0.1;
        return;
    }

    bool   negative      = false;
    bool   seen_decimal  = false;
    double value         = 0.0;
    double decimal_place = 0.1;

    for (uint8_t i = 0; i < calc_input_text_len; i++) {
        const char ch = calc_input_text[i];

        if (i == 0 && ch == '-') {
            negative = true;
            continue;
        }

        if (ch == '.') {
            if (seen_decimal) {
                continue;
            }
            seen_decimal = true;
            continue;
        }

        if (ch < '0' || ch > '9') {
            continue;
        }

        const uint8_t digit = (uint8_t)(ch - '0');
        if (!seen_decimal) {
            value = value * 10.0 + (double)digit;
        } else {
            value += (double)digit * decimal_place;
            decimal_place *= 0.1;
        }
    }

    if (negative) {
        value = -value;
    }

    calc_input_active  = true;
    calc_input_decimal = seen_decimal;
    calc_input_value   = calc_round_for_display(value);
    calc_decimal_place = seen_decimal ? decimal_place : 0.1;
}

static void calc_input_text_append(char ch) {
    if (calc_input_text_len >= sizeof(calc_input_text) - 1) {
        return;
    }

    calc_input_text[calc_input_text_len++] = ch;
    calc_input_text[calc_input_text_len]   = '\0';
}

static void calc_input_text_toggle_sign(void) {
    if (calc_input_text_len == 0) {
        calc_input_text_append('-');
        calc_input_text_append('0');
        return;
    }

    if (calc_input_text[0] == '-') {
        memmove(calc_input_text, &calc_input_text[1], calc_input_text_len);
        calc_input_text_len--;
        return;
    }

    if (calc_input_text_len >= sizeof(calc_input_text) - 1) {
        return;
    }

    memmove(&calc_input_text[1], calc_input_text, calc_input_text_len + 1);
    calc_input_text[0] = '-';
    calc_input_text_len++;
}

static void calc_format_value(double value, char *out, size_t out_size) {
    if (out_size == 0) {
        return;
    }

    if (value != value) {
        (void)snprintf(out, out_size, "ERR");
        return;
    }

    if (value > -0.0000005 && value < 0.0000005) {
        value = 0.0;
    }

    value = calc_round_for_display(value);

    if (calc_should_use_scientific(value)) {
        calc_format_scientific(value, out, out_size);
        return;
    }

    bool negative = false;
    if (value < 0.0) {
        negative = true;
        value    = -value;
    }

    const uint64_t scaled = (uint64_t)(value * CALC_DISPLAY_SCALE + 0.5);
    const uint64_t whole  = scaled / (uint64_t)CALC_DISPLAY_SCALE;
    uint32_t       frac_u = (uint32_t)(scaled % (uint64_t)CALC_DISPLAY_SCALE);

    uint8_t frac_digits[CALC_DISPLAY_DECIMALS];
    uint8_t frac_count = CALC_DISPLAY_DECIMALS;
    for (int8_t i = (int8_t)CALC_DISPLAY_DECIMALS - 1; i >= 0; i--) {
        frac_digits[i] = (uint8_t)(frac_u % 10U);
        frac_u /= 10U;
    }

    while (frac_count > 0 && frac_digits[frac_count - 1] == 0) {
        frac_count--;
    }

    uint8_t whole_digits = calc_u64_digits(whole);
    uint8_t base_len     = whole_digits + (negative ? 1 : 0);
    if (base_len < 12 && frac_count > 0) {
        uint8_t max_frac = (uint8_t)(12 - base_len - 1);
        if (frac_count > max_frac) {
            frac_count = max_frac;
        }
    } else {
        frac_count = 0;
    }

    char   fixed[32];
    size_t pos = 0;
    if (negative && pos < sizeof(fixed) - 1) {
        fixed[pos++] = '-';
    }

    pos += calc_u64_to_str(whole, &fixed[pos], sizeof(fixed) - pos);

    if (frac_count > 0 && pos < sizeof(fixed) - 1) {
        fixed[pos++] = '.';
        for (uint8_t i = 0; i < frac_count && pos < sizeof(fixed) - 1; i++) {
            fixed[pos++] = (char)('0' + frac_digits[i]);
        }
    }
    fixed[pos] = '\0';

    size_t len = strlen(fixed);
    if (len > 12) {
        len = 12;
    }

    size_t copy = len;
    if (copy > out_size - 1) {
        copy = out_size - 1;
    }
    memcpy(out, fixed, copy);
    out[copy] = '\0';
}

/* Main segment rendering path with status/error/memory/input priorities. */
static void calc_render_display(void) {
    if (calc_status_active) {
        (void)ht16k33_segment_display_write_text(calc_status_text);
        return;
    }

    if (calc_error) {
        if (calc_error_overflow) {
            (void)ht16k33_segment_display_write_text("ERR OVERFLOW");
        } else {
            (void)ht16k33_segment_display_write_text("CALC  ERR");
        }
        return;
    }

    if (calc_mem_preview_active) {
        calc_render_memory_preview_display();
        return;
    }

    const bool show_sci_label = calc_sci_label_active && (!calc_sci_browsing || calc_sci_visible);

    if (calc_input_active && calc_input_text_len > 0) {
        char text[24];
        if (show_sci_label) {
            calc_overlay_left_label(calc_scientific_label(calc_sci_index), calc_input_text, text, sizeof(text));
        } else {
            calc_right_align_for_segment(calc_input_text, text, sizeof(text));
        }
        (void)ht16k33_segment_display_write_text(text);
        return;
    }

    const double display_value = calc_input_active ? calc_input_value : calc_accumulator;
    if (calc_value_overflowed(display_value)) {
        calc_error          = true;
        calc_error_overflow = true;
        (void)ht16k33_segment_display_write_text("ERR OVERFLOW");
        return;
    }

    char value[16];
    calc_format_value(display_value, value, sizeof(value));

    char text[24];
    if (show_sci_label) {
        calc_overlay_left_label(calc_scientific_label(calc_sci_index), value, text, sizeof(text));
    } else {
        calc_right_align_for_segment(value, text, sizeof(text));
    }
    (void)ht16k33_segment_display_write_text(text);
}

/* Shared operator evaluator for basic ops and pending scientific binary ops. */
static bool calc_apply_op(char op, double rhs) {
    double result = calc_accumulator;

    switch (op) {
        case '+':
            result = calc_accumulator + rhs;
            break;
        case '-':
            result = calc_accumulator - rhs;
            break;
        case '*':
            result = calc_accumulator * rhs;
            break;
        case '/':
            if (rhs == 0.0) {
                calc_error          = true;
                calc_error_overflow = false;
                return false;
            }
            result = calc_accumulator / rhs;
            break;
        case '^':
            result = pow(calc_accumulator, rhs);
            break;
        case 'H':
            result = hypot(calc_accumulator, rhs);
            break;
        default:
            result = rhs;
            break;
    }

    if (calc_value_overflowed(result)) {
        calc_error          = true;
        calc_error_overflow = true;
        return false;
    }

    calc_accumulator = calc_round_for_display(result);
    return true;
}

/* Numeric key entry path updates value and exact typed text. */
static void calc_push_digit(uint8_t digit) {
    if (!calc_input_active) {
        calc_input_active  = true;
        calc_input_decimal = false;
        calc_input_value   = 0.0;
        calc_decimal_place = 0.1;
        calc_last_eval_valid = false;
        calc_last_sci_unary_valid = false;
        calc_input_text_clear();
    }

    if (!calc_input_decimal) {
        calc_input_value = (calc_input_value * 10.0) + (double)digit;
    } else {
        calc_input_value += (double)digit * calc_decimal_place;
        calc_decimal_place *= 0.1;
    }

    calc_input_text_append((char)('0' + digit));

    if (calc_value_overflowed(calc_input_value)) {
        calc_error          = true;
        calc_error_overflow = true;
    }
}

/* Operator key handling with '-' double-tap sign toggle behavior. */
static void calc_handle_operator(char op) {
    if (calc_error) {
        return;
    }

    calc_last_sci_unary_valid = false;

    // Double-tap '-' when '-' is pending to toggle sign and clear pending op.
    if (op == '-' && calc_pending_op == '-') {
        if (calc_input_active) {
            calc_input_value = calc_round_for_display(-calc_input_value);
            calc_input_text_toggle_sign();
        } else {
            calc_accumulator = calc_round_for_display(-calc_accumulator);
        }

        calc_pending_op = 0;
        calc_last_eval_valid = false;
        return;
    }

    if (calc_input_active) {
        if (!calc_apply_op(calc_pending_op, calc_input_value)) {
            return;
        }
        calc_input_active  = false;
        calc_input_decimal = false;
        calc_input_value   = 0.0;
        calc_decimal_place = 0.1;
        calc_input_text_clear();
    }

    calc_pending_op = op;
    calc_last_eval_valid = false;
}

/* Equals executes pending op or repeats last op when no new input exists. */
static void calc_handle_equals(void) {
    if (calc_error) {
        return;
    }

    if (calc_pending_op == 0) {
        if (calc_input_active) {
            calc_accumulator = calc_round_for_display(calc_input_value);
            calc_input_active = false;
            calc_input_decimal = false;
            calc_decimal_place = 0.1;
            calc_last_eval_valid = false;
            calc_input_text_clear();
            return;
        }

        if (calc_last_eval_valid) {
            if (calc_apply_op(calc_last_eval_op, calc_last_eval_rhs)) {
                calc_repeat_flash_op = calc_last_eval_op;
            }
            return;
        }

        if (calc_last_sci_unary_valid) {
            if (calc_apply_unary_scientific(calc_last_sci_unary_index)) {
                calc_repeat_flash_symbol = matrix_symbol_for_scientific(calc_last_sci_unary_index);
            }
        }
        return;
    }

    const double rhs = calc_input_active ? calc_input_value : calc_accumulator;
    const char   op  = calc_pending_op;

    if (!calc_apply_op(op, rhs)) {
        return;
    }

    calc_last_eval_op    = op;
    calc_last_eval_rhs   = rhs;
    calc_last_eval_valid = true;
    calc_last_sci_unary_valid = false;

    calc_pending_op    = 0;
    calc_input_active  = false;
    calc_input_decimal = false;
    calc_input_value   = 0.0;
    calc_decimal_place = 0.1;
    calc_input_text_clear();
}

void calc_mode_init(void) {
    calc_mode_enabled = true;
    calc_reset_all();
    calc_render_display();
}

bool calc_mode_is_enabled(void) {
    return calc_mode_enabled;
}

void calc_mode_toggle(void) {
    calc_mode_enabled = !calc_mode_enabled;
    if (calc_mode_enabled) {
        calc_reset_all();
        calc_render_display();
    } else {
        calc_mem_modifier_held = false;
        calc_mem_preview_active = false;
        calc_mem_preview_visible = false;
        calc_status_active = false;
    }
}

void calc_mode_refresh_display(void) {
    if (calc_mode_enabled) {
        calc_render_display();
    }
}

void calc_mode_tick(void) {
    if (!calc_mode_enabled) {
        return;
    }

    if (calc_status_active && timer_elapsed32(calc_status_started_ms) >= CALC_STATUS_MSG_MS) {
        calc_status_active = false;
        calc_render_display();
    }

    if (calc_sci_browsing) {
        // Segment label and matrix icon share this same visibility phase.
        const bool prior_visible = calc_sci_visible;
        const uint32_t elapsed = timer_elapsed32(calc_sci_blink_started_ms);
        calc_sci_visible = ((elapsed / CALC_SCI_BLINK_MS) % 2U) == 0U;
        if (prior_visible != calc_sci_visible && calc_sci_label_active) {
            calc_render_display();
        }
    }

    if (!calc_mem_preview_active) {
        return;
    }

    const uint32_t elapsed = timer_elapsed32(calc_mem_preview_started_ms);

    const bool visible = ((elapsed / CALC_MEM_BLINK_MS) % 2U) == 0U;
    if (visible != calc_mem_preview_visible) {
        calc_mem_preview_visible = visible;
        calc_render_display();
    }
}

void calc_mode_set_mem_modifier(bool held) {
    calc_mem_modifier_held = held;
}

bool calc_mode_handle_function_key(uint16_t keycode) {
    const int8_t bank = calc_bank_index_from_keycode(keycode);
    if (bank < 0) {
        return false;
    }

    if (!calc_mode_enabled) {
        return false;
    }

    if (calc_mem_modifier_held) {
        calc_memory_banks[bank] = calc_round_for_display(calc_current_value());
        calc_mem_preview_active  = false;
        calc_mem_preview_visible = false;
        (void)snprintf(calc_status_text, sizeof(calc_status_text), "STORED M%d", (int)(bank + 1));
        calc_status_active = true;
        calc_status_started_ms = timer_read32();
        calc_render_display();
        return true;
    }

    if (calc_mem_preview_active && calc_mem_preview_bank == (uint8_t)bank) {
        const double loaded = calc_memory_banks[bank];
        calc_mem_preview_active  = false;
        calc_mem_preview_visible = false;

        if (calc_pending_op != 0) {
            calc_input_active  = true;
            calc_input_decimal = false;
            calc_input_value   = loaded;
            calc_decimal_place = 0.1;
            calc_input_text_clear();
        } else {
            calc_accumulator   = loaded;
            calc_input_active  = false;
            calc_input_decimal = false;
            calc_input_value   = 0.0;
            calc_decimal_place = 0.1;
            calc_input_text_clear();
        }

        calc_last_eval_valid = false;
        calc_render_display();
        return true;
    }

    calc_mem_preview_active     = true;
    calc_mem_preview_bank       = (uint8_t)bank;
    calc_mem_preview_started_ms = timer_read32();
    calc_mem_preview_visible    = true;
    calc_render_display();
    return true;
}

char calc_mode_pending_op(void) {
    return calc_pending_op;
}

char calc_mode_take_repeat_flash_op(void) {
    char op = calc_repeat_flash_op;
    calc_repeat_flash_op = 0;
    return op;
}

uint8_t calc_mode_take_repeat_flash_symbol(void) {
    const uint8_t symbol = calc_repeat_flash_symbol;
    calc_repeat_flash_symbol = MATRIX_SYMBOL_INVALID;
    return symbol;
}

bool calc_mode_browse_scientific(int8_t delta) {
    if (!calc_mode_enabled || delta == 0) {
        return false;
    }

    // When waiting for rhs of a binary operator, only constant insertions are
    // valid from scientific browse (currently PI).
    const bool constants_only = (calc_pending_op != 0);

    if (!calc_sci_active) {
        calc_sci_active = true;
    }

    if (constants_only) {
        calc_sci_index = MATRIX_SCI_PI;
    } else {
        calc_sci_index = calc_wrap_sci_index((int16_t)calc_sci_index + (int16_t)delta);
    }

    if (constants_only && !calc_sci_is_constant(calc_sci_index)) {
        calc_sci_index = MATRIX_SCI_PI;
    }

    calc_sci_browsing = true;
    calc_sci_visible = true;
    calc_sci_blink_started_ms = timer_read32();
    calc_sci_label_active = true;
    calc_render_display();
    return true;
}

bool calc_mode_confirm_scientific(void) {
    if (!calc_mode_enabled) {
        return false;
    }

    if (!calc_sci_active) {
        calc_sci_active = true;
    }

    const uint8_t selected = calc_sci_index;
    calc_clear_scientific_selection();

    // Confirm both locks selection and executes its mapped behavior.
    switch (selected) {
        case MATRIX_SCI_POWER:
        case MATRIX_SCI_HYPOT:
            (void)calc_prepare_binary_scientific(selected);
            break;
        case MATRIX_SCI_PI:
            (void)calc_insert_pi_value();
            break;
        case MATRIX_SCI_SQRT:
        case MATRIX_SCI_SIN:
        case MATRIX_SCI_COS:
        case MATRIX_SCI_SEC:
        case MATRIX_SCI_CSC:
        case MATRIX_SCI_TAN:
        case MATRIX_SCI_COT:
        case MATRIX_SCI_FACT:
            (void)calc_apply_unary_scientific(selected);
            break;
        default:
            break;
    }

    return true;
}

uint8_t calc_mode_scientific_matrix_symbol(void) {
    if (!calc_mode_enabled || !calc_sci_active) {
        return MATRIX_SYMBOL_INVALID;
    }

    if (calc_sci_browsing && !calc_sci_visible) {
        return MATRIX_SYMBOL_INVALID;
    }

    return matrix_symbol_for_scientific(calc_sci_index);
}

bool calc_mode_handle_keycode(uint16_t keycode) {
    if (!calc_mode_enabled) {
        return false;
    }

    if (calc_error && keycode != CK_CLR && keycode != KC_DEL && keycode != CK_MOD) {
        return true;
    }

    if (calc_mem_preview_active && (keycode == CK_CLR || keycode == KC_DEL)) {
        calc_mem_preview_active  = false;
        calc_mem_preview_visible = false;
        calc_render_display();
        return true;
    }

    if ((calc_sci_active || calc_sci_browsing || calc_sci_label_active) && (keycode == CK_CLR || keycode == KC_DEL)) {
        calc_clear_scientific_selection();
        calc_render_display();
        return true;
    }

    switch (keycode) {
        case KC_P0:
            calc_push_digit(0);
            break;
        case KC_P1:
            calc_push_digit(1);
            break;
        case KC_P2:
            calc_push_digit(2);
            break;
        case KC_P3:
            calc_push_digit(3);
            break;
        case KC_P4:
            calc_push_digit(4);
            break;
        case KC_P5:
            calc_push_digit(5);
            break;
        case KC_P6:
            calc_push_digit(6);
            break;
        case KC_P7:
            calc_push_digit(7);
            break;
        case KC_P8:
            calc_push_digit(8);
            break;
        case KC_P9:
            calc_push_digit(9);
            break;
        case KC_PDOT:
            if (!calc_input_active) {
                calc_input_active = true;
                calc_input_value  = 0.0;
                calc_last_eval_valid = false;
                calc_input_text_clear();
                calc_input_text_append('0');
            }

            if (!calc_input_decimal) {
                calc_input_decimal = true;
                calc_decimal_place = 0.1;
                calc_input_text_append('.');
            }
            break;
        case KC_PPLS:
            calc_handle_operator('+');
            break;
        case KC_PMNS:
            calc_handle_operator('-');
            break;
        case KC_PAST:
            calc_handle_operator('*');
            break;
        case KC_PSLS:
            calc_handle_operator('/');
            break;
        case KC_PENT:
            calc_handle_equals();
            break;
        case CK_SEL:
            (void)calc_mode_confirm_scientific();
            break;
        case KC_DEL:
            if (calc_input_active && calc_input_text_len > 0) {
                calc_input_text_len--;
                calc_input_text[calc_input_text_len] = '\0';
                calc_input_rebuild_from_text();
                calc_last_eval_valid = false;
                calc_last_sci_unary_valid = false;
            }
            break;
        case CK_CLR:
            calc_reset_all();
            break;
        case CK_MEM:
        case CK_CPY:
        case CK_PST:
            break;
        default:
            return false;
    }

    calc_render_display();
    return true;
}

bool calc_mode_get_current_value_text(char *out, size_t out_size) {
    if (!out || out_size == 0) {
        return false;
    }

    const double value = calc_current_value();
    if (calc_value_overflowed(value)) {
        return false;
    }

    calc_format_value(value, out, out_size);
    return true;
}

bool calc_mode_set_input_from_text(const char *text) {
    if (!calc_mode_enabled || !text) {
        return false;
    }

    const char *start = text;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    size_t len = strlen(start);
    while (len > 0 && isspace((unsigned char)start[len - 1])) {
        len--;
    }

    if (len == 0 || len >= sizeof(calc_input_text)) {
        return false;
    }

    bool seen_digit = false;
    bool seen_dot   = false;
    uint8_t out_len = 0;

    for (size_t i = 0; i < len; i++) {
        char ch = start[i];

        if (ch == ',') {
            ch = '.';
        }

        if (ch == '-') {
            if (i != 0) {
                return false;
            }
            calc_input_text[out_len++] = ch;
            continue;
        }

        if (ch == '.') {
            if (seen_dot) {
                return false;
            }
            seen_dot = true;
            calc_input_text[out_len++] = ch;
            continue;
        }

        if (ch < '0' || ch > '9') {
            return false;
        }

        seen_digit = true;
        calc_input_text[out_len++] = ch;
    }

    if (!seen_digit) {
        return false;
    }

    calc_input_text[out_len] = '\0';
    calc_input_text_len = out_len;
    calc_input_rebuild_from_text();

    if (!calc_input_active) {
        return false;
    }

    calc_error = false;
    calc_error_overflow = false;
    calc_status_active = false;
    calc_mem_preview_active = false;
    calc_mem_preview_visible = false;
    calc_last_eval_valid = false;
    calc_last_sci_unary_valid = false;
    calc_clear_scientific_selection();
    calc_render_display();
    return true;
}