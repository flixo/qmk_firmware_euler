// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "gpio.h"
#include <math.h>

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
#define PT_SENSITIVITY_MIN     10
#define PT_SENSITIVITY_MAX     255
#define PT_SENSITIVITY_STEP    20
#define AUTO_MOUSE_TIMEOUT_MS  700

static uint8_t pt_sensitivity = PT_SENSITIVITY_DEFAULT;

// Rotate trackpoint input by this many degrees (positive = counter-clockwise)
#define PT_ROTATION_DEGREES 10.0f

// Lower threshold = more sensitive scrolling
#define PT_SCROLL_THRESHOLD 28

// Low-pass filter for scroll source: higher keeps more previous value (0-255)
#define PT_SCROLL_FILTER_ALPHA 192

// Volume mode tuning when _FL is held (Y axis only)
#define PT_VOLUME_THRESHOLD 56
#define PT_VOLUME_FILTER_ALPHA 224

static float pt_rot_sin = 0.0f;
static float pt_rot_cos = 1.0f;

static int16_t pt_scroll_x_acc = 0;
static int16_t pt_scroll_y_acc = 0;
static int16_t pt_scroll_x_filtered = 0;
static int16_t pt_scroll_y_filtered = 0;
static int16_t pt_volume_y_acc = 0;
static int16_t pt_volume_y_filtered = 0;

static uint32_t auto_mouse_timer = 0;
static uint8_t auto_mouse_lbtn_holds = 0;
static uint8_t auto_mouse_rbtn_holds = 0;
static bool left_gui_pending = false;
static bool left_gui_chorded = false;

static inline void auto_mouse_refresh_timer(void) {
    auto_mouse_timer = timer_read32();
}

