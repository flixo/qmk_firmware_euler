// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "gpio.h"
#include <math.h>

#if defined(PS2_MOUSE_ENABLE)
#    include "ps2.h"
#    include "ps2_mouse.h"
#endif

#define ______ KC_TRANSPARENT
#define MASKED KC_NO


const key_override_t delete_key_override = ko_make_basic(MOD_MASK_SHIFT, KC_BSPC, KC_DEL);
const key_override_t *key_overrides[] = {
	&delete_key_override
};

enum layer_names {
    _BL,
    _UL,
    _FL,
    _TEMPLATE,
};

enum custom_keycodes {
    KC_FN = SAFE_RANGE,
    CK_PTI,
    CK_PTD,
    CK_WINL,
};


#define PT_SENSITIVITY_DEFAULT 128
#define PT_SENSITIVITY_MIN     1
#define PT_SENSITIVITY_MAX     255
#define PT_SENSITIVITY_STEP    20
#define AUTO_MOUSE_TIMEOUT_MS  700

static uint8_t pt_sensitivity = PT_SENSITIVITY_DEFAULT;

// Rotate trackpoint input by this many degrees (positive = counter-clockwise)
#define PT_ROTATION_DEGREES 0.0f

// Lower threshold = more sensitive scrolling
#define PT_SCROLL_THRESHOLD 44
#define PT_CENTER_DEADZONE  6
#define PT_AXIS_LOCK_RELEASE_MS 400

// Low-pass filter for scroll source: higher keeps more previous value (0-255)
#define PT_SCROLL_FILTER_ALPHA 192

// Volume mode tuning when _FL is held (Y axis only)
#define PT_VOLUME_THRESHOLD 42
#define PT_VOLUME_FILTER_ALPHA 224

// SK8707-06 z-tap setting value for TrackPoint extended command 0x2C.
// Most implementations use 0x00 = enabled, 0x01 = disabled.
#ifndef SK8707_ZTAP_VALUE
#    define SK8707_ZTAP_VALUE 0x01
#endif

static float pt_rot_sin = 0.0f;
static float pt_rot_cos = 1.0f;

static int16_t pt_scroll_x_acc = 0;
static int16_t pt_scroll_y_acc = 0;
static int16_t pt_scroll_x_filtered = 0;
static int16_t pt_scroll_y_filtered = 0;
static int16_t pt_volume_x_acc = 0;
static int16_t pt_volume_x_filtered = 0;

typedef enum {
    PT_MODE_UNLOCKED = 0,
    PT_MODE_SCROLL,
    PT_MODE_VOLUME,
} pt_mode_lock_t;

static pt_mode_lock_t pt_mode_lock = PT_MODE_UNLOCKED;
static uint32_t pt_mode_deadzone_timer = 0;

static bool use_auto_mouse = false;
static uint32_t auto_mouse_timer = 0;
static uint8_t auto_mouse_lbtn_holds = 0;
static uint8_t auto_mouse_rbtn_holds = 0;
static bool left_gui_pending = false;
static bool left_gui_chorded = false;

static inline void auto_mouse_refresh_timer(void) {
    auto_mouse_timer = timer_read32();
}

static inline bool auto_mouse_is_active(void) {
    return use_auto_mouse && timer_elapsed32(auto_mouse_timer) < AUTO_MOUSE_TIMEOUT_MS;
}

static inline void auto_mouse_press(uint16_t btn_keycode, uint8_t *hold_count) {
    if (*hold_count == 0) {
        register_code(btn_keycode);
    }
    (*hold_count)++;
    auto_mouse_refresh_timer();
}

static inline void auto_mouse_release(uint16_t btn_keycode, uint8_t *hold_count) {
    if (*hold_count > 0) {
        (*hold_count)--;
        if (*hold_count == 0) {
            unregister_code(btn_keycode);
        }
    }
}

