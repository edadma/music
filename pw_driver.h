#ifndef PW_DRIVER_H
#define PW_DRIVER_H

#include "audio_driver.h"

// PipeWire implementation of audio_driver_t
extern const audio_driver_t pipewire_driver;

// Setup signal handling for clean shutdown (call from main)
void pw_driver_setup_signals(void);

// Run PipeWire main loop (blocks until completion or interrupted)
void pw_driver_run_main_loop(void *context);

#endif // PW_DRIVER_H