// Simple PipeWire Audio Test - Proves core architecture
// Compile: gcc -o audio_test audio_test.c -lpipewire-0.3 -lm

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

// ============================================================================
// GLOBAL STATE FOR PIPEWIRE CONTROL
// ============================================================================

static volatile bool running = true;
static struct pw_main_loop *g_main_loop = NULL;  // For signal handler

void signal_handler(int sig) {
    (void)sig;  // Unused parameter
    running = false;
    if (g_main_loop) {
        pw_main_loop_quit(g_main_loop);
    }
}

// ============================================================================
// CORE TYPE DEFINITIONS
// ============================================================================

typedef bool (*audio_callback_t)(int16_t *buffer, size_t num_samples, void *user_data);

typedef struct {
    void* (*init)(uint32_t sample_rate, audio_callback_t callback, int *error);
    void (*play)(void *context, void *user_data);
    void (*stop)(void *context);
    void (*resume)(void *context);
    void (*cleanup)(void *context);
    const char* (*strerror)(int error_code);
} audio_driver_t;

typedef struct {
    uint32_t phase_accum;      // Unsigned for proper DDS wraparound
    uint32_t phase_increment;  // Unsigned phase increment per sample
    int32_t amplitude;         // Q1.31 amplitude for this partial
} partial_t;

// Simple exponential decay envelope state
typedef struct {
    int32_t initial_amplitude; // Q1.31
    int32_t decay_multiplier;  // Q1.31 per-sample multiplier (e.g., 0.9999)
    int32_t current_level;     // Q1.31 current amplitude
} pluck_decay_t;

typedef int32_t (*envelope_fn_t)(void *envelope_state, uint32_t samples_since_start, int32_t samples_until_release);

typedef struct {
    envelope_fn_t envelope;
    uint8_t num_partials;
    float harmonic_ratios[8];     // For setup time
    float partial_amplitudes[8]; // For setup time
} instrument_t;

typedef struct {
    // === Timing (immutable) ===
    uint32_t start_sample;
    uint32_t duration_samples;
    uint32_t release_sample;

    // === Audio Properties (immutable) ===
    instrument_t *instrument;
    int32_t volume_scale;

    // === Envelope State (mutable) ===
    union {
        pluck_decay_t pluck;
    } envelope_state;

    // === Variable Partial Data ===
    uint8_t num_partials;
    partial_t partials[];
} event_t;

#define MAX_SIMULTANEOUS_EVENTS 32
#define AUDIBLE_THRESHOLD 0x00080000  // Higher threshold - about 1% of full scale

typedef struct {
    event_t **events;
    size_t num_events;
    uint32_t sample_rate;
    uint64_t current_sample_index;
    uint64_t total_duration_samples; // Total song length
    size_t next_event_index;
    event_t *active_events[MAX_SIMULTANEOUS_EVENTS];
    size_t num_active;
    bool completed;                // Set by callback when song ends, checked by main thread
} sequencer_state_t;

// ============================================================================
// GLOBAL SINE TABLE AND UTILITIES
// ============================================================================

#define SINE_TABLE_SIZE 1024
static int32_t sine_table[SINE_TABLE_SIZE];

void init_sine_table(void) {
    for (int i = 0; i < SINE_TABLE_SIZE; i++) {
        double angle = 2.0 * M_PI * i / SINE_TABLE_SIZE;
        sine_table[i] = (int32_t)(sin(angle) * 0x7FFFFFFF);
    }
}

// Convert frequency to phase increment (unsigned for DDS)
uint32_t freq_to_phase_increment(float freq, uint32_t sample_rate) {
    return (uint32_t)((freq / sample_rate) * 0x100000000LL);
}

// ============================================================================
// ENVELOPE FUNCTIONS
// ============================================================================

int32_t pluck_envelope(void *state, uint32_t samples_since_start, int32_t samples_until_release) {
    (void)samples_since_start;    // Unused for this envelope type
    (void)samples_until_release;  // Unused for this envelope type

    pluck_decay_t *pluck = (pluck_decay_t*)state;

    // Exponential decay: current_level *= decay_multiplier
    // Q1.31 * Q1.31 = Q2.62, shift back to Q1.31
    int64_t temp = (int64_t)pluck->current_level * pluck->decay_multiplier;
    pluck->current_level = (int32_t)(temp >> 31);

    return pluck->current_level;
}