static inline bool host_is_windows(void) {
    return detected_host_os() == OS_WINDOWS;
}

#define KC_OE ALGR(KC_SCLN)
#define KC_AE ALGR(KC_QUOT)
#define KC_AA ALGR(KC_LBRC)

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BL] = LAYOUT_fullsize_ansi( /* Base Layer */
        /**|  ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐                                                                 ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌───────────────┐  |**/
        /**/    KC_ESC,   KC_GRV,    KC_1,     KC_2,     KC_3,     KC_4,    KC_5,                                                                       KC_6,     KC_7,     KC_8,    KC_9,     KC_0,     KC_MINS,   KC_EQL,     KC_BSPC,      /**/
        /**|  └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└────────┘                                                                 └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└───────────────┘  |**/
        /**|            ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐                      ┌────────┐┌────────┐┌────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐         |**/
        /**/               KC_TAB,   KC_Q,     KC_W,     KC_E,    KC_R,    KC_T,                           KC_PSLS,   KC_PAST,  KC_PMNS,  KC_INS,       KC_Y,     KC_U,     KC_I,    KC_O,     KC_P,     KC_LBRC,  KC_RBRC,  KC_BSLS,         /**/
        /**|            └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘                      └────────┘└────────┘└────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└────────┘         |**/
        /**|            ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐            ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌──────────────────┐         |**/
        /**/              KC_CAPS,   KC_A,     KC_S,     KC_D,    KC_F,    KC_G,                   KC_P7,    KC_P8,   KC_P9,              KC_DEL,       KC_H,     KC_J,     KC_K,    KC_L,     KC_SCLN,  KC_QUOT,       KC_ENT,               /**/
        /**|            └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘            └────────┘└────────┘└────────┘│        │└────────┘   └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└──────────────────┘         |**/
        /**|            ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐            ┌────────┐┌────────┐┌────────┐│        │             ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌──────────────────┐┌────────┐         |**/
        /**/              KC_LSFT,   KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,                 KC_P4,    KC_P5,   KC_P6,   KC_PPLS,                 KC_N,     KC_M,    KC_COMM,  KC_DOT,   KC_SLSH,       KC_RSFT,      MO(_UL),          /**/
        /**|            └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘            └────────┘└────────┘└────────┘└────────┘             └────────┘└────────┘└────────┘└────────┘└────────┘└──────────────────┘└────────┘         |**/
        /**|            ┌─────────────┐┌───────────┐┌──────────┐┌──────────────────┐            ┌────────┐┌────────┐┌────────┐┌────────┐             ┌─────────────────────┐┌───────────┐┌───────────┐┌─────────────┐┌──────────────┐         |**/
        /**/                KC_LCTL,      KC_LGUI,    KC_LALT,        KC_SPACE,                   KC_P1,     KC_P2,   KC_P3,   KC_PENT,                      MO(_FL),          KC_RALT,     KC_RGUI,      KC_MENU,       KC_RCTL,             /**/
        /**|            └─────────────┘└───────────┘└──────────┘└──────────────────┘            └────────┘└────────┘└────────┘│        │             └─────────────────────┘└───────────┘└───────────┘└─────────────┘└──────────────┘         |**/
        /**|                                                   ┌────────┐┌────────┐┌────────┐   ┌──────────────────┐┌────────┐│        │   ┌────────┐┌────────┐┌────────┐                                                                     |**/
        /**/                                                    KC_PGUP,  KC_HOME,  MO(_UL),           KC_P0,        KC_PDOT,               KC_BSPC,  KC_END,   KC_PGDN                                                                       /**/
        /**|                                                   └────────┘└────────┘└────────┘   └──────────────────┘└────────┘└────────┘   └────────┘└────────┘└────────┘                                                                     |**/
    ),

    [_FL] = LAYOUT_fullsize_ansi( /* Function Layer */
        /**|  ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐                                                                 ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌───────────────┐  |**/
        /**/    ______,   ______,   KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,                                                                     KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,   KC_F12,      ______,       /**/
        /**|  └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└────────┘                                                                 └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└───────────────┘  |**/
        /**|            ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐                      ┌────────┐┌────────┐┌────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐         |**/
        /**/              ______,   ______,   ______,   ______,   ______,   ______,                         ______,   ______,   ______,   ______,      ______,   KC_HOME,   KC_UP,   KC_END,   ______,   KC_AA,   ______,   ______,          /**/
        /**|            └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘                      └────────┘└────────┘└────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└────────┘         |**/
        /**|            ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐            ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌──────────────────┐         |**/
        /**/              ______,   ______,   ______,   ______,   ______,   ______,               ______,   ______,   ______,             ______,      ______,   KC_LEFT,  KC_DOWN,  KC_RGHT,   KC_OE,   KC_AE,        ______,               /**/
        /**|            └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘            └────────┘└────────┘└────────┘│        │└────────┘   └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└──────────────────┘         |**/
        /**|            ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐            ┌────────┐┌────────┐┌────────┐│        │             ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌──────────────────┐┌────────┐         |**/
        /**/              ______,   ______,   ______,   ______,   ______,   ______,               ______,   ______,   ______,   ______,                ______,   ______,   ______,   ______,   ______,       ______,         ______,          /**/
        /**|            └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘            └────────┘└────────┘└────────┘└────────┘             └────────┘└────────┘└────────┘└────────┘└────────┘└──────────────────┘└────────┘         |**/
        /**|            ┌─────────────┐┌───────────┐┌──────────┐┌──────────────────┐            ┌────────┐┌────────┐┌────────┐┌────────┐             ┌─────────────────────┐┌───────────┐┌───────────┐┌─────────────┐┌──────────────┐         |**/
        /**/                ______,       ______,      ______,         ______,                    ______,   ______,   ______,   ______,                      ______,           ______,      ______,       ______,         ______,             /**/
        /**|            └─────────────┘└───────────┘└──────────┘└──────────────────┘            └────────┘└────────┘└────────┘│        │             └─────────────────────┘└───────────┘└───────────┘└─────────────┘└──────────────┘         |**/
        /**|                                                   ┌────────┐┌────────┐┌────────┐   ┌──────────────────┐┌────────┐│        │   ┌────────┐┌────────┐┌────────┐                                                                     |**/
        /**/                                                     ______,   ______,   ______,           ______,        ______,                ______,   ______,   ______                                                                       /**/
        /**|                                                   └────────┘└────────┘└────────┘   └──────────────────┘└────────┘└────────┘   └────────┘└────────┘└────────┘                                                                     |**/
    ),

    
    [_UL] = LAYOUT_fullsize_ansi( /* Utility Layer */
        /**|  ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐                                                                 ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌───────────────┐  |**/
        /**/    QK_BOOT,  ______,  ______,    ______,   ______,   ______,   ______,                                                                    ______,   ______,   ______,   ______,   ______,   CK_PTD,   CK_PTI,      ______,       /**/
        /**|  └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└────────┘                                                                 └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└───────────────┘  |**/
        /**|            ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐                      ┌────────┐┌────────┐┌────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐         |**/
        /**/              ______,   ______,   ______,   ______,   ______,   ______,                        KC_PSCR,  KC_SCRL,  KC_PAUS,   ______,      ______,   ______,   ______,   ______,   ______,   ______,   ______,   ______,          /**/
        /**|            └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘                      └────────┘└────────┘└────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└────────┘         |**/
        /**|            ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐            ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌──────────────────┐         |**/
        /**/              ______,   ______,   ______,   ______,   ______,   ______,               ______,   ______,   ______,             KC_MUTE,     ______,   ______,   ______,   ______,   ______,   ______,        ______,               /**/
        /**|            └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘            └────────┘└────────┘└────────┘│        │└────────┘   └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘└──────────────────┘         |**/
        /**|            ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐            ┌────────┐┌────────┐┌────────┐│        │             ┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐┌──────────────────┐┌────────┐         |**/
        /**/              ______,   ______,   ______,   ______,   ______,   ______,               ______,   ______,   ______,   ______,                ______,   ______,   ______,   ______,   ______,       ______,         ______,          /**/
        /**|            └────────┘└────────┘└────────┘└────────┘└────────┘└────────┘            └────────┘└────────┘└────────┘└────────┘             └────────┘└────────┘└────────┘└────────┘└────────┘└──────────────────┘└────────┘         |**/
        /**|            ┌─────────────┐┌───────────┐┌──────────┐┌──────────────────┐            ┌────────┐┌────────┐┌────────┐┌────────┐             ┌─────────────────────┐┌───────────┐┌───────────┐┌─────────────┐┌──────────────┐         |**/
        /**/                ______,       ______,      ______,         ______,                    ______,   ______,   ______,   ______,                      ______,           ______,      ______,       ______,         ______,             /**/
        /**|            └─────────────┘└───────────┘└──────────┘└──────────────────┘            └────────┘└────────┘└────────┘│        │             └─────────────────────┘└───────────┘└───────────┘└─────────────┘└──────────────┘         |**/
        /**|                                                   ┌────────┐┌────────┐┌────────┐   ┌──────────────────┐┌────────┐│        │   ┌────────┐┌────────┐┌────────┐                                                                     |**/
        /**/                                                     ______,   ______,   ______,           ______,        ______,                KC_MPRV, KC_MNXT,  KC_MPLY                                                                       /**/
        /**|                                                   └────────┘└────────┘└────────┘   └──────────────────┘└────────┘└────────┘   └────────┘└────────┘└────────┘                                                                     |**/
    ),
};

