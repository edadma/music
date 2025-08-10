# PipeWire Music System Architecture Design

## Project Overview

**Goal**: Create a music synthesis system that works with PipeWire's pull-based audio architecture and is portable to Raspberry Pico. The system prioritizes real-time performance with minimal floating-point operations during sample generation.

**Key Constraints**:
- PipeWire pull-based callbacks (audio system requests samples via callback)
- Eventual Raspberry Pico port (32-bit ARM, limited memory)
- Real-time performance: no malloc() during playback, minimal floating-point math
- Pre-computed music parsing with chronologically sorted sequencer events

## Audio Driver Interface

### Polymorphic Driver Design
```c
typedef bool (*audio_callback_t)(int16_t *buffer, size_t num_samples, void *user_data);

typedef struct {
    void* (*init)(uint32_t sample_rate, audio_callback_t callback, int *error);
    void (*play)(void *context, void *user_data);
    void (*stop)(void *context);
    void (*resume)(void *context);
    void (*cleanup)(void *context);
    const char* (*strerror)(int error_code);
} audio_driver_t;
```

### Usage Pattern
```c
// Audio system provides constant vtable
extern const audio_driver_t pipewire_driver;

// Music system uses it
const audio_driver_t *driver = &pipewire_driver;
void *audio_ctx = driver->init(44100, sequencer_callback, &error);
if (!audio_ctx) { handle_error(driver->strerror(error)); }

song_state_t *song = create_song_state();
driver->play(audio_ctx, song);
// ... later
driver->stop(audio_ctx);
free(song);  // Music system owns and frees user_data
driver->cleanup(audio_ctx);
```

### Memory Management Contract
- **Natural song end**: Callback returns `false` and frees user_data itself. Audio driver guarantees no further callback calls with that user_data.
- **Forced stop**: Music system calls `stop()`, then frees user_data after stop() returns. Audio driver guarantees callback has stopped when stop() returns.
- **Audio driver**: Never frees user_data, only manages audio system connection.

## Fixed-Point Arithmetic System

**Primary Format**: Q1.31 throughout for maximum precision on 32-bit systems
- `0x00000000` = 0.0
- `0x7FFFFFFF` ≈ 1.0
- `0x80000000` = -1.0

### Pre-computation Strategy
- **Parse time**: All floating-point math, convert final values to Q1.31
- **Real-time**: Pure integer arithmetic, table lookups, bit shifts only

### Lookup Tables
- **Sine table**: 1024 entries, Q1.31 format (tested, gives excellent quality)
- **Exponential table**: For envelope decay curves, maps time → Q1.31 multiplier

## Event Structure and Memory Management

### Core Event Structure
```c
typedef struct {
    int32_t phase_accum;       // Q1.31 current phase
    int32_t phase_increment;   // Q1.31 phase step per sample  
    int32_t amplitude;         // Q1.31 amplitude for this partial
} partial_t;

typedef struct {
    // === Timing (immutable) ===
    uint32_t start_sample;        // When event begins
    uint32_t duration_samples;    // Written note duration (for spacing)
    uint32_t release_sample;      // When envelope starts release (for articulation)
    
    // === Audio Properties (immutable) ===
    instrument_t *instrument;     // Envelope & waveform functions
    int32_t volume_scale;         // Pre-computed Q1.31 volume multiplier
    
    // === Envelope State (mutable) ===
    union {
        struct {
            int32_t attack_samples, decay_samples, release_samples;
            int32_t sustain_level;    // Q1.31
            int32_t current_level;    // Q1.31
            uint8_t phase;            // ENV_ATTACK, ENV_DECAY, etc.
        } adsr;
        
        struct {
            int32_t initial_amplitude; // Q1.31
            int32_t decay_multiplier;  // Q1.31 per-sample decay
        } pluck;
    } envelope_state;
    
    // === Variable Partial Data ===
    uint8_t num_partials;
    partial_t partials[];         // Flexible array - partials[0] = fundamental
} event_t;
```

