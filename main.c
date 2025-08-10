#include <stdio.h>
#include "audio_driver.h"
#include "pw_driver.h"
#include "music.h"
#include "test.h"

int main() {
    printf("Initializing simple audio test...\n");

    // Setup signal handling
    pw_driver_setup_signals();

    // Initialize music system
    music_init();

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
    printf("Expected: ADSR keyboard-style notes: C4 -> gap -> E4 -> gap -> G4 -> gap -> C5 -> end\n");

    // Run main loop (blocks until completion or interrupted)
    pw_driver_run_main_loop(audio_ctx);

    printf("Stopping playback...\n");
    driver->stop(audio_ctx);

    // Clean up
    cleanup_song(song);
    driver->cleanup(audio_ctx);
    printf("Test complete.\n");
    
    return 0;
}