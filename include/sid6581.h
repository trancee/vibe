#ifndef SID6581_H
#define SID6581_H

#include <stdint.h>
#include <stdbool.h>

// SID Memory Map
#define SID_MEM_START 0xD400
#define SID_MEM_END 0xD7FF
#define SID_MEM_SIZE 0x0400
#define SID_REG_COUNT 29

// Voice 1 Registers (base + offset)
#define SID_V1_FREQ_LO    0x00
#define SID_V1_FREQ_HI    0x01
#define SID_V1_PW_LO      0x02
#define SID_V1_PW_HI      0x03
#define SID_V1_CTRL       0x04
#define SID_V1_AD         0x05
#define SID_V1_SR         0x06

// Voice 2 Registers
#define SID_V2_FREQ_LO    0x07
#define SID_V2_FREQ_HI    0x08
#define SID_V2_PW_LO      0x09
#define SID_V2_PW_HI      0x0A
#define SID_V2_CTRL       0x0B
#define SID_V2_AD         0x0C
#define SID_V2_SR         0x0D

// Voice 3 Registers
#define SID_V3_FREQ_LO    0x0E
#define SID_V3_FREQ_HI    0x0F
#define SID_V3_PW_LO      0x10
#define SID_V3_PW_HI      0x11
#define SID_V3_CTRL       0x12
#define SID_V3_AD         0x13
#define SID_V3_SR         0x14

// Filter Registers
#define SID_FC_LO         0x15
#define SID_FC_HI         0x16
#define SID_RES_FILT      0x17
#define SID_MODE_VOL      0x18

// Misc Registers (Read-Only)
#define SID_POT_X         0x19
#define SID_POT_Y         0x1A
#define SID_OSC3          0x1B
#define SID_ENV3          0x1C

// Control Register Bits
#define SID_CTRL_GATE     0x01
#define SID_CTRL_SYNC     0x02
#define SID_CTRL_RING     0x04
#define SID_CTRL_TEST     0x08
#define SID_CTRL_TRI      0x10
#define SID_CTRL_SAW      0x20
#define SID_CTRL_PULSE    0x40
#define SID_CTRL_NOISE    0x80

// Filter/Mode Register Bits
#define SID_FILT_V1       0x01
#define SID_FILT_V2       0x02
#define SID_FILT_V3       0x04
#define SID_FILT_EXT      0x08
#define SID_MODE_LP       0x10
#define SID_MODE_BP       0x20
#define SID_MODE_HP       0x40
#define SID_MODE_3OFF     0x80

// Envelope states
typedef enum {
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE,
    ENV_IDLE
} envelope_state_t;

// Voice structure
typedef struct {
    // Frequency (16-bit)
    uint16_t frequency;
    
    // Pulse width (12-bit)
    uint16_t pulse_width;
    
    // Control register
    uint8_t control;
    
    // ADSR values
    uint8_t attack;
    uint8_t decay;
    uint8_t sustain;
    uint8_t release;
    
    // Oscillator state
    uint32_t accumulator;      // 24-bit phase accumulator
    uint32_t shift_register;   // 23-bit LFSR for noise
    
    // Envelope state
    envelope_state_t env_state;
    uint32_t env_counter;      // Envelope rate counter
    uint8_t env_level;         // Current envelope level (0-255)
    uint32_t env_rate;         // Current rate counter period
    uint8_t exp_counter;       // Exponential counter for decay/release
    
    // Sync/Ring state
    bool sync_bit;             // MSB of accumulator (for sync)
    bool prev_sync_bit;        // Previous MSB (for edge detection)
    
    // Output
    int16_t output;            // Waveform output after envelope
} SID_Voice;

// Filter structure
typedef struct {
    // Filter cutoff (11-bit)
    uint16_t cutoff;
    
    // Resonance (4-bit)
    uint8_t resonance;
    
    // Filter routing
    uint8_t filter_voices;     // Which voices go through filter
    
    // Filter mode
    uint8_t mode;              // LP/BP/HP/3OFF
    
    // Filter state variables (for state-variable filter)
    int32_t Vhp;               // High-pass output
    int32_t Vbp;               // Band-pass output
    int32_t Vlp;               // Low-pass output
} SID_Filter;

// Main SID structure
typedef struct {
    // Voices
    SID_Voice voice[3];
    
    // Filter
    SID_Filter filter;
    
    // Master volume (4-bit)
    uint8_t volume;
    
    // Paddle inputs
    uint8_t pot_x;
    uint8_t pot_y;
    
    // Register shadow (for write-only registers)
    uint8_t registers[SID_REG_COUNT];
    
    // Clock rate
    uint32_t clock_rate;
    
    // Sample rate
    uint32_t sample_rate;
    
    // Cycle counter for sample generation
    uint32_t cycle_count;
    uint32_t cycles_per_sample;
    
    // Audio output buffer
    int16_t *audio_buffer;
    uint32_t buffer_size;
    uint32_t buffer_pos;
} SID;

// Function declarations
void sid_init(SID *sid, uint32_t clock_rate, uint32_t sample_rate);
void sid_reset(SID *sid);
void sid_write(SID *sid, uint16_t addr, uint8_t data);
uint8_t sid_read(SID *sid, uint16_t addr);
void sid_clock(SID *sid, uint32_t cycles);
int16_t sid_output(SID *sid);

// Audio buffer management
void sid_set_audio_buffer(SID *sid, int16_t *buffer, uint32_t size);
uint32_t sid_get_samples(SID *sid);

// Internal functions (exposed for testing)
uint16_t sid_oscillator(SID_Voice *voice, SID_Voice *sync_source, bool ring_mod);
void sid_envelope_clock(SID_Voice *voice);
int16_t sid_filter_output(SID *sid, int32_t input);

#endif // SID6581_H
