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

// Convert frequency to phase increment (unsigned for DDS)
static uint32_t freq_to_phase_increment(double freq, uint16_t sample_rate) {
    return (uint32_t)((freq / sample_rate) * 0x100000000LL);
}

// Helper function to check if two notes should be simultaneous (part of same chord)
static bool notes_are_simultaneous(const note_t* note1, const note_t* note2) {
    return (note1->chord_id > 0 && note1->chord_id == note2->chord_id);
}

// Convert parsed notes to sequencer events with proper fixed-point arithmetic
event_array_t sequence_events(const note_array_t* notes, uint16_t sample_rate, int tempo_bpm,
                              const key_signature_t* key, const temperament_t* temperament, int transposition,
                              float volume) {
    event_array_t events = {0};
    if (!notes || !notes->data || notes->count == 0) {
        return events;
    }

    // Calculate timing
    int samples_per_beat = (60 * sample_rate) / tempo_bpm;
    uint64_t current_sample = 0;

    // Calculate chord volume scaling
    float base_volume = volume;

    for (int i = 0; i < notes->count; i++) {
        const note_t* note = &notes->data[i];

        // Calculate duration in samples
        int duration_samples = (samples_per_beat * 4) / note->value;
        if (note->dotted) {
            duration_samples = (duration_samples * 3) / 2;
        }
        if (note->tuplet > 0) {
            float tuplet_ratio = get_tuplet_ratio(note->tuplet);
            duration_samples = (int)((float)duration_samples * tuplet_ratio);
        }

        // Only create events for non-rests
        if (!is_rest(note)) {
            // Calculate frequency
            double freq = note_to_frequency(note, temperament, key, transposition);

            if (freq > 0.0) {
                // Allocate event with one partial (can be extended for additive synthesis later)
                event_t event = {0};

                // Set up basic event parameters
                event.start_sample = current_sample;
                event.duration_samples = (uint32_t)(duration_samples * 0.9); // 90% of written duration
                event.release_sample = current_sample + event.duration_samples;
                event.num_partials = 1;
                event.instrument = note->instrument;

                // Volume scaling for chords
                float event_volume = base_volume;
                if (note->chord_id > 0) {
                    // Count chord size for volume scaling
                    int chord_size = 1;
                    for (int j = 0; j < notes->count; j++) {
                        if (j != i && notes->data[j].chord_id == note->chord_id) {
                            chord_size++;
                        }
                    }
                    event_volume = base_volume / sqrtf((float)chord_size);
                }
                event.volume_scale = (int32_t)(event_volume * 0x10000000); // Convert to Q1.31

                // Setup single partial (fundamental frequency)
                event.partials[0].phase_accum = 0;
                event.partials[0].phase_increment = freq_to_phase_increment(freq, sample_rate);
                event.partials[0].amplitude = 0x7FFFFFFF; // Full amplitude for this partial

                // Setup ADSR envelope (keyboard-like with anti-click release)
                event.envelope_state.adsr.attack_samples = (uint32_t)(sample_rate * 0.05f); // 50ms attack
                event.envelope_state.adsr.decay_samples = (uint32_t)(sample_rate * 0.2f); // 200ms decay
                event.envelope_state.adsr.sustain_level = (int32_t)(0.6f * 0x7FFFFFFF); // 60% sustain
                event.envelope_state.adsr.release_samples = (uint32_t)(sample_rate * 0.5f); // 500ms release
                event.envelope_state.adsr.min_release_samples = (uint32_t)(sample_rate * 0.02f); // 20ms minimum
                event.envelope_state.adsr.current_level = AUDIBLE_THRESHOLD;
                event.envelope_state.adsr.release_start_level = 0;
                event.envelope_state.adsr.release_coeff = 0;
                event.envelope_state.adsr.phase = ADSR_ATTACK;

                // Add to events array (we'll need to implement this push function)
                event_array_push(&events, event);
            }
        }

        // Advance time logic - only if not part of a simultaneous chord
        bool advance_time = true;
        if (i + 1 < notes->count) {
            advance_time = !notes_are_simultaneous(note, &notes->data[i + 1]);
        }
        if (is_rest(note)) {
            advance_time = true;
        }
        if (advance_time) {
            current_sample += duration_samples;
        }
    }

    // Shrink array to fit for memory efficiency
    event_array_shrink_to_fit(&events);

    printf("Converted %d notes to %d events\n", notes->count, events.count);
    return events;
}

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

void cleanup_sequencer_state(sequencer_state_t* seq) {
    if (!seq)
        return;

    event_array_free(&seq->events);
    free(seq);
}