### Memory Efficiency
- **Flexible array**: Events allocated with `sizeof(event_t) + num_partials * sizeof(partial_t)`
- **Pico-friendly**: Simple instruments use minimal memory, complex instruments scale appropriately
- **Cache-friendly**: All event data contiguous in memory

### Articulation Support
**Key Innovation**: `release_sample` separate from `duration_samples` enables musical articulation:
- **Staccato**: `release_sample = start_sample + (duration_samples * 0.5)`
- **Legato**: `release_sample = start_sample + duration_samples + overlap_samples`
- **Tenuto**: `release_sample = start_sample + duration_samples`

## Envelope System

### Generic Envelope Interface
```c
typedef int32_t (*envelope_fn_t)(void *envelope_state, uint32_t samples_since_start, int32_t samples_until_release);
// Returns Q1.31 amplitude value
```

### Envelope Timing Logic
- **samples_since_start**: `current_sample_index - event->start_sample`
- **samples_until_release**: `event->release_sample - current_sample_index`
- **Envelope function**: Uses both parameters to determine current phase (attack/decay/sustain/release)

### ADSR Implementation Strategy
```c
if (samples_until_release <= 0) {
    // In release phase - use samples since release began
} else if (samples_since_start < attack_samples) {
    // In attack phase
} else if (samples_since_start < attack_samples + decay_samples) {
    // In decay phase  
} else {
    // In sustain phase
}
```

## Instrument and Volume System

### Instrument Structure
```c
typedef struct {
    envelope_fn_t envelope;
    uint8_t num_partials;
    float harmonic_ratios[MAX_PARTIALS];     // 1.0, 2.0, 3.0 for harmonics (parse time)
    float partial_amplitudes[MAX_PARTIALS]; // Relative amplitudes (parse time)
} instrument_t;
```

### Hierarchical Volume Control
**Philosophy**: All volume scaling pre-computed during parsing to avoid real-time fluctuations.

1. **Song Volume**: `1.0 / sqrt(max_expected_voices)` - global scaling
2. **Chord Volume**: `1.0 / sqrt(chord_size)` - for simultaneous notes
3. **Event Volume**: velocity × dynamics × articulation
4. **Instrument Volume**: `1.0 / num_partials` - for additive synthesis

**Final Calculation** (during notes→events conversion):
```c
float song_scale = 1.0f / sqrtf(max_voices);
float chord_scale = 1.0f / sqrtf(chord_sizes[note->chord_id]);
float event_volume = note->velocity * note->dynamics;

event.volume_scale = (int32_t)(song_scale * chord_scale * event_volume * 0x7FFFFFFF);
```

## Additive Synthesis Design

### Partial Representation
- **partials[0]**: Fundamental frequency (note pitch)
- **partials[1..n]**: Harmonics/overtones with independent frequencies and amplitudes
- **Each partial**: Independent phase accumulator and amplitude

### Sample Generation
```c
int32_t event_sample = 0;
for (int i = 0; i < event->num_partials; i++) {
    partial_t *p = &event->partials[i];
    
    // Get waveform sample (Q1.31)
    uint32_t table_index = p->phase_accum >> 22;  // Convert to table index
    int32_t wave_sample = sine_table[table_index];
    
    // Apply partial amplitude
    int64_t scaled = (int64_t)wave_sample * p->amplitude;
    event_sample += (int32_t)(scaled >> 31);
    
    // Advance phase
    p->phase_accum += p->phase_increment;
}

// Apply envelope and final volume scaling
int32_t envelope_val = event->instrument->envelope(&event->envelope_state, samples_since_start, samples_until_release);
int64_t final_sample = (int64_t)event_sample * envelope_val * event->volume_scale;
return (int16_t)(final_sample >> 47);  // Convert to S16
```

## Callback Architecture