#if defined PS2_MOUSE_ENABLE && defined MOUSEKEY_ENABLE
    void ps2_mouse_init_user(void) {
        // Enable/disable z-tap (press-to-select) via extended TrackPoint command.
        PS2_MOUSE_SEND(0xE2, "sk8707: ext cmd");
        PS2_MOUSE_SEND(0x47, "sk8707: write cmd");
        PS2_MOUSE_SEND(0x2C, "sk8707: ztap reg");
        PS2_MOUSE_SEND(SK8707_ZTAP_VALUE, "sk8707: ztap value");

        
        
        PS2_MOUSE_SEND(0xE2, "sk8707: ext cmd");
        PS2_MOUSE_SEND(0x80, "sk8707: read cmd");
        PS2_MOUSE_SEND(0x5C, "sk8707: read ztap reg");

        PS2_MOUSE_SEND(0xE2, "sk8707: ext cmd");
        PS2_MOUSE_SEND(0x81, "sk8707: read cmd");
        PS2_MOUSE_SEND(0x5C, "sk8707: read ztap reg");
        PS2_MOUSE_SEND(0x02, "sk8707: read ztap value");
        
    }

    void ps2_mouse_moved_user(report_mouse_t *mouse_report) {
        // Apply rotation
        float rx = mouse_report->x * pt_rot_cos - mouse_report->y * pt_rot_sin;
        float ry = mouse_report->x * pt_rot_sin + mouse_report->y * pt_rot_cos;
        
        mouse_report->x = (int8_t)rx;
        mouse_report->y = (int8_t)ry;

        // Combined scroll/volume mode when _FL layer is active.
        // Vertical movement scrolls and horizontal movement adjusts volume.
        // Axis choice locks until 400 ms after returning to deadzone.
        if (IS_LAYER_ON(_FL)) {
            // Smooth trackpoint noise for less "ratchety" behavior.
            pt_scroll_x_filtered = (int16_t)((pt_scroll_x_filtered * PT_SCROLL_FILTER_ALPHA + mouse_report->x * (256 - PT_SCROLL_FILTER_ALPHA)) / 256);
            pt_scroll_y_filtered = (int16_t)((pt_scroll_y_filtered * PT_SCROLL_FILTER_ALPHA + mouse_report->y * (256 - PT_SCROLL_FILTER_ALPHA)) / 256);
            pt_volume_x_filtered = (int16_t)((pt_volume_x_filtered * PT_VOLUME_FILTER_ALPHA + mouse_report->x * (256 - PT_VOLUME_FILTER_ALPHA)) / 256);

            int16_t abs_x = pt_volume_x_filtered < 0 ? -pt_volume_x_filtered : pt_volume_x_filtered;
            int16_t abs_y = pt_scroll_y_filtered < 0 ? -pt_scroll_y_filtered : pt_scroll_y_filtered;
            bool in_deadzone = abs_x < PT_CENTER_DEADZONE && abs_y < PT_CENTER_DEADZONE;

            if (pt_mode_lock == PT_MODE_UNLOCKED && !in_deadzone) {
                pt_mode_lock = (abs_y >= abs_x) ? PT_MODE_SCROLL : PT_MODE_VOLUME;
            }

            if (in_deadzone) {
                if (pt_mode_lock != PT_MODE_UNLOCKED) {
                    if (pt_mode_deadzone_timer == 0) {
                        pt_mode_deadzone_timer = timer_read32();
                    } else if (timer_elapsed32(pt_mode_deadzone_timer) >= PT_AXIS_LOCK_RELEASE_MS) {
                        pt_mode_lock = PT_MODE_UNLOCKED;
                        pt_mode_deadzone_timer = 0;
                    }
                }
            } else {
                pt_mode_deadzone_timer = 0;
            }

            mouse_report->x = 0;
            mouse_report->y = 0;
            mouse_report->h = 0;
            mouse_report->v = 0;

            if (pt_mode_lock == PT_MODE_SCROLL) {
                pt_scroll_y_acc += pt_scroll_y_filtered;

                if (pt_scroll_y_acc >= PT_SCROLL_THRESHOLD) {
                    mouse_report->v = -1;
                    pt_scroll_y_acc -= PT_SCROLL_THRESHOLD;
                } else if (pt_scroll_y_acc <= -PT_SCROLL_THRESHOLD) {
                    mouse_report->v = 1;
                    pt_scroll_y_acc += PT_SCROLL_THRESHOLD;
                }
            } else if (pt_mode_lock == PT_MODE_VOLUME) {
                pt_volume_x_acc += pt_volume_x_filtered;

                // Right on stick = volume up, left on stick = volume down.
                if (pt_volume_x_acc >= PT_VOLUME_THRESHOLD) {
                    tap_code(KC_VOLU);
                    pt_volume_x_acc -= PT_VOLUME_THRESHOLD;
                } else if (pt_volume_x_acc <= -PT_VOLUME_THRESHOLD) {
                    tap_code(KC_VOLD);
                    pt_volume_x_acc += PT_VOLUME_THRESHOLD;
                }
            }

            if (pt_mode_lock != PT_MODE_UNLOCKED && !in_deadzone) {
                auto_mouse_refresh_timer();
            }
            return;
        }

        // Reset accumulators when not in scroll mode
        pt_scroll_x_acc = 0;
        pt_scroll_y_acc = 0;
        pt_scroll_x_filtered = 0;
        pt_scroll_y_filtered = 0;
        pt_volume_x_acc = 0;
        pt_volume_x_filtered = 0;
        pt_mode_lock = PT_MODE_UNLOCKED;
        pt_mode_deadzone_timer = 0;

        // Double sensitivity when shift is held
        if (get_mods() & MOD_MASK_SHIFT) {
            mouse_report->x = (int8_t)((int16_t)mouse_report->x * 2);
            mouse_report->y = (int8_t)((int16_t)mouse_report->y * 2);
        }

        // Apply global sensitivity scaling (128 = 1x)
        mouse_report->x = (int8_t)((int16_t)mouse_report->x * pt_sensitivity / PT_SENSITIVITY_DEFAULT);
        mouse_report->y = (int8_t)((int16_t)mouse_report->y * pt_sensitivity / PT_SENSITIVITY_DEFAULT);

        // Enter/refresh auto-mouse mode only when pointer movement is produced.
        if (mouse_report->x != 0 || mouse_report->y != 0) {
            auto_mouse_refresh_timer();
        }
    }

    void matrix_scan_user(void) {
        // Keep auto-mouse alive while button emulation keys are held.
        if (auto_mouse_lbtn_holds > 0 || auto_mouse_rbtn_holds > 0) {
            auto_mouse_refresh_timer();
        }
    }
