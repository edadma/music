#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <stdint.h>

#include "instrument.h"

// ============================================================================
// DYNAMIC ARRAY SYSTEM
// ============================================================================

#define DEFINE_ARRAY_TYPE(type_name, element_type)                                                                     \
    typedef struct {                                                                                                   \
        element_type* data;                                                                                            \
        int count;                                                                                                     \
        int capacity;                                                                                                  \
    } type_name##_array_t;

#define DEFINE_ARRAY_FUNCTIONS(type_name, element_type)                                                                \
    static inline void type_name##_array_init(type_name##_array_t* arr) {                                              \
        arr->data = NULL;                                                                                              \
        arr->count = 0;                                                                                                \
        arr->capacity = 0;                                                                                             \
    }                                                                                                                  \
                                                                                                                       \
    static inline void type_name##_array_free(type_name##_array_t* arr) {                                              \
        free(arr->data);                                                                                               \
        arr->data = NULL;                                                                                              \
        arr->count = 0;                                                                                                \
        arr->capacity = 0;                                                                                             \
    }                                                                                                                  \
                                                                                                                       \
    static inline int type_name##_array_push(type_name##_array_t* arr, element_type item) {                            \
        if (arr->count >= arr->capacity) {                                                                             \
            arr->capacity = arr->capacity ? arr->capacity * 2 : 16;                                                    \
            element_type* new_data = realloc(arr->data, arr->capacity * sizeof(element_type));                         \
            if (!new_data)                                                                                             \
                return 0;                                                                                              \
            arr->data = new_data;                                                                                      \
        }                                                                                                              \
        arr->data[arr->count++] = item;                                                                                \
        return 1;                                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    static inline void type_name##_array_clear(type_name##_array_t* arr) { arr->count = 0; }                           \
                                                                                                                       \
    static inline int type_name##_array_shrink_to_fit(type_name##_array_t* arr) {                                      \
        if (!arr || !arr->data || arr->count == 0) {                                                                   \
            return 1; /* Nothing to do or already optimal */                                                           \
        }                                                                                                              \
                                                                                                                       \
        if (arr->capacity == arr->count) {                                                                             \
            return 1; /* Already optimal */                                                                            \
        }                                                                                                              \
                                                                                                                       \
        element_type* new_data = realloc(arr->data, arr->count * sizeof(element_type));                                \
        if (!new_data) {                                                                                               \
            return 0; /* Realloc failed, but original data is still valid */                                           \
        }                                                                                                              \
                                                                                                                       \
        arr->data = new_data;                                                                                          \
        arr->capacity = arr->count;                                                                                    \
        return 1;                                                                                                      \
    }

// ============================================================================
// CORE MUSIC TYPES
// ============================================================================

// Forward declaration for instrument
// typedef struct instrument instrument_t;

// Note representation
typedef struct {
    char note_name; // 'c', 'd', 'e', 'f', 'g', 'a', 'b', 'r' (lowercase)
    int8_t accidental; // -1 for flat(f), 0 for natural, +1 for sharp(s)
    int8_t octave_shift; // relative to reference: 0, +1, -1, +2, etc.
    int8_t value; // 1, 2, 4, 8, 16, etc. (note value)
    bool dotted; // true if this is a dotted note (1.5x duration)
    int8_t tuplet; // 0 = normal, 3 = triplet, 5 = quintuplet, etc.
    int16_t chord_id; // 0 = single note, >0 = chord identifier
    const instrument_t* instrument; // Instrument assignment from parsing
} note_t;

// Key signature definition
typedef struct {
    const char* name;
    int accidentals[7]; // Accidentals for C, D, E, F, G, A, B (-1=flat, 0=natural, +1=sharp)
} key_signature_t;

// Temperament system
typedef struct {
    const char* name;
    double (*compute_frequency)(int absolute_semitone);
} temperament_t;

// Define note array type
DEFINE_ARRAY_TYPE(note, note_t)

// ============================================================================
// STANDARD DEFINITIONS
// ============================================================================

// Standard key signatures
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
extern const key_signature_t a_minor;
extern const key_signature_t e_minor;
extern const key_signature_t b_minor;
extern const key_signature_t fs_minor;
extern const key_signature_t cs_minor;
extern const key_signature_t gs_minor;
extern const key_signature_t ds_minor;
extern const key_signature_t as_minor;
extern const key_signature_t d_minor;
extern const key_signature_t g_minor;
extern const key_signature_t c_minor;
extern const key_signature_t f_minor;
extern const key_signature_t bf_minor;
extern const key_signature_t ef_minor;
extern const key_signature_t af_minor;

// Standard temperaments
extern const temperament_t equal_temperament;
extern const temperament_t werckmeister3_temperament;

// ============================================================================
// PARSER FUNCTIONS
// ============================================================================

// Core parsing functions
note_t parse_note(const char** input_pos, int* last_duration);
note_t parse_note_without_duration(const char** input_pos);
void parse_duration_and_modifiers(const char** input_pos, int* last_duration, note_t* notes, int note_count);
note_t* parse_chord(const char** input_pos, int* chord_size, int* last_duration);
note_array_t parse_music(const char* input);

// Note utility functions
bool is_valid_note_name(char c);
bool is_rest(const note_t* note);
bool is_dotted(const note_t* note);
bool is_tuplet(const note_t* note);
float get_tuplet_ratio(int tuplet);

// Music theory functions
int note_name_to_semitone(char note_name);
int calculate_semitone(const note_t* note);
int note_to_absolute_semitone(const note_t* note, const key_signature_t* key, int transposition);
double note_to_frequency(const note_t* note, const temperament_t* temperament, const key_signature_t* key,
                         int transposition);
int get_key_accidental(char note_name, const key_signature_t* key);
int get_key_tonic_semitone(const key_signature_t* key);
int calculate_key_transposition(const key_signature_t* from_key, const key_signature_t* to_key);

// Temperament implementations
double equal_temperament_freq(int absolute_semitone);
double werckmeister3_freq(int absolute_semitone);

// Instrument lookup
const instrument_t* lookup_instrument(const char* name);

// Debug/print functions
void print_note(const note_t* note);
void print_note_array(const note_array_t* array);

// Memory management
void free_note_array(note_array_t* array);

#endif // PARSER_H
