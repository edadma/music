#ifndef TEST_H
#define TEST_H

#include "sequencer.h"

// Create test songs using the parser and demonstrate various features
sequencer_state_t* create_simple_melody_test(uint32_t sample_rate);
sequencer_state_t* create_chord_test(uint32_t sample_rate);
sequencer_state_t* create_multi_voice_test(uint32_t sample_rate);
sequencer_state_t* create_complex_test(uint32_t sample_rate);

#endif // TEST_H