#endif


void keyboard_post_init_user(void) {
    float rad = PT_ROTATION_DEGREES * 0.017453292519943295f; // degrees to radians
    pt_rot_sin = sinf(rad);
    pt_rot_cos = cosf(rad);

    rgblight_enable_noeeprom();
    rgblight_mode_noeeprom(RGBLIGHT_MODE_RAINBOW_MOOD);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;

    if (left_gui_pending && !left_gui_chorded && keycode != CK_WINL && record->event.pressed) {
        register_code(KC_LGUI);
        left_gui_chorded = true;
    }

    switch (keycode) {
        case CK_WINL:
            if (record->event.pressed) {
                left_gui_pending = true;
                left_gui_chorded = false;
            } else {
                if (left_gui_chorded) {
                    unregister_code(KC_LGUI);
                } else if (host_is_windows()) {
                    tap_code16(ALGR(KC_SPC));
                } else {
                    tap_code(KC_LGUI);
                }

                left_gui_pending = false;
                left_gui_chorded = false;
            }
            return false;
        case MO(_FL):
            if (auto_mouse_is_active()) {
                if (record->event.pressed) {
                    auto_mouse_press(MS_BTN2, &auto_mouse_rbtn_holds);
                } else {
                    auto_mouse_release(MS_BTN2, &auto_mouse_rbtn_holds);
                }
                return false;
            }
            return true;
        case KC_Y:
            if (auto_mouse_is_active()) {
                if (record->event.pressed) {
                    auto_mouse_press(MS_BTN1, &auto_mouse_lbtn_holds);
                } else {
                    auto_mouse_release(MS_BTN1, &auto_mouse_lbtn_holds);
                }
                return false;
            }
            return true;
        case KC_U:
            if (auto_mouse_is_active()) {
                if (record->event.pressed) {
                    auto_mouse_press(MS_BTN2, &auto_mouse_rbtn_holds);
                } else {
                    auto_mouse_release(MS_BTN2, &auto_mouse_rbtn_holds);
                }
                return false;
            }
            return true;
        case CK_PTI:
            if (record->event.pressed) {
                if (pt_sensitivity <= PT_SENSITIVITY_MAX - PT_SENSITIVITY_STEP) {
                    pt_sensitivity += PT_SENSITIVITY_STEP;
                } else {
                    pt_sensitivity = PT_SENSITIVITY_MAX;
                }
            }
            return false;
        case CK_PTD:
            if (record->event.pressed) {
                if (pt_sensitivity >= PT_SENSITIVITY_MIN + PT_SENSITIVITY_STEP) {
                    pt_sensitivity -= PT_SENSITIVITY_STEP;
                } else {
                    pt_sensitivity = PT_SENSITIVITY_MIN;
                }
            }
            return false;
        // case KC_CAPS:
        //     if (record->event.pressed) {
        //         #ifdef AUDIO_ENABLE
        //         if (host_keyboard_led_state().caps_lock) {
        //             PLAY_SONG(caps_off_song);
        //         } else {
        //             PLAY_SONG(caps_on_song);
        //         }
        //         #endif
        //     }
        //     return true;
    }

    return true;
}