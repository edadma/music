#include "pulseaudio_driver.h"

#include <math.h>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "music.h"

void* pulseaudio_init(int rate, int channels, int* error) {
    // Audio format specification
    pa_sample_spec ss = {.format = PA_SAMPLE_FLOAT32LE, .channels = channels, .rate = rate};
    pa_buffer_attr buffer_attr = {.maxlength = (uint32_t)-1,
                                  .tlength = rate / 50, // 20ms target latency
                                  .prebuf = rate / 200, // 5ms prebuffer (was -1)
                                  .minreq = rate / 200, // 5ms minimum request (was -1)
                                  .fragsize = (uint32_t)-1};

    // Create PulseAudio connection
    pa_simple* s = pa_simple_new(NULL, "Music Player", PA_STREAM_PLAYBACK, NULL, "music", &ss, NULL, &buffer_attr, error);

    if (!s) {
        printf("PulseAudio error: %s\n", pa_strerror(*error));
        return NULL;
    }

    int warmup_samples = 44100 / 4; // 250ms at 44.1kHz
    float* silence = calloc(warmup_samples, sizeof(float));

    if (pa_simple_write(s, silence, warmup_samples * sizeof(float), &error)) {
        free(silence);
        return error;
    }
    if (pa_simple_drain(s, &error))
        return error;
    free(silence);

    return s;
}

int pulseaudio_play(void* s, float* samples, int sample_count) {
    int error;

    if (pa_simple_write(s, samples, sample_count * sizeof(float), &error))
        return error;
    if (pa_simple_drain(s, &error))
        return error;
    return 0;
}

const audio_driver_t pulseaudio_driver = {
    .name = "PulseAudio", .init = pulseaudio_init, .play = pulseaudio_play, .cleanup = (void (*)(void*))pa_simple_free};

void play_tone_pulse(double frequency, int duration_ms, float volume) {
    const int sample_rate = 44100;
    const int samples_per_ms = sample_rate / 1000;
    const int total_samples = duration_ms * samples_per_ms;

    // Create PulseAudio connection
    int error;
    void* context = pulseaudio_driver.init(sample_rate, 1, &error);

    // Generate audio samples
    float* samples = malloc(total_samples * sizeof(float));

    if (frequency == 0.0) {
        // Rest - silence (zeros)
        memset(samples, 0, total_samples * sizeof(float));
    } else {
        // Generate sine wave
        for (int i = 0; i < total_samples; i++) {
            samples[i] = volume * sin(2.0 * M_PI * frequency * i / sample_rate);
        }
    }

    // Play the audio
    pulseaudio_driver.play(context, samples, total_samples);

    // Cleanup
    free(samples);
    pulseaudio_driver.cleanup(context);
}
