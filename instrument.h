#ifndef INSTRUMENT_H
#define INSTRUMENT_H

#include <stdint.h>

// ============================================================================
// ENVELOPE SYSTEM
// ============================================================================

// Forward declaration for envelope state union
typedef union envelope_state envelope_state_t;

// Exponential decay envelope state
typedef struct {
    int32_t initial_amplitude; // Q1.31
    int32_t decay_multiplier; // Q1.31 per-sample multiplier (e.g., 0.9999)
    int32_t current_level; // Q1.31 current amplitude
} pluck_decay_t;

// ADSR envelope state
typedef struct {
    uint32_t attack_samples; // Number of samples for attack phase
    uint32_t decay_samples; // Number of samples for decay phase
    int32_t sustain_level; // Q1.31 sustain amplitude level
    uint32_t release_samples; // Number of samples for release phase
    int32_t current_level; // Q1.31 current amplitude
    int32_t release_start_level; // Q1.31 level when release phase began
    int32_t release_coeff; // Q1.31 exponential release coefficient
    uint32_t min_release_samples; // Minimum release time to prevent clicks
    uint8_t phase; // Current ADSR phase
} adsr_t;

// ADSR phase constants
#define ADSR_ATTACK 0
#define ADSR_DECAY 1
#define ADSR_SUSTAIN 2
#define ADSR_RELEASE 3

// Envelope state union
typedef union envelope_state {
    pluck_decay_t pluck;
    adsr_t adsr;
} envelope_state_t;

// ============================================================================
// INSTRUMENT SYSTEM
// ============================================================================

// ADSR phase constants
#define ADSR_ATTACK 0
#define ADSR_DECAY 1
#define ADSR_SUSTAIN 2
#define ADSR_RELEASE 3

// Complete instrument definition with function pointers
typedef struct {
    int32_t (*envelope)(void* envelope_state, uint32_t samples_since_start, int32_t samples_until_release);
    uint8_t num_partials;
    float harmonic_ratios[8]; // For setup time
    float partial_amplitudes[8]; // For setup time
} instrument_t;

// ============================================================================
// STANDARD INSTRUMENTS
// ============================================================================

// Global ADSR instrument for keyboard-like sounds
extern const instrument_t adsr_instrument;

// Standard instrument definitions for parsing
extern const instrument_t pluck_sine_instrument;
extern const instrument_t pluck_square_instrument;

// ============================================================================
// ENVELOPE FUNCTIONS
// ============================================================================

// Exponential decay envelope (for plucked string simulation)
int32_t pluck_envelope(void* state, uint32_t samples_since_start, int32_t samples_until_release);

// ADSR envelope with anti-click exponential release
int32_t adsr_envelope(void* state, uint32_t samples_since_start, int32_t samples_until_release);

// ============================================================================
// INSTRUMENT LOOKUP
// ============================================================================

// Lookup instrument by name (case-insensitive)
const instrument_t* lookup_instrument(const char* name);

#endif // INSTRUMENT_H
