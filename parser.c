#include "parser.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"
#include "instrument.h"

// ============================================================================
// DYNAMIC ARRAY IMPLEMENTATIONS
// ============================================================================

DEFINE_ARRAY_FUNCTIONS(note, note_t)

// ============================================================================
// KEY SIGNATURE DEFINITIONS
// ============================================================================

const key_signature_t c_major = {"C major", {0, 0, 0, 0, 0, 0, 0}};
const key_signature_t g_major = {"G major", {0, 0, 0, 1, 0, 0, 0}}; // F#
const key_signature_t d_major = {"D major", {1, 0, 0, 1, 0, 0, 0}}; // F#, C#
const key_signature_t a_major = {"A major", {1, 0, 0, 1, 1, 0, 0}}; // F#, C#, G#
const key_signature_t e_major = {"E major", {1, 1, 0, 1, 1, 0, 0}}; // F#, C#, G#, D#
const key_signature_t b_major = {"B major", {1, 1, 0, 1, 1, 1, 0}}; // F#, C#, G#, D#, A#
const key_signature_t fs_major = {"F# major", {1, 1, 1, 1, 1, 1, 0}}; // F#, C#, G#, D#, A#, E#
const key_signature_t cs_major = {"C# major", {1, 1, 1, 1, 1, 1, 1}}; // All sharp
const key_signature_t f_major = {"F major", {0, 0, 0, 0, 0, 0, -1}}; // Bb
const key_signature_t bf_major = {"Bb major", {0, 0, -1, 0, 0, 0, -1}}; // Bb, Eb
const key_signature_t ef_major = {"Eb major", {0, 0, -1, 0, 0, -1, -1}}; // Bb, Eb, Ab
const key_signature_t af_major = {"Ab major", {0, -1, -1, 0, 0, -1, -1}}; // Bb, Eb, Ab, Db
const key_signature_t df_major = {"Db major", {0, -1, -1, 0, -1, -1, -1}}; // Bb, Eb, Ab, Db, Gb
const key_signature_t gf_major = {"Gb major", {-1, -1, -1, 0, -1, -1, -1}}; // Bb, Eb, Ab, Db, Gb, Cb
const key_signature_t cf_major = {"Cb major", {-1, -1, -1, -1, -1, -1, -1}}; // All flat

// Minor keys (same accidentals as their relative majors)
const key_signature_t a_minor = {"A minor", {0, 0, 0, 0, 0, 0, 0}}; // Same as C major
const key_signature_t e_minor = {"E minor", {0, 0, 0, 1, 0, 0, 0}}; // Same as G major
const key_signature_t b_minor = {"B minor", {1, 0, 0, 1, 0, 0, 0}}; // Same as D major
const key_signature_t fs_minor = {"F# minor", {1, 0, 0, 1, 1, 0, 0}}; // Same as A major
const key_signature_t cs_minor = {"C# minor", {1, 1, 0, 1, 1, 0, 0}}; // Same as E major
const key_signature_t gs_minor = {"G# minor", {1, 1, 0, 1, 1, 1, 0}}; // Same as B major
const key_signature_t ds_minor = {"D# minor", {1, 1, 1, 1, 1, 1, 0}}; // Same as F# major
const key_signature_t as_minor = {"A# minor", {1, 1, 1, 1, 1, 1, 1}}; // Same as C# major
const key_signature_t d_minor = {"D minor", {0, 0, 0, 0, 0, 0, -1}}; // Same as F major
const key_signature_t g_minor = {"G minor", {0, 0, -1, 0, 0, 0, -1}}; // Same as Bb major
const key_signature_t c_minor = {"C minor", {0, 0, -1, 0, 0, -1, -1}}; // Same as Eb major
const key_signature_t f_minor = {"F minor", {0, -1, -1, 0, 0, -1, -1}}; // Same as Ab major
const key_signature_t bf_minor = {"Bb minor", {0, -1, -1, 0, -1, -1, -1}}; // Same as Db major
const key_signature_t ef_minor = {"Eb minor", {-1, -1, -1, 0, -1, -1, -1}}; // Same as Gb major
const key_signature_t af_minor = {"Ab minor", {-1, -1, -1, -1, -1, -1, -1}}; // Same as Cb major

