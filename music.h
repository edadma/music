#ifndef MUSIC_H
#define MUSIC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct instrument instrument_t;

// Note representation
typedef struct {
    char note_name; // 'c', 'd', 'e', 'f', 'g', 'a', 'b', 'r' (lowercase)
    int8_t accidental; // -1 for flat(f), 0 for natural, +1 for sharp(s)
    int8_t octave_shift; // relative to reference: 0, +1, -1, +2, etc.
    int value; // 1, 2, 4, 8, 16, etc. (note value)
    bool dotted; // true if this is a dotted note (1.5x duration)
    int8_t tuplet; // 0 = normal, 3 = triplet, 5 = quintuplet, 6 = sextuplet, 7 = septuplet
    int16_t chord_id; // 0 = single note, >0 = chord identifier (same ID = same chord)
    const instrument_t* instrument; // <- New field
} note_t;

// Temperament system
typedef struct {
    const char* name;
    double (*note_to_freq)(const note_t* note);
} temperament_t;

// Forward declare for the function pointer
typedef struct sequencer_event sequencer_event_t;

// Waveform function type - generates basic waveform (0.0 to 1.0 range)
typedef float (*waveform_fn_t)(const sequencer_event_t* event, int sample_index, int sample_rate);

// Envelope function type - shapes amplitude over time (0.0 to 1.0 range)
typedef float (*envelope_fn_t)(const sequencer_event_t* event, int sample_index, int sample_rate);

// Instrument
typedef struct instrument {
    const char* name;
    waveform_fn_t waveform;
    envelope_fn_t envelope;
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

typedef struct {
    float frequency_ratio; // 1.0=fundamental, 3.0=3rd harmonic, etc.
    float amplitude; // relative amplitude
} harmonic_t;

typedef struct {
    const char* name;
    harmonic_t* harmonics;
    int harmonic_count;
    int max_musical_harmonics;
} timbre_t;

// Key signature definition
typedef struct {
    const char* name;
    int accidentals[7]; // Accidentals for C, D, E, F, G, A, B (-1=flat, 0=natural, +1=sharp)
} key_signature_t;

// Standard key signatures (add these declarations to music.h)
extern const key_signature_t c_major;
extern const key_signature_t g_major;
extern const key_signature_t d_major;
extern const key_signature_t a_major;
extern const key_signature_t e_major;
extern const key_signature_t b_major;
extern const key_signature_t fs_major;
extern const key_signature_t cs_major;
extern const key_signature_t f_major;
extern const key_signature_t bf_major;
extern const key_signature_t ef_major;
extern const key_signature_t af_major;
extern const key_signature_t df_major;
extern const key_signature_t gf_major;
extern const key_signature_t cf_major;

// Minor keys (same accidentals as relative majors)
extern const key_signature_t a_minor; // Same as C major
extern const key_signature_t e_minor; // Same as G major
extern const key_signature_t b_minor; // Same as D major
extern const key_signature_t fs_minor; // Same as A major
extern const key_signature_t cs_minor; // Same as E major
extern const key_signature_t gs_minor; // Same as B major
extern const key_signature_t ds_minor; // Same as F# major
extern const key_signature_t as_minor; // Same as C# major
extern const key_signature_t d_minor; // Same as F major
extern const key_signature_t g_minor; // Same as Bb major
extern const key_signature_t c_minor; // Same as Eb major
extern const key_signature_t f_minor; // Same as Ab major
extern const key_signature_t bf_minor; // Same as Db major
extern const key_signature_t ef_minor; // Same as Gb major
extern const key_signature_t af_minor; // Same as Cb major

// Updated function signatures (add these to music.h)
note_t apply_key_signature(const note_t* note, const key_signature_t* key);

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
                                   const key_signature_t* key, float volume, chord_volume_fn_t chord_volume_fn);
void adjust_chord_volumes(sequencer_event_t* events, int event_count, chord_volume_fn_t volume_fn);
void generate_samples(sequencer_event_t* events, int event_count, float* output_buffer, int buffer_size, int sample_rate);
void play_sequence(sequencer_event_t* events, int event_count, audio_driver_t* driver, int sample_rate);

// Standard chord volume adjustment functions
void no_chord_adjustment(sequencer_event_t* events, int start_index, int chord_size);
void linear_chord_adjustment(sequencer_event_t* events, int start_index, int chord_size);
void sqrt_chord_adjustment(sequencer_event_t* events, int start_index, int chord_size);
void bass_boost_adjustment(sequencer_event_t* events, int start_index, int chord_size);

// Waveform generators
float sine_wave(const sequencer_event_t* event, int sample_index, int sample_rate);
float square_wave(const sequencer_event_t* event, int sample_index, int sample_rate);

// Envelope generators
float pluck_envelope(const sequencer_event_t* event, int sample_index, int sample_rate);

// Instruments
extern const instrument_t pluck_sine_instrument;
extern const instrument_t pluck_square_instrument;

const instrument_t* lookup_instrument(const char* name);

// High-level playback functions
void play(const char* name, int tempo_bpm, const key_signature_t* key, const audio_driver_t* driver, ...);
sequencer_event_t* merge_event_arrays(sequencer_event_t* events1, int count1, sequencer_event_t* events2, int count2,
                                      int* total_count);

// Test function
void test_parser(void);
void test_frequencies(void);
void test_chords(const audio_driver_t* driver);
void play_twinkle_twinkle(const audio_driver_t* driver);
void play_mary_had_a_little_lamb(const audio_driver_t* driver);
void play_row_row_row(const audio_driver_t* driver);
void play_triplets(const audio_driver_t* driver);

#endif // MUSIC_H
