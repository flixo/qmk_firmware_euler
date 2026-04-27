#pragma once

#include <stdbool.h>
#include <stdint.h>

// Convenience tokens for visual matrix definitions.
#define M_X 1
#define M_  0

#define HT16K33_BIT(v, bit) ((v) ? (1u << (bit)) : 0u)
#define HT16K33_ROW8(c0, c1, c2, c3, c4, c5, c6, c7) \
	((uint8_t)(HT16K33_BIT((c0), 0) | HT16K33_BIT((c1), 1) | HT16K33_BIT((c2), 2) | HT16K33_BIT((c3), 3) | HT16K33_BIT((c4), 4) | HT16K33_BIT((c5), 5) | HT16K33_BIT((c6), 6) | HT16K33_BIT((c7), 7)))

// Visual 8x8 matrix setter; pass 64 values row-by-row.
#define set_matrix( \
	r0c0, r0c1, r0c2, r0c3, r0c4, r0c5, r0c6, r0c7, \
	r1c0, r1c1, r1c2, r1c3, r1c4, r1c5, r1c6, r1c7, \
	r2c0, r2c1, r2c2, r2c3, r2c4, r2c5, r2c6, r2c7, \
	r3c0, r3c1, r3c2, r3c3, r3c4, r3c5, r3c6, r3c7, \
	r4c0, r4c1, r4c2, r4c3, r4c4, r4c5, r4c6, r4c7, \
	r5c0, r5c1, r5c2, r5c3, r5c4, r5c5, r5c6, r5c7, \
	r6c0, r6c1, r6c2, r6c3, r6c4, r6c5, r6c6, r6c7, \
	r7c0, r7c1, r7c2, r7c3, r7c4, r7c5, r7c6, r7c7 \
) \
	ht16k33_matrix_set_rows((const uint8_t[8]){ \
		HT16K33_ROW8(r0c0, r0c1, r0c2, r0c3, r0c4, r0c5, r0c6, r0c7), \
		HT16K33_ROW8(r1c0, r1c1, r1c2, r1c3, r1c4, r1c5, r1c6, r1c7), \
		HT16K33_ROW8(r2c0, r2c1, r2c2, r2c3, r2c4, r2c5, r2c6, r2c7), \
		HT16K33_ROW8(r3c0, r3c1, r3c2, r3c3, r3c4, r3c5, r3c6, r3c7), \
		HT16K33_ROW8(r4c0, r4c1, r4c2, r4c3, r4c4, r4c5, r4c6, r4c7), \
		HT16K33_ROW8(r5c0, r5c1, r5c2, r5c3, r5c4, r5c5, r5c6, r5c7), \
		HT16K33_ROW8(r6c0, r6c1, r6c2, r6c3, r6c4, r6c5, r6c6, r6c7), \
		HT16K33_ROW8(r7c0, r7c1, r7c2, r7c3, r7c4, r7c5, r7c6, r7c7), \
	})

bool ht16k33_matrix_init(void);
bool ht16k33_matrix_set_brightness(uint8_t level);
bool ht16k33_matrix_set_rows(const uint8_t rows[8]);
bool ht16k33_matrix_set_visual(const char *visual);
bool ht16k33_matrix_show_all_on(void);

// Parse a visual UTF-8 matrix string (⚪=ON, ⚫=OFF) and write it to the display.
#define set_matrix_visual(visual_literal) ht16k33_matrix_set_visual((visual_literal))

typedef enum {
	HT16K33_ROTATION_0   = 0,
	HT16K33_ROTATION_90  = 1,
	HT16K33_ROTATION_180 = 2,
	HT16K33_ROTATION_270 = 3,
} ht16k33_rotation_t;

bool ht16k33_matrix_set_rotation(ht16k33_rotation_t rotation);