// ============================================================================
// TEMPERAMENT IMPLEMENTATIONS
// ============================================================================

double equal_temperament_freq(int absolute_semitone) {
    const double c0_freq = 16.351597831287414;
    return c0_freq * powf(2.0f, absolute_semitone / 12.0);
}

double werckmeister3_freq(int absolute_semitone) {
    static const double ratios[12] = {1.0000000, 1.0535686, 1.1174011, 1.1852459, 1.2533331, 1.3333333,
                                      1.4062500, 1.4953488, 1.5802469, 1.6735537, 1.7777778, 1.8877551};

    int chromatic_pos = absolute_semitone % 12;
    int octave = absolute_semitone / 12;

    const double c4_freq = 261.626;
    return c4_freq * ratios[chromatic_pos] * powf(2.0f, (float)octave - 4);
}

const temperament_t equal_temperament = {.name = "Equal Temperament", .compute_frequency = equal_temperament_freq};
const temperament_t werckmeister3_temperament = {.name = "Werckmeister III", .compute_frequency = werckmeister3_freq};

// ============================================================================
// MUSIC THEORY FUNCTIONS
// ============================================================================

int get_key_accidental(char note_name, const key_signature_t* key) {
    if (!key)
        return 0;
    int index;
    switch (note_name) {
    case 'c':
        index = 0;
        break;
    case 'd':
        index = 1;
        break;
    case 'e':
        index = 2;
        break;
    case 'f':
        index = 3;
        break;
    case 'g':
        index = 4;
        break;
    case 'a':
        index = 5;
        break;
    case 'b':
        index = 6;
        break;
    default:
        return 0; // Rest or invalid
    }
    return key->accidentals[index];
}

bool is_valid_note_name(char c) { return (c >= 'a' && c <= 'g') || (c == 'r'); }

bool is_rest(const note_t* note) { return note && note->note_name == 'r'; }

bool is_dotted(const note_t* note) { return note && note->dotted; }

bool is_tuplet(const note_t* note) { return note && note->tuplet > 0; }

float get_tuplet_ratio(int tuplet) {
    switch (tuplet) {
    case 0:
        return 1.0f; // normal notes
    case 3:
        return 2.0f / 3.0f; // triplet (3 in time of 2)
    case 5:
        return 4.0f / 5.0f; // quintuplet (5 in time of 4)
    case 6:
        return 4.0f / 6.0f; // sextuplet (6 in time of 4)
    case 7:
        return 4.0f / 7.0f; // septuplet (7 in time of 4)
    default:
        return 1.0f;
    }
}

int note_name_to_semitone(char note_name) {
    switch (note_name) {
    case 'c':
        return 0;
    case 'd':
        return 2;
    case 'e':
        return 4;
    case 'f':
        return 5;
    case 'g':
        return 7;
    case 'a':
        return 9;
    case 'b':
        return 11;
    default:
        return -1; // Invalid or rest
    }
}

int calculate_semitone(const note_t* note) {
    if (is_rest(note)) {
        return 0; // Rests have no frequency
    }

    int note_semitone = note_name_to_semitone(note->note_name);
    if (note_semitone < 0) {
        return 0; // Invalid note name
    }

    // Calculate absolute semitone: (octave_shift + 4) * 12 + base_semitone + accidental
    return (note->octave_shift + 4) * 12 + note_semitone + note->accidental;
}

int note_to_absolute_semitone(const note_t* note, const key_signature_t* key, int transposition) {
    if (is_rest(note)) {
        return -1; // Special value for rests
    }

    int note_semitone = note_name_to_semitone(note->note_name);
    if (note_semitone < 0) {
        return -1; // Invalid note
    }

    // Apply key signature + explicit accidental + transposition
    int key_accidental = key ? get_key_accidental(note->note_name, key) : 0;
    int total_accidental = key_accidental + note->accidental;

    return (note->octave_shift + 4) * 12 + note_semitone + total_accidental + transposition;
}

