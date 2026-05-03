// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdint.h>

#define MATRIX_SYMBOL_INVALID   0xFF
#define MATRIX_SYMBOL_BASE      0
#define MATRIX_SYMBOL_ADDITION  1
#define MATRIX_SYMBOL_MULTIPLY  2
#define MATRIX_SYMBOL_DIVIDE    3
#define MATRIX_SYMBOL_SUBTRACT  4
#define MATRIX_SYMBOL_POWER     5
#define MATRIX_SYMBOL_SQRT      6
#define MATRIX_SYMBOL_PI        7
#define MATRIX_SYMBOL_SIN       8
#define MATRIX_SYMBOL_COS       9
#define MATRIX_SYMBOL_SEC       10
#define MATRIX_SYMBOL_CSC       11
#define MATRIX_SYMBOL_TAN       12
#define MATRIX_SYMBOL_COT       13
#define MATRIX_SYMBOL_FACT      14
#define MATRIX_SYMBOL_HYPOT     15

#define MATRIX_SCI_POWER        0
#define MATRIX_SCI_SQRT         1
#define MATRIX_SCI_PI           2
#define MATRIX_SCI_SIN          3
#define MATRIX_SCI_COS          4
#define MATRIX_SCI_SEC          5
#define MATRIX_SCI_CSC          6
#define MATRIX_SCI_TAN          7
#define MATRIX_SCI_COT          8
#define MATRIX_SCI_FACT         9
#define MATRIX_SCI_HYPOT        10
#define MATRIX_SCI_COUNT        11

const char *matrix_symbol_get(uint8_t index);
const char *matrix_symbol_blank(void);
uint8_t matrix_symbol_for_op(char op);
uint8_t matrix_symbol_for_scientific(uint8_t scientific_index);