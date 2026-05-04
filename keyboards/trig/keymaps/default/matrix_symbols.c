// Copyright 2026 Jens Nomtak
// SPDX-License-Identifier: GPL-2.0-or-later

#include "matrix_symbols.h"

static const char *const matrix_blank_symbol =
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫";

static const char *const matrix_base_symbol =
    "⚪⚪⚪⚪⚪⚪⚪⚪"
    "⚪⚪⚪⚪⚪⚪⚪⚪"
    "⚪⚪⚪⚪⚪⚪⚪⚪"
    "⚪⚪⚪⚪⚪⚪⚪⚪"
    "⚪⚪⚪⚪⚪⚪⚪⚪"
    "⚪⚪⚪⚪⚪⚪⚪⚪"
    "⚪⚪⚪⚪⚪⚪⚪⚪"
    "⚪⚪⚪⚪⚪⚪⚪⚪";

static const char *const matrix_addition_symbol =
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚪⚫⚫⚫⚫"
    "⚫⚫⚫⚪⚫⚫⚫⚫"
    "⚫⚪⚪⚪⚪⚪⚫⚫"
    "⚫⚫⚫⚪⚫⚫⚫⚫"
    "⚫⚫⚫⚪⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫";

static const char *const matrix_multiply_symbol =
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚪⚫⚫⚫⚪⚫⚫"
    "⚫⚫⚪⚫⚪⚫⚫⚫"
    "⚫⚫⚫⚪⚫⚫⚫⚫"
    "⚫⚫⚪⚫⚪⚫⚫⚫"
    "⚫⚪⚫⚫⚫⚪⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫";

static const char *const matrix_divide_symbol =
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚪⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚪⚪⚪⚪⚪⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚪⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫";

static const char *const matrix_subtract_symbol =
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚪⚪⚪⚪⚪⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫";


    // Power of
static const char *const matrix_power_symbol =
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚪⚫⚫⚫"
    "⚫⚫⚫⚪⚫⚪⚫⚫"
    "⚫⚫⚪⚫⚫⚫⚪⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫";

    // Square root
static const char *const matrix_square_root_symbol =
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚪⚪⚪⚫"
    "⚫⚪⚫⚫⚪⚫⚫⚫"
    "⚫⚫⚪⚫⚪⚫⚫⚫"
    "⚫⚫⚫⚪⚪⚫⚫⚫"
    "⚫⚫⚫⚫⚪⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫";

    // Pi
static const char *const matrix_pi_symbol =
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚪⚪⚪⚪⚪⚪⚫"
    "⚫⚫⚪⚫⚫⚪⚫⚫"
    "⚫⚫⚪⚫⚫⚪⚫⚫"
    "⚫⚫⚪⚫⚫⚪⚫⚫"
    "⚫⚫⚪⚫⚫⚫⚪⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫";

    // Sine
static const char *const matrix_sine_symbol =
    "⚫⚫⚫⚪⚪⚪⚫⚫"
    "⚫⚫⚪⚫⚪⚫⚪⚫"
    "⚫⚪⚫⚫⚪⚫⚫⚪"
    "⚫⚪⚫⚫⚪⚫⚫⚪"
    "⚫⚪⚫⚫⚫⚫⚫⚪"
    "⚫⚫⚪⚫⚫⚫⚪⚫"
    "⚫⚫⚫⚪⚪⚪⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫";

    // Cosine
static const char *const matrix_cosine_symbol =
    "⚫⚫⚫⚪⚪⚪⚫⚫"
    "⚫⚫⚪⚫⚫⚫⚪⚫"
    "⚫⚪⚫⚫⚫⚫⚫⚪"
    "⚫⚪⚫⚫⚪⚪⚪⚪"
    "⚫⚪⚫⚫⚫⚫⚫⚪"
    "⚫⚫⚪⚫⚫⚫⚪⚫"
    "⚫⚫⚫⚪⚪⚪⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫";

    // Secant
static const char *const matrix_secant_symbol =
    "⚫⚫⚫⚪⚪⚪⚫⚪"
    "⚫⚫⚪⚫⚫⚫⚪⚫"
    "⚫⚪⚫⚫⚫⚪⚫⚪"
    "⚫⚪⚫⚫⚪⚫⚫⚪"
    "⚫⚪⚫⚫⚫⚫⚫⚪"
    "⚫⚫⚪⚫⚫⚫⚪⚫"
    "⚫⚫⚫⚪⚪⚪⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫";

    // Cosecant
static const char *const matrix_cosecant_symbol =
    "⚫⚪⚫⚪⚪⚪⚫⚫"
    "⚫⚫⚪⚫⚫⚫⚪⚫"
    "⚫⚪⚫⚪⚫⚫⚫⚪"
    "⚫⚪⚫⚫⚪⚫⚫⚪"
    "⚫⚪⚫⚫⚫⚫⚫⚪"
    "⚫⚫⚪⚫⚫⚫⚪⚫"
    "⚫⚫⚫⚪⚪⚪⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫";

    // Tangent