double note_to_frequency(const note_t* note, const temperament_t* temperament, const key_signature_t* key,
                         int transposition) {
    if (!note || !temperament || !temperament->compute_frequency) {
        return 0.0;
    }

    int absolute_semitone = note_to_absolute_semitone(note, key, transposition);
    if (absolute_semitone < 0) {
        return 0.0; // Rest or invalid note
    }

    return temperament->compute_frequency(absolute_semitone);
}

int get_key_tonic_semitone(const key_signature_t* key) {
    // Map key names to tonic semitones
    if (key == &c_major || key == &a_minor)
        return 0; // C
    if (key == &g_major || key == &e_minor)
        return 7; // G
    if (key == &d_major || key == &b_minor)
        return 2; // D
    if (key == &a_major || key == &fs_minor)
        return 9; // A
    if (key == &e_major || key == &cs_minor)
        return 4; // E
    if (key == &b_major || key == &gs_minor)
        return 11; // B
    if (key == &fs_major || key == &ds_minor)
        return 6; // F#
    if (key == &f_major || key == &d_minor)
        return 5; // F
    if (key == &bf_major || key == &g_minor)
        return 10; // Bb
    if (key == &ef_major || key == &c_minor)
        return 3; // Eb
    if (key == &af_major || key == &f_minor)
        return 8; // Ab
    if (key == &df_major || key == &bf_minor)
        return 1; // Db
    if (key == &gf_major || key == &ef_minor)
        return 6; // Gb (enharmonic with F#)
    return 0; // Default to C
}

int calculate_key_transposition(const key_signature_t* from_key, const key_signature_t* to_key) {
    int from_tonic = get_key_tonic_semitone(from_key);
    int to_tonic = get_key_tonic_semitone(to_key);
    return to_tonic - from_tonic;
}

// ============================================================================
// PARSING FUNCTIONS
// ============================================================================

#define MAX_CHORD_SIZE 8

note_t parse_note_without_duration(const char** input_pos) {
    note_t note = {0}; // Initialize all fields to 0
    const char* p = *input_pos;

    // Skip whitespace
    while (*p && isspace(*p)) {
        p++;
    }

    if (!*p) {
        *input_pos = p;
        return note;
    }

    // Check for rest
    if (*p == 'r') {
        note.note_name = 'r';
        p++;
        *input_pos = p;
        return note;
    }

    // Parse note name (a-g)
    if (!is_valid_note_name(*p)) {
        *input_pos = p;
        return note;
    }

    note.note_name = *p;
    p++;

    // Parse accidentals (s for sharp, f for flat)
    while (*p == 's' || *p == 'f') {
        if (*p == 's') {
            note.accidental++;
        } else if (*p == 'f') {
            note.accidental--;
        }
        p++;
    }

    // Parse octave shifts (' for up, , for down)
    while (*p == '\'' || *p == ',') {
        if (*p == '\'') {
            note.octave_shift++;
        } else if (*p == ',') {
            note.octave_shift--;
        }
        p++;
    }

    *input_pos = p;
    return note;
}

