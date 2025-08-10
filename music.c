#include "music.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

// ============================================================================
// GLOBAL SINE TABLE AND UTILITIES
// ============================================================================

#define SINE_TABLE_SIZE 1024
static int32_t sine_table[SINE_TABLE_SIZE];

void music_init(void) {
    for (int i = 0; i < SINE_TABLE_SIZE; i++) {
        double angle = 2.0 * M_PI * i / SINE_TABLE_SIZE;
        sine_table[i] = (int32_t)(sin(angle) * 0x7FFFFFFF);
    }
}

// Convert frequency to phase increment (unsigned for DDS)
static uint32_t freq_to_phase_increment(float freq, uint32_t sample_rate) {
    return (uint32_t)((freq / sample_rate) * 0x100000000LL);
}

// ============================================================================
// ENVELOPE FUNCTIONS
// ============================================================================

int32_t pluck_envelope(void *state, uint32_t samples_since_start, int32_t samples_until_release) {
    (void)samples_since_start;    // Unused for this envelope type
    (void)samples_until_release;  // Unused for this envelope type

    pluck_decay_t *pluck = (pluck_decay_t*)state;

    // Exponential decay: current_level *= decay_multiplier
    // Q1.31 * Q1.31 = Q2.62, shift back to Q1.31
    int64_t temp = (int64_t)pluck->current_level * pluck->decay_multiplier;
    pluck->current_level = (int32_t)(temp >> 31);

    return pluck->current_level;
}

// ============================================================================
// EVENT AND SAMPLE GENERATION
// ============================================================================

event_t* create_simple_event(uint32_t start_sample, float freq, float duration_sec, uint32_t sample_rate) {
    // Allocate event with one partial
    event_t *event = malloc(sizeof(event_t) + sizeof(partial_t));
    if (!event) return NULL;

    event->start_sample = start_sample;
    event->duration_samples = (uint32_t)(duration_sec * sample_rate);
    event->release_sample = start_sample + event->duration_samples;
    event->volume_scale = 0x08000000;  // About 1/16 volume in Q1.31 (much quieter)
    event->num_partials = 1;

    // Setup single partial (fundamental frequency)
    event->partials[0].phase_accum = 0;
    event->partials[0].phase_increment = freq_to_phase_increment(freq, sample_rate);
    event->partials[0].amplitude = 0x7FFFFFFF;  // Full amplitude for this partial

    // Setup pluck envelope (exponential decay with ~2 second decay time)
    event->envelope_state.pluck.initial_amplitude = 0x7FFFFFFF;
    // Decay multiplier: for 2 second decay, we want level to drop to ~1/e in 2 seconds
    // decay_multiplier = exp(-1/(2*sample_rate)) ≈ 0.999989 for 44.1kHz
    double decay_rate = 1.0 / (0.2 * sample_rate);  // 2 second time constant
    double multiplier = exp(-decay_rate);
    event->envelope_state.pluck.decay_multiplier = (int32_t)(multiplier * 0x7FFFFFFF);
    event->envelope_state.pluck.current_level = 0x7FFFFFFF;

    return event;
}

int16_t generate_event_sample(event_t *event, uint64_t current_sample_index) {
    uint32_t samples_since_start = current_sample_index - event->start_sample;
    int32_t samples_until_release = event->release_sample - current_sample_index;

    // Get envelope level
    int32_t envelope_level = pluck_envelope(&event->envelope_state.pluck,
                                           samples_since_start, samples_until_release);

    // Generate samples from all partials
    int32_t event_sample = 0;
    for (int i = 0; i < event->num_partials; i++) {
        partial_t *p = &event->partials[i];

        // Get sine wave sample using proper DDS lookup
        uint32_t table_index = (p->phase_accum >> 22) & (SINE_TABLE_SIZE - 1);
        int32_t wave_sample = sine_table[table_index];

        // Apply partial amplitude (Q1.31 * Q1.31 = Q2.62, shift back to Q1.31)
        int64_t partial_sample = (int64_t)wave_sample * p->amplitude;
        event_sample += (int32_t)(partial_sample >> 31);

        // Advance phase (unsigned arithmetic wraps properly)
        p->phase_accum += p->phase_increment;
    }

    // Apply envelope (Q1.31 * Q1.31 = Q2.62, shift back to Q1.31)
    int64_t enveloped_sample = (int64_t)event_sample * envelope_level;
    enveloped_sample >>= 31;

    // Apply volume scaling (Q1.31 * Q1.31 = Q2.62, shift back to Q1.31)
    int64_t final_sample = enveloped_sample * event->volume_scale;
    final_sample >>= 31;

    // Convert Q1.31 to S16 (shift by 16 more bits)
    return (int16_t)(final_sample >> 16);
}