static const char *const matrix_tangent_symbol =
    "⚫⚪⚪⚪⚪⚪⚪⚪"
    "⚫⚫⚪⚫⚫⚫⚪⚫"
    "⚫⚪⚫⚫⚫⚫⚫⚪"
    "⚫⚪⚫⚫⚫⚫⚫⚪"
    "⚫⚪⚫⚫⚫⚫⚫⚪"
    "⚫⚫⚪⚫⚫⚫⚪⚫"
    "⚫⚫⚫⚪⚪⚪⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫";

    // Cotangent
static const char *const matrix_cotangent_symbol =
    "⚫⚫⚫⚪⚪⚪⚫⚫"
    "⚫⚫⚪⚫⚫⚫⚪⚫"
    "⚫⚪⚫⚫⚫⚫⚫⚪"
    "⚫⚪⚫⚫⚫⚫⚫⚪"
    "⚫⚪⚫⚫⚫⚫⚫⚪"
    "⚫⚫⚪⚫⚫⚫⚪⚫"
    "⚫⚪⚪⚪⚪⚪⚪⚪"
    "⚫⚫⚫⚫⚫⚫⚫⚫";

    // Hypot
static const char *const matrix_hypot_symbol =
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚪⚫⚫⚫⚫⚫⚫"
    "⚫⚪⚪⚫⚫⚫⚫⚫"
    "⚫⚪⚫⚪⚫⚫⚫⚫"
    "⚫⚪⚫⚫⚪⚫⚫⚫"
    "⚫⚪⚪⚪⚪⚪⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫";

    // Factorial
static const char *const matrix_factorial_symbol =
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚪⚫"
    "⚫⚪⚪⚪⚫⚫⚪⚫"
    "⚫⚪⚫⚫⚪⚫⚪⚫"
    "⚫⚪⚫⚫⚪⚫⚫⚫"
    "⚫⚪⚫⚫⚪⚫⚪⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫"
    "⚫⚫⚫⚫⚫⚫⚫⚫";



const char *matrix_symbol_get(uint8_t index) {
    switch (index) {
        case MATRIX_SYMBOL_BASE:
            return matrix_base_symbol;
        case MATRIX_SYMBOL_ADDITION:
            return matrix_addition_symbol;
        case MATRIX_SYMBOL_MULTIPLY:
            return matrix_multiply_symbol;
        case MATRIX_SYMBOL_DIVIDE:
            return matrix_divide_symbol;
        case MATRIX_SYMBOL_SUBTRACT:
            return matrix_subtract_symbol;
        case MATRIX_SYMBOL_POWER:
            return matrix_power_symbol;
        case MATRIX_SYMBOL_SQRT:
            return matrix_square_root_symbol;
        case MATRIX_SYMBOL_PI:
            return matrix_pi_symbol;
        case MATRIX_SYMBOL_SIN:
            return matrix_sine_symbol;
        case MATRIX_SYMBOL_COS:
            return matrix_cosine_symbol;
        case MATRIX_SYMBOL_SEC:
            return matrix_secant_symbol;
        case MATRIX_SYMBOL_CSC:
            return matrix_cosecant_symbol;
        case MATRIX_SYMBOL_TAN:
            return matrix_tangent_symbol;
        case MATRIX_SYMBOL_COT:
            return matrix_cotangent_symbol;
        case MATRIX_SYMBOL_FACT:
            return matrix_factorial_symbol;
        case MATRIX_SYMBOL_HYPOT:
            return matrix_hypot_symbol;
        default:
            return matrix_blank_symbol;
    }
}

const char *matrix_symbol_blank(void) {
    return matrix_blank_symbol;
}

uint8_t matrix_symbol_for_op(char op) {
    switch (op) {
        case '+':
            return MATRIX_SYMBOL_ADDITION;
        case '*':
            return MATRIX_SYMBOL_MULTIPLY;
        case '/':
            return MATRIX_SYMBOL_DIVIDE;
        case '-':
            return MATRIX_SYMBOL_SUBTRACT;
        case '^':
            return MATRIX_SYMBOL_POWER;
        case 'H':
            return MATRIX_SYMBOL_HYPOT;
        default:
            return MATRIX_SYMBOL_INVALID;
    }
}

uint8_t matrix_symbol_for_scientific(uint8_t scientific_index) {
    switch (scientific_index) {
        case MATRIX_SCI_POWER:
            return MATRIX_SYMBOL_POWER;
        case MATRIX_SCI_SQRT:
            return MATRIX_SYMBOL_SQRT;
        case MATRIX_SCI_PI:
            return MATRIX_SYMBOL_PI;
        case MATRIX_SCI_SIN:
            return MATRIX_SYMBOL_SIN;
        case MATRIX_SCI_COS:
            return MATRIX_SYMBOL_COS;
        case MATRIX_SCI_SEC:
            return MATRIX_SYMBOL_SEC;
        case MATRIX_SCI_CSC:
            return MATRIX_SYMBOL_CSC;
        case MATRIX_SCI_TAN:
            return MATRIX_SYMBOL_TAN;
        case MATRIX_SCI_COT:
            return MATRIX_SYMBOL_COT;
        case MATRIX_SCI_FACT:
            return MATRIX_SYMBOL_FACT;
        case MATRIX_SCI_HYPOT:
            return MATRIX_SYMBOL_HYPOT;
        default:
            return MATRIX_SYMBOL_INVALID;
    }
}