void parse_duration_and_modifiers(const char** input_pos, int* last_duration, note_t* notes, int note_count) {
    const char* p = *input_pos;

    // Parse duration
    int duration = *last_duration; // default
    if (isdigit(*p)) {
        duration = 0;
        while (isdigit(*p)) {
            duration = duration * 10 + (*p - '0');
            p++;
        }

        // Validate duration (power of 2)
        if (duration == 1 || duration == 2 || duration == 4 || duration == 8 || duration == 16 || duration == 32 ||
            duration == 64 || duration == 128) {
            *last_duration = duration;
        } else {
            duration = *last_duration; // Invalid duration, keep previous
        }
    }

    // Parse dots
    bool dotted = false;
    if (*p == '.') {
        dotted = true;
        p++;
    }

    // Parse tuplets
    int tuplet = 0;
    if (*p == 't') {
        tuplet = 3;
        p++;
    } else if (*p == 'q') {
        tuplet = 5;
        p++;
    } else if (*p == 'x') {
        tuplet = 6;
        p++;
    } else if (*p == 's') {
        tuplet = 7;
        p++;
    } else if (*p == 'n') {
        tuplet = 9;
        p++;
    }

    // Apply to all notes
    for (int i = 0; i < note_count; i++) {
        notes[i].value = duration;
        notes[i].dotted = dotted;
        notes[i].tuplet = tuplet;
    }

    *input_pos = p;
}

note_t* parse_chord(const char** input_pos, int* chord_size, int* last_duration) {
    const char* p = *input_pos;
    *chord_size = 0;

    // Skip whitespace
    while (*p && isspace(*p)) {
        p++;
    }

    // Expect '<'
    if (*p != '<') {
        return NULL;
    }
    p++;

    note_t* chord_notes = malloc(MAX_CHORD_SIZE * sizeof(note_t));
    if (!chord_notes) {
        return NULL;
    }

    // Parse notes until '>'
    while (*p && *p != '>') {
        if (*chord_size >= MAX_CHORD_SIZE) {
            break; // Chord too large
        }

        note_t note = parse_note_without_duration(&p);
        if (note.note_name == 0) {
            break; // Failed to parse note
        }

        chord_notes[*chord_size] = note;
        (*chord_size)++;

        // Skip whitespace
        while (*p && isspace(*p)) {
            p++;
        }
    }

    // Expect '>'
    if (*p == '>') {
        p++;
    }

    // Parse duration/modifiers that apply to whole chord
    if (*chord_size > 0) {
        parse_duration_and_modifiers(&p, last_duration, chord_notes, *chord_size);
    }

    *input_pos = p;
    return chord_notes;
}

note_t parse_note(const char** input_pos, int* last_duration) {
    note_t note = {0}; // Initialize all fields to zeros

    if (!input_pos || !*input_pos || !last_duration) {
        return note; // Return empty note on error
    }

    // Set default duration
    note.value = *last_duration;

    const char* p = *input_pos;

    // Skip whitespace
    while (*p && isspace(*p)) {
        p++;
    }

    if (!*p) {
        *input_pos = p;
        return note; // End of string - return empty note
    }

    // Check for rest
    if (*p == 'r') {
        note.note_name = 'r';
        p++;
        goto parse_duration;
    }

    // Parse note name (a-g)
    if (!is_valid_note_name(*p)) {
        return note; // Invalid note name - return empty note
    }

    note.note_name = *p;
    p++;

    // Parse accidentals (s for sharp, f for flat)
    while (*p == 's' || *p == 'f') {
        if (*p == 's') {
            note.accidental++;
        } else if (*p == 'f') {
            note.accidental--;
        }
        p++;
    }

    // Parse octave shifts (' for up, , for down)
    while (*p == '\'' || *p == ',') {
        if (*p == '\'') {
            note.octave_shift++;
        } else if (*p == ',') {
            note.octave_shift--;
        }
        p++;
    }

parse_duration:
    // Parse duration (if present)
    if (isdigit(*p)) {
        int duration = 0;
        while (isdigit(*p)) {
            duration = duration * 10 + (*p - '0');
            p++;
        }

        // Validate duration (power of 2)
        if (duration == 1 || duration == 2 || duration == 4 || duration == 8 || duration == 16 || duration == 32 ||
            duration == 64 || duration == 128) {
            note.value = duration;
            *last_duration = duration; // Update last duration for next note
        } else {
            // Invalid duration - return empty note
            note_t empty = {0};
            return empty;
        }
    }

    // Parse dots for dotted notes
    if (*p == '.') {
        p++;
        note.dotted = true;
    }

    // Parse tuplet markers
    if (*p == 't') {
        p++;
        note.tuplet = 3; // triplet
    } else if (*p == 'q') {
        p++;
        note.tuplet = 5; // quintuplet
    } else if (*p == 'x') {
        p++;
        note.tuplet = 6; // sextuplet
    } else if (*p == 's') {
        p++;
        note.tuplet = 7; // septuplet
    } else if (*p == 'n') {
        p++;
        note.tuplet = 9; // nonuplet
    }

    *input_pos = p; // Update input position
    return note;
}

