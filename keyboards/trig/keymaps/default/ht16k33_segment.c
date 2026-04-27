// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "i2c_master.h"
#include "ht16k33_segment.h"

#define HT16K33_CMD_SYSTEM_SETUP      0x20
#define HT16K33_CMD_SYSTEM_OSCILLATOR 0x01
#define HT16K33_CMD_DISPLAY_SETUP     0x80
#define HT16K33_CMD_DISPLAY_ON        0x01
#define HT16K33_CMD_DIMMING_SETUP     0xE0
#define HT16K33_DEFAULT_BRIGHTNESS    0x08

#define HT16K33_SEGMENT_TIMEOUT_MS 100
#define HT16K33_SEGMENT_DIGITS     4

static uint16_t ht16k33_segment_ram[8] = {0};
static uint8_t ht16k33_segment_i2c_addr = HT16K33_SEGMENT_I2C_ADDRESS;

static uint16_t ht16k33_segment_ascii_to_bits(char c) {
    switch (c) {
        case ' ': return 0x0000;
        case '!': return 0x0006;
        case '"': return 0x0220;
        case '#': return 0x12CE;
        case '$': return 0x12ED;
        case '%': return 0x0C24;
        case '&': return 0x235D;
        case '\'': return 0x0400;
        case '(': return 0x2400;
        case ')': return 0x0900;
        case '*': return 0x3FC0;
        case '+': return 0x12C0;
        case ',': return 0x0800;
        case '-': return 0x00C0;
        case '.': return HT16K33_SEGMENT_DP;
        case '/': return 0x0C00;
        case '0': return 0x0C3F;
        case '1': return 0x0006;
        case '2': return 0x00DB;
        case '3': return 0x008F;
        case '4': return 0x00E6;
        case '5': return 0x2069;
        case '6': return 0x00FD;
        case '7': return 0x0007;
        case '8': return 0x00FF;
        case '9': return 0x00EF;
        case ':': return 0x1200;
        case ';': return 0x0A00;
        case '<': return 0x2400;
        case '=': return 0x00C8;
        case '>': return 0x0900;
        case '?': return 0x1083;
        case '@': return 0x02BB;
        case 'A': return 0x00F7;
        case 'B': return 0x128F;
        case 'C': return 0x0039;
        case 'D': return 0x120F;
        case 'E': return 0x00F9;
        case 'F': return 0x0071;
        case 'G': return 0x00BD;
        case 'H': return 0x00F6;
        case 'I': return 0x1209;
        case 'J': return 0x001E;
        case 'K': return 0x2470;
        case 'L': return 0x0038;
        case 'M': return 0x0536;
        case 'N': return 0x2136;
        case 'O': return 0x003F;
        case 'P': return 0x00F3;
        case 'Q': return 0x203F;
        case 'R': return 0x20F3;
        case 'S': return 0x00ED;
        case 'T': return 0x1201;
        case 'U': return 0x003E;
        case 'V': return 0x0C30;
        case 'W': return 0x2836;
        case 'X': return 0x2D00;
        case 'Y': return 0x1500;
        case 'Z': return 0x0C09;
        case '[': return 0x0039;
        case '\\': return 0x2100;
        case ']': return 0x000F;
        case '^': return 0x0C03;
        case '_': return 0x0008;
        case '`': return 0x0100;
        case 'a': return 0x1058;
        case 'b': return 0x2078;
        case 'c': return 0x00D8;
        case 'd': return 0x088E;
        case 'e': return 0x0858;
        case 'f': return 0x0071;
        case 'g': return 0x048E;
        case 'h': return 0x1070;
        case 'i': return 0x1000;
        case 'j': return 0x000E;
        case 'k': return 0x3600;
        case 'l': return 0x0030;
        case 'm': return 0x10D4;
        case 'n': return 0x1050;
        case 'o': return 0x00DC;
        case 'p': return 0x0170;
        case 'q': return 0x0486;
        case 'r': return 0x0050;
        case 's': return 0x2088;
        case 't': return 0x0078;
        case 'u': return 0x001C;
        case 'v': return 0x2004;
        case 'w': return 0x2814;
        case 'x': return 0x28C0;
        case 'y': return 0x200C;
        case 'z': return 0x0848;
        case '{': return 0x0949;
        case '|': return 0x1200;
        case '}': return 0x2489;
        case '~': return 0x0520;
        default: return 0x0000;
    }
}

