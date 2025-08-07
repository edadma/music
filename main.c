#include <stdio.h>
#include <stdlib.h>
#include <tgmath.h>
#include <time.h>

#include "music.h"
#include "pa.h"

void sleep_ms(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

int main(void) {
    play_tone_pulse(440.0, 1000, 0.5); // A4 for 1 second at half volume
    // test_parser();
    // test_frequencies();
    return 0;
}
