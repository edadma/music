#include "pw_driver.h"
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

// ============================================================================
// PIPEWIRE TYPES
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

// ============================================================================
// PIPEWIRE GLOBAL STATE
// ============================================================================

static volatile bool running = true;
static struct pw_main_loop *g_main_loop = NULL;  // For signal handler

static void signal_handler(int sig) {
    (void)sig;  // Unused parameter
    running = false;
    if (g_main_loop) {
        pw_main_loop_quit(g_main_loop);
    }
}

void pw_driver_setup_signals(void) {
    signal(SIGINT, signal_handler);
}

void pw_driver_run_main_loop(void *context) {
    pw_audio_context_t *ctx = (pw_audio_context_t*)context;
    pw_main_loop_run(ctx->loop);
}

// ============================================================================
// PIPEWIRE DRIVER IMPLEMENTATION
// ============================================================================

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

static void* pipewire_init(uint32_t sample_rate, audio_callback_t callback, int *error) {
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

static void pw_play(void *context, void *user_data) {
    pw_audio_context_t *ctx = context;
    ctx->user_data = user_data;
    ctx->playing = true;
    printf("Started playback\n");
}

static void pw_stop(void *context) {
    pw_audio_context_t *ctx = context;
    ctx->playing = false;
    printf("Stopped playback\n");
}

static void pw_resume(void *context) {
    pw_audio_context_t *ctx = context;
    if (ctx->user_data) {
        ctx->playing = true;
        printf("Resumed playback\n");
    }
}

static void pw_cleanup(void *context) {
    pw_audio_context_t *ctx = context;

    if (ctx->stream) pw_stream_destroy(ctx->stream);
    if (ctx->core) pw_core_disconnect(ctx->core);
    if (ctx->context) pw_context_destroy(ctx->context);
    if (ctx->loop) pw_main_loop_destroy(ctx->loop);

    free(ctx);
    pw_deinit();
}

static const char* pw_strerror(int error_code) {
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