note_array_t parse_music(const char* input) {
    note_array_t array = {0}; // Clean initialization
    const instrument_t* current_instrument = &pluck_sine_instrument;

    if (!input) {
        return array;
    }

    const char* p = input;
    int last_duration = 4;
    int chord_counter = 1;

    while (*p) {
        // Skip whitespace
        while (*p && isspace(*p)) {
            p++;
        }

        if (!*p)
            break;

        // Check for instrument change [name]
        if (*p == '[') {
            p++;
            char instrument_name[32] = {0};
            int name_idx = 0;
            while (*p && *p != ']' && name_idx < 31) {
                instrument_name[name_idx++] = *p++;
            }

            if (*p == ']') {
                p++;
                instrument_name[name_idx] = '\0';
                current_instrument = lookup_instrument(instrument_name);
            }
            continue;
        }

        // Check for chord syntax
        if (*p == '<') {
            int chord_size;
            note_t* chord_notes = parse_chord(&p, &chord_size, &last_duration);

            if (chord_notes && chord_size > 0) {
                int current_chord_id = chord_counter++;

                for (int i = 0; i < chord_size; i++) {
                    chord_notes[i].chord_id = (int16_t)current_chord_id;
                    chord_notes[i].instrument = current_instrument;
                    note_array_push(&array, chord_notes[i]);
                }
                free(chord_notes);
            }
        } else {
            // Parse single note
            note_t note = parse_note(&p, &last_duration);

            if (note.note_name == 0) {
                break;
            }

            note.instrument = current_instrument;
            note.chord_id = 0;
            note_array_push(&array, note);
        }
    }

    return array;
}

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

void free_note_array(note_array_t* array) {
    if (array && array->data) {
        free(array->data);
        array->data = NULL;
        array->count = 0;
        array->capacity = 0;
    }
}

// ============================================================================
// DEBUG/PRINT FUNCTIONS
// ============================================================================

void print_note(const note_t* note) {
    if (!note) {
        printf("NULL note\n");
        return;
    }

    if (is_rest(note)) {
        printf("r%d", note->value);
    } else {
        printf("%c", note->note_name);

        // Print accidentals
        for (int i = 0; i < note->accidental; i++) {
            printf("s");
        }
        for (int i = 0; i < -note->accidental; i++) {
            printf("f");
        }

        // Print octave shifts
        for (int i = 0; i < note->octave_shift; i++) {
            printf("'");
        }
        for (int i = 0; i < -note->octave_shift; i++) {
            printf(",");
        }

        printf("%d", note->value);
    }

    // Print dot if dotted
    if (note->dotted) {
        printf(".");
    }

    // Print tuplet marker
    if (note->tuplet == 3) {
        printf("t");
    } else if (note->tuplet == 5) {
        printf("q");
    } else if (note->tuplet == 6) {
        printf("x");
    } else if (note->tuplet == 7) {
        printf("s");
    }

    // Print chord ID for debugging
    if (note->chord_id > 0) {
        printf("[%d]", note->chord_id);
    }
}

void print_note_array(const note_array_t* array) {
    if (!array || !array->data) {
        printf("Empty array\n");
        return;
    }

    printf("Notes (%d): ", array->count);
    for (int i = 0; i < array->count; i++) {
        if (i > 0)
            printf(" ");
        print_note(&array->data[i]);
    }
    printf("\n");
}