static inline bool auto_mouse_is_active(void) {
    return timer_elapsed32(auto_mouse_timer) < AUTO_MOUSE_TIMEOUT_MS;
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
    /**|  ┌───────────────┐┌────────┐┌────────┐ ┌────────┐┌────────┐┌────────┐┌────────┐    ┌────────┐┌─────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐ ┌────────┐┌────────┐┌────────┐┌─────────────┐   |**/
    /**/       KC_ESC,       KC_GRV,    KC_1,      KC_2,     KC_3,     KC_4,     KC_5,        KC_PSCR,  KC_SCRL,  KC_PAUS,      KC_6,     KC_7,     KC_8,    KC_9,       KC_0,    KC_MINS,   KC_EQL,     KC_BSPC,      /**/
    /**|  └───────────────┘└────────┘└────────┘ └────────┘└────────┘└────────┘└────────┘    └────────┘└─────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘ └────────┘└────────┘└────────┘└─────────────┘   |**/
    /**|     ┌─────────┐┌─────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐    ┌────────┐┌─────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐      |**/
    /**/       KC_BSPC,   KC_TAB,    KC_Q,         KC_W,     KC_E,     KC_R,     KC_T,        KC_INS,   KC_HOME,  KC_PGUP,      KC_Y,     KC_U,     KC_I,    KC_O,         KC_P,    KC_LBRC,  KC_RBRC,  KC_BSLS,       /**/
    /**|     └─────────┘└─────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘    └────────┘└─────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘      |**/
    /**|     ┌────────┐┌────────┐┌────────┐     ┌────────┐┌────────┐┌────────┐┌────────┐    ┌────────┐┌─────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐     ┌────────┐┌────────┐┌────────────────┐      |**/
    /**/      MO(_UL),   KC_CAPS,    KC_A,          KC_S,    KC_D,    KC_F,     KC_G,         KC_DEL,   KC_END,   KC_PGDN,      KC_H,     KC_J,     KC_K,    KC_L,          KC_SCLN,  KC_QUOT,      KC_ENT,            /**/
    /**|     └────────┘└────────┘└────────┘     └────────┘└────────┘└────────┘└────────┘    └────────┘└─────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘     └────────┘└────────┘└────────────────┘      |**/
    /**|           ┌──────────┐┌────────┐       ┌────────┐┌────────┐┌────────┐┌────────┐              ┌─────────┐             ┌────────┐┌────────┐┌────────┐┌────────┐       ┌────────┐┌───────────────────┐           |**/
    /**/  MS_BTN2,   KC_LSFT,     KC_Z,           KC_X,      KC_C,    KC_V,      KC_B,       MASKED,     KC_UP,    MASKED,      KC_N,     KC_M,   KC_COMM,   KC_DOT,          KC_SLSH,        KC_RSFT,                 /**/
    /**|           └──────────┘└────────┘       └────────┘└────────┘└────────┘└────────┘              └─────────┘             └────────┘└────────┘└────────┘└────────┘       └────────┘└───────────────────┘           |**/
    /**|                ┌──────────┐┌────────┐┌────────┐┌──────────────────────┐┌────────┐  ┌────────┐┌─────────┐┌────────┐  ┌────────┐┌──────────────────────┐┌────────┐┌────────┐┌────────┐┌─────────┐               |**/
    /**/  MS_BTN1,        KC_LCTL,   CK_WINL,  KC_LALT,          KC_SPC,         MO(_UL),     KC_LEFT,  KC_DOWN,  KC_RGHT,     KC_BSPC,       MO(_FL),          KC_RALT,  KC_RGUI,   KC_APP,   KC_RCTL                 /**/
    /**|                └──────────┘└────────┘└────────┘└──────────────────────┘│        │  └────────┘└─────────┘└────────┘  │        │└──────────────────────┘└────────┘└────────┘└────────┘└─────────┘               |**/
    /**|                                                                        │        │                                   │        │                                                                                |**/
    /**|                                                                        └────────┘                                   └────────┘                                                                                |**/
    ),

    [_FL] = LAYOUT_fullsize_ansi( /* Function Layer */
    /**|  ┌───────────────┐┌────────┐┌────────┐ ┌────────┐┌────────┐┌────────┐┌────────┐    ┌────────┐┌─────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐ ┌────────┐┌────────┐┌────────┐┌─────────────┐   |**/
    /**/        ______,      ______,   ______,    ______,   ______,   ______,   ______,       ______,   ______,   ______,       ______,   ______,   ______,   ______,    ______,   ______,   ______,     ______,       /**/
    /**|  └───────────────┘└────────┘└────────┘ └────────┘└────────┘└────────┘└────────┘    └────────┘└─────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘ └────────┘└────────┘└────────┘└─────────────┘   |**/
    /**|     ┌─────────┐┌─────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐    ┌────────┐┌─────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐      |**/
    /**/        ______,   ______,    ______,      ______,   KC_PGUP,   ______,   ______,       ______,   ______,   ______,       ______,  KC_HOME,   KC_UP,   KC_END,      ______,   KC_AA,   ______,   ______,       /**/
    /**|     └─────────┘└─────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘    └────────┘└─────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘      |**/
    /**|     ┌────────┐┌────────┐┌────────┐     ┌────────┐┌────────┐┌────────┐┌────────┐    ┌────────┐┌─────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐     ┌────────┐┌────────┐┌────────────────┐      |**/
    /**/       ______,   ______,   ______,        ______,   KC_PGDN,   ______,   ______,       ______,   ______,   ______,       ______,  KC_LEFT,  KC_DOWN,  KC_RGHT,       KC_OE,    KC_AE,        ______,           /**/
    /**|     └────────┘└────────┘└────────┘     └────────┘└────────┘└────────┘└────────┘    └────────┘└─────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘     └────────┘└────────┘└────────────────┘      |**/
    /**|           ┌──────────┐┌────────┐       ┌────────┐┌────────┐┌────────┐┌────────┐              ┌─────────┐             ┌────────┐┌────────┐┌────────┐┌────────┐       ┌────────┐┌───────────────────┐           |**/
    /**/   ______,    ______,    ______,          ______,   ______,   ______,   ______,       MASKED,   ______,    MASKED,      ______,   ______,   ______,   ______,          ______,        ______,                  /**/
    /**|           └──────────┘└────────┘       └────────┘└────────┘└────────┘└────────┘              └─────────┘             └────────┘└────────┘└────────┘└────────┘       └────────┘└───────────────────┘           |**/
    /**|                ┌──────────┐┌────────┐┌────────┐┌──────────────────────┐┌────────┐  ┌────────┐┌─────────┐┌────────┐  ┌────────┐┌──────────────────────┐┌────────┐┌────────┐┌────────┐┌─────────┐               |**/
    /**/   ______,         ______,    ______,  ______,          ______,           ______,     ______,   ______,   ______,      ______,          ______,          ______,   ______,   ______,   ______                  /**/
    /**|                └──────────┘└────────┘└────────┘└──────────────────────┘│        │  └────────┘└─────────┘└────────┘  │        │└──────────────────────┘└────────┘└────────┘└────────┘└─────────┘               |**/
    /**|                                                                        │        │                                   │        │                                                                                |**/
    /**|                                                                        └────────┘                                   └────────┘                                                                                |**/
    ),

    [_UL] = LAYOUT_fullsize_ansi( /* Utility Layer */
    /**|  ┌───────────────┐┌────────┐┌────────┐ ┌────────┐┌────────┐┌────────┐┌────────┐    ┌────────┐┌─────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐ ┌────────┐┌────────┐┌────────┐┌─────────────┐   |**/
    /**/       QK_BOOT,      ______,   ______,    ______,   ______,   ______,   ______,       ______,   ______,   KC_MUTE,      KC_PEQL,  KC_PSLS,  KC_PAST,  KC_PMNS,   ______,   CK_PTD,   CK_PTI,     ______,       /**/
    /**|  └───────────────┘└────────┘└────────┘ └────────┘└────────┘└────────┘└────────┘    └────────┘└─────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘ └────────┘└────────┘└────────┘└─────────────┘   |**/
    /**|     ┌─────────┐┌─────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐    ┌────────┐┌─────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐      |**/
    /**/        ______,   ______,    ______,      ______,   ______,   ______,   ______,       KC_MPLY,  KC_MSTP,  KC_VOLU,      KC_KP_7,  KC_KP_8, KC_KP_9,   KC_PPLS,     ______,   ______,   ______,   ______,       /**/
    /**|     └─────────┘└─────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘    └────────┘└─────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘      |**/
    /**|     ┌────────┐┌────────┐┌────────┐     ┌────────┐┌────────┐┌────────┐┌────────┐    ┌────────┐┌─────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐     ┌────────┐┌────────┐┌────────────────┐      |**/
    /**/       ______,   ______,   ______,        ______,   ______,  MS_BTN1,  MS_BTN2,       KC_MPRV,  KC_MNXT,  KC_VOLD,      KC_KP_4,  KC_KP_5, KC_KP_6,   KC_PPLS,       ______,  ______,        ______,           /**/
    /**|     └────────┘└────────┘└────────┘     └────────┘└────────┘└────────┘└────────┘    └────────┘└─────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘     └────────┘└────────┘└────────────────┘      |**/
    /**|           ┌──────────┐┌────────┐       ┌────────┐┌────────┐┌────────┐┌────────┐              ┌─────────┐             ┌────────┐┌────────┐┌────────┐┌────────┐       ┌────────┐┌───────────────────┐           |**/
    /**/  ______,     ______,    ______,          ______,   ______,   ______,   ______,       MASKED,   ______,    MASKED,      KC_KP_1,  KC_KP_2, KC_KP_3,   ______,          ______,        ______,                  /**/
    /**|           └──────────┘└────────┘       └────────┘└────────┘└────────┘└────────┘              └─────────┘             └────────┘└────────┘└────────┘└────────┘       └────────┘└───────────────────┘           |**/
    /**|                ┌──────────┐┌────────┐┌────────┐┌──────────────────────┐┌────────┐  ┌────────┐┌─────────┐┌────────┐  ┌────────┐┌──────────────────────┐┌────────┐┌────────┐┌────────┐┌─────────┐               |**/
    /**/  ______,          ______,    ______,  ______,          MS_BTN1,           ______,     ______,   ______,   ______,      ______,          MS_BTN2,         KC_PDOT,   ______,   ______,   ______                  /**/
    /**|                └──────────┘└────────┘└────────┘└──────────────────────┘│        │  └────────┘└─────────┘└────────┘  │        │└──────────────────────┘└────────┘└────────┘└────────┘└─────────┘               |**/
    /**|                                                                        │        │                                   │        │                                                                                |**/
    /**|                                                                        └────────┘                                   └────────┘                                                                                |**/
    ),
    
    [_TEMPLATE] = LAYOUT_fullsize_ansi( /* Template Layer */
    /**|  ┌───────────────┐┌────────┐┌────────┐ ┌────────┐┌────────┐┌────────┐┌────────┐    ┌────────┐┌─────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐ ┌────────┐┌────────┐┌────────┐┌─────────────┐   |**/
    /**/       ______,       ______,   ______,    ______,   ______,   ______,   ______,       ______,   ______,   ______,       ______,   ______,   ______,   ______,    ______,   ______,   ______,     ______,       /**/
    /**|  └───────────────┘└────────┘└────────┘ └────────┘└────────┘└────────┘└────────┘    └────────┘└─────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘ └────────┘└────────┘└────────┘└─────────────┘   |**/
    /**|     ┌─────────┐┌─────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐    ┌────────┐┌─────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐      |**/
    /**/        ______,   ______,    ______,      ______,   ______,   ______,   ______,       ______,   ______,   ______,       ______,   ______,   ______,   ______,      ______,   ______,   ______,   ______,       /**/
    /**|     └─────────┘└─────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘    └────────┘└─────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘      |**/
    /**|     ┌────────┐┌────────┐┌────────┐     ┌────────┐┌────────┐┌────────┐┌────────┐    ┌────────┐┌─────────┐┌────────┐   ┌────────┐┌────────┐┌────────┐┌────────┐     ┌────────┐┌────────┐┌────────────────┐      |**/
    /**/       ______,   ______,   ______,        ______,   ______,   ______,   ______,       ______,   ______,   ______,       ______,   ______,   ______,   ______,        ______,  ______,        ______,           /**/
    /**|     └────────┘└────────┘└────────┘     └────────┘└────────┘└────────┘└────────┘    └────────┘└─────────┘└────────┘   └────────┘└────────┘└────────┘└────────┘     └────────┘└────────┘└────────────────┘      |**/
    /**|           ┌──────────┐┌────────┐       ┌────────┐┌────────┐┌────────┐┌────────┐              ┌─────────┐             ┌────────┐┌────────┐┌────────┐┌────────┐       ┌────────┐┌───────────────────┐           |**/
    /**/  ______,     ______,    ______,          ______,   ______,   ______,   ______,       MASKED,   ______,    MASKED,      ______,   ______,   ______,   ______,          ______,        ______,                  /**/
    /**|           └──────────┘└────────┘       └────────┘└────────┘└────────┘└────────┘              └─────────┘             └────────┘└────────┘└────────┘└────────┘       └────────┘└───────────────────┘           |**/
    /**|                ┌──────────┐┌────────┐┌────────┐┌──────────────────────┐┌────────┐  ┌────────┐┌─────────┐┌────────┐  ┌────────┐┌──────────────────────┐┌────────┐┌────────┐┌────────┐┌─────────┐               |**/
    /**/  ______,          ______,    ______,  ______,          ______,           ______,     ______,   ______,   ______,      ______,          ______,          ______,   ______,   ______,   ______                  /**/
    /**|                └──────────┘└────────┘└────────┘└──────────────────────┘│        │  └────────┘└─────────┘└────────┘  │        │└──────────────────────┘└────────┘└────────┘└────────┘└─────────┘               |**/
    /**|                                                                        │        │                                   │        │                                                                                |**/
    /**|                                                                        └────────┘                                   └────────┘                                                                                |**/
    )
};