static bool ht16k33_segment_write_cmd(uint8_t cmd) {
    return i2c_transmit(ht16k33_segment_i2c_addr, &cmd, 1, HT16K33_SEGMENT_TIMEOUT_MS) == I2C_STATUS_SUCCESS;
}

void ht16k33_segment_set_address(uint8_t i2c_address) {
    ht16k33_segment_i2c_addr = i2c_address;
}

uint8_t ht16k33_segment_get_address(void) {
    return ht16k33_segment_i2c_addr;
}

bool ht16k33_segment_set_display_on(bool on) {
    uint8_t cmd = HT16K33_CMD_DISPLAY_SETUP;

    if (on) {
        cmd |= HT16K33_CMD_DISPLAY_ON;
    }

    return ht16k33_segment_write_cmd(cmd);
}

bool ht16k33_segment_set_blink(ht16k33_segment_blink_t blink) {
    if (blink > HT16K33_SEGMENT_BLINK_HALF) {
        return false;
    }

    uint8_t cmd = HT16K33_CMD_DISPLAY_SETUP | HT16K33_CMD_DISPLAY_ON | ((uint8_t)blink << 1);
    return ht16k33_segment_write_cmd(cmd);
}

bool ht16k33_segment_set_brightness(uint8_t level) {
    if (level > 0x0F) {
        level = 0x0F;
    }

    return ht16k33_segment_write_cmd((uint8_t)(HT16K33_CMD_DIMMING_SETUP | level));
}

void ht16k33_segment_clear(void) {
    for (uint8_t i = 0; i < 8; i++) {
        ht16k33_segment_ram[i] = 0;
    }
}

bool ht16k33_segment_set_ram(const uint16_t ram_words[8]) {
    if (!ram_words) {
        return false;
    }

    for (uint8_t i = 0; i < 8; i++) {
        ht16k33_segment_ram[i] = ram_words[i];
    }

    return true;
}

bool ht16k33_segment_set_digit_raw(uint8_t digit, uint16_t segments, bool dot) {
    if (digit >= HT16K33_SEGMENT_DIGITS) {
        return false;
    }

    uint16_t value = segments;
    if (dot) {
        value |= HT16K33_SEGMENT_DP;
    }

    ht16k33_segment_ram[digit] = value;
    return true;
}

bool ht16k33_segment_set_char(uint8_t digit, char c, bool dot) {
    return ht16k33_segment_set_digit_raw(digit, ht16k33_segment_ascii_to_bits(c), dot);
}

bool ht16k33_segment_write_text(const char *text) {
    if (!text) {
        return false;
    }

    ht16k33_segment_clear();

    uint8_t digit = 0;
    for (const char *p = text; *p && digit < HT16K33_SEGMENT_DIGITS; p++) {
        if (*p == '.' && digit > 0) {
            ht16k33_segment_ram[digit - 1] |= HT16K33_SEGMENT_DP;
            continue;
        }

        if (!ht16k33_segment_set_char(digit, *p, false)) {
            return false;
        }

        digit++;
    }

    return ht16k33_segment_write();
}

bool ht16k33_segment_write(void) {
    uint8_t packet[17] = {0};
    packet[0]          = 0x00;

    for (uint8_t i = 0; i < 8; i++) {
        packet[1 + (2 * i)] = (uint8_t)(ht16k33_segment_ram[i] & 0xFFu);
        packet[2 + (2 * i)] = (uint8_t)((ht16k33_segment_ram[i] >> 8) & 0xFFu);
    }

    return i2c_transmit(ht16k33_segment_i2c_addr, packet, sizeof(packet), HT16K33_SEGMENT_TIMEOUT_MS) == I2C_STATUS_SUCCESS;
}

bool ht16k33_segment_init_address(uint8_t i2c_address) {
    i2c_init();

    ht16k33_segment_set_address(i2c_address);

    if (!ht16k33_segment_write_cmd(HT16K33_CMD_SYSTEM_SETUP | HT16K33_CMD_SYSTEM_OSCILLATOR)) {
        return false;
    }

    if (!ht16k33_segment_set_display_on(true)) {
        return false;
    }

    if (!ht16k33_segment_set_brightness(HT16K33_DEFAULT_BRIGHTNESS)) {
        return false;
    }

    ht16k33_segment_clear();
    return ht16k33_segment_write();
}

bool ht16k33_segment_init(void) {
    return ht16k33_segment_init_address(HT16K33_SEGMENT_I2C_ADDRESS);
}