### Sequencer State Management
```c
#define MAX_SIMULTANEOUS_EVENTS 32  // Worst-case simultaneous voices

typedef struct {
    // === Song Data (immutable during playback) ===
    event_t **events;              // Chronologically sorted array of event pointers
    size_t num_events;             // Total events in song
    uint32_t sample_rate;          // For timing calculations if needed
    
    // === Playback State (mutable) ===
    uint64_t current_sample_index; // Global position in song timeline
    size_t next_event_index;       // Next event in events[] to potentially activate
    
    // === Active Events Management ===
    event_t *active_events[MAX_SIMULTANEOUS_EVENTS];  // Currently playing events
    size_t num_active;             // Current number of active events
} sequencer_state_t;
```

### Event Lifecycle Management
**Activation**: Events added to `active_events[]` when `current_sample_index >= event->start_sample`

**Removal**: Events removed when envelope level drops below audible threshold:
```c
#define AUDIBLE_THRESHOLD 0x00000100  // Q1.31 threshold for inaudible level

// Remove inaudible events (handles natural decay, legato release, etc.)
for (int i = num_active - 1; i >= 0; i--) {  // Backwards iteration for safe removal
    int32_t envelope_level = get_envelope_level(active_events[i]);
    if (envelope_level < AUDIBLE_THRESHOLD) {
        active_events[i] = active_events[num_active - 1];  // Swap with last
        num_active--;
    }
}
```

**Memory Ownership**: `sequencer_state_t` is the `user_data` owned by music library, freed when callback returns `false`.

### Callback Logic Flow
```c
bool sequencer_callback(int16_t *buffer, size_t num_samples, void *user_data) {
    sequencer_state_t *seq = (sequencer_state_t*)user_data;
    
    for (size_t i = 0; i < num_samples; i++) {
        // 1. Activate new events
        while (seq->next_event_index < seq->num_events && 
               seq->events[seq->next_event_index]->start_sample <= seq->current_sample_index) {
            add_active_event(seq, seq->events[seq->next_event_index]);
            seq->next_event_index++;
        }
        
        // 2. Remove finished events  
        remove_finished_events(seq);
        
        // 3. Generate sample from active events
        int32_t mixed_sample = 0;
        for (size_t j = 0; j < seq->num_active; j++) {
            mixed_sample += generate_event_sample(seq->active_events[j], seq->current_sample_index);
        }
        
        buffer[i] = (int16_t)mixed_sample;
        seq->current_sample_index++;
    }
    
    // Return false when song complete and free sequencer_state
    if (seq->num_active == 0 && seq->next_event_index >= seq->num_events) {
        free_sequencer_state(seq);
        return false;  // Song finished
    }
    
    return true;  // Continue playback
}
```

## Integration with Existing Parser

**Current System**: Already parses polyphonic music, handles chord parsing with `chord_id` assignment, creates chronologically arranged data.

**Reuse Strategy**: Keep existing music parsing completely intact. Add new conversion layer:
`notes[] → events[]` that:
1. Converts floating-point to Q1.31
2. Pre-computes phase increments
3. Calculates volume scaling based on chord groupings
4. Allocates events with appropriate number of partials
5. Sorts events chronologically by start_sample

## Next Implementation Steps

1. **Simple PipeWire Test**: Hard-coded events with basic sine wave synthesis
2. **Envelope Integration**: Add ADSR envelope to test events
3. **Multi-partial Test**: Test additive synthesis with 2-3 harmonics
4. **Parser Integration**: Connect existing music parser to new event system
5. **Performance Optimization**: Profile and optimize for real-time constraints
6. **Pico Port Preparation**: Ensure all code is ready for embedded constraints

## Critical Design Benefits

- **Portability**: Same callback works for PipeWire and Pico IRQ
- **Performance**: All heavy computation moved to parse time
- **Memory Efficiency**: Flexible arrays scale to actual instrument complexity
- **Musical Flexibility**: Articulation support enables expressive performance
- **Maintainability**: Clean separation between audio systems and music logic