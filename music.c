#include "music.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool is_valid_note_name(char c) { return (c >= 'a' && c <= 'g') || (c == 'r'); }

bool is_rest(const note_t* note) { return note && note->note_name == 'r'; }

bool is_dotted(const note_t* note) { return note && note->dotted; }

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

    // Parse dots for dotted notes
    if (*p == '.') {
        p++;
        note.dotted = true;
    }

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

    // Print dot if dotted
    if (note->dotted) {
        printf(".");
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

float pluck_sine_instrument_samples(const sequencer_event_t* event, int sample_index, int sample_rate) {
    // Generate sine wave
    float sine = sin(2.0 * M_PI * event->frequency * sample_index / sample_rate);
    // Apply pluck envelope (exponential decay)
    float t = (float)sample_index / event->duration_samples;
    float envelope = exp(-3.0 * t);

    return event->volume * envelope * sine;
}

const instrument_t pluck_sine_instrument = {.name = "Pluck Sine", .event_to_samples = pluck_sine_instrument_samples};

void generate_samples(sequencer_event_t* events, int event_count, float* output_buffer, int buffer_size, int sample_rate) {
    memset(output_buffer, 0, buffer_size * sizeof(float));

    for (int e = 0; e < event_count; e++) {
        sequencer_event_t* event = &events[e];

        for (int i = 0; i < event->duration_samples; i++) {
            int output_index = event->start_sample + i;
            if (output_index >= buffer_size)
                break;

            // Let the instrument generate the sample
            float sample = event->instrument.event_to_samples(event, i, sample_rate);
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

// Convert note array to sequencer events
// tempo_bpm: beats per minute (e.g., 120)
// sample_rate: audio sample rate (e.g., 44100)
// reference: reference note for frequency calculation
// reference_freq: frequency of the reference note in Hz
// temperament: temperament system to use
// instrument: instrument to assign to all events
// volume: volume level (0.0 to 1.0)
sequencer_event_t* notes_to_events(const note_array_t* notes, int tempo_bpm, int sample_rate, const note_t* reference,
                                   double reference_freq, const temperament_t* temperament, const instrument_t* instrument,
                                   float volume) {
    if (!notes || !notes->notes || notes->count == 0) {
        return NULL;
    }

    sequencer_event_t* events = malloc(notes->count * sizeof(sequencer_event_t));
    if (!events) {
        return NULL;
    }

    // Calculate samples per beat (quarter note duration in samples)
    // 60 seconds/minute ÷ tempo_bpm × sample_rate = samples per beat
    int samples_per_beat = (60 * sample_rate) / tempo_bpm;

    int current_sample = 0;

    for (int i = 0; i < notes->count; i++) {
        const note_t* note = &notes->notes[i];

        // Calculate frequency (0.0 for rests)
        double freq = is_rest(note) ? 0.0 : note_to_frequency(note, reference, reference_freq, temperament);

        // Calculate duration in samples
        // Quarter note (4) gets samples_per_beat samples
        // Half note (2) gets samples_per_beat * 2 samples
        // Eighth note (8) gets samples_per_beat / 2 samples, etc.
        int duration_samples = (samples_per_beat * 4) / note->duration;

        // Handle dotted notes (1.5x duration)
        if (note->dotted) {
            duration_samples = (duration_samples * 3) / 2;
        }

        // Create the event
        events[i] = (sequencer_event_t){
            .frequency = freq,
            .start_sample = current_sample,
            .duration_samples = duration_samples,
            .volume = volume,
            .instrument = *instrument // Copy the instrument struct
        };

        current_sample += duration_samples;
    }

    return events;
}

// Generalized melody player function
void test_play_melody(const char* song_name, const char* melody, int tempo_bpm, const audio_driver_t* driver) {
    printf("=== Playing %s with Sequencer ===\n", song_name);
    printf("Melody: %s\n", melody);
    printf("Tempo: %d BPM\n\n", tempo_bpm);

    // Parse the melody
    note_array_t notes = parse_string(melody);
    print_note_array(&notes);

    // Set up reference note and temperament
    note_t middle_c = {.note_name = 'c', .octave_shift = 0, .duration = 4};
    double middle_c_freq = 261.625565; // Middle C frequency

    // Convert notes to sequencer events
    const int sample_rate = 44100;

    printf("\nConverting to sequencer events...\n");

    sequencer_event_t* events = notes_to_events(&notes, tempo_bpm, sample_rate, &middle_c, middle_c_freq, &equal_temperament,
                                                &pluck_sine_instrument, 0.3f);

    if (!events) {
        printf("Error: Failed to convert notes to events\n");
        free_note_array(&notes);
        return;
    }

    // Print the events for debugging
    for (int i = 0; i < notes.count; i++) {
        printf("Event %d: ", i);
        print_note(&notes.notes[i]);
        printf(" -> %.1f Hz, start: %d, duration: %d samples\n", events[i].frequency, events[i].start_sample,
               events[i].duration_samples);
    }

    printf("Playing sequence with %d events using %s driver...\n", notes.count, driver->name);
    // Play the sequence using the sequencer and provided audio driver
    play_sequence(events, notes.count, (audio_driver_t*)driver, sample_rate);

    printf("%s complete!\n\n", song_name);

    // Cleanup
    free(events);
    free_note_array(&notes);
}

// Wrapper function to keep backward compatibility
void test_twinkle_twinkle(const audio_driver_t* driver) {
    const char* melody = "c4 c g g a a g2 f4 f e e d d c2";
    test_play_melody("Twinkle Twinkle Little Star", melody, 120, driver);
}

// New test for Row Row Row Your Boat
void test_row_row_row(const audio_driver_t* driver) {
    // Traditional "Row Row Row Your Boat" melody in 6/8 time
    // Row row row your boat, gently down the stream
    // Merrily merrily merrily merrily, life is but a dream
    const char* melody = "c4. c4. c4 d8 e4. e4 d8 e4 f8 g2. c'8 c'8 c'8 g8 g8 g8 e8 e8 e8 c8 c8 c8 g4 f8 e4 d8 c2.";
    test_play_melody("Row Row Row Your Boat", melody, 120, driver);
}

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
    const char* note_tests[] = {"c4", "gs2", "bf'", "r8", "a''4", "d,2", "ess16", "bff8", "c4.", "g2.", "r4."};

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
        "c4. d8 e4. f8 g2.", // Dotted notes (6/8 time pattern)
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
