#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "music.h"
#include "pulseaudio_driver.h"

void sleep_ms(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

int main(void) {
    // Test single tone first
    // printf("Testing single A4 tone...\n");
    // play_tone_pulse(440.0, 1000, 0.5); // A4 for 1 second at half volume
    // sleep_ms(500);

    // Test the melody
    // const char* custom_melody = "c4 d e f g a b c'2 r4 c' b a g f e d c2";
    // test_play_melody("C Major Scale", custom_melody, 140, &pulseaudio_driver);
    test_row_row_row(&pulseaudio_driver);

    // Uncomment these for additional tests:
    // test_parser();
    // test_frequencies();

    return 0;
}
