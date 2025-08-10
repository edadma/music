#ifndef MUSIC_H
#define MUSIC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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
    } envelope_state;

    // === Variable Partial Data ===
    uint8_t num_partials;
    partial_t partials[];
} event_t;

#define MAX_SIMULTANEOUS_EVENTS 32
#define AUDIBLE_THRESHOLD 0x00080000  // Higher threshold - about 1% of full scale

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

// Create a simple test event
event_t* create_simple_event(uint32_t start_sample, float freq, float duration_sec, uint32_t sample_rate);

// Generate one sample from an event
int16_t generate_event_sample(event_t *event, uint64_t current_sample_index);

// Main sequencer callback function (audio system agnostic)
bool sequencer_callback(int16_t *buffer, size_t num_samples, void *user_data);

// Create test song
sequencer_state_t* create_test_song(uint32_t sample_rate);

// Clean up sequencer state
void cleanup_song(sequencer_state_t *seq);

// Envelope functions
int32_t pluck_envelope(void *state, uint32_t samples_since_start, int32_t samples_until_release);

#endif // MUSIC_H