#ifdef AUDIO_ENABLE
    static bool boot_song_cancellable = true;

    #define PT_INCREASE_SOUND E__NOTE(_C5), E__NOTE(_E5), E__NOTE(_G5)
    #define PT_DECREASE_SOUND E__NOTE(_G5), E__NOTE(_E5), E__NOTE(_C5)
    #define CAPS_ON_SOUND     E__NOTE(_A4), E__NOTE(_A5)
    #define CAPS_OFF_SOUND    E__NOTE(_A5), E__NOTE(_A4)

    float pt_increase_song[][2] = SONG(PT_INCREASE_SOUND);
    float pt_decrease_song[][2] = SONG(PT_DECREASE_SOUND);
    float caps_on_song[][2]     = SONG(CAPS_ON_SOUND);
    float caps_off_song[][2]    = SONG(CAPS_OFF_SOUND);
#endif





#if defined PS2_MOUSE_ENABLE && defined MOUSEKEY_ENABLE
    void ps2_mouse_moved_user(report_mouse_t *mouse_report) {
        // Apply rotation
        float rx = mouse_report->x * pt_rot_cos - mouse_report->y * pt_rot_sin;
        float ry = mouse_report->x * pt_rot_sin + mouse_report->y * pt_rot_cos;
        
        mouse_report->x = (int8_t)rx;
        mouse_report->y = (int8_t)ry;

        // Scroll mode when _UL layer is active
        if (IS_LAYER_ON(_UL)) {
            // Smooth trackpoint noise for less "ratchety" scroll behavior.
            pt_scroll_x_filtered = (int16_t)((pt_scroll_x_filtered * PT_SCROLL_FILTER_ALPHA + mouse_report->x * (256 - PT_SCROLL_FILTER_ALPHA)) / 256);
            pt_scroll_y_filtered = (int16_t)((pt_scroll_y_filtered * PT_SCROLL_FILTER_ALPHA + mouse_report->y * (256 - PT_SCROLL_FILTER_ALPHA)) / 256);

            pt_scroll_x_acc += pt_scroll_x_filtered;
            pt_scroll_y_acc += pt_scroll_y_filtered;

            mouse_report->x = 0;
            mouse_report->y = 0;
            mouse_report->h = 0;
            mouse_report->v = 0;

            // Emit at most one tick per axis per report to keep scroll smooth.
            if (pt_scroll_x_acc >= PT_SCROLL_THRESHOLD) {
                mouse_report->h = 1;
                pt_scroll_x_acc -= PT_SCROLL_THRESHOLD;
            } else if (pt_scroll_x_acc <= -PT_SCROLL_THRESHOLD) {
                mouse_report->h = -1;
                pt_scroll_x_acc += PT_SCROLL_THRESHOLD;
            }

            if (pt_scroll_y_acc >= PT_SCROLL_THRESHOLD) {
                mouse_report->v = -1;
                pt_scroll_y_acc -= PT_SCROLL_THRESHOLD;
            } else if (pt_scroll_y_acc <= -PT_SCROLL_THRESHOLD) {
                mouse_report->v = 1;
                pt_scroll_y_acc += PT_SCROLL_THRESHOLD;
            }

            // Keep auto-mouse mode alive while scrolling with _UL held.
            if (mouse_report->h != 0 || mouse_report->v != 0) {
                auto_mouse_refresh_timer();
            }
            return;
        }

        // Volume mode when _FL layer is active (Y axis only)
        if (IS_LAYER_ON(_FL)) {
            pt_volume_y_filtered = (int16_t)((pt_volume_y_filtered * PT_VOLUME_FILTER_ALPHA + mouse_report->y * (256 - PT_VOLUME_FILTER_ALPHA)) / 256);
            pt_volume_y_acc += pt_volume_y_filtered;

            mouse_report->x = 0;
            mouse_report->y = 0;
            mouse_report->h = 0;
            mouse_report->v = 0;

            // Up on stick = volume up, down on stick = volume down.
            if (pt_volume_y_acc >= PT_VOLUME_THRESHOLD) {
                tap_code(KC_VOLD);
                pt_volume_y_acc -= PT_VOLUME_THRESHOLD;
            } else if (pt_volume_y_acc <= -PT_VOLUME_THRESHOLD) {
                tap_code(KC_VOLU);
                pt_volume_y_acc += PT_VOLUME_THRESHOLD;
            }
            return;
        }

        // Reset accumulators when not in scroll mode
        pt_scroll_x_acc = 0;
        pt_scroll_y_acc = 0;
        pt_scroll_x_filtered = 0;
        pt_scroll_y_filtered = 0;
        pt_volume_y_acc = 0;
        pt_volume_y_filtered = 0;
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

    #ifdef AUDIO_ENABLE
        // Cancel the startup tune as soon as any key is physically pressed.
        if (boot_song_cancellable && record->event.pressed) {
            stop_all_notes();
            boot_song_cancellable = false;
        }
    #endif

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
        case KC_SPC:
            if (auto_mouse_is_active()) {
                if (record->event.pressed) {
                    auto_mouse_press(MS_BTN1, &auto_mouse_lbtn_holds);
                } else {
                    auto_mouse_release(MS_BTN1, &auto_mouse_lbtn_holds);
                }
                return false;
            }
            return true;
        case KC_BSPC:
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
                #ifdef AUDIO_ENABLE
                PLAY_SONG(pt_increase_song);
                #endif
            }
            return false;
        case CK_PTD:
            if (record->event.pressed) {
                if (pt_sensitivity >= PT_SENSITIVITY_MIN + PT_SENSITIVITY_STEP) {
                    pt_sensitivity -= PT_SENSITIVITY_STEP;
                } else {
                    pt_sensitivity = PT_SENSITIVITY_MIN;
                }
                #ifdef AUDIO_ENABLE
                PLAY_SONG(pt_decrease_song);
                #endif
            }
            return false;
        case KC_CAPS:
            if (record->event.pressed) {
                #ifdef AUDIO_ENABLE
                if (host_keyboard_led_state().caps_lock) {
                    PLAY_SONG(caps_off_song);
                } else {
                    PLAY_SONG(caps_on_song);
                }
                #endif
            }
            return true;
    }

    return true;
}