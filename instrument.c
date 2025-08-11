#include "instrument.h"

#include <math.h>
#include <string.h>
#include <strings.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define AUDIBLE_THRESHOLD 0x00001000 // Q1.31 threshold for inaudible level (~0.1% of full scale)

// ============================================================================
// ENVELOPE FUNCTIONS
// ============================================================================

int32_t pluck_envelope(void* state, uint32_t samples_since_start, int32_t samples_until_release) {
    (void)samples_since_start; // Unused for this envelope type
    (void)samples_until_release; // Unused for this envelope type

    pluck_decay_t* pluck = (pluck_decay_t*)state;

    // Exponential decay: current_level *= decay_multiplier
    // Q1.31 * Q1.31 = Q2.62, shift back to Q1.31
    int64_t temp = (int64_t)pluck->current_level * pluck->decay_multiplier;
    pluck->current_level = (int32_t)(temp >> 31);

    return pluck->current_level;
}

int32_t adsr_envelope(void* state, uint32_t samples_since_start, int32_t samples_until_release) {
    adsr_t* adsr = (adsr_t*)state;

    // Determine current phase based on timing
    if (samples_until_release <= 0) {
        // Release phase
        if (adsr->phase != ADSR_RELEASE) {
            // First time entering release - setup exponential release
            adsr->release_start_level = adsr->current_level;
            adsr->phase = ADSR_RELEASE;

            // Calculate exponential release coefficient
            // Using target ratio of -60dB (0.001) for natural decay
            double target_ratio = 0.00001; // -60dB
            uint32_t effective_release_samples = adsr->release_samples;

            // Enforce minimum release time to prevent clicks (20ms minimum)
            if (effective_release_samples < adsr->min_release_samples) {
                effective_release_samples = adsr->min_release_samples;
            }

            // RC-circuit inspired exponential coefficient
            // rate = exp(-log((1 + targetRatio) / targetRatio) / time)
            double rate = exp(-log((1.0 + target_ratio) / target_ratio) / effective_release_samples);
            adsr->release_coeff = (int32_t)(rate * 0x7FFFFFFF); // Convert to Q1.31
        }

        // Exponential decay using iterative multiplication
        int64_t temp = (int64_t)adsr->current_level * adsr->release_coeff;
        adsr->current_level = (int32_t)(temp >> 31);

        // Clamp to zero when it gets very small (prevents denormals and infinite decay)
        if (adsr->current_level < AUDIBLE_THRESHOLD / 4) {
            adsr->current_level = 0;
        }
    } else if (samples_since_start < adsr->attack_samples) {
        // Attack phase - ramp from AUDIBLE_THRESHOLD to full scale
        adsr->phase = ADSR_ATTACK;
        int32_t attack_range = 0x7FFFFFFF - AUDIBLE_THRESHOLD;
        int32_t attack_progress = ((int64_t)samples_since_start * attack_range) / adsr->attack_samples;
        adsr->current_level = AUDIBLE_THRESHOLD + attack_progress;
    } else if (samples_since_start < adsr->attack_samples + adsr->decay_samples) {
        // Decay phase
        adsr->phase = ADSR_DECAY;
        // Linear ramp from full scale to sustain_level over decay_samples
        uint32_t decay_progress = samples_since_start - adsr->attack_samples;
        int32_t decay_range = 0x7FFFFFFF - adsr->sustain_level;
        int32_t decay_amount = ((int64_t)decay_progress * decay_range) / adsr->decay_samples;
        adsr->current_level = 0x7FFFFFFF - decay_amount;
    } else {
        // Sustain phase
        adsr->phase = ADSR_SUSTAIN;
        adsr->current_level = adsr->sustain_level;
    }

    return adsr->current_level;
}

// ============================================================================
// STANDARD INSTRUMENT DEFINITIONS
// ============================================================================

// Global ADSR instrument for keyboard-like sounds
const instrument_t adsr_instrument = {
    .envelope = adsr_envelope, .num_partials = 1, .harmonic_ratios = {1.0f}, .partial_amplitudes = {1.0f}};

// Standard instruments with parsing names
const instrument_t pluck_sine_instrument = {.envelope = adsr_envelope, // Use ADSR for consistent behavior
                                            .num_partials = 1,
                                            .harmonic_ratios = {1.0f},
                                            .partial_amplitudes = {1.0f}};

const instrument_t pluck_square_instrument = {.envelope = pluck_envelope,
                                              .num_partials = 3, // Add some harmonics for square wave character
                                              .harmonic_ratios = {1.0f, 3.0f, 5.0f},
                                              .partial_amplitudes = {1.0f, 0.333f, 0.2f}};

// ============================================================================
// INSTRUMENT REGISTRY AND LOOKUP
// ============================================================================

// Global instrument registry
static const struct {
    const char* name;
    const instrument_t* instrument;
} available_instruments[] = {
    {"pluck sine", &pluck_sine_instrument}, {"pluck square", &pluck_square_instrument}, {NULL, NULL} // Sentinel
};

const instrument_t* lookup_instrument(const char* name) {
    if (!name) {
        return &pluck_sine_instrument; // Default fallback
    }

    for (int i = 0; available_instruments[i].name != NULL; i++) {
        if (strcasecmp(name, available_instruments[i].name) == 0) {
            return available_instruments[i].instrument;
        }
    }

    return &pluck_sine_instrument; // Default fallback
}
