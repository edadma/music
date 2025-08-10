#ifndef MUSIC_H
#define MUSIC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "audio_driver.h"

// ============================================================================
// MUSIC SYSTEM TYPES
// ============================================================================

typedef struct {
    uint32_t phase_accum;      // Unsigned for proper DDS wraparound
    uint32_t phase_increment;  // Unsigned phase increment per sample
    int32_t amplitude;         // Q1.31 amplitude for this partial
} partial_t;

// Exponential decay envelope state
typedef struct {
    int32_t initial_amplitude; // Q1.31
    int32_t decay_multiplier;  // Q1.31 per-sample multiplier (e.g., 0.9999)
    int32_t current_level;     // Q1.31 current amplitude
} pluck_decay_t;

// ADSR envelope state
typedef struct {
    uint32_t attack_samples;        // Number of samples for attack phase
    uint32_t decay_samples;         // Number of samples for decay phase
    int32_t sustain_level;          // Q1.31 sustain amplitude level
    uint32_t release_samples;       // Number of samples for release phase
    int32_t current_level;          // Q1.31 current amplitude
    int32_t release_start_level;    // Q1.31 level when release phase began
    int32_t release_coeff;          // Q1.31 exponential release coefficient
    uint32_t min_release_samples;   // Minimum release time to prevent clicks
    uint8_t phase;                  // Current ADSR phase
} adsr_t;

// ADSR phase constants
#define ADSR_ATTACK  0
#define ADSR_DECAY   1
#define ADSR_SUSTAIN 2
#define ADSR_RELEASE 3

typedef int32_t (*envelope_fn_t)(void *envelope_state, uint32_t samples_since_start, int32_t samples_until_release);

typedef struct {
    envelope_fn_t envelope;
    uint8_t num_partials;
    float harmonic_ratios[8];     // For setup time
    float partial_amplitudes[8]; // For setup time
} instrument_t;

typedef struct {
    // === Timing (immutable) ===
    uint32_t start_sample;
    uint32_t duration_samples;
    uint32_t release_sample;

    // === Audio Properties (immutable) ===
    instrument_t *instrument;
    int32_t volume_scale;

    // === Envelope State (mutable) ===
    union {
        pluck_decay_t pluck;
        adsr_t adsr;
    } envelope_state;

    // === Variable Partial Data ===
    uint8_t num_partials;
    partial_t partials[];
} event_t;

#define MAX_SIMULTANEOUS_EVENTS 32
#define AUDIBLE_THRESHOLD 0x00001000  // Much lower threshold - about 0.1% of full scale

typedef struct {
    event_t **events;
    size_t num_events;
    uint32_t sample_rate;
    uint64_t current_sample_index;
    uint64_t total_duration_samples; // Total song length
    size_t next_event_index;
    event_t *active_events[MAX_SIMULTANEOUS_EVENTS];
    size_t num_active;
    bool completed;                // Set by callback when song ends, checked by main thread
} sequencer_state_t;

// ============================================================================
// MUSIC SYSTEM FUNCTIONS
// ============================================================================

// Initialize sine table (call once at startup)
void music_init(void);

// Generate one sample from an event
int16_t generate_event_sample(event_t *event, uint64_t current_sample_index);

// Get current envelope level for threshold checking
int32_t get_current_envelope_level(event_t *event);

// Main sequencer callback function (audio system agnostic)
bool sequencer_callback(int16_t *buffer, size_t num_samples, void *user_data);

// Clean up sequencer state
void cleanup_song(sequencer_state_t *seq);

// Envelope functions
int32_t pluck_envelope(void *state, uint32_t samples_since_start, int32_t samples_until_release);
int32_t adsr_envelope(void *state, uint32_t samples_since_start, int32_t samples_until_release);

#endif // MUSIC_H