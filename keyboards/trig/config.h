#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET
#define RP2040_BOOTLOADER_DOUBLE_TAP_TIMEOUT 500U // milliseconds

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

#define WS2812_DI_PIN GP21
#define RGBLIGHT_EFFECT_RAINBOW_MOOD

#define RGBLIGHT_LED_COUNT 1
#define RGBLIGHT_DEFAULT_MODE RGBLIGHT_MODE_RAINBOW_MOOD
#define RGBLIGHT_DEFAULT_HUE 170
#define RGBLIGHT_DEFAULT_SAT 255
#define RGBLIGHT_DEFAULT_VAL 120
#define RGBLIGHT_LIMIT_VAL 150
