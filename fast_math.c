#include "fast_math.h"

#include <math.h>

// Sine lookup table
#define SINE_TABLE_SIZE 1024
static float sine_table[SINE_TABLE_SIZE];
static bool sine_table_initialized = false;

static void init_sine_table(void) {
    if (sine_table_initialized)
        return;

    for (int i = 0; i < SINE_TABLE_SIZE; i++) {
        sine_table[i] = sinf(2.0f * M_PI * i / SINE_TABLE_SIZE);
    }
    sine_table_initialized = true;
}

// General-purpose exponential lookup table
#define EXP_TABLE_SIZE 1024
static float exp_table[EXP_TABLE_SIZE];
static bool exp_table_initialized = false;

static void init_exp_table(void) {
    if (exp_table_initialized)
        return;

    // Cover range [-10, +5] which handles most audio use cases
    // exp(-10) ≈ 0.000045, exp(5) ≈ 148
    for (int i = 0; i < EXP_TABLE_SIZE; i++) {
        float x = -10.0f + 15.0f * i / (EXP_TABLE_SIZE - 1); // -10 to +5
        exp_table[i] = expf(x);
    }
    exp_table_initialized = true;
}

float fast_sin(float x) {
    // Normalize to [0, 1] range
    x = x / (2.0f * M_PI);
    x = x - floorf(x); // Remove integer part

    // Convert to table index
    float index_f = x * SINE_TABLE_SIZE;
    int index = (int)index_f;
    float frac = index_f - index;

    // Linear interpolation
    int next_index = (index + 1) % SINE_TABLE_SIZE;
    return sine_table[index] + frac * (sine_table[next_index] - sine_table[index]);
}

float fast_exp(float x) {
    // Handle extreme cases
    if (x < -10.0f)
        return 0.0f;
    if (x > 5.0f)
        return expf(x); // Fall back to real exp for very large values

    // Convert to table index
    float index_f = (x + 10.0f) * (EXP_TABLE_SIZE - 1) / 15.0f;
    int index = (int)index_f;
    float frac = index_f - index;

    // Linear interpolation
    return exp_table[index] + frac * (exp_table[index + 1] - exp_table[index]);
}

// Power-of-2 lookup table for frequency calculations
#define POW2_TABLE_SIZE 1024
static float pow2_table[POW2_TABLE_SIZE];
static bool pow2_table_initialized = false;

static void init_pow2_table(void) {
    if (pow2_table_initialized)
        return;
    // Cover range [-6, 10] which handles all musical octave calculations
    // pow(2, -6) = 1/64 ≈ 0.0156, pow(2, 10) = 1024
    for (int i = 0; i < POW2_TABLE_SIZE; i++) {
        float x = -6.0f + 16.0f * i / (POW2_TABLE_SIZE - 1); // -6 to +10
        pow2_table[i] = powf(2.0f, x);
    }
    pow2_table_initialized = true;
}

float fast_pow2(float x) {
    // Handle extreme cases
    if (x < -6.0f)
        return powf(2.0f, x); // Fall back for very small values
    if (x > 10.0f)
        return powf(2.0f, x); // Fall back for very large values
    // Convert to table index
    float index_f = (x + 6.0f) * (POW2_TABLE_SIZE - 1) / 16.0f;
    int index = (int)index_f;
    float frac = index_f - index;

    // Bounds check
    if (index >= POW2_TABLE_SIZE - 1) {
        return pow2_table[POW2_TABLE_SIZE - 1];
    }

    // Linear interpolation
    return pow2_table[index] + frac * (pow2_table[index + 1] - pow2_table[index]);
}

void init_fast_math(void) {
    init_sine_table();
    init_exp_table();
    init_pow2_table();
}
