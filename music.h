#ifndef MUSIC_H
#define MUSIC_H

#include <stdbool.h>
#include <stdint.h>

// Note representation
typedef struct {
    char note_name; // 'c', 'd', 'e', 'f', 'g', 'a', 'b', 'r' (lowercase)
    int8_t accidental; // -1 for flat(f), 0 for natural, +1 for sharp(s)
    int8_t octave_shift; // relative to reference: 0, +1, -1, +2, etc.
    int value; // 1, 2, 4, 8, 16, etc. (note value)
    bool dotted; // true if this is a dotted note (1.5x duration)
    int8_t tuplet; // 0 = normal, 3 = triplet, 5 = quintuplet, 6 = sextuplet, 7 = septuplet
    int16_t chord_id; // 0 = single note, >0 = chord identifier (same ID = same chord)
} note_t;

// Temperament system
typedef struct {
    const char* name;
    double (*note_to_freq)(const note_t* note);
} temperament_t;

// Forward declare for the function pointer
struct sequencer_event;

// Instrument
typedef struct {
    const char* name;
    float (*event_to_samples)(const struct sequencer_event* event, int sample_index, int sample_rate);
} instrument_t;

typedef struct sequencer_event {
    double frequency;
    int start_sample;
    int duration_samples;
    float volume;
    instrument_t instrument;
} sequencer_event_t;

// Chord volume adjustment function type
typedef void (*chord_volume_fn_t)(sequencer_event_t* events, int start_index, int chord_size);

// Dynamic array for notes
typedef struct {
    note_t* notes;
    int count;
    int capacity;
} note_array_t;

typedef struct {
    const char* name;
    void* (*init)(int rate, int channels, int* error);
    int (*play)(void* context, float* samples, int sample_count);
    void (*cleanup)(void* context);
    const char* (*strerror)(int error);
} audio_driver_t;

// Parser functions
note_t parse_note(const char** input_pos, int* last_duration);
note_t parse_note_without_duration(const char** input_pos);
void parse_duration_and_modifiers(const char** input_pos, int* last_duration, note_t* notes, int note_count);
note_t* parse_chord(const char** input_pos, int* chord_size, int* last_duration);
note_array_t parse_music(const char* input);
void free_note_array(note_array_t* array);

// Utility functions
bool is_valid_note_name(char c);
bool is_rest(const note_t* note);
bool is_dotted(const note_t* note);
void print_note(const note_t* note);
void print_note_array(const note_array_t* array);

// Frequency conversion
double note_to_frequency(const note_t* note, const temperament_t* temperament);
double equal_temperament_freq(const note_t* note);

// Standard temperaments
extern const temperament_t equal_temperament;

// Sequencer
sequencer_event_t* notes_to_events(const note_array_t* notes, int tempo_bpm, int sample_rate, const temperament_t* temperament,
                                   const instrument_t* instrument, float volume, chord_volume_fn_t chord_volume_fn);
void adjust_chord_volumes(sequencer_event_t* events, int event_count, chord_volume_fn_t volume_fn);
void generate_samples(sequencer_event_t* events, int event_count, float* output_buffer, int buffer_size, int sample_rate);
void play_sequence(sequencer_event_t* events, int event_count, audio_driver_t* driver, int sample_rate);

// Standard chord volume adjustment functions
void no_chord_adjustment(sequencer_event_t* events, int start_index, int chord_size);
void linear_chord_adjustment(sequencer_event_t* events, int start_index, int chord_size);
void sqrt_chord_adjustment(sequencer_event_t* events, int start_index, int chord_size);
void bass_boost_adjustment(sequencer_event_t* events, int start_index, int chord_size);

// Instruments
extern const instrument_t pluck_sine_instrument;

// High-level playback functions
void play(const char* name, int tempo_bpm, const audio_driver_t* driver, ...);
sequencer_event_t* merge_event_arrays(sequencer_event_t* events1, int count1, sequencer_event_t* events2, int count2,
                                      int* total_count);

// Test function
void test_parser(void);
void test_frequencies(void);
void test_chords(const audio_driver_t* driver);
void test_play_melody(const char* song_name, const char* melody, int tempo_bpm, const audio_driver_t* driver);
void test_twinkle_twinkle(const audio_driver_t* driver);
void test_mary_had_a_little_lamb(const audio_driver_t* driver);
void test_row_row_row(const audio_driver_t* driver);
void test_crow_song(const audio_driver_t* driver);
void test_triplets(const audio_driver_t* driver);

#endif // MUSIC_H
