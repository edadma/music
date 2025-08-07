#ifndef MUSIC_H
#define MUSIC_H

#include <stdbool.h>
#include <stdint.h>

// Note representation
typedef struct {
    char note_name;        // 'c', 'd', 'e', 'f', 'g', 'a', 'b', 'r' (lowercase)
    int8_t accidental;     // -1 for flat(f), 0 for natural, +1 for sharp(s)
    int8_t octave_shift;   // relative to reference: 0, +1, -1, +2, etc.
    int duration;          // 1, 2, 4, 8, 16, etc. (note value)
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
typedef struct {
    float freq;            // frequency in Hz
    int32_t start;         // start time in milliseconds
    int32_t duration;      // duration in milliseconds
    float volume;          // 0.0 to 1.0
} sequencer_event_t;

// Dynamic array for notes
typedef struct {
    note_t* notes;
    int count;
    int capacity;
} note_array_t;

// Parser functions
note_t parse_note(const char** input_pos, int* last_duration);
note_array_t parse_string(const char* input);
void free_note_array(note_array_t* array);

// Utility functions
bool is_valid_note_name(char c);
bool is_rest(const note_t* note);
void print_note(const note_t* note);
void print_note_array(const note_array_t* array);

// Test function
void test_parser(void);
void test_frequencies(void);

#endif // MUSIC_H