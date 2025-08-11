#ifndef MUSIC_H
#define MUSIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "instrument.h"
#include "parser.h"

// ============================================================================
// MUSIC SYSTEM TYPES
// ============================================================================

typedef struct {
    uint32_t phase_accum; // Unsigned for proper DDS wraparound
    uint32_t phase_increment; // Unsigned phase increment per sample
    int32_t amplitude; // Q1.31 amplitude for this partial
} partial_t;

typedef struct {
    // === Timing (immutable) ===
    uint32_t start_sample;
    uint32_t duration_samples;
    uint32_t release_sample;

    // === Audio Properties (immutable) ===
    const instrument_t* instrument;
    int32_t volume_scale;

    // === Envelope State (mutable) ===
    union {
        pluck_decay_t pluck;
        adsr_t adsr;
    } envelope_state;

    // === Variable Partial Data ===
    uint8_t num_partials;
    partial_t partials[MAX_PARTIALS];
} event_t;

DEFINE_ARRAY_TYPE(event, event_t)

#define MAX_SIMULTANEOUS_EVENTS 32
#define AUDIBLE_THRESHOLD 0x00001000 // Much lower threshold - about 0.1% of full scale

typedef struct {
    event_array_t events;
    uint32_t sample_rate;
    uint64_t current_sample_index;
    uint64_t total_duration_samples; // Total song length
    int next_event_index;
    event_t* active_events[MAX_SIMULTANEOUS_EVENTS];
    int num_active;
    bool completed; // Set by callback when song ends, checked by main thread
} sequencer_state_t;

// ============================================================================
// MUSIC SYSTEM FUNCTIONS
// ============================================================================

// Initialize sine table (call once at startup)
void music_init(void);

// Generate one sample from an event
int16_t generate_event_sample(event_t* event, uint32_t current_sample_index);

// Get current envelope level for threshold checking
int32_t get_current_envelope_level(event_t* event);

// Main sequencer callback function (audio system agnostic)
bool sequencer_callback(int16_t* buffer, size_t num_samples, void* user_data);

// Clean up sequencer state
void cleanup_sequencer_state(sequencer_state_t* seq);

// Envelope functions
int32_t pluck_envelope(void* state, uint32_t samples_since_start, int32_t samples_until_release);
int32_t adsr_envelope(void* state, uint32_t samples_since_start, int32_t samples_until_release);

// Helper function to convert parsed notes to sequencer events
event_array_t sequence_events(const note_array_t* notes, uint16_t sample_rate, int tempo_bpm,
                              const key_signature_t* key, const temperament_t* temperament, int transposition,
                              float volume);

#endif // MUSIC_H
