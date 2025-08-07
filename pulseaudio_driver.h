#ifndef PA_H
#define PA_H

// Play a single tone using PulseAudio
// frequency: Hz (0.0 for silence/rest)
// duration_ms: milliseconds to play
// volume: 0.0 to 1.0
void play_tone_pulse(double frequency, int duration_ms, float volume);

#endif // PA_H
