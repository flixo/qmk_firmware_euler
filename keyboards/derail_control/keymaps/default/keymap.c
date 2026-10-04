// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "analog.h"
#include "eeconfig.h"
#include "virtser.h"
#include "ws2812.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ws2812_led_t ws2812_leds[WS2812_LED_COUNT];

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(JS_0, JS_1, JS_2)
};

static uint32_t last_blink_time;
static uint32_t last_adc_sample_time;
static uint32_t last_adc_debug_time;
static uint16_t active_axis_tests;

#define ADC_FILTER_SAMPLES 8
#define ADC_DEBUG_RAW_DELTA 15
#define MANUAL_AXIS_RELEASE_RAW_DELTA 300
#define MULTIPLEXER_SETTLE_TIME_US 5
#define SLIDER_CONFIG_VERSION 11
#define SLIDER_CONFIG_MAX_AXES 10
#define SLIDER_CONFIG_MAX_NOTCHES 16
#define SLIDER_AXIS_NAME_LENGTH 24
#define SLIDER_MAP_COUNT 5
#define SLIDER_MAP_DEFAULT (1 << 0)
#define SLIDER_CONFIG_MAGIC 0x534C4944UL
#define CONFIG_COMMAND_BUFFER_SIZE 8192
#define JOYSTICK_AXIS_LEG_MS 500
#define JOYSTICK_AXIS_CENTER_PAUSE_MS 500
#define JOYSTICK_AXIS_WAIT_MS 1000
#define JOYSTICK_AXIS_TEST_MS (4 * JOYSTICK_AXIS_LEG_MS + 2 * JOYSTICK_AXIS_CENTER_PAUSE_MS + JOYSTICK_AXIS_WAIT_MS)
#define JOYSTICK_AXIS_PHASE_OFFSET_MS 200

static char     config_command[CONFIG_COMMAND_BUFFER_SIZE];
static uint16_t config_command_length;
static bool     config_command_overflow;

typedef struct __attribute__((packed)) {
    uint8_t version;
    uint8_t joy_id;
    uint8_t axis_id;
    uint8_t addr;
    uint8_t notches;
    uint8_t snap_pct;
    uint8_t map_mask;
    bool    invert;
    bool    filter_adc;
    char    name[SLIDER_AXIS_NAME_LENGTH];
    uint16_t valid_range_min;
    uint16_t valid_range_max;
    uint16_t adc_min;
    uint16_t adc_max;
    int16_t  output_min;
    int16_t  output_max;
    int16_t  maps[SLIDER_MAP_COUNT][SLIDER_CONFIG_MAX_NOTCHES];
} slider_axis_config_t;

typedef struct __attribute__((packed)) {
    uint32_t             magic;
    uint8_t              axis_count;
    uint8_t              active_map;
    slider_axis_config_t axes[SLIDER_CONFIG_MAX_AXES];
    uint16_t             checksum;
} slider_config_storage_t;

typedef struct {
    uint16_t raw_adc_reading;
    uint16_t last_reported_raw_adc_reading;
    uint16_t last_valid_adc_reading;
    uint16_t manual_override_raw_adc_reading;
    uint16_t adc_samples[ADC_FILTER_SAMPLES];
    uint16_t adc_sample_total;
    int16_t  axis_value;
    int16_t  last_axis_value;
    int16_t  last_axis_delta;
    int16_t  last_reported_axis_value;
    int16_t  manual_override_axis_value;
    uint8_t  adc_sample_index;
    uint8_t  adc_sample_count;
    uint8_t  notch;
    uint8_t  last_reported_notch;
    bool     axis_initialized;
    bool     manual_override_active;
    bool     notch_initialized;
    bool     raw_adc_reading_reported;
    bool     has_valid_adc_reading;
} slider_axis_runtime_t;

#define SLIDER_AXIS_0_DEFAULT_MAP {50, 116, 182, 248, 313, 379, 445, 511}
#define SLIDER_AXIS_1_DEFAULT_MAP {50, 92, 134, 176, 218, 260, 301, 343, 385, 427, 469, 511}
#define SLIDER_AXIS_2_DEFAULT_MAP {50, 101, 152, 204, 255, 306, 357, 409, 460, 511}
#define SLIDER_AXIS_3_DEFAULT_MAP {50, 280, 511}

#define SLIDER_AXIS_2_DE2_MAP     {50, 92, 134, 176, 218, 260, 301, 343, 427, 511}
#define SLIDER_AXIS_2_DE6_MAP     {50, 96, 142, 188, 234, 281, 327, 373, 419, 511}
#define SLIDER_AXIS_2_DH4_MAP     {50, 116, 182, 248, 313, 379, 445, 511, 511, 511}
#define SLIDER_AXIS_2_BE2_260_MAP {50, 101, 152, 204, 255, 306, 357, 409, 460, 511}

#define SWITCH_AXIS_0_DEFAULT_MAP     {0, -511, -253, 0, 154, 329, 511} // 7 notches
#define SWITCH_AXIS_4_DEFAULT_MAP     {0, -511, -253, 0, 255, 511, 511} // 7 notches

static const char *const slider_map_names[SLIDER_MAP_COUNT] = {"default", "DE2", "DE6", "DH4", "BE2-260"};
static const char *const slider_map_json_keys[SLIDER_MAP_COUNT] = {"\"default\"", "\"DE2\"", "\"DE6\"", "\"DH4\"", "\"BE2-260\""};

