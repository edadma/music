#ifndef AUDIO_DRIVER_H
#define AUDIO_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Audio callback function type - completely audio system agnostic
// Returns: true = continue playback, false = song finished
typedef bool (*audio_callback_t)(int16_t *buffer, size_t num_samples, void *user_data);

// Generic audio driver interface (vtable pattern)
typedef struct {
    void* (*init)(uint32_t sample_rate, audio_callback_t callback, int *error);
    void (*play)(void *context, void *user_data);
    void (*stop)(void *context);
    void (*resume)(void *context);
    void (*cleanup)(void *context);
    const char* (*strerror)(int error_code);
} audio_driver_t;

#endif // AUDIO_DRIVER_H