#pragma once

#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET
#define RP2040_BOOTLOADER_DOUBLE_TAP_TIMEOUT 500U // milliseconds

#define USB_DEVICE_NAME "Trig"
#define MANUFACTURER "Jens Nomtak"
#define PRODUCT "Trig"
// HT16K33 I2C defaults for RP2040; adjust pins if your wiring is different.
#ifndef I2C_DRIVER
#    define I2C_DRIVER I2CD0
#endif
#ifndef I2C1_SDA_PIN
#    define I2C1_SDA_PIN GP28
#endif
#ifndef I2C1_SCL_PIN
#    define I2C1_SCL_PIN GP29
#endif

// QMK I2C API expects the 7-bit address shifted left by 1.
#ifndef HT16K33_I2C_ADDRESS
#    define HT16K33_I2C_ADDRESS (0x70 << 1)
#endif
#ifndef HT16K33_SEGMENT_I2C_ADDRESS
#    define HT16K33_SEGMENT_I2C_ADDRESS (0x71 << 1)
#endif

// Rotary encoder: A=GP0, B=GP1.
#define ENCODER_A_PINS { GP0 }
#define ENCODER_B_PINS { GP1 }
// Use 1 step to avoid losing the first detent on direction changes.
#define ENCODER_RESOLUTION 1

// Piezo speaker on GP20 (PWM slice 2 on RP2040).
#define AUDIO_PIN GP20
#define AUDIO_PWM_DRIVER PWMD2
#define AUDIO_PWM_CHANNEL RP2040_PWM_CHANNEL_A
#define AUDIO_INIT_DELAY

// Battery sense on GP27 through 100k/100k divider (single-cell Li-ion).
#define BATTERY_ADC_PIN GP27
#define BATTERY_ADC_REF_VOLTAGE_MV 3300
#define BATTERY_ADC_RESOLUTION 10
#define BATTERY_VOLTAGE_DIVIDER_R1 100
#define BATTERY_ADC_VOLTAGE_DIVIDER_R2 100

#define WS2812_DI_PIN GP21
#define RGBLIGHT_EFFECT_RAINBOW_MOOD

#define RGBLIGHT_LED_COUNT 1
#define RGBLIGHT_DEFAULT_MODE RGBLIGHT_MODE_RAINBOW_MOOD
#define RGBLIGHT_DEFAULT_HUE 170
#define RGBLIGHT_DEFAULT_SAT 255
#define RGBLIGHT_DEFAULT_VAL 120
#define RGBLIGHT_LIMIT_VAL 150