static const slider_axis_config_t default_slider_axis_configs[] = {
    
  {
      .version          = 11,
      .joy_id           = 0,
      .axis_id          = 0,
      .addr             = 0,
      .notches          = 10,
      .snap_pct         = 25,
      .map_mask         = (1 << SLIDER_MAP_COUNT) - 1,
      .invert           = true,
      .filter_adc       = false,
      .name             = "Throttle",
      .valid_range_min  = 0,
      .valid_range_max  = 1023,
      .adc_min          = 9,
      .adc_max          = 1020,
      .output_min       = -JOYSTICK_MAX_VALUE,
      .output_max       = JOYSTICK_MAX_VALUE,
      .maps             = {
        {   50,  101,  152,  204,  255,  306,  357,  409,  460,  511},   // default
        {   50,   92,  134,  176,  218,  260,  301,  343,  427,  511},   // DE2
        {   50,   96,  142,  188,  234,  281,  327,  373,  419,  511},   // DE6
        {   50,  116,  182,  248,  313,  379,  445,  511,  511,  511},   // DH4
        {   50,  101,  152,  204,  255,  306,  357,  409,  460,  511},   // BE2-260
        
      }
  },

  {
      .version          = 11,
      .joy_id           = 0,
      .axis_id          = 1,
      .addr             = 1,
      .notches          = 3,
      .snap_pct         = 25,
      .map_mask         = (1 << 0),
      .invert           = false,
      .filter_adc       = false,
      .name             = "Reverser",
      .valid_range_min  = 0,
      .valid_range_max  = 1023,
      .adc_min          = 9,
      .adc_max          = 1020,
      .output_min       = -JOYSTICK_MAX_VALUE,
      .output_max       = JOYSTICK_MAX_VALUE,
      .maps             = {
        {   50,  280,  511},   // default
        
      }
  },

  {
      .version          = 11,
      .joy_id           = 0,
      .axis_id          = 2,
      .addr             = 3,
      .notches          = 8,
      .snap_pct         = 25,
      .map_mask         = (1 << 0),
      .invert           = true,
      .filter_adc       = false,
      .name             = "Independent Break",
      .valid_range_min  = 0,
      .valid_range_max  = 1023,
      .adc_min          = 9,
      .adc_max          = 1020,
      .output_min       = -JOYSTICK_MAX_VALUE,
      .output_max       = JOYSTICK_MAX_VALUE,
      .maps             = {
        {   50,  116,  182,  248,  313,  379,  445,  511},   // default
        
      }
  },

  {
      .version          = 11,
      .joy_id           = 0,
      .axis_id          = 3,
      .addr             = 2,
      .notches          = 12,
      .snap_pct         = 25,
      .map_mask         = (1 << 0),
      .invert           = true,
      .filter_adc       = false,
      .name             = "Train Break",
      .valid_range_min  = 0,
      .valid_range_max  = 1023,
      .adc_min          = 70,
      .adc_max          = 1022,
      .output_min       = -JOYSTICK_MAX_VALUE,
      .output_max       = JOYSTICK_MAX_VALUE,
      .maps             = {
        {   50,   92,  134,  176,  218,  260,  301,  343,  385,  427,  469,  511},   // default
        
      }
  },

  {
      .version          = 11,
      .joy_id           = 1,
      .axis_id          = 0,
      .addr             = 4,
      .notches          = 7,
      .snap_pct         = 50,
      .map_mask         = (1 << SLIDER_MAP_COUNT) - 1,
      .invert           = true,
      .filter_adc       = true,
      .name             = "Switch 1 Front Light",
      .valid_range_min  = 0,
      .valid_range_max  = 900,
      .adc_min          = 0,
      .adc_max          = 1020,
      .output_min       = -JOYSTICK_MAX_VALUE,
      .output_max       = JOYSTICK_MAX_VALUE,
      .maps             = {
        {    0, -511, -320, -140,  125,  327,  511},   // default
        {    0, -511, -320, -140,  125,  327,  511},   // DE2
        {    0, -511, -320, -140,  125,  327,  511},   // DE6
        {    0, -511, -320, -140,  125,  327,  511},   // DH4
        {    0,    0, -511,    0,   10,   30,   40},   // BE2-260
        
      }
  },

  {
      .version          = 11,
      .joy_id           = 1,
      .axis_id          = 1,
      .addr             = 5,
      .notches          = 7,
      .snap_pct         = 50,
      .map_mask         = (1 << SLIDER_MAP_COUNT) - 1,
      .invert           = true,
      .filter_adc       = true,
      .name             = "Switch 2 Rear Light",
      .valid_range_min  = 0,
      .valid_range_max  = 900,
      .adc_min          = 0,
      .adc_max          = 1020,
      .output_min       = -JOYSTICK_MAX_VALUE,
      .output_max       = JOYSTICK_MAX_VALUE,
      .maps             = {
        {    0, -511, -320, -140,  125,  327,  511},   // default
        {    0, -511, -320, -140,  125,  327,  511},   // DE2
        {    0, -511, -320, -140,  125,  327,  511},   // DE6
        {    0, -511, -320, -140,  125,  327,  511},   // DH4
        {    0,    0, -511,    0,  511,    0,    0},   // BE2-260
        
      }
  },

  {
      .version          = 11,
      .joy_id           = 1,
      .axis_id          = 2,
      .addr             = 6,
      .notches          = 7,
      .snap_pct         = 50,
      .map_mask         = (1 << SLIDER_MAP_COUNT) - 1,
      .invert           = true,
      .filter_adc       = true,
      .name             = "Switch 3 Cab Light",
      .valid_range_min  = 0,
      .valid_range_max  = 900,
      .adc_min          = 0,
      .adc_max          = 1020,
      .output_min       = -JOYSTICK_MAX_VALUE,
      .output_max       = JOYSTICK_MAX_VALUE,
      .maps             = {
        {    0,    0,    0,    0,  255,  511,  511},   // default
        {    0,    0,    0,    0,  255,  511,  511},   // DE2
        {    0,    0,    0,    0,  255,  511,  511},   // DE6
        {    0,    0,    0,    0,  255,  511,  511},   // DH4
        {    0,    0,    0,    0,  255,  511,  511},   // BE2-260
        
      }
  },

  {
      .version          = 11,
      .joy_id           = 1,
      .axis_id          = 3,
      .addr             = 7,
      .notches          = 7,
      .snap_pct         = 50,
      .map_mask         = (1 << SLIDER_MAP_COUNT) - 1,
      .invert           = true,
      .filter_adc       = true,
      .name             = "Switch 4 Wiper",
      .valid_range_min  = 0,
      .valid_range_max  = 900,
      .adc_min          = 0,
      .adc_max          = 1020,
      .output_min       = -JOYSTICK_MAX_VALUE,
      .output_max       = JOYSTICK_MAX_VALUE,
      .maps             = {
        {    0,    0,    0,    0,  225,  360,  511},   // default
        {    0,    0,    0,    0,  225,  360,  511},   // DE2
        {    0,    0,    0,    0,  225,  360,  511},   // DE6
        {    0,    0,    0,    0,  225,  360,  511},   // DH4
        {    0,    0,    0,    0,    0,    0,    0},   // BE2-260
        
      }
  },

  {
      .version          = 11,
      .joy_id           = 1,
      .axis_id          = 4,
      .addr             = 8,
      .notches          = 7,
      .snap_pct         = 50,
      .map_mask         = (1 << SLIDER_MAP_COUNT) - 1,
      .invert           = true,
      .filter_adc       = true,
      .name             = "Switch 5 Sander",
      .valid_range_min  = 0,
      .valid_range_max  = 900,
      .adc_min          = 0,
      .adc_max          = 1020,
      .output_min       = -JOYSTICK_MAX_VALUE,
      .output_max       = JOYSTICK_MAX_VALUE,
      .maps             = {
        {    0,    0,    0,    0,  256,  511,  511},   // default
        {    0,    0,    0,    0,  511,  511,  511},   // DE2
        {    0,    0,    0,    0,  256,  511,  511},   // DE6
        {    0,    0,    0,    0,  256,  511,  511},   // DH4
        {    0,    0,    0,    0,  511,  511,  511},   // BE2-260
        
      }
  }
};

