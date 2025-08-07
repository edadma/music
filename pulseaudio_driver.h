#ifndef PULSEAUDIO_DRIVER_H
#define PULSEAUDIO_DRIVER_H

#include "music.h"

// Play a single tone using PulseAudio
// frequency: Hz (0.0 for silence/rest)
// duration_ms: milliseconds to play
// volume: 0.0 to 1.0
void play_tone_pulse(double frequency, int duration_ms, float volume);

extern const audio_driver_t pulseaudio_driver;

#endif // PULSEAUDIO_DRIVER_H