// ============================================================================
// EVENT AND SAMPLE GENERATION
// ============================================================================

event_t* create_simple_event(uint32_t start_sample, float freq, float duration_sec, uint32_t sample_rate) {
    // Allocate event with one partial
    event_t *event = malloc(sizeof(event_t) + sizeof(partial_t));
    if (!event) return NULL;

    event->start_sample = start_sample;
    event->duration_samples = (uint32_t)(duration_sec * sample_rate);
    event->release_sample = start_sample + event->duration_samples;
    event->volume_scale = 0x08000000;  // About 1/16 volume in Q1.31 (much quieter)
    event->num_partials = 1;

    // Setup single partial (fundamental frequency)
    event->partials[0].phase_accum = 0;
    event->partials[0].phase_increment = freq_to_phase_increment(freq, sample_rate);
    event->partials[0].amplitude = 0x7FFFFFFF;  // Full amplitude for this partial

    // Setup pluck envelope (exponential decay with ~2 second decay time)
    event->envelope_state.pluck.initial_amplitude = 0x7FFFFFFF;
    // Decay multiplier: for 2 second decay, we want level to drop to ~1/e in 2 seconds
    // decay_multiplier = exp(-1/(2*sample_rate)) ≈ 0.999989 for 44.1kHz
    double decay_rate = 1.0 / (0.2 * sample_rate);  // 2 second time constant
    double multiplier = exp(-decay_rate);
    event->envelope_state.pluck.decay_multiplier = (int32_t)(multiplier * 0x7FFFFFFF);
    event->envelope_state.pluck.current_level = 0x7FFFFFFF;

    return event;
}

int16_t generate_event_sample(event_t *event, uint64_t current_sample_index) {
    uint32_t samples_since_start = current_sample_index - event->start_sample;
    int32_t samples_until_release = event->release_sample - current_sample_index;

    // Get envelope level
    int32_t envelope_level = pluck_envelope(&event->envelope_state.pluck,
                                           samples_since_start, samples_until_release);

    // Generate samples from all partials
    int32_t event_sample = 0;
    for (int i = 0; i < event->num_partials; i++) {
        partial_t *p = &event->partials[i];

        // Get sine wave sample using proper DDS lookup
        uint32_t table_index = (p->phase_accum >> 22) & (SINE_TABLE_SIZE - 1);
        int32_t wave_sample = sine_table[table_index];

        // Apply partial amplitude (Q1.31 * Q1.31 = Q2.62, shift back to Q1.31)
        int64_t partial_sample = (int64_t)wave_sample * p->amplitude;
        event_sample += (int32_t)(partial_sample >> 31);

        // Advance phase (unsigned arithmetic wraps properly)
        p->phase_accum += p->phase_increment;
    }

    // Apply envelope (Q1.31 * Q1.31 = Q2.62, shift back to Q1.31)
    int64_t enveloped_sample = (int64_t)event_sample * envelope_level;
    enveloped_sample >>= 31;

    // Apply volume scaling (Q1.31 * Q1.31 = Q2.62, shift back to Q1.31)
    int64_t final_sample = enveloped_sample * event->volume_scale;
    final_sample >>= 31;

    // Convert Q1.31 to S16 (shift by 16 more bits)
    return (int16_t)(final_sample >> 16);
}

// ============================================================================
// SEQUENCER CALLBACK
// ============================================================================

