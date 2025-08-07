#include "pa.h"

#include <math.h>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void play_tone_pulse(double frequency, int duration_ms, float volume) {
    const int sample_rate = 44100;
    const int channels = 1; // Mono
    const int samples_per_ms = sample_rate / 1000;
    const int total_samples = duration_ms * samples_per_ms;

    // Audio format specification
    pa_sample_spec ss = {.format = PA_SAMPLE_FLOAT32LE, .channels = channels, .rate = sample_rate};

    // Create PulseAudio connection
    int error;
    pa_simple* s = pa_simple_new(NULL, "Music Player", PA_STREAM_PLAYBACK, NULL, "music", &ss, NULL, NULL, &error);
    if (!s) {
        printf("PulseAudio error: %s\n", pa_strerror(error));
        return;
    }

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
    pa_simple_write(s, samples, total_samples * sizeof(float), &error);
    pa_simple_drain(s, &error); // Wait for playback to complete

    // Cleanup
    free(samples);
    pa_simple_free(s);
}