#undef SLIDER_AXIS_0_DEFAULT_MAP
#undef SLIDER_AXIS_1_DEFAULT_MAP
#undef SLIDER_AXIS_2_DEFAULT_MAP
#undef SLIDER_AXIS_2_DE2_MAP
#undef SLIDER_AXIS_2_DE6_MAP
#undef SLIDER_AXIS_2_DH4_MAP
#undef SLIDER_AXIS_2_BE2_260_MAP
#undef SWITCH_AXIS_4_DEFAULT_MAP

static slider_axis_config_t slider_axis_configs[SLIDER_CONFIG_MAX_AXES];
static slider_axis_runtime_t slider_axis_runtimes[SLIDER_CONFIG_MAX_AXES];
static slider_axis_config_t pending_slider_axis_configs[SLIDER_CONFIG_MAX_AXES];
static slider_config_storage_t slider_config_storage;
static uint8_t slider_axis_count;
    static uint8_t active_slider_map;

static void serial_send_adc_reading(const slider_axis_config_t *config, const slider_axis_runtime_t *runtime);

joystick_config_t joystick_axes[JOYSTICK_AXIS_COUNT] = {
    JOYSTICK_AXIS_VIRTUAL, // x
    JOYSTICK_AXIS_VIRTUAL, // y
    JOYSTICK_AXIS_VIRTUAL, // z
    JOYSTICK_AXIS_VIRTUAL, // Rx
    JOYSTICK_AXIS_VIRTUAL, // Ry
    JOYSTICK_AXIS_VIRTUAL  // Rz
};

static uint16_t slider_config_checksum(const slider_config_storage_t *storage) {
    const uint8_t *bytes    = (const uint8_t *)storage;
    uint16_t       checksum = 0;

    for (uint16_t index = 0; index < sizeof(*storage) - sizeof(storage->checksum); index++) {
        checksum = (checksum << 5) | (checksum >> 11);
        checksum ^= bytes[index];
    }

    return checksum;
}

static bool slider_axis_name_is_valid(const char *name) {
    for (uint8_t index = 0; index < SLIDER_AXIS_NAME_LENGTH; index++) {
        if (name[index] == '\0') {
            return index > 0;
        }

        if (name[index] < ' ' || name[index] == '"' || name[index] == '\\') {
            return false;
        }
    }

    return false;
}

static bool slider_config_is_valid(const slider_axis_config_t *config) {
    if (config->version != SLIDER_CONFIG_VERSION || config->joy_id > 1 || config->addr > 15 || config->notches < 2 || config->notches > SLIDER_CONFIG_MAX_NOTCHES || config->snap_pct == 0 || config->snap_pct > 50 || !slider_axis_name_is_valid(config->name) || (config->map_mask & SLIDER_MAP_DEFAULT) == 0 || config->map_mask >= (1 << SLIDER_MAP_COUNT) || config->valid_range_min > config->valid_range_max || config->valid_range_max > 1023 || config->adc_min >= config->adc_max || config->output_min == config->output_max) {
        return false;
    }

    if ((config->joy_id == 0 && config->axis_id >= JOYSTICK_AXIS_COUNT) || (config->joy_id == 1 && config->axis_id >= JOYSTICK2_AXIS_COUNT)) {
        return false;
    }

    int16_t output_low  = config->output_min < config->output_max ? config->output_min : config->output_max;
    int16_t output_high = config->output_min < config->output_max ? config->output_max : config->output_min;

    for (uint8_t map_index = 0; map_index < SLIDER_MAP_COUNT; map_index++) {
        if ((config->map_mask & (1 << map_index)) == 0) {
            continue;
        }

        for (uint8_t notch_index = 0; notch_index < config->notches; notch_index++) {
            if (config->maps[map_index][notch_index] < output_low || config->maps[map_index][notch_index] > output_high) {
                return false;
            }
        }
    }

    return true;
}