bool sequencer_callback(int16_t *buffer, size_t num_samples, void *user_data) {
    sequencer_state_t *seq = (sequencer_state_t*)user_data;

    for (size_t i = 0; i < num_samples; i++) {
        // 1. Activate new events that should start now
        while (seq->next_event_index < seq->num_events &&
               seq->events[seq->next_event_index]->start_sample <= seq->current_sample_index) {

            if (seq->num_active < MAX_SIMULTANEOUS_EVENTS) {
                seq->active_events[seq->num_active] = seq->events[seq->next_event_index];
                seq->num_active++;
                printf("Activated event %zu at sample %lu\n", seq->next_event_index, seq->current_sample_index);
            }
            seq->next_event_index++;
        }

        // 2. Generate sample from all active events
        int32_t mixed_sample = 0;
        for (size_t j = 0; j < seq->num_active; j++) {
            mixed_sample += generate_event_sample(seq->active_events[j], seq->current_sample_index);
        }

        buffer[i] = (int16_t)mixed_sample;

        // 3. Remove inaudible events (backwards iteration for safe removal)
        for (int j = seq->num_active - 1; j >= 0; j--) {
            if (seq->active_events[j]->envelope_state.pluck.current_level < AUDIBLE_THRESHOLD) {
                printf("Removing inaudible event at sample %lu\n", seq->current_sample_index);
                // Swap with last element and decrease count
                seq->active_events[j] = seq->active_events[seq->num_active - 1];
                seq->num_active--;
            }
        }

        seq->current_sample_index++;
    }

    // Check if song is complete
    if (seq->num_active == 0 && seq->next_event_index >= seq->num_events) {
        printf("Song complete, marking as finished\n");
        seq->completed = true;
        return false;  // Tell audio driver to stop calling us
    }

    return true;  // Continue playback
}

// ============================================================================
// PIPEWIRE DRIVER IMPLEMENTATION
// ============================================================================

typedef struct {
    struct pw_main_loop *loop;
    struct pw_context *context;
    struct pw_core *core;
    struct pw_stream *stream;
    audio_callback_t callback;
    void *user_data;
    bool playing;
} pw_audio_context_t;

static void on_process(void *userdata) {
    pw_audio_context_t *ctx = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    int16_t *samples;
    uint32_t n_samples;

    if ((b = pw_stream_dequeue_buffer(ctx->stream)) == NULL) {
        printf("Out of buffers\n");
        return;
    }

    buf = b->buffer;
    samples = buf->datas[0].data;
    n_samples = buf->datas[0].maxsize / sizeof(int16_t);

    if (ctx->playing && ctx->callback) {
        // Call our callback to fill the buffer
        bool continue_playing = ctx->callback(samples, n_samples, ctx->user_data);
        if (!continue_playing) {
            ctx->playing = false;
            ctx->user_data = NULL;  // Callback finished the song

            // PipeWire-specific: tell main loop to quit
            if (g_main_loop) {
                pw_main_loop_quit(g_main_loop);
            }
        }
    } else {
        // Fill with silence
        memset(samples, 0, n_samples * sizeof(int16_t));
    }

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = sizeof(int16_t);
    buf->datas[0].chunk->size = n_samples * sizeof(int16_t);

    pw_stream_queue_buffer(ctx->stream, b);
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_process,
};

void* pipewire_init(uint32_t sample_rate, audio_callback_t callback, int *error) {
    pw_audio_context_t *ctx = calloc(1, sizeof(pw_audio_context_t));
    if (!ctx) {
        *error = 1;
        return NULL;
    }

    pw_init(NULL, NULL);  // Initialize PipeWire library

    ctx->loop = pw_main_loop_new(NULL);
    g_main_loop = ctx->loop;  // Store for signal handler
    ctx->context = pw_context_new(pw_main_loop_get_loop(ctx->loop), NULL, 0);
    ctx->core = pw_context_connect(ctx->context, NULL, 0);
    ctx->callback = callback;
    ctx->playing = false;

    // Create stream
    ctx->stream = pw_stream_new_simple(
        pw_main_loop_get_loop(ctx->loop),
        "Audio Test",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Playback",
            PW_KEY_MEDIA_ROLE, "Music",
            NULL),
        &stream_events,
        ctx);

    // Setup audio format
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod *params[1];

    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
        &SPA_AUDIO_INFO_RAW_INIT(
            .format = SPA_AUDIO_FORMAT_S16,
            .channels = 1,
            .rate = sample_rate));

    pw_stream_connect(ctx->stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS,
                      params, 1);

    *error = 0;
    return ctx;
}

void pw_play(void *context, void *user_data) {
    pw_audio_context_t *ctx = context;
    ctx->user_data = user_data;
    ctx->playing = true;
    printf("Started playback\n");
}

void pw_stop(void *context) {
    pw_audio_context_t *ctx = context;
    ctx->playing = false;
    printf("Stopped playback\n");
}

void pw_resume(void *context) {
    pw_audio_context_t *ctx = context;
    if (ctx->user_data) {
        ctx->playing = true;
        printf("Resumed playback\n");
    }
}

