#include "test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Convert frequency to phase increment (unsigned for DDS)
static uint32_t freq_to_phase_increment(float freq, uint32_t sample_rate)
{
    return (uint32_t)((freq / sample_rate) * 0x100000000LL);
}

// Global ADSR instrument for keyboard-like sounds
static instrument_t adsr_instrument = {
    .envelope = adsr_envelope, .num_partials = 1, .harmonic_ratios = {1.0f}, .partial_amplitudes = {1.0f}};

event_t* create_simple_event(uint32_t start_sample, float freq, float duration_sec, uint32_t sample_rate)
{
    // Allocate event with one partial
    event_t* event = malloc(sizeof(event_t) + sizeof(partial_t));
    if (!event)
        return NULL;


    event->start_sample = start_sample;
    event->duration_samples = (uint32_t)(duration_sec * 0.7 * sample_rate);
    event->release_sample = start_sample + event->duration_samples;
    event->volume_scale = 0x10000000; // About 1/8 volume in Q1.31
    event->num_partials = 1;
    event->instrument = &adsr_instrument;

    // Setup single partial (fundamental frequency)
    event->partials[0].phase_accum = 0;
    event->partials[0].phase_increment = freq_to_phase_increment(freq, sample_rate);
    event->partials[0].amplitude = 0x7FFFFFFF; // Full amplitude for this partial

    // Setup ADSR envelope (keyboard-like with anti-click release)
    event->envelope_state.adsr.attack_samples = (uint32_t)(sample_rate * 0.05f); // 50ms attack
    event->envelope_state.adsr.decay_samples = (uint32_t)(sample_rate * 0.2f); // 200ms decay
    event->envelope_state.adsr.sustain_level = (int32_t)(0.6f * 0x7FFFFFFF); // 60% sustain
    event->envelope_state.adsr.release_samples = (uint32_t)(sample_rate * 0.5f); // 500ms release
    event->envelope_state.adsr.min_release_samples = (uint32_t)(sample_rate * 0.02f); // 20ms minimum to prevent clicks
    event->envelope_state.adsr.current_level = AUDIBLE_THRESHOLD; // Start at audible threshold
    event->envelope_state.adsr.release_start_level = 0; // Will be set when release begins
    event->envelope_state.adsr.release_coeff = 0; // Will be calculated when release begins
    event->envelope_state.adsr.phase = ADSR_ATTACK; // Start in attack phase

    return event;
}

// Create simultaneous notes test song
sequencer_state_t* create_test_song(uint32_t sample_rate)
{
    sequencer_state_t* seq = calloc(1, sizeof(sequencer_state_t));
    seq->sample_rate = sample_rate;
    seq->current_sample_index = 0;
    seq->next_event_index = 0;
    seq->num_active = 0;
    seq->completed = false;

    // Create test with simultaneous and overlapping notes
    seq->num_events = 8;
    seq->events = malloc(seq->num_events * sizeof(event_t*));

    // === Test 1: Simple chord (C major triad) - simultaneous start ===
    // All three notes start at the same time
    seq->events[0] = create_simple_event(0, 261.63f, 2.0f, sample_rate); // C4
    seq->events[1] = create_simple_event(0, 329.63f, 2.0f, sample_rate); // E4
    seq->events[2] = create_simple_event(0, 392.00f, 2.0f, sample_rate); // G4

    // Adjust volume for chord (1/sqrt(3) ≈ 0.577)
    int32_t chord_volume = (int32_t)(0.577f * 0x10000000); // Scaled down from normal volume
    seq->events[0]->volume_scale = chord_volume;
    seq->events[1]->volume_scale = chord_volume;
    seq->events[2]->volume_scale = chord_volume;

    // === Test 2: Overlapping melody notes - staggered start ===
    uint32_t melody_start = sample_rate * 3.0f; // Start after chord

    // First melody note: A4 - starts at 3s, lasts 1.5s
    seq->events[3] = create_simple_event(melody_start, 440.0f, 1.5f, sample_rate);

    // Second melody note: F4 - starts at 4s (overlaps with A4), lasts 1.5s
    seq->events[4] = create_simple_event(melody_start + sample_rate * 1.0f, 349.23f, 1.5f, sample_rate);

    // Third melody note: D4 - starts at 5s (overlaps with F4), lasts 1.5s
    seq->events[5] = create_simple_event(melody_start + sample_rate * 2.0f, 293.66f, 1.5f, sample_rate);

    // === Test 3: Final chord (F major triad) - simultaneous start ===
    uint32_t final_chord_start = sample_rate * 7.0f;
    seq->events[6] = create_simple_event(final_chord_start, 349.23f, 2.0f, sample_rate); // F4
    seq->events[7] = create_simple_event(final_chord_start, 440.00f, 2.0f, sample_rate); // A4

    // Adjust volume for final chord (only 2 notes, so 1/sqrt(2) ≈ 0.707)
    int32_t final_chord_volume = (int32_t)(0.707f * 0x10000000);
    seq->events[6]->volume_scale = final_chord_volume;
    seq->events[7]->volume_scale = final_chord_volume;

    // Calculate total song duration
    seq->total_duration_samples = sample_rate * 10.0f; // 10 seconds total

    printf("Created simultaneous notes test song with %zu events\n", seq->num_events);
    printf("Test structure:\n");
    printf("  0.0s: C major chord (C+E+G) - 2.0s duration\n");
    printf("  3.0s: A4 melody note - 1.5s duration\n");
    printf("  4.0s: F4 melody note (overlaps A4) - 1.5s duration\n");
    printf("  5.0s: D4 melody note (overlaps F4) - 1.5s duration\n");
    printf("  7.0s: F major chord (F+A) - 2.0s duration\n");
    printf("Expected: Chord harmony, smooth overlapping melody, volume-balanced mixing\n");
    printf("Total duration: %lu samples (%.1f seconds)\n", seq->total_duration_samples,
           seq->total_duration_samples / (float)sample_rate);

    return seq;
}