static bool slider_configs_are_valid(const slider_axis_config_t *configs, uint8_t config_count) {
    if (config_count == 0 || config_count > SLIDER_CONFIG_MAX_AXES) {
        return false;
    }

    for (uint8_t index = 0; index < config_count; index++) {
        if (!slider_config_is_valid(&configs[index])) {
            return false;
        }

        for (uint8_t previous_index = 0; previous_index < index; previous_index++) {
            if (configs[index].joy_id == configs[previous_index].joy_id && configs[index].axis_id == configs[previous_index].axis_id) {
                return false;
            }
        }
    }

    return true;
}

static void slider_config_save(void) {
    slider_config_storage.magic      = SLIDER_CONFIG_MAGIC;
    slider_config_storage.axis_count = slider_axis_count;
    slider_config_storage.active_map = active_slider_map;

    memcpy(slider_config_storage.axes, slider_axis_configs, sizeof(slider_axis_configs));
    slider_config_storage.checksum = slider_config_checksum(&slider_config_storage);

    eeconfig_update_user_datablock(&slider_config_storage, 0, sizeof(slider_config_storage));
}

static void slider_config_load(void) {
    if (eeconfig_is_user_datablock_valid()) {
        eeconfig_read_user_datablock(&slider_config_storage, 0, sizeof(slider_config_storage));
        if (slider_config_storage.magic == SLIDER_CONFIG_MAGIC && slider_config_storage.active_map < SLIDER_MAP_COUNT && slider_config_storage.checksum == slider_config_checksum(&slider_config_storage) && slider_configs_are_valid(slider_config_storage.axes, slider_config_storage.axis_count)) {
            memcpy(slider_axis_configs, slider_config_storage.axes, sizeof(slider_axis_configs));
            slider_axis_count = slider_config_storage.axis_count;
            active_slider_map = slider_config_storage.active_map;
            return;
        }
    }

    slider_axis_count = ARRAY_SIZE(default_slider_axis_configs);
    active_slider_map = 0;
    memcpy(slider_axis_configs, default_slider_axis_configs, sizeof(default_slider_axis_configs));
    slider_config_save();
}

static void serial_send_buffer(const char *buffer, uint16_t length) {
    for (uint16_t index = 0; index < length; index++) {
        virtser_send(buffer[index]);
    }
}

static void serial_send_text(const char *text) {
    serial_send_buffer(text, strlen(text));
}

static const char *json_skip_space(const char *text) {
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }

    return text;
}

static const char *json_value(const char *json, const char *key) {
    const char *found = strstr(json, key);

    if (found == NULL) {
        return NULL;
    }

    found = strchr(found + strlen(key), ':');
    return found == NULL ? NULL : json_skip_space(found + 1);
}

static bool json_read_number(const char *json, const char *key, int32_t *value) {
    const char *text = json_value(json, key);
    char       *end;

    if (text == NULL) {
        return false;
    }

    long parsed = strtol(text, &end, 10);
    if (end == text || parsed < INT32_MIN || parsed > INT32_MAX) {
        return false;
    }

    *value = parsed;
    return true;
}

static bool json_read_string(const char *json, const char *key, char *value, uint8_t value_size) {
    const char *text = json_value(json, key);
    uint8_t     length = 0;

    if (text == NULL || *text++ != '"') {
        return false;
    }

    while (*text != '"') {
        if (*text == '\0' || *text == '\\' || length + 1 >= value_size) {
            return false;
        }

        value[length++] = *text++;
    }

    value[length] = '\0';
    return slider_axis_name_is_valid(value);
}

static bool json_read_bool(const char *json, const char *key, bool *value) {
    const char *text = json_value(json, key);

    if (text != NULL && strncmp(text, "true", 4) == 0) {
        *value = true;
        return true;
    }

    if (text != NULL && strncmp(text, "false", 5) == 0) {
        *value = false;
        return true;
    }

    return false;
}

static bool parse_led_command(const char *text, uint8_t *index, uint8_t *red, uint8_t *green, uint8_t *blue, uint8_t *white) {
    uint8_t *values[] = {index, red, green, blue, white};

    for (uint8_t value_index = 0; value_index < ARRAY_SIZE(values); value_index++) {
        char *end;
        long  value = strtol(text, &end, 10);

        if (end == text || value < 0 || value > UINT8_MAX) {
            return false;
        }

        *values[value_index] = value;
        text                 = end;
        if (value_index + 1 < ARRAY_SIZE(values)) {
            if (*text++ != ',') {
                return false;
            }
        }
    }

    return *json_skip_space(text) == '\0' && *index < RGBLIGHT_LED_COUNT;
}

static bool json_read_map(const char *json, uint8_t notches, int16_t *map) {
    const char *text = json_value(json, "\"map\"");

    if (text == NULL || *text++ != '[') {
        return false;
    }

    for (uint8_t index = 0; index < notches; index++) {
        char *end;

        text = json_skip_space(text);
        long parsed = strtol(text, &end, 10);
        if (end == text || parsed < INT16_MIN || parsed > INT16_MAX) {
            return false;
        }

        map[index] = parsed;
        text       = json_skip_space(end);
        if (index + 1 < notches && *text++ != ',') {
            return false;
        }
    }

    return *json_skip_space(text) == ']';
}

