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

int32_t adsr_envelope(void *state, uint32_t samples_since_start, int32_t samples_until_release) {
    adsr_t *adsr = (adsr_t*)state;

    // Determine current phase based on timing
    if (samples_until_release <= 0) {
        // Release phase
        if (adsr->phase != ADSR_RELEASE) {
            // First time entering release - setup exponential release
            adsr->release_start_level = adsr->current_level;
            adsr->phase = ADSR_RELEASE;

            // Calculate exponential release coefficient
            // Using target ratio of -60dB (0.001) for natural decay
            double target_ratio = 0.00001;  // -60dB
            uint32_t effective_release_samples = adsr->release_samples;

            // Enforce minimum release time to prevent clicks (20ms minimum)
            if (effective_release_samples < adsr->min_release_samples) {
                effective_release_samples = adsr->min_release_samples;
            }

            // RC-circuit inspired exponential coefficient
            // rate = exp(-log((1 + targetRatio) / targetRatio) / time)
            double rate = exp(-log((1.0 + target_ratio) / target_ratio) / effective_release_samples);
            adsr->release_coeff = (int32_t)(rate * 0x7FFFFFFF);  // Convert to Q1.31
        }

        // Exponential decay using iterative multiplication
        int64_t temp = (int64_t)adsr->current_level * adsr->release_coeff;
        adsr->current_level = (int32_t)(temp >> 31);

        // Clamp to zero when it gets very small (prevents denormals and infinite decay)
        if (adsr->current_level < AUDIBLE_THRESHOLD / 4) {
            adsr->current_level = 0;
        }
    }
    else if (samples_since_start < adsr->attack_samples) {
        // Attack phase - ramp from AUDIBLE_THRESHOLD to full scale
        adsr->phase = ADSR_ATTACK;
        int32_t attack_range = 0x7FFFFFFF - AUDIBLE_THRESHOLD;
        int32_t attack_progress = ((int64_t)samples_since_start * attack_range) / adsr->attack_samples;
        adsr->current_level = AUDIBLE_THRESHOLD + attack_progress;
    }
    else if (samples_since_start < adsr->attack_samples + adsr->decay_samples) {
        // Decay phase
        adsr->phase = ADSR_DECAY;
        // Linear ramp from full scale to sustain_level over decay_samples
        uint32_t decay_progress = samples_since_start - adsr->attack_samples;
        int32_t decay_range = 0x7FFFFFFF - adsr->sustain_level;
        int32_t decay_amount = ((int64_t)decay_progress * decay_range) / adsr->decay_samples;
        adsr->current_level = 0x7FFFFFFF - decay_amount;
    }
    else {
        // Sustain phase
        adsr->phase = ADSR_SUSTAIN;
        adsr->current_level = adsr->sustain_level;
    }

    return adsr->current_level;
}

// ============================================================================
// EVENT AND SAMPLE GENERATION
// ============================================================================

int16_t generate_event_sample(event_t *event, uint64_t current_sample_index) {
    uint32_t samples_since_start = current_sample_index - event->start_sample;
    int32_t samples_until_release = event->release_sample - current_sample_index;

    // Get envelope level using the instrument's envelope function
    int32_t envelope_level;
    if (event->instrument && event->instrument->envelope) {
        envelope_level = event->instrument->envelope(&event->envelope_state,
                                                   samples_since_start, samples_until_release);
    } else {
        // Fallback to full volume if no envelope function
        envelope_level = 0x7FFFFFFF;
    }

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

int32_t get_current_envelope_level(event_t *event) {
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

        // 2. Generate sample from all active events (this updates envelopes)
        int32_t mixed_sample = 0;
        for (size_t j = 0; j < seq->num_active; j++) {
            mixed_sample += generate_event_sample(seq->active_events[j], seq->current_sample_index);
        }

        buffer[i] = (int16_t)mixed_sample;

        // 3. Remove events that have completed their release phase (backwards iteration for safe removal)
        for (int j = seq->num_active - 1; j >= 0; j--) {
            event_t *event = seq->active_events[j];

            if (event->instrument && event->instrument->envelope == adsr_envelope) {
                adsr_t *adsr = &event->envelope_state.adsr;

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
    if (seq->num_active == 0 && seq->next_event_index >= seq->num_events) {
        printf("Song complete, marking as finished\n");
        seq->completed = true;
        return false;  // Tell audio driver to stop calling us
    }

    return true;  // Continue playback
}

void cleanup_song(sequencer_state_t *seq) {
    if (!seq) return;

    for (size_t i = 0; i < seq->num_events; i++) {
        free(seq->events[i]);
    }
    free(seq->events);
    free(seq);
}