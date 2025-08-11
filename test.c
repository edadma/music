#include "test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "instrument.h"
#include "parser.h"

// Convert frequency to phase increment (unsigned for DDS)
static uint32_t freq_to_phase_increment(double freq, uint32_t sample_rate) {
    return (uint32_t)((freq / sample_rate) * 0x100000000LL);
}

// Helper function to check if two notes should be simultaneous (part of same chord)
static bool notes_are_simultaneous(const note_t* note1, const note_t* note2) {
    return (note1->chord_id > 0 && note1->chord_id == note2->chord_id);
}

// Convert parsed notes to sequencer events with proper fixed-point arithmetic
event_array_t notes_to_sequencer_events(const note_array_t* notes, uint32_t sample_rate, int tempo_bpm,
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
                event_t* event = malloc(sizeof(event_t) + sizeof(partial_t));
                if (!event)
                    continue;

                // Set up basic event parameters
                event->start_sample = current_sample;
                event->duration_samples = (uint32_t)(duration_samples * 0.7); // 70% of written duration
                event->release_sample = current_sample + event->duration_samples;
                event->num_partials = 1;
                event->instrument = note->instrument;

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
                event->volume_scale = (int32_t)(event_volume * 0x10000000); // Convert to Q1.31

                // Setup single partial (fundamental frequency)
                event->partials[0].phase_accum = 0;
                event->partials[0].phase_increment = freq_to_phase_increment(freq, sample_rate);
                event->partials[0].amplitude = 0x7FFFFFFF; // Full amplitude for this partial

                // Setup ADSR envelope (keyboard-like with anti-click release)
                event->envelope_state.adsr.attack_samples = (uint32_t)(sample_rate * 0.05f); // 50ms attack
                event->envelope_state.adsr.decay_samples = (uint32_t)(sample_rate * 0.2f); // 200ms decay
                event->envelope_state.adsr.sustain_level = (int32_t)(0.6f * 0x7FFFFFFF); // 60% sustain
                event->envelope_state.adsr.release_samples = (uint32_t)(sample_rate * 0.5f); // 500ms release
                event->envelope_state.adsr.min_release_samples = (uint32_t)(sample_rate * 0.02f); // 20ms minimum
                event->envelope_state.adsr.current_level = AUDIBLE_THRESHOLD;
                event->envelope_state.adsr.release_start_level = 0;
                event->envelope_state.adsr.release_coeff = 0;
                event->envelope_state.adsr.phase = ADSR_ATTACK;

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

// Create simple melody test
sequencer_state_t* create_simple_melody_test(uint32_t sample_rate) {
    printf("Creating simple melody test...\n");

    // Parse a simple C major scale
    note_array_t notes = parse_music("c4 d e f g a b c'2");

    // Convert to events
    event_array_t events = notes_to_sequencer_events(&notes, sample_rate, 120, &c_major, &equal_temperament, 0, 0.3f);

    // Create sequencer state
    sequencer_state_t* seq = calloc(1, sizeof(sequencer_state_t));
    seq->events = events; // Direct assignment of array struct
    seq->sample_rate = sample_rate;
    seq->current_sample_index = 0;
    seq->next_event_index = 0;
    seq->num_active = 0;
    seq->completed = false;

    // Calculate total duration
    uint64_t max_end = 0;
    for (int i = 0; i < events.count; i++) {
        event_t* event = events.data[i];
        uint64_t event_end = event->start_sample + event->duration_samples + event->envelope_state.adsr.release_samples;
        if (event_end > max_end) {
            max_end = event_end;
        }
    }
    seq->total_duration_samples = max_end;

    free_note_array(&notes);
    printf("Simple melody: %d events, %.2f seconds\n", events.count, max_end / (float)sample_rate);

    return seq;
}

// Create chord progression test
sequencer_state_t* create_chord_test(uint32_t sample_rate) {
    printf("Creating chord test...\n");

    // Parse chord progression: C major -> F major -> G major -> C major
    note_array_t notes = parse_music("<c e g>2 <f a c'>2 <g b d'>2 <c e g c'>1");

    event_array_t events = notes_to_sequencer_events(&notes, sample_rate, 100, &c_major, &equal_temperament, 0, 0.4f);

    sequencer_state_t* seq = calloc(1, sizeof(sequencer_state_t));
    seq->events = events;
    seq->sample_rate = sample_rate;
    seq->current_sample_index = 0;
    seq->next_event_index = 0;
    seq->num_active = 0;
    seq->completed = false;

    // Calculate total duration
    uint64_t max_end = 0;
    for (int i = 0; i < events.count; i++) {
        event_t* event = events.data[i];
        uint64_t event_end = event->start_sample + event->duration_samples + event->envelope_state.adsr.release_samples;
        if (event_end > max_end) {
            max_end = event_end;
        }
    }
    seq->total_duration_samples = max_end;

    free_note_array(&notes);
    printf("Chord test: %d events, %.2f seconds\n", events.count, max_end / (float)sample_rate);

    return seq;
}

// Create multi-voice test (simple canon)
sequencer_state_t* create_multi_voice_test(uint32_t sample_rate) {
    printf("Creating multi-voice test...\n");

    // Voice 1: Lead melody
    note_array_t voice1 = parse_music("c4 d e f g a g f e f d e c2");

    // Voice 2: Same melody delayed by 2 beats (half note = 2 quarter note beats)
    note_array_t voice2 = parse_music("r2 c4 d e f g a g f e f d e c2");

    // Convert both voices
    event_array_t events1 =
        notes_to_sequencer_events(&voice1, sample_rate, 140, &c_major, &equal_temperament, 0, 0.25f);
    event_array_t events2 =
        notes_to_sequencer_events(&voice2, sample_rate, 140, &c_major, &equal_temperament, 0, 0.25f);

    // Merge events (we'll need to implement this)
    event_array_t merged_events = {0};

    // Add all events from both voices
    for (int i = 0; i < events1.count; i++) {
        event_array_push(&merged_events, events1.data[i]);
    }
    for (int i = 0; i < events2.count; i++) {
        event_array_push(&merged_events, events2.data[i]);
    }

    // Sort by start time (simple bubble sort for now)
    for (int i = 0; i < merged_events.count - 1; i++) {
        for (int j = 0; j < merged_events.count - i - 1; j++) {
            if (merged_events.data[j]->start_sample > merged_events.data[j + 1]->start_sample) {
                event_t* temp = merged_events.data[j];
                merged_events.data[j] = merged_events.data[j + 1];
                merged_events.data[j + 1] = temp;
            }
        }
    }

    sequencer_state_t* seq = calloc(1, sizeof(sequencer_state_t));
    seq->events = merged_events;
    seq->sample_rate = sample_rate;
    seq->current_sample_index = 0;
    seq->next_event_index = 0;
    seq->num_active = 0;
    seq->completed = false;

    // Calculate total duration
    uint64_t max_end = 0;
    for (int i = 0; i < merged_events.count; i++) {
        event_t* event = merged_events.data[i];
        uint64_t event_end = event->start_sample + event->duration_samples + event->envelope_state.adsr.release_samples;
        if (event_end > max_end) {
            max_end = event_end;
        }
    }
    seq->total_duration_samples = max_end;

    // Cleanup
    free_note_array(&voice1);
    free_note_array(&voice2);
    event_array_free(&events1);
    event_array_free(&events2);

    printf("Multi-voice test: %d events, %.2f seconds\n", merged_events.count, max_end / (float)sample_rate);

    return seq;
}

// Create complex test with various features
sequencer_state_t* create_complex_test(uint32_t sample_rate) {
    printf("Creating complex test...\n");

    // Complex piece with chords, different rhythms, and instrument changes
    note_array_t notes = parse_music("[pluck sine] c4 d e f <g c' e'>2 " // Melody with chord ending
                                     "[pluck square] r2 c4 d e f g2 " // Second voice with different instrument
                                     "c,1 g,,1 <c,, e,, g,, c,>1" // Bass line with final chord
    );

    event_array_t events = notes_to_sequencer_events(&notes, sample_rate, 120, &c_major, &equal_temperament, 0, 0.3f);

    sequencer_state_t* seq = calloc(1, sizeof(sequencer_state_t));
    seq->events = events;
    seq->sample_rate = sample_rate;
    seq->current_sample_index = 0;
    seq->next_event_index = 0;
    seq->num_active = 0;
    seq->completed = false;

    // Calculate total duration
    uint64_t max_end = 0;
    for (int i = 0; i < events.count; i++) {
        event_t* event = events.data[i];
        uint64_t event_end = event->start_sample + event->duration_samples + event->envelope_state.adsr.release_samples;
        if (event_end > max_end) {
            max_end = event_end;
        }
    }
    seq->total_duration_samples = max_end;

    free_note_array(&notes);
    printf("Complex test: %d events, %.2f seconds\n", events.count, max_end / (float)sample_rate);

    return seq;
}

// Cleanup function
void cleanup_sequencer_state(sequencer_state_t* seq) {
    if (!seq)
        return;

    // Free all individual event structures
    for (int i = 0; i < seq->events.count; i++) {
        free(seq->events.data[i]);
    }

    // Free the events array
    event_array_free(&seq->events);

    // Free the sequencer state itself
    free(seq);
}