static bool json_read_slider_config(const char *json, uint8_t version, slider_axis_config_t *config) {
    int32_t     joy_id;
    int32_t     axis_id;
    int32_t     addr;
    int32_t     notches;
    int32_t     snap_pct;
    int32_t     valid_range_min;
    int32_t     valid_range_max;
    int32_t     adc_min;
    int32_t     adc_max;
    int32_t     output_min;
    int32_t     output_max;
    char        name[SLIDER_AXIS_NAME_LENGTH];
    bool        invert;
    const char *adc     = json_value(json, "\"adc\"");
    const char *output  = json_value(json, "\"output\"");
    const char *profile = output == NULL ? NULL : json_value(output, slider_map_json_keys[0]);
    const char *range   = profile == NULL ? NULL : json_value(profile, "\"range\"");

    bool filter_adc;

    if (adc == NULL || range == NULL || !json_read_string(json, "\"name\"", name, sizeof(name)) || !json_read_bool(json, "\"invert\"", &invert) || !json_read_bool(json, "\"filter_adc\"", &filter_adc) || !json_read_number(json, "\"joy_id\"", &joy_id) || !json_read_number(json, "\"axis_id\"", &axis_id) || !json_read_number(json, "\"addr\"", &addr) || !json_read_number(json, "\"notches\"", &notches) || !json_read_number(json, "\"snap_pct\"", &snap_pct) || !json_read_number(json, "\"valid_range_min\"", &valid_range_min) || !json_read_number(json, "\"valid_range_max\"", &valid_range_max) || !json_read_number(adc, "\"min\"", &adc_min) || !json_read_number(adc, "\"max\"", &adc_max) || !json_read_number(range, "\"min\"", &output_min) || !json_read_number(range, "\"max\"", &output_max)) {
        return false;
    }

    if (joy_id < 0 || joy_id > UINT8_MAX || axis_id < 0 || axis_id > UINT8_MAX || addr < 0 || addr > UINT8_MAX || notches < 0 || notches > UINT8_MAX || snap_pct < 0 || snap_pct > UINT8_MAX || valid_range_min < 0 || valid_range_min > 1023 || valid_range_max < 0 || valid_range_max > 1023 || adc_min < 0 || adc_min > UINT16_MAX || adc_max < 0 || adc_max > UINT16_MAX || output_min < INT16_MIN || output_min > INT16_MAX || output_max < INT16_MIN || output_max > INT16_MAX) {
        return false;
    }

    *config = (slider_axis_config_t){
        .version    = version,
        .joy_id     = joy_id,
        .axis_id    = axis_id,
        .addr       = addr,
        .notches    = notches,
        .snap_pct   = snap_pct,
        .map_mask   = 0,
        .invert     = invert,
        .filter_adc = filter_adc,
        .valid_range_min = valid_range_min,
        .valid_range_max = valid_range_max,
        .adc_min    = adc_min,
        .adc_max    = adc_max,
        .output_min = output_min,
        .output_max = output_max,
    };
    memcpy(config->name, name, sizeof(name));

    for (uint8_t map_index = 0; map_index < SLIDER_MAP_COUNT; map_index++) {
        int16_t profile_map[SLIDER_CONFIG_MAX_NOTCHES];
        int32_t profile_output_min;
        int32_t profile_output_max;

        profile = json_value(output, slider_map_json_keys[map_index]);
        if (profile == NULL) {
            continue;
        }

        range   = profile == NULL ? NULL : json_value(profile, "\"range\"");
        if (range == NULL || !json_read_number(range, "\"min\"", &profile_output_min) || !json_read_number(range, "\"max\"", &profile_output_max) || profile_output_min != config->output_min || profile_output_max != config->output_max || !json_read_map(profile, config->notches, profile_map)) {
            return false;
        }

        memcpy(config->maps[map_index], profile_map, sizeof(profile_map));
    config->map_mask |= 1 << map_index;
    }

    return slider_config_is_valid(config);
}

static char *json_object_end(char *text) {
    uint8_t depth = 0;

    do {
        if (*text == '{') {
            depth++;
        } else if (*text == '}' && --depth == 0) {
            return text + 1;
        }
    } while (*++text != '\0');

    return NULL;
}

static int8_t slider_map_index_from_json(const char *text) {
    for (uint8_t map_index = 0; map_index < SLIDER_MAP_COUNT; map_index++) {
        size_t name_length = strlen(slider_map_names[map_index]);

        if (text[0] == '"' && strncmp(text + 1, slider_map_names[map_index], name_length) == 0 && text[name_length + 1] == '"') {
            return map_index;
        }
    }

    return -1;
}

static int8_t slider_map_index_from_name(const char *name) {
    for (uint8_t map_index = 0; map_index < SLIDER_MAP_COUNT; map_index++) {
        if (strcmp(name, slider_map_names[map_index]) == 0) {
            return map_index;
        }
    }

    return -1;
}

static bool json_read_slider_configs(char *json, slider_axis_config_t *configs, uint8_t *config_count, uint8_t *active_map) {
    int32_t     version;
    const char *active = json_value(json, "\"active\"");
    char       *axes   = (char *)json_value(json, "\"axes\"");
    int8_t      selected_map;

    if (active == NULL || (selected_map = slider_map_index_from_json(active)) < 0 || axes == NULL || !json_read_number(json, "\"version\"", &version) || version != SLIDER_CONFIG_VERSION || *axes++ != '[') {
        return false;
    }

    for (*config_count = 0; *config_count < SLIDER_CONFIG_MAX_AXES; (*config_count)++) {
        char *object_end;
        char  delimiter;

        axes = (char *)json_skip_space(axes);
        if (*axes != '{' || (object_end = json_object_end(axes)) == NULL) {
            return false;
        }

        delimiter   = *object_end;
        *object_end = '\0';
        if (!json_read_slider_config(axes, version, &configs[*config_count])) {
            *object_end = delimiter;
            return false;
        }
        *object_end = delimiter;

        axes = (char *)json_skip_space(object_end);
        if (*axes == ']') {
            (*config_count)++;
            *active_map = selected_map;
            return slider_configs_are_valid(configs, *config_count);
        }

        if (*axes++ != ',') {
            return false;
        }
    }

    return false;
}

