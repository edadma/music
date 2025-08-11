#ifndef TEST_H
#define TEST_H

#include "sequencer.h"

// Create a simple test event with ADSR envelope
event_t* create_simple_event(uint32_t start_sample, float freq, float duration_sec, uint32_t sample_rate);

// Create simultaneous notes test song (chords and overlapping melody)
sequencer_state_t* create_test_song(uint32_t sample_rate);

#endif // TEST_H
