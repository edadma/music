#ifndef TEST_H
#define TEST_H

#include "instrument.h"
#include "parser.h"
#include "sequencer.h"

// Create test songs using the parser and demonstrate various features
sequencer_state_t* create_simple_melody_test(uint32_t sample_rate);
sequencer_state_t* create_chord_test(uint32_t sample_rate);
sequencer_state_t* create_multi_voice_test(uint32_t sample_rate);
sequencer_state_t* create_complex_test(uint32_t sample_rate);

// Helper function to convert parsed notes to sequencer events
event_array_t notes_to_sequencer_events(const note_array_t* notes, uint32_t sample_rate, int tempo_bpm,
                                        const key_signature_t* key, const temperament_t* temperament, int transposition,
                                        float volume);

// Cleanup function for sequencer state
void cleanup_sequencer_state(sequencer_state_t* seq);

#endif // TEST_H
