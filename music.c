#include "music.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool is_valid_note_name(char c) { return (c >= 'a' && c <= 'g') || (c == 'r'); }

bool is_rest(const note_t* note) { return note && note->note_name == 'r'; }

note_t parse_note(const char** input_pos, int* last_duration) {
    note_t note = {0}; // Initialize to zeros

    if (!input_pos || !*input_pos || !last_duration) {
        return note; // Return empty note on error
    }

    // Set default duration
    note.duration = *last_duration;

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
            note.duration = duration;
            *last_duration = duration; // Update last duration for next note
        } else {
            // Invalid duration - return empty note
            note_t empty = {0};
            return empty;
        }
    }

    // TODO: Parse dots for dotted notes (later enhancement)

    *input_pos = p; // Update input position
    return note;
}

note_array_t parse_string(const char* input) {
    note_array_t array = {0};

    if (!input) {
        return array;
    }

    // Initial allocation
    array.capacity = 16;
    array.notes = malloc(array.capacity * sizeof(note_t));
    if (!array.notes) {
        array.capacity = 0;
        return array;
    }

    const char* p = input;
    int last_duration = 4; // Default quarter note

    while (*p) {
        note_t note = parse_note(&p, &last_duration);

        // If note_name is 0, we've reached end or error
        if (note.note_name == 0) {
            break;
        }

        // Resize array if needed
        if (array.count >= array.capacity) {
            array.capacity *= 2;
            note_t* new_notes = realloc(array.notes, array.capacity * sizeof(note_t));
            if (!new_notes) {
                // Allocation failed - return what we have
                break;
            }
            array.notes = new_notes;
        }

        array.notes[array.count++] = note;
    }

    return array;
}

void free_note_array(note_array_t* array) {
    if (array && array->notes) {
        free(array->notes);
        array->notes = NULL;
        array->count = 0;
        array->capacity = 0;
    }
}

void print_note(const note_t* note) {
    if (!note) {
        printf("NULL note\n");
        return;
    }

    if (is_rest(note)) {
        printf("r%d", note->duration);
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

        printf("%d", note->duration);
    }
}

void print_note_array(const note_array_t* array) {
    if (!array || !array->notes) {
        printf("Empty array\n");
        return;
    }

    printf("Notes (%d): ", array->count);
    for (int i = 0; i < array->count; i++) {
        if (i > 0)
            printf(" ");
        print_note(&array->notes[i]);
    }
    printf("\n");
}

// Frequency conversion functions

// Helper: get chromatic semitone number (0-11) for a note name
// c=0, d=2, e=4, f=5, g=7, a=9, b=11
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

// Calculate absolute semitone offset from reference note
int calculate_semitone_offset(const note_t* note, const note_t* reference) {
    if (is_rest(note) || is_rest(reference)) {
        return 0; // Rests have no frequency
    }

    int note_semitone = note_name_to_semitone(note->note_name);
    int ref_semitone = note_name_to_semitone(reference->note_name);

    if (note_semitone < 0 || ref_semitone < 0) {
        return 0; // Invalid note names
    }

    // Calculate total semitone difference
    int semitone_diff = note_semitone - ref_semitone;
    int octave_diff = note->octave_shift - reference->octave_shift;
    int accidental_diff = note->accidental - reference->accidental;

    return semitone_diff + (octave_diff * 12) + accidental_diff;
}

double equal_temperament_freq(const note_t* note, const note_t* reference, double reference_freq) {
    if (is_rest(note)) {
        return 0.0; // Rests have no frequency
    }

    int semitone_offset = calculate_semitone_offset(note, reference);

    // Equal temperament: each semitone is 2^(1/12) ratio
    return reference_freq * pow(2.0, semitone_offset / 12.0);
}

double note_to_frequency(const note_t* note, const note_t* reference, double reference_freq, const temperament_t* temperament) {
    if (!note || !reference || !temperament || !temperament->note_to_freq) {
        return 0.0;
    }

    return temperament->note_to_freq(note, reference, reference_freq);
}

// Standard temperament definitions
const temperament_t equal_temperament = {.name = "Equal Temperament", .note_to_freq = equal_temperament_freq};

void test_frequencies(void) {
    printf("=== Frequency Conversion Tests ===\n");

    // Reference: middle C (c') = 261.625565 Hz
    note_t middle_c = {.note_name = 'c', .octave_shift = 0, .duration = 4};
    double middle_c_freq = 261.625565;

    const char* test_notes[] = {
        "c",   "d",   "e",  "f",  "g", "a", "b", // Same octave as reference
        "c'",  "d'",  "e'", // One octave up
        "c,",  "g,", // One octave down
        "cs",  "df",  "gs", "af", // Accidentals
        "c''", "a,,", // Multiple octaves
        "r4" // Rest
    };

    int num_freq_tests = sizeof(test_notes) / sizeof(test_notes[0]);

    for (int i = 0; i < num_freq_tests; i++) {
        const char* p = test_notes[i];
        int last_duration = 4;
        note_t note = parse_note(&p, &last_duration);

        double freq = note_to_frequency(&note, &middle_c, middle_c_freq, &equal_temperament);

        printf("  ");
        print_note(&note);
        printf(" -> %.2f Hz\n", freq);
    }

    printf("\n");
}

void test_parser(void) {
    printf("Testing music parser:\n\n");

    // Test individual notes
    printf("=== Individual Note Tests ===\n");
    const char* note_tests[] = {"c4", "gs2", "bf'", "r8", "a''4", "d,2", "ess16", "bff8"};

    int num_note_tests = sizeof(note_tests) / sizeof(note_tests[0]);
    for (int i = 0; i < num_note_tests; i++) {
        printf("Input: \"%s\" -> ", note_tests[i]);
        const char* p = note_tests[i];
        int last_duration = 4;
        note_t note = parse_note(&p, &last_duration);
        print_note(&note);
        printf("\n");
    }

    printf("\n=== String Parsing Tests ===\n");
    const char* string_tests[] = {
        "c g a f", // Simple sequence
        "c4 d e f g", // Duration inheritance
        "c4 c g' g a a g2", // Twinkle Twinkle first line
        "f4 f e e d d c2", // Twinkle Twinkle second line
        "r4 c d r2 e", // With rests
        "c'' d, ess bf' r8 g4", // Mixed complex notes
    };

    int num_string_tests = sizeof(string_tests) / sizeof(string_tests[0]);
    for (int i = 0; i < num_string_tests; i++) {
        printf("Input: \"%s\"\n", string_tests[i]);
        note_array_t array = parse_string(string_tests[i]);
        printf("  ");
        print_note_array(&array);
        free_note_array(&array);
        printf("\n");
    }
}
