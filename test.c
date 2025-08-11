#include "test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "instrument.h"
#include "parser.h"

// Create simple melody test
sequencer_state_t* create_simple_melody_test(uint32_t sample_rate) {
    printf("Creating simple melody test...\n");

    // Parse a simple C major scale
    note_array_t notes = parse_music("c4 d e f g a b c'2");

    // Convert to events
    event_array_t events = sequence_events(&notes, sample_rate, 120, &c_major, &equal_temperament, 0, 0.9f);

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
        event_t* event = &events.data[i];
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
    note_array_t notes = parse_music("<c e g>2 <f a c'>2 <g b d'>2 <c' e' g' c''>1");

    event_array_t events = sequence_events(&notes, sample_rate, 100, &c_major, &equal_temperament, 0, 0.9f);

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
        event_t* event = &events.data[i];
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


// Comparison function for qsort
static int compare_events_by_start_time(const void* a, const void* b) {
    const event_t* event_a = (const event_t*)a;
    const event_t* event_b = (const event_t*)b;

    if (event_a->start_sample < event_b->start_sample)
        return -1;
    if (event_a->start_sample > event_b->start_sample)
        return 1;
    return 0;
}


// Create multi-voice test (simple canon)
sequencer_state_t* create_multi_voice_test(uint32_t sample_rate) {
    printf("Creating multi-voice test...\n");

    // Voice 1: Lead melody
    note_array_t voice1 = parse_music("c4 d e f g a g f e f d e c2");

    // Voice 2: Same melody delayed by 2 beats (half note = 2 quarter note beats)
    note_array_t voice2 = parse_music("r2 c4 d e f g a g f e f d e c2");

    // Convert both voices
    event_array_t events1 = sequence_events(&voice1, sample_rate, 140, &c_major, &equal_temperament, 0, 0.4f);
    event_array_t events2 = sequence_events(&voice2, sample_rate, 140, &c_major, &equal_temperament, 0, 0.4f);

    // Merge events (we'll need to implement this)
    event_array_t merged_events = {0};

    // Add all events from both voices
    for (int i = 0; i < events1.count; i++) {
        event_array_push(&merged_events, events1.data[i]);
    }
    for (int i = 0; i < events2.count; i++) {
        event_array_push(&merged_events, events2.data[i]);
    }

    // Sort by start time
    qsort(merged_events.data, merged_events.count, sizeof(event_t), compare_events_by_start_time);

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
        event_t* event = &merged_events.data[i];
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
    note_array_t notes = parse_music( //"[pluck sine] c4 d e f <g c' e'>2 " // Melody with chord ending
        "r2 c4 d e f g2 " // Second voice with different instrument
        // "c,1 g,,1 <c,, e,, g,, c,>1" // Bass line with final chord
    );

    event_array_t events = sequence_events(&notes, sample_rate, 120, &c_major, &equal_temperament, 0, 0.3f);

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
        event_t* event = &events.data[i];
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
