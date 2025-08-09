#ifndef FAST_MATH_H
#define FAST_MATH_H

#include <stdbool.h>

// Fast math lookup tables for embedded audio
// Optimized for Raspberry Pi Pico and similar microcontrollers

// Initialize all lookup tables - call once at startup
void init_fast_math(void);

// Fast sine function using lookup table with linear interpolation
// Input: any float value (radians)
// Output: sine value [-1.0, 1.0]
float fast_sin(float x);

// Fast exponential function using lookup table with linear interpolation
// Input: any float value (covers typical audio range efficiently)
// Output: exp(x)
float fast_exp(float x);

// Fast power-of-2 function using lookup table with linear interpolation
// Input: any float value (optimized for musical range [-6, 10])
// Output: pow(2.0, x)
// OPTIMIZED REPLACEMENT for pow(2.0, x)
float fast_pow2(float x);

#endif // FAST_MATH_H