static void serial_send_config(void) {
    char line[256];
    int32_t length = snprintf(line, sizeof(line), "CONF={\"version\":%u,\"active\":\"%s\",\"axes\":[", SLIDER_CONFIG_VERSION, slider_map_names[active_slider_map]);
    serial_send_buffer(line, length);

    for (uint8_t axis_index = 0; axis_index < slider_axis_count; axis_index++) {
        const slider_axis_config_t *config = &slider_axis_configs[axis_index];
        bool                        profile_written = false;

        length = snprintf(line, sizeof(line), "%s{\"name\":\"%s\",\"joy_id\":%u,\"axis_id\":%u,\"addr\":%u,\"notches\":%u,\"snap_pct\":%u,\"invert\":%s,\"filter_adc\":%s,\"valid_range_min\":%u,\"valid_range_max\":%u,\"adc\":{\"min\":%u,\"max\":%u},\"output\":{", axis_index == 0 ? "" : ",", config->name, config->joy_id, config->axis_id, config->addr, config->notches, config->snap_pct, config->invert ? "true" : "false", config->filter_adc ? "true" : "false", config->valid_range_min, config->valid_range_max, config->adc_min, config->adc_max);
        serial_send_buffer(line, length);

        for (uint8_t profile_index = 0; profile_index < SLIDER_MAP_COUNT; profile_index++) {
            if ((config->map_mask & (1 << profile_index)) == 0) {
                continue;
            }

            length = snprintf(line, sizeof(line), "%s\"%s\":{\"range\":{\"min\":%d,\"max\":%d},\"map\":[", profile_written ? "," : "", slider_map_names[profile_index], config->output_min, config->output_max);
            serial_send_buffer(line, length);
            for (uint8_t map_index = 0; map_index < config->notches; map_index++) {
                length = snprintf(line, sizeof(line), "%s%d", map_index == 0 ? "" : ",", config->maps[profile_index][map_index]);
                serial_send_buffer(line, length);
            }
            serial_send_text("]}");
            profile_written = true;
        }

        serial_send_text("}}");
    }

    serial_send_text("]}\r\n");
}

static void process_config_command(void) {
    uint8_t              new_config_count;
    uint8_t              new_active_map;

    if (strcmp(config_command, "get_config") == 0) {
        serial_send_config();
        for (uint8_t axis_index = 0; axis_index < slider_axis_count; axis_index++) {
            serial_send_adc_reading(&slider_axis_configs[axis_index], &slider_axis_runtimes[axis_index]);
        }
    } else if (strncmp(config_command, "set_config ", 11) == 0) {
        if (json_read_slider_configs(config_command + 11, pending_slider_axis_configs, &new_config_count, &new_active_map)) {
            memcpy(slider_axis_configs, pending_slider_axis_configs, sizeof(pending_slider_axis_configs));
            memset(slider_axis_runtimes, 0, sizeof(slider_axis_runtimes));
            slider_axis_count = new_config_count;
            active_slider_map = new_active_map;
            slider_config_save();
            serial_send_text("{\"status\":\"ok\"}\r\n");
        } else {
            serial_send_text("{\"status\":\"error\",\"error\":\"invalid_config\"}\r\n");
        }
    } else if (strncmp(config_command, "set_axis_config ", 16) == 0) {
        slider_axis_config_t updated_config;

        if (json_read_slider_config(config_command + 16, SLIDER_CONFIG_VERSION, &updated_config)) {
            for (uint8_t axis_index = 0; axis_index < slider_axis_count; axis_index++) {
                if (slider_axis_configs[axis_index].joy_id == updated_config.joy_id && slider_axis_configs[axis_index].axis_id == updated_config.axis_id) {
                    slider_axis_configs[axis_index] = updated_config;
                    memset(&slider_axis_runtimes[axis_index], 0, sizeof(slider_axis_runtimes[axis_index]));
                    slider_config_save();
                    serial_send_text("{\"status\":\"ok\"}\r\n");
                    return;
                }
            }
        }

        serial_send_text("{\"status\":\"error\",\"error\":\"invalid_axis_config\"}\r\n");
    } else if (strncmp(config_command, "set_led ", 8) == 0) {
        uint8_t index;
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        uint8_t white;

        if (parse_led_command(config_command + 8, &index, &red, &green, &blue, &white)) {
            rgblight_mode_noeeprom(RGBLIGHT_MODE_STATIC_LIGHT);
            ws2812_leds[index].r = red;
            ws2812_leds[index].g = green;
            ws2812_leds[index].b = blue;
            ws2812_leds[index].w = white;
            ws2812_flush();
            serial_send_text("{\"status\":\"ok\"}\r\n");
        } else {
            serial_send_text("{\"status\":\"error\",\"error\":\"invalid_led\"}\r\n");
        }
    } else if (strncmp(config_command, "set_active ", 11) == 0) {
        int8_t selected_map = slider_map_index_from_name(config_command + 11);

        if (selected_map >= 0) {
            char line[48];

            active_slider_map = selected_map;
            slider_config_save();
            int32_t length = snprintf(line, sizeof(line), "ACTIVE=%s\r\n", slider_map_names[active_slider_map]);
            serial_send_buffer(line, length);
        } else {
            serial_send_text("{\"status\":\"error\",\"error\":\"invalid_active\"}\r\n");
        }
    } else if (strncmp(config_command, "set_axis ", 9) == 0) {
        const char *text = config_command + 9;
        int32_t     joy_id = 0;
        int32_t     axis_id = 0;
        int32_t     value = 0;
        char       *end;

        joy_id = strtol(text, &end, 10);
        if (end != text) {
            text = json_skip_space(end);
            axis_id = strtol(text, &end, 10);
        }
        if (end != text) {
            text = json_skip_space(end);
            value = strtol(text, &end, 10);
        }

        if (end != text && *json_skip_space(end) == '\0' && joy_id >= 0 && joy_id <= UINT8_MAX && axis_id >= 0 && axis_id <= UINT8_MAX && value >= -JOYSTICK_MAX_VALUE && value <= JOYSTICK_MAX_VALUE) {
            for (uint8_t config_index = 0; config_index < slider_axis_count; config_index++) {
                slider_axis_config_t  *config  = &slider_axis_configs[config_index];
                slider_axis_runtime_t *runtime = &slider_axis_runtimes[config_index];

                if (config->joy_id == joy_id && config->axis_id == axis_id) {
                    runtime->manual_override_raw_adc_reading = runtime->raw_adc_reading;
                    runtime->manual_override_axis_value      = value;
                    runtime->manual_override_active          = true;
                    runtime->axis_value                      = value;
                    serial_send_adc_reading(config, runtime);
                    return;
                }
            }
        }

        serial_send_text("{\"status\":\"error\",\"error\":\"invalid_axis\"}\r\n");
    } else if (config_command[0] != '\0') {
        serial_send_text("{\"status\":\"error\",\"error\":\"unknown_command\"}\r\n");
    }
}

