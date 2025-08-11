#include "sequencer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "array.h"

DEFINE_ARRAY_FUNCTIONS(event, event_t)

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

// ============================================================================
// EVENT AND SAMPLE GENERATION
// ============================================================================

int16_t generate_event_sample(event_t* event, uint32_t current_sample_index) {
    uint32_t samples_since_start = current_sample_index - event->start_sample;
    uint32_t samples_until_release = event->release_sample - current_sample_index;

    // Get envelope level using the instrument's envelope function
    int32_t envelope_level;
    if (event->instrument && event->instrument->envelope) {
        envelope_level =
            event->instrument->envelope(&event->envelope_state, samples_since_start, samples_until_release);
    } else {
        // Fallback to full volume if no envelope function
        envelope_level = 0x7FFFFFFF;
    }

    // Generate samples from all partials
    int32_t event_sample = 0;
    for (int i = 0; i < event->num_partials; i++) {
        partial_t* p = &event->partials[i];

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

int32_t get_current_envelope_level(event_t* event) {
    // Return the current envelope level based on envelope type
    // We can determine type by checking which envelope function is assigned
    if (event->instrument && event->instrument->envelope == adsr_envelope) {
        return event->envelope_state.adsr.current_level;
    } else if (event->instrument && event->instrument->envelope == pluck_envelope) {
        return event->envelope_state.pluck.current_level;
    } else {
        // Unknown or no envelope, assume audible
        return 0x7FFFFFFF;
    }
}

// ============================================================================
// SEQUENCER CALLBACK
// ============================================================================

bool sequencer_callback(int16_t* buffer, size_t num_samples, void* user_data) {
    sequencer_state_t* seq = (sequencer_state_t*)user_data;

    for (size_t i = 0; i < num_samples; i++) {
        // 1. Activate new events that should start now
        while (seq->next_event_index < seq->events.count &&
               seq->events.data[seq->next_event_index].start_sample <= seq->current_sample_index) {

            if (seq->num_active < MAX_SIMULTANEOUS_EVENTS) {
                seq->active_events[seq->num_active] = &seq->events.data[seq->next_event_index];
                seq->num_active++;
                printf("Activated event %d at sample %lu\n", seq->next_event_index, seq->current_sample_index);
            }
            seq->next_event_index++;
        }

        // 2. Generate sample from all active events (this updates envelopes)
        int32_t mixed_sample = 0;
        for (int j = 0; j < seq->num_active; j++) {
            mixed_sample += generate_event_sample(seq->active_events[j], seq->current_sample_index);
        }

        buffer[i] = (int16_t)mixed_sample;

        // 3. Remove events that have completed their release phase (backwards iteration for safe removal)
        for (int j = seq->num_active - 1; j >= 0; j--) {
            event_t* event = seq->active_events[j];

            if (event->instrument && event->instrument->envelope == adsr_envelope) {
                adsr_t* adsr = &event->envelope_state.adsr;

                // For ADSR: remove when in release phase and envelope has decayed to near zero
                int32_t samples_until_release = event->release_sample - seq->current_sample_index;

                if (samples_until_release <= 0 && adsr->current_level == 0) {
                    // Release phase and exponential decay has reached zero
                    printf("Removing completed ADSR event at sample %lu\n", seq->current_sample_index);
                    seq->active_events[j] = seq->active_events[seq->num_active - 1];
                    seq->num_active--;
                }
            } else {
                // For other envelope types, use the threshold method
                if (get_current_envelope_level(event) < AUDIBLE_THRESHOLD) {
                    printf("Removing inaudible event at sample %lu\n", seq->current_sample_index);
                    seq->active_events[j] = seq->active_events[seq->num_active - 1];
                    seq->num_active--;
                }
            }
        }

        seq->current_sample_index++;
    }

    // Check if song is complete
    if (seq->num_active == 0 && seq->next_event_index >= seq->events.count) {
        printf("Song complete, marking as finished\n");
        seq->completed = true;
        return false; // Tell audio driver to stop calling us
    }

    return true; // Continue playback
}

void cleanup_song(sequencer_state_t* seq) {
    if (!seq)
        return;

    event_array_free(&seq->events);
    free(seq);
}