// ============================================================================
// SEQUENCER CALLBACK
// ============================================================================

bool sequencer_callback(int16_t *buffer, size_t num_samples, void *user_data) {
    sequencer_state_t *seq = (sequencer_state_t*)user_data;

    for (size_t i = 0; i < num_samples; i++) {
        // 1. Activate new events that should start now
        while (seq->next_event_index < seq->num_events &&
               seq->events[seq->next_event_index]->start_sample <= seq->current_sample_index) {

            if (seq->num_active < MAX_SIMULTANEOUS_EVENTS) {
                seq->active_events[seq->num_active] = seq->events[seq->next_event_index];
                seq->num_active++;
                printf("Activated event %zu at sample %lu\n", seq->next_event_index, seq->current_sample_index);
            }
            seq->next_event_index++;
        }

        // 2. Generate sample from all active events
        int32_t mixed_sample = 0;
        for (size_t j = 0; j < seq->num_active; j++) {
            mixed_sample += generate_event_sample(seq->active_events[j], seq->current_sample_index);
        }

        buffer[i] = (int16_t)mixed_sample;

        // 3. Remove inaudible events (backwards iteration for safe removal)
        for (int j = seq->num_active - 1; j >= 0; j--) {
            if (seq->active_events[j]->envelope_state.pluck.current_level < AUDIBLE_THRESHOLD) {
                printf("Removing inaudible event at sample %lu\n", seq->current_sample_index);
                // Swap with last element and decrease count
                seq->active_events[j] = seq->active_events[seq->num_active - 1];
                seq->num_active--;
            }
        }

        seq->current_sample_index++;
    }

    // Check if song is complete
    if (seq->num_active == 0 && seq->next_event_index >= seq->num_events) {
        printf("Song complete, marking as finished\n");
        seq->completed = true;
        return false;  // Tell audio driver to stop calling us
    }

    return true;  // Continue playback
}

// ============================================================================
// TEST SETUP
// ============================================================================

sequencer_state_t* create_test_song(uint32_t sample_rate) {
    sequencer_state_t *seq = calloc(1, sizeof(sequencer_state_t));
    seq->sample_rate = sample_rate;
    seq->current_sample_index = 0;
    seq->next_event_index = 0;
    seq->num_active = 0;
    seq->completed = false;

    // Create 4 test events with gaps (rests) between them
    seq->num_events = 4;
    seq->events = malloc(seq->num_events * sizeof(event_t*));

    // Event 0: C4 (261.63 Hz) at start, 1 second duration
    seq->events[0] = create_simple_event(0, 261.63f, 1.0f, sample_rate);

    // Event 1: E4 (329.63 Hz) after 0.5 second gap, 1 second duration
    seq->events[1] = create_simple_event(sample_rate * 1.5f, 329.63f, 1.0f, sample_rate);

    // Event 2: G4 (392.00 Hz) after 0.5 second gap, 1.5 second duration
    seq->events[2] = create_simple_event(sample_rate * 3.0f, 392.00f, 1.5f, sample_rate);

    // Event 3: C5 (523.25 Hz) after 1 second gap, 2 second duration
    seq->events[3] = create_simple_event(sample_rate * 5.5f, 523.25f, 2.0f, sample_rate);

    // Calculate total song duration (last event start + duration + some decay time)
    seq->total_duration_samples = sample_rate * 5.5f + sample_rate * 2.0f + sample_rate * 2.0f; // Extra time for decay

    printf("Created test song with %zu events\n", seq->num_events);
    printf("Event 0: C4 at sample 0\n");
    printf("Event 1: E4 at sample %u\n", (uint32_t)(sample_rate * 1.5f));
    printf("Event 2: G4 at sample %u\n", (uint32_t)(sample_rate * 3.0f));
    printf("Event 3: C5 at sample %u\n", (uint32_t)(sample_rate * 5.5f));
    printf("Total duration: %lu samples (%.1f seconds)\n",
           seq->total_duration_samples, seq->total_duration_samples / (float)sample_rate);

    return seq;
}

void cleanup_song(sequencer_state_t *seq) {
    if (!seq) return;

    for (size_t i = 0; i < seq->num_events; i++) {
        free(seq->events[i]);
    }
    free(seq->events);
    free(seq);
}