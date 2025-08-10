#include "test.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// Convert frequency to phase increment (unsigned for DDS)
static uint32_t freq_to_phase_increment(float freq, uint32_t sample_rate) {
    return (uint32_t)((freq / sample_rate) * 0x100000000LL);
}

// Global ADSR instrument for keyboard-like sounds
static instrument_t adsr_instrument = {
    .envelope = adsr_envelope,
    .num_partials = 1,
    .harmonic_ratios = {1.0f},
    .partial_amplitudes = {1.0f}
};

event_t* create_simple_event(uint32_t start_sample, float freq, float duration_sec, uint32_t sample_rate) {
    // Allocate event with one partial
    event_t *event = malloc(sizeof(event_t) + sizeof(partial_t));
    if (!event) return NULL;

    event->start_sample = start_sample;
    event->duration_samples = (uint32_t)(duration_sec * sample_rate);
    event->release_sample = start_sample + event->duration_samples;
    event->volume_scale = 0x10000000;  // About 1/8 volume in Q1.31
    event->num_partials = 1;
    event->instrument = &adsr_instrument;

    // Setup single partial (fundamental frequency)
    event->partials[0].phase_accum = 0;
    event->partials[0].phase_increment = freq_to_phase_increment(freq, sample_rate);
    event->partials[0].amplitude = 0x7FFFFFFF;  // Full amplitude for this partial

    // Setup ADSR envelope (keyboard-like)
    event->envelope_state.adsr.attack_samples = sample_rate * 0.05f;   // 50ms attack
    event->envelope_state.adsr.decay_samples = sample_rate * 0.2f;     // 200ms decay
    event->envelope_state.adsr.sustain_level = (int32_t)(0.6f * 0x7FFFFFFF);  // 60% sustain
    event->envelope_state.adsr.release_samples = sample_rate * 0.5f;   // 500ms release
    event->envelope_state.adsr.current_level = AUDIBLE_THRESHOLD;  // Start at audible threshold
    event->envelope_state.adsr.phase = ADSR_ATTACK;  // Start in attack phase

    return event;
}

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
    seq->total_duration_samples = sample_rate * 5.5f + sample_rate * 2.0f + sample_rate * 1.0f; // Less extra time since ADSR releases faster

    printf("Created ADSR test song with %zu events\n", seq->num_events);
    printf("Event 0: C4 at sample 0 (attack=50ms, decay=200ms, sustain=60%%, release=500ms)\n");
    printf("Event 1: E4 at sample %u\n", (uint32_t)(sample_rate * 1.5f));
    printf("Event 2: G4 at sample %u\n", (uint32_t)(sample_rate * 3.0f));
    printf("Event 3: C5 at sample %u\n", (uint32_t)(sample_rate * 5.5f));
    printf("Expected: Keyboard-like notes with attack/decay/sustain/release\n");
    printf("Total duration: %lu samples (%.1f seconds)\n",
           seq->total_duration_samples, seq->total_duration_samples / (float)sample_rate);

    return seq;
}