static void set_multiplexer_address(uint8_t address) {
    gpio_write_pin(GP9, address & (1 << 0));
    gpio_write_pin(GP8, address & (1 << 1));
    gpio_write_pin(GP7, address & (1 << 2));
    gpio_write_pin(GP6, address & (1 << 3));
}

void keyboard_post_init_user(void) {
    gpio_set_pin_output(GP25);
    gpio_write_pin_low(GP25);
    gpio_set_pin_output(GP9);
    gpio_set_pin_output(GP8);
    gpio_set_pin_output(GP7);
    gpio_set_pin_output(GP6);
    set_multiplexer_address(0);
    slider_config_load();
    last_blink_time = timer_read32();
    last_adc_sample_time = last_blink_time;
    last_adc_debug_time  = last_blink_time;
}

static uint16_t filter_adc_reading(slider_axis_runtime_t *runtime, uint16_t reading) {
    if (runtime->adc_sample_count < ADC_FILTER_SAMPLES) {
        runtime->adc_sample_count++;
    } else {
        runtime->adc_sample_total -= runtime->adc_samples[runtime->adc_sample_index];
    }

    runtime->adc_samples[runtime->adc_sample_index] = reading;
    runtime->adc_sample_total += reading;
    runtime->adc_sample_index = (runtime->adc_sample_index + 1) % ADC_FILTER_SAMPLES;

    return runtime->adc_sample_total / runtime->adc_sample_count;
}

static uint8_t quantize_adc_reading(const slider_axis_config_t *config, slider_axis_runtime_t *runtime, uint16_t reading) {
    if (reading < config->adc_min) {
        reading = config->adc_min;
    } else if (reading > config->adc_max) {
        reading = config->adc_max;
    }

    uint16_t range = config->adc_max - config->adc_min;
    uint32_t scaled_reading = (uint32_t)(reading - config->adc_min) * (config->notches - 1);

    if (!runtime->notch_initialized) {
        runtime->notch = (scaled_reading + range / 2) / range;
        runtime->notch_initialized = true;
        return runtime->notch;
    }

    while (runtime->notch + 1 < config->notches && scaled_reading >= (uint32_t)(runtime->notch + 1) * range - (uint32_t)config->snap_pct * range / 100) {
        runtime->notch++;
    }

    while (runtime->notch > 0 && scaled_reading <= (uint32_t)(runtime->notch - 1) * range + (uint32_t)config->snap_pct * range / 100) {
        runtime->notch--;
    }

    return runtime->notch;
}

static int16_t slider_axis_value(const slider_axis_config_t *config, slider_axis_runtime_t *runtime, uint16_t reading) {
    if (config->invert) {
        reading = config->adc_min + config->adc_max - reading;
    }

    uint8_t notch = quantize_adc_reading(config, runtime, reading);
    uint8_t map_index = config->map_mask & (1 << active_slider_map) ? active_slider_map : 0;

    return config->maps[map_index][notch];
}

static void set_configured_axis(const slider_axis_config_t *config, int16_t value) {
    if (config->joy_id == 0) {
        joystick_set_axis(config->axis_id, value);
    } else {
        joystick2_set_axis(config->axis_id, value);
    }
}

static void update_slider_axes(void) {
    for (uint8_t axis_index = 0; axis_index < slider_axis_count; axis_index++) {
        const slider_axis_config_t *config  = &slider_axis_configs[axis_index];
        slider_axis_runtime_t      *runtime = &slider_axis_runtimes[axis_index];

        set_multiplexer_address(config->addr);
        wait_us(MULTIPLEXER_SETTLE_TIME_US);
        runtime->raw_adc_reading = analogReadPin(GP26);
        if (!config->filter_adc || (runtime->raw_adc_reading >= config->valid_range_min && runtime->raw_adc_reading <= config->valid_range_max)) {
            runtime->last_valid_adc_reading = runtime->raw_adc_reading;
            runtime->has_valid_adc_reading  = true;
        } else if (runtime->has_valid_adc_reading) {
            runtime->raw_adc_reading = runtime->last_valid_adc_reading;
        } else {
            continue;
        }

        if (runtime->manual_override_active && (int32_t)runtime->raw_adc_reading - runtime->manual_override_raw_adc_reading >= MANUAL_AXIS_RELEASE_RAW_DELTA) {
            runtime->manual_override_active = false;
        } else if (runtime->manual_override_active && (int32_t)runtime->manual_override_raw_adc_reading - runtime->raw_adc_reading >= MANUAL_AXIS_RELEASE_RAW_DELTA) {
            runtime->manual_override_active = false;
        }

        if (runtime->manual_override_active) {
            runtime->axis_value = runtime->manual_override_axis_value;
        } else {
            runtime->axis_value = slider_axis_value(config, runtime, filter_adc_reading(runtime, runtime->raw_adc_reading));
        }

        if (runtime->axis_initialized && runtime->axis_value != runtime->last_axis_value) {
            runtime->last_axis_delta = runtime->axis_value - runtime->last_axis_value;
        }

        runtime->last_axis_value = runtime->axis_value;
        runtime->axis_initialized = true;
        set_configured_axis(config, runtime->axis_value);
    }
}

static void serial_send_adc_reading(const slider_axis_config_t *config, const slider_axis_runtime_t *runtime) {
    char line[48];

    int32_t length = snprintf(line, sizeof(line), "AXIS=%u, %u, %u, %u, %d, %u\r\n", config->joy_id, config->axis_id, config->addr, runtime->raw_adc_reading, runtime->axis_value, runtime->notch);

    serial_send_buffer(line, length);
}