void pw_cleanup(void *context) {
    pw_audio_context_t *ctx = context;

    if (ctx->stream) pw_stream_destroy(ctx->stream);
    if (ctx->core) pw_core_disconnect(ctx->core);
    if (ctx->context) pw_context_destroy(ctx->context);
    if (ctx->loop) pw_main_loop_destroy(ctx->loop);

    free(ctx);
    pw_deinit();
}

const char* pw_strerror(int error_code) {
    switch (error_code) {
        case 0: return "Success";
        case 1: return "Memory allocation failed";
        default: return "Unknown error";
    }
}

// PipeWire driver vtable
const audio_driver_t pipewire_driver = {
    .init = pipewire_init,
    .play = pw_play,
    .stop = pw_stop,
    .resume = pw_resume,
    .cleanup = pw_cleanup,
    .strerror = pw_strerror
};

// ============================================================================
// TEST SETUP AND MAIN
// ============================================================================

sequencer_state_t* create_test_song(uint32_t sample_rate) {
    sequencer_state_t *seq = calloc(1, sizeof(sequencer_state_t));
    seq->sample_rate = sample_rate;
    seq->current_sample_index = 0;
    seq->next_event_index = 0;
    seq->num_active = 0;
    seq->completed = false;

    // Create 4 test events with gaps (rests) between them
    seq->num_events = 4;
    seq->events = malloc(seq->num_events * sizeof(event_t*));

    // Event 0: C4 (261.63 Hz) at start, 1 second duration
    seq->events[0] = create_simple_event(0, 261.63f, 1.0f, sample_rate);

    // Event 1: E4 (329.63 Hz) after 0.5 second gap, 1 second duration
    seq->events[1] = create_simple_event(sample_rate * 1.5f, 329.63f, 1.0f, sample_rate);

    // Event 2: G4 (392.00 Hz) after 0.5 second gap, 1.5 second duration
    seq->events[2] = create_simple_event(sample_rate * 3.0f, 392.00f, 1.5f, sample_rate);

    // Event 3: C5 (523.25 Hz) after 1 second gap, 2 second duration
    seq->events[3] = create_simple_event(sample_rate * 5.5f, 523.25f, 2.0f, sample_rate);

    // Calculate total song duration (last event start + duration + some decay time)
    seq->total_duration_samples = sample_rate * 5.5f + sample_rate * 2.0f + sample_rate * 2.0f; // Extra time for decay

    printf("Created test song with %zu events\n", seq->num_events);
    printf("Event 0: C4 at sample 0\n");
    printf("Event 1: E4 at sample %u\n", (uint32_t)(sample_rate * 1.5f));
    printf("Event 2: G4 at sample %u\n", (uint32_t)(sample_rate * 3.0f));
    printf("Event 3: C5 at sample %u\n", (uint32_t)(sample_rate * 5.5f));
    printf("Total duration: %lu samples (%.1f seconds)\n",
           seq->total_duration_samples, seq->total_duration_samples / (float)sample_rate);

    return seq;
}

int main() {
    printf("Initializing simple audio test...\n");

    signal(SIGINT, signal_handler);
    init_sine_table();

    const audio_driver_t *driver = &pipewire_driver;
    int error;

    // Initialize audio system
    void *audio_ctx = driver->init(44100, sequencer_callback, &error);
    if (!audio_ctx) {
        printf("Failed to initialize audio: %s\n", driver->strerror(error));
        return 1;
    }

    // Create test song and start playback
    sequencer_state_t *song = create_test_song(44100);
    driver->play(audio_ctx, song);

    printf("Playing test song. Press Ctrl+C to stop.\n");
    printf("Expected: C4 -> gap -> E4 -> gap -> G4 -> gap -> C5 -> end\n");

    // Run PipeWire main loop (blocks until completion or interrupted)
    pw_audio_context_t *ctx = (pw_audio_context_t*)audio_ctx;
    pw_main_loop_run(ctx->loop);

    printf("Stopping playback...\n");
    driver->stop(audio_ctx);

    // Now safely clean up song state
    for (size_t i = 0; i < song->num_events; i++) {
        free(song->events[i]);
    }
    free(song->events);
    free(song);
    
    driver->cleanup(audio_ctx);
    printf("Test complete.\n");
    
    return 0;
}