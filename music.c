#include "music.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHORD_SIZE 8

// Key signature definitions
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

// Helper function to get key signature accidental for a note name
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

// Helper function to apply key signature to a note
note_t apply_key_signature(const note_t* note, const key_signature_t* key) {
    note_t modified_note = *note; // Copy the original note

    if (!is_rest(note) && key) {
        // Add key signature accidental to the note's explicit accidental
        int key_accidental = get_key_accidental(note->note_name, key);
        modified_note.accidental += key_accidental;
    }

    return modified_note;
}

bool is_valid_note_name(char c) { return (c >= 'a' && c <= 'g') || (c == 'r'); }

bool is_rest(const note_t* note) { return note && note->note_name == 'r'; }

bool is_dotted(const note_t* note) { return note && note->dotted; }

bool is_tuplet(const note_t* note) { return note && note->tuplet > 0; }

// Get the ratio for tuplet timing (e.g., triplets are 2/3 of normal duration)
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

// Parse note name, accidentals, octave - but NOT duration
note_t parse_note_without_duration(const char** input_pos) {
    note_t note = {0}; // Initialize all fields to 0, including chord_id
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

// Apply duration/dots/tuplets to notes
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

// Parse chord: <c e g>4
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
    note_t note = {0}; // Initialize all fields to zeros, including chord_id

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
    note_array_t array = {0};
    const instrument_t* current_instrument = &pluck_sine_instrument; // Default

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
    int chord_counter = 1; // Start at 1, 0 means single note

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
            continue; // Skip to next iteration
        }

        // Check for chord syntax
        if (*p == '<') {
            int chord_size;
            note_t* chord_notes = parse_chord(&p, &chord_size, &last_duration);

            if (chord_notes && chord_size > 0) {
                // Assign same chord ID to all notes in this chord
                int current_chord_id = chord_counter++;

                // Add all chord notes to array
                for (int i = 0; i < chord_size; i++) {
                    // Resize array if needed
                    if (array.count >= array.capacity) {
                        array.capacity *= 2;
                        note_t* new_notes = realloc(array.notes, array.capacity * sizeof(note_t));
                        if (!new_notes) {
                            free(chord_notes);
                            return array; // Return what we have
                        }
                        array.notes = new_notes;
                    }

                    chord_notes[i].chord_id = current_chord_id; // Mark as part of chord
                    array.notes[array.count].instrument = current_instrument;
                    array.notes[array.count++] = chord_notes[i];
                }
                free(chord_notes);
            }
        } else {
            // Parse single note
            note_t note = parse_note(&p, &last_duration);

            // If note_name is 0, we've reached end or error
            if (note.note_name == 0) {
                break;
            }

            note.instrument = current_instrument;
            note.chord_id = 0; // Mark as single note

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
        printf("[ch%d]", note->chord_id);
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

// Calculate absolute semitone from octave 0
int calculate_semitone(const note_t* note) {
    if (is_rest(note)) {
        return 0; // Rests have no frequency
    }

    int note_semitone = note_name_to_semitone(note->note_name);
    if (note_semitone < 0) {
        return 0; // Invalid note name
    }

    // Calculate absolute semitone: (octave_shift + 4) * 12 + base_semitone + accidental
    // octave_shift 0 = octave 4 (middle C), octave_shift 1 = octave 5, etc.
    return (note->octave_shift + 4) * 12 + note_semitone + note->accidental;
}

double equal_temperament_freq(const note_t* note) {
    if (is_rest(note)) {
        return 0.0; // Rests have no frequency
    }

    int semitone = calculate_semitone(note);

    // C0 frequency (base frequency for octave 0)
    const double c0_freq = 16.351597831287414;

    // Equal temperament: each semitone is 2^(1/12) ratio
    return c0_freq * pow(2.0, semitone / 12.0);
}

double note_to_frequency(const note_t* note, const temperament_t* temperament) {
    if (!note || !temperament || !temperament->note_to_freq) {
        return 0.0;
    }

    return temperament->note_to_freq(note);
}

// Standard temperament definitions
const temperament_t equal_temperament = {.name = "Equal Temperament", .note_to_freq = equal_temperament_freq};

int calculate_total_samples(sequencer_event_t* events, int event_count) {
    int max_end = 0;
    for (int i = 0; i < event_count; i++) {
        int event_end = events[i].start_sample + events[i].duration_samples;
        if (event_end > max_end) {
            max_end = event_end;
        }
    }
    return max_end;
}

float sine_wave(const sequencer_event_t* event, int sample_index, int sample_rate) {
    return sin(2.0 * M_PI * event->frequency * sample_index / sample_rate);
}

float square_wave(const sequencer_event_t* event, int sample_index, int sample_rate) {
    float t = 2.0 * M_PI * event->frequency * sample_index / sample_rate;
    float result = 0.0f;

    // Calculate maximum safe harmonic
    float nyquist = sample_rate / 2.0f;
    int technical_max = (int)(nyquist / event->frequency);
    int musical_max = 11;
    int max_harmonic = (technical_max < musical_max) ? technical_max : musical_max;

    if (max_harmonic % 2 == 0)
        max_harmonic--;

    for (int h = 1; h <= max_harmonic; h += 2) {
        result += sin(h * t) / h;
    }

    return result * 0.4f;
}

// Square wave: odd harmonics with 1/n amplitude
static harmonic_t square_harmonics[] = {{1.0f, 1.0f},     {3.0f, 1.0f / 3}, {5.0f, 1.0f / 5},
                                        {7.0f, 1.0f / 7}, {9.0f, 1.0f / 9}, {11.0f, 1.0f / 11}};

// Sawtooth: all harmonics with 1/n amplitude
static harmonic_t saw_harmonics[] = {{1.0f, 1.0f},     {2.0f, 1.0f / 2}, {3.0f, 1.0f / 3},
                                     {4.0f, 1.0f / 4}, {5.0f, 1.0f / 5}, {6.0f, 1.0f / 6}};

// Organ-like: selected harmonics
static harmonic_t organ_harmonics[] = {{1.0f, 1.0f}, {2.0f, 0.8f}, {3.0f, 0.6f}, {4.0f, 0.4f}, {6.0f, 0.3f}};

float additive_wave(const sequencer_event_t* event, int sample_index, int sample_rate, const timbre_t* spec) {
    float t = 2.0 * M_PI * event->frequency * sample_index / sample_rate;
    float result = 0.0f;

    float nyquist = sample_rate / 2.0f;

    for (int i = 0; i < spec->harmonic_count; i++) {
        float harmonic_freq = event->frequency * spec->harmonics[i].frequency_ratio;
        if (harmonic_freq >= nyquist)
            break; // Skip harmonics above Nyquist

        result += spec->harmonics[i].amplitude * sin(spec->harmonics[i].frequency_ratio * t);
    }

    return result * 0.4f;
}

float pluck_envelope(const sequencer_event_t* event, int sample_index, int sample_rate) {
    float t = (float)sample_index / event->duration_samples;
    float attack_time = fmin(0.005f * sample_rate / event->duration_samples, 0.02f);

    if (t < attack_time) {
        return t / attack_time;
    }

    float decay_t = (t - attack_time) / (1.0f - attack_time);
    return exp(-4.0f * decay_t);
}

const instrument_t pluck_sine_instrument = {.name = "Pluck Sine", .waveform = sine_wave, .envelope = pluck_envelope};
const instrument_t pluck_square_instrument = {.name = "Pluck Square", .waveform = square_wave, .envelope = pluck_envelope};

// Global instrument registry
static const instrument_t* available_instruments[] = {
    &pluck_sine_instrument, &pluck_square_instrument,
    NULL // Sentinel
};

const instrument_t* lookup_instrument(const char* name) {
    for (int i = 0; available_instruments[i] != NULL; i++) {
        if (strcasecmp(name, available_instruments[i]->name) == 0) {
            return available_instruments[i];
        }
    }

    return &pluck_sine_instrument; // Default fallback
}

void generate_samples(sequencer_event_t* events, int event_count, float* output_buffer, int buffer_size, int sample_rate) {
    memset(output_buffer, 0, buffer_size * sizeof(float));

    for (int e = 0; e < event_count; e++) {
        sequencer_event_t* event = &events[e];

        for (int i = 0; i < event->duration_samples; i++) {
            int output_index = event->start_sample + i;
            if (output_index >= buffer_size)
                break;

            // Let the instrument generate the sample
            float waveform_sample = event->instrument.waveform(event, i, sample_rate);
            float envelope_sample = event->instrument.envelope(event, i, sample_rate);
            float sample = event->volume * envelope_sample * waveform_sample;

            output_buffer[output_index] += sample;
        }
    }
}

void play_sequence(sequencer_event_t* events, int event_count, audio_driver_t* driver, int sample_rate) {
    // Pre-allocate based on total length
    int total_samples = calculate_total_samples(events, event_count);
    float* output_buffer = malloc(total_samples * sizeof(float));
    int error;
    void* context = driver->init(sample_rate, 1, &error);

    // Generate all audio
    generate_samples(events, event_count, output_buffer, total_samples, sample_rate);

    // Play it all at once
    driver->play(context, output_buffer, total_samples);

    // Cleanup
    driver->cleanup(context);
    free(output_buffer);
}

// Standard chord volume adjustment functions
void no_chord_adjustment(sequencer_event_t* events, int start_index, int chord_size) {
    // Do nothing
}

void linear_chord_adjustment(sequencer_event_t* events, int start_index, int chord_size) {
    float scale = 1.0f / chord_size;
    for (int i = start_index; i < start_index + chord_size; i++) {
        events[i].volume *= scale;
    }
}

void sqrt_chord_adjustment(sequencer_event_t* events, int start_index, int chord_size) {
    float scale = 1.0f / sqrt(chord_size);
    for (int i = start_index; i < start_index + chord_size; i++) {
        events[i].volume *= scale;
    }
}

void bass_boost_adjustment(sequencer_event_t* events, int start_index, int chord_size) {
    // Find lowest frequency note
    int bass_idx = start_index;
    for (int i = start_index + 1; i < start_index + chord_size; i++) {
        if (events[i].frequency < events[bass_idx].frequency) {
            bass_idx = i;
        }
    }

    // Apply different scaling
    float bass_scale = 1.0f / sqrt(chord_size * 0.7f); // Less reduction for bass
    float other_scale = 1.0f / sqrt(chord_size * 1.2f); // More reduction for others

    for (int i = start_index; i < start_index + chord_size; i++) {
        events[i].volume *= (i == bass_idx) ? bass_scale : other_scale;
    }
}

// Adjust volumes for simultaneous events (chords)
void adjust_chord_volumes(sequencer_event_t* events, int event_count, chord_volume_fn_t volume_fn) {
    if (!volume_fn)
        return; // No adjustment

    for (int i = 0; i < event_count; i++) {
        int simultaneous_count = 1;

        // Count events that start at same time
        for (int j = i + 1; j < event_count; j++) {
            if (events[j].start_sample == events[i].start_sample) {
                simultaneous_count++;
            } else {
                break; // Events are in chronological order
            }
        }

        if (simultaneous_count > 1) {
            // Let the function handle all the complexity
            volume_fn(events, i, simultaneous_count);
            i += simultaneous_count - 1; // Skip processed notes
        }
    }
}

// Helper function to check if two notes should be simultaneous (part of same chord)
bool notes_are_simultaneous(const note_t* note1, const note_t* note2) {
    // Notes are simultaneous only if they have the same non-zero chord_id
    return (note1->chord_id > 0 && note1->chord_id == note2->chord_id);
}

// Convert note array to sequencer events with tuplet support
// Now uses absolute frequency calculation and chord volume adjustment
sequencer_event_t* notes_to_events(const note_array_t* notes, int tempo_bpm, int sample_rate, const temperament_t* temperament,
                                   const key_signature_t* key, float volume, chord_volume_fn_t chord_volume_fn) {
    if (!notes || !notes->notes || notes->count == 0) {
        return NULL;
    }

    sequencer_event_t* events = malloc(notes->count * sizeof(sequencer_event_t));
    if (!events) {
        return NULL;
    }

    int samples_per_beat = (60 * sample_rate) / tempo_bpm;
    int current_sample = 0;

    for (int i = 0; i < notes->count; i++) {
        const note_t* note = &notes->notes[i];

        // Apply key signature and calculate frequency using provided temperament
        double freq;
        if (is_rest(note)) {
            freq = 0.0;
        } else {
            note_t key_adjusted_note = apply_key_signature(note, key);
            freq = note_to_frequency(&key_adjusted_note, temperament);
        }

        // Calculate duration (same as before)
        int duration_samples = (samples_per_beat * 4) / note->value;

        if (note->dotted) {
            duration_samples = (duration_samples * 3) / 2;
        }

        if (note->tuplet > 0) {
            float tuplet_ratio = get_tuplet_ratio(note->tuplet);
            duration_samples = (int)(duration_samples * tuplet_ratio);
        }

        events[i] = (sequencer_event_t){.frequency = freq,
                                        .start_sample = current_sample,
                                        .duration_samples = duration_samples,
                                        .volume = volume,
                                        .instrument = *note->instrument};

        bool advance_time = true;
        if (i + 1 < notes->count) {
            advance_time = !notes_are_simultaneous(note, &notes->notes[i + 1]);
        }

        if (advance_time) {
            current_sample += duration_samples;
        }
    }

    if (chord_volume_fn) {
        adjust_chord_volumes(events, notes->count, chord_volume_fn);
    }

    return events;
}

// Helper function to compare events by start time for sorting
int compare_start_times(const void* a, const void* b) {
    const sequencer_event_t* event_a = (const sequencer_event_t*)a;
    const sequencer_event_t* event_b = (const sequencer_event_t*)b;

    if (event_a->start_sample < event_b->start_sample)
        return -1;
    if (event_a->start_sample > event_b->start_sample)
        return 1;
    return 0;
}

// Merge two event arrays into one, maintaining chronological order
sequencer_event_t* merge_event_arrays(sequencer_event_t* events1, int count1, sequencer_event_t* events2, int count2,
                                      int* total_count) {
    *total_count = count1 + count2;
    sequencer_event_t* merged = malloc(*total_count * sizeof(sequencer_event_t));
    if (!merged) {
        *total_count = 0;
        return NULL;
    }

    // Copy first voice
    memcpy(merged, events1, count1 * sizeof(sequencer_event_t));

    // Copy second voice
    memcpy(merged + count1, events2, count2 * sizeof(sequencer_event_t));

    // Sort by start_sample to maintain chronological order
    qsort(merged, *total_count, sizeof(sequencer_event_t), compare_start_times);

    return merged;
}

// High-level play function - takes multiple voice strings as var args
void play(const char* name, int tempo_bpm, const key_signature_t* key, const audio_driver_t* driver, ...) {
    printf("=== Playing %s in %s ===\n", name, key ? key->name : "C major");
    printf("Tempo: %d BPM\n", tempo_bpm);

    va_list args;
    va_start(args, driver);

    const char* voices[16];
    int voice_count = 0;
    const char* music_string;

    while ((music_string = va_arg(args, const char*)) != NULL && voice_count < 16) {
        voices[voice_count++] = music_string;
    }
    va_end(args);

    if (voice_count == 0) {
        printf("No music provided!\n");
        return;
    }

    printf("Voices: %d\n", voice_count);
    for (int i = 0; i < voice_count; i++) {
        printf("  Voice %d: %s\n", i + 1, voices[i]);
    }
    printf("\n");

    sequencer_event_t* all_events = NULL;
    int total_events = 0;
    const int sample_rate = 44100;

    for (int v = 0; v < voice_count; v++) {
        note_array_t notes = parse_music(voices[v]);
        if (notes.count == 0) {
            printf("Warning: Voice %d is empty\n", v + 1);
            free_note_array(&notes);
            continue;
        }

        // Use key-aware function with proper temperament parameter
        sequencer_event_t* voice_events =
            notes_to_events(&notes, tempo_bpm, sample_rate, &equal_temperament, key, 0.3f, sqrt_chord_adjustment);

        if (v == 0) {
            all_events = voice_events;
            total_events = notes.count;
        } else {
            sequencer_event_t* merged = merge_event_arrays(all_events, total_events, voice_events, notes.count, &total_events);
            free(all_events);
            free(voice_events);
            all_events = merged;
        }

        free_note_array(&notes);
    }

    if (!all_events || total_events == 0) {
        printf("No events to play!\n");
        return;
    }

    printf("Playing %d total events...\n", total_events);
    play_sequence(all_events, total_events, (audio_driver_t*)driver, sample_rate);
    printf("%s complete!\n\n", name);

    free(all_events);
}

// Test chord functionality
void test_chords(const audio_driver_t* driver) {
    // const char* chord_progression = "c4 <c e g>2 f4 <f a c'>2 g4 <g b d'>1";
    // test_play_melody("Basic Chord Progression", chord_progression, 100, driver);
    //
    // const char* arpeggiated = "c4 e g c' <c e g c'>1";
    // test_play_melody("Arpeggiated vs Chord", arpeggiated, 120, driver);

    // Demonstrate multi-voice polyphony with new play() function
    play("Two-Voice Counterpoint", 120, &c_major, driver,
         "c4 d e f g a b c'2", // Voice 1: ascending scale
         "c2 f2 e2 g2 c'1", // Voice 2: slower harmony
         NULL);

    // Three-voice example
    play("Three-Voice Harmony", 100, &c_major, driver,
         "c4 d e f g f e d c2", // Melody
         "c2 f2 g2 c2", // Bass
         "e2 a2 g2 e2", // Middle voice
         NULL);
}

// Wrapper function to keep backward compatibility
void play_twinkle_twinkle(const audio_driver_t* driver) {
    play("Twinkle Twinkle Little Star", 120, &c_major, driver, "c4 c g g a a g2 f4 f e e d d c2");
}

// Mary Had a Little Lamb - enhanced version with accompaniment
void play_mary_had_a_little_lamb(const audio_driver_t* driver) {
    // Simple single-voice version using new API
    // play("Mary Had a Little Lamb (Simple)", 120, driver, "e4 d c d e e e2 d4 d d2 e4 g g2 e4 d c d e e e e d d e d c2", NULL);

    // Multi-voice version with bass accompaniment
    play("Mary Had a Little Lamb (With Bass)", 120, &c_major, driver,
         "e4 d c d e e e2 d4 d d2   e4 g g2   e4 d c  d e e e e d d e d   c1", // melody
         "c,2   g,,2   c,2   g,,2  d,2   g,,2  c,2   g,,2  c,2 g,,2 c,2  g,,2 d,2  g,,2 c,1", // bass
         NULL);
}

// Row Row Row Your Boat with correct 6/8 rhythm
void play_row_row_row(const audio_driver_t* driver) {
    play("Row Row Row Your Boat", 120, &c_major, driver,
         "c4. c4. c4 d8 e4. e4 d8 e4 f8 g2. c'8 c'8 c'8 g8 g8 g8 e8 e8 e8 c8 c8 c8 g4 f8 e4 d8 c2.");
}

// Test triplets with a simple example
void play_triplets(const audio_driver_t* driver) {
    const char* melody = "c4 d4 e8t f8t g8t a2 g4 f4 e8t d8t c8t d2";
    play("Triplet Test", 120, &c_major, driver);
}

void test_frequencies(void) {
    printf("=== Frequency Conversion Tests ===\n");

    const char* test_notes[] = {
        "c",   "d",   "e",  "f",  "g", "a", "b", // Octave 4 (middle C octave)
        "c'",  "d'",  "e'", // Octave 5
        "c,",  "g,", // Octave 3
        "cs",  "df",  "gs", "af", // Accidentals
        "c''", "a,,", // Multiple octaves
        "r4" // Rest
    };

    int num_freq_tests = sizeof(test_notes) / sizeof(test_notes[0]);

    for (int i = 0; i < num_freq_tests; i++) {
        const char* p = test_notes[i];
        int last_duration = 4;
        note_t note = parse_note(&p, &last_duration);

        double freq = note_to_frequency(&note, &equal_temperament);

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
    const char* note_tests[] = {"c4",  "gs2", "bf'", "r8",  "a''4", "d,2", "ess16", "bff8",
                                "c4.", "g2.", "r4.", "c8t", "d4q",  "e8x", "f4s"};

    int num_note_tests = sizeof(note_tests) / sizeof(note_tests[0]);
    for (int i = 0; i < num_note_tests; i++) {
        printf("Input: \"%s\" -> ", note_tests[i]);
        const char* p = note_tests[i];
        int last_duration = 4;
        note_t note = parse_note(&p, &last_duration);
        print_note(&note);
        printf("\n");
    }

    printf("\n=== String Parsing Tests (including chords) ===\n");
    const char* string_tests[] = {
        "c g a f", // Simple sequence
        "c4 d e f g", // Duration inheritance
        "c4 c g' g a a g2", // Twinkle Twinkle first line
        "f4 f e e d d c2", // Twinkle Twinkle second line
        "r4 c d r2 e", // With rests
        "c'' d, ess bf' r8 g4", // Mixed complex notes
        "c4. d8 e4. f8 g2.", // Dotted notes (6/8 time pattern)
        "c8t d8t e8t f4 g4", // Triplets
        "c4q d4q e4q f4q g4q a2", // Quintuplets
        "<c e g>4", // Basic chord
        "<c e' g>2", // Chord with octave shifts
        "c4 <c e g>2 d4", // Mixed single notes and chords
        "<c, e g'>4. <f a c''>2", // Complex chords with dots
    };

    int num_string_tests = sizeof(string_tests) / sizeof(string_tests[0]);
    for (int i = 0; i < num_string_tests; i++) {
        printf("Input: \"%s\"\n", string_tests[i]);
        note_array_t array = parse_music(string_tests[i]);
        printf("  ");
        print_note_array(&array);
        free_note_array(&array);
        printf("\n");
    }
}