static void serial_send_heartbeat(void) {
    char line[32];
    int32_t length = snprintf(line, sizeof(line), "HEARTBEAT\r\n");
    serial_send_buffer(line, length);
}

static bool adc_debug_value_changed(slider_axis_runtime_t *runtime) {
    int32_t delta = (int32_t)runtime->raw_adc_reading - runtime->last_reported_raw_adc_reading;

    if (!runtime->raw_adc_reading_reported || delta >= ADC_DEBUG_RAW_DELTA || delta <= -ADC_DEBUG_RAW_DELTA || runtime->axis_value != runtime->last_reported_axis_value || runtime->notch != runtime->last_reported_notch) {
        runtime->last_reported_raw_adc_reading = runtime->raw_adc_reading;
        runtime->last_reported_axis_value      = runtime->axis_value;
        runtime->last_reported_notch           = runtime->notch;
        runtime->raw_adc_reading_reported      = true;
        return true;
    }

    return false;
}

static int8_t serial_axis_for_key(uint8_t byte) {
    static const char axis_keys[] = "qwertyuiop[]";

    for (uint8_t axis = 0; axis < sizeof(axis_keys) - 1; axis++) {
        if (byte == axis_keys[axis]) {
            return axis;
        }
    }

    return -1;
}

void virtser_recv(uint8_t byte) {
    if (byte == '\r') {
        return;
    }

    if (byte == '\n') {
        if (!config_command_overflow) {
            config_command[config_command_length] = '\0';
            process_config_command();
        } else {
            serial_send_text("{\"status\":\"error\",\"error\":\"command_too_long\"}\r\n");
        }

        config_command_length   = 0;
        config_command_overflow = false;
        return;
    }

    int8_t axis = serial_axis_for_key(byte);
    if (config_command_length == 0 && axis >= 0) {
        active_axis_tests ^= (uint16_t)1 << axis;
    } else if (config_command_length < sizeof(config_command) - 1) {
        config_command[config_command_length++] = byte;
    } else {
        config_command_overflow = true;
    }
}

static int16_t joystick_axis_value(uint32_t phase) {
    if (phase < JOYSTICK_AXIS_LEG_MS) {
        return -JOYSTICK_MAX_VALUE + (int32_t)phase * JOYSTICK_MAX_VALUE / JOYSTICK_AXIS_LEG_MS;
    }

    if (phase < JOYSTICK_AXIS_LEG_MS + JOYSTICK_AXIS_CENTER_PAUSE_MS) {
        return 0;
    }

    phase -= JOYSTICK_AXIS_LEG_MS + JOYSTICK_AXIS_CENTER_PAUSE_MS;
    if (phase < JOYSTICK_AXIS_LEG_MS) {
        return (int32_t)phase * JOYSTICK_MAX_VALUE / JOYSTICK_AXIS_LEG_MS;
    }

    phase -= JOYSTICK_AXIS_LEG_MS;
    if (phase < JOYSTICK_AXIS_LEG_MS) {
        return JOYSTICK_MAX_VALUE - (int32_t)phase * JOYSTICK_MAX_VALUE / JOYSTICK_AXIS_LEG_MS;
    }

    phase -= JOYSTICK_AXIS_LEG_MS;
    if (phase < JOYSTICK_AXIS_CENTER_PAUSE_MS) {
        return 0;
    }

    phase -= JOYSTICK_AXIS_CENTER_PAUSE_MS;
    if (phase < JOYSTICK_AXIS_LEG_MS) {
        return -(int32_t)phase * JOYSTICK_MAX_VALUE / JOYSTICK_AXIS_LEG_MS;
    }

    return 0;
}

void matrix_scan_user(void) {
    uint32_t elapsed_ms = timer_read32();

    // QK_BOOT when JS_0, JS_1, JS_2 are pressed together
    if (matrix_is_on(0, 0) && matrix_is_on(0, 1) && matrix_is_on(0, 2)) {
        reset_keyboard();
    }


    if (timer_elapsed32(last_blink_time) >= 500) {
        gpio_toggle_pin(GP25);
        serial_send_heartbeat();
        last_blink_time = elapsed_ms;
    }

    if (timer_elapsed32(last_adc_sample_time) >= 10) {
        update_slider_axes();
        last_adc_sample_time = elapsed_ms;
    }

    if (timer_elapsed32(last_adc_debug_time) >= 10) {
        for (uint8_t axis_index = 0; axis_index < slider_axis_count; axis_index++) {
            if (adc_debug_value_changed(&slider_axis_runtimes[axis_index])) {
                serial_send_adc_reading(&slider_axis_configs[axis_index], &slider_axis_runtimes[axis_index]);
            }
        }
        last_adc_debug_time = elapsed_ms;
    }

    for (uint8_t axis = 0; axis < JOYSTICK_AXIS_COUNT; axis++) {
        uint32_t phase = (elapsed_ms + axis * JOYSTICK_AXIS_PHASE_OFFSET_MS) % JOYSTICK_AXIS_TEST_MS;
        joystick_set_axis(axis, active_axis_tests & ((uint16_t)1 << axis) ? joystick_axis_value(phase) : 0);
    }

    for (uint8_t axis = 0; axis < JOYSTICK2_AXIS_COUNT; axis++) {
        uint8_t  test_axis = JOYSTICK_AXIS_COUNT + axis;
        uint32_t phase     = (elapsed_ms + test_axis * JOYSTICK_AXIS_PHASE_OFFSET_MS) % JOYSTICK_AXIS_TEST_MS;
        joystick2_set_axis(axis, active_axis_tests & ((uint16_t)1 << test_axis) ? joystick_axis_value(phase) : 0);
    }

    for (uint8_t axis_index = 0; axis_index < slider_axis_count; axis_index++) {
        set_configured_axis(&slider_axis_configs[axis_index], slider_axis_runtimes[axis_index].axis_value);
    }
    
}
