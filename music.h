#ifndef MUSIC_H
#define MUSIC_H

#include <stdbool.h>
#include <stdint.h>

// Note representation
typedef struct {
    char note_name; // 'c', 'd', 'e', 'f', 'g', 'a', 'b', 'r' (lowercase)
    int8_t accidental; // -1 for flat(f), 0 for natural, +1 for sharp(s)
    int8_t octave_shift; // relative to reference: 0, +1, -1, +2, etc.
    int duration; // 1, 2, 4, 8, 16, etc. (note value)
} note_t;

// Temperament system
typedef struct {
    const char* name;
    double (*note_to_freq)(const note_t* note, const note_t* reference, double reference_freq);
} temperament_t;

// Frequency conversion
double note_to_frequency(const note_t* note, const note_t* reference, double reference_freq, const temperament_t* temperament);
double equal_temperament_freq(const note_t* note, const note_t* reference, double reference_freq);

// Standard temperaments
extern const temperament_t equal_temperament;

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
note_array_t parse_string(const char* input);
void free_note_array(note_array_t* array);

// Utility functions
bool is_valid_note_name(char c);
bool is_rest(const note_t* note);
void print_note(const note_t* note);
void print_note_array(const note_array_t* array);

// Instruments
extern const instrument_t pluck_sine_instrument;

// Test function
void test_parser(void);
void test_frequencies(void);

#endif // MUSIC_H
