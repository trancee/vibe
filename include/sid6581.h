/**
 * @file sid6581.h
 * @brief MOS 6581/8580 Sound Interface Device (SID) Emulation
 * 
 * Hardware-accurate emulation based on:
 * - C64 Programmer's Reference Guide (Appendix O)
 * - reSID by Dag Lem (reverse-engineered from die photographs)
 * 
 * Key specifications from reSID:
 * - 24-bit phase accumulator oscillators (freq added to lower 16 bits)
 * - 23-bit LFSR noise generator clocked by bit 19 with 2-cycle pipeline
 * - LFSR feedback: bit0 = (bit22 | test) ^ bit17
 * - ADSR envelope with 15-bit rate counter and exponential decay
 * - Two-integrator-loop biquadratic state-variable filter
 */

#ifndef SID6581_H
#define SID6581_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// SID Memory Map and Register Definitions
// =============================================================================

#define SID_MEM_START   0xD400
#define SID_MEM_END     0xD7FF
#define SID_MEM_SIZE    0x0400
#define SID_REG_COUNT   29

// Voice 1 Registers (offset from base)
#define SID_V1_FREQ_LO  0x00    // Frequency Low Byte
#define SID_V1_FREQ_HI  0x01    // Frequency High Byte
#define SID_V1_PW_LO    0x02    // Pulse Width Low Byte
#define SID_V1_PW_HI    0x03    // Pulse Width High Byte (4 bits)
#define SID_V1_CTRL     0x04    // Control Register
#define SID_V1_AD       0x05    // Attack/Decay
#define SID_V1_SR       0x06    // Sustain/Release

// Voice 2 Registers
#define SID_V2_FREQ_LO  0x07
#define SID_V2_FREQ_HI  0x08
#define SID_V2_PW_LO    0x09
#define SID_V2_PW_HI    0x0A
#define SID_V2_CTRL     0x0B
#define SID_V2_AD       0x0C
#define SID_V2_SR       0x0D

// Voice 3 Registers
#define SID_V3_FREQ_LO  0x0E
#define SID_V3_FREQ_HI  0x0F
#define SID_V3_PW_LO    0x10
#define SID_V3_PW_HI    0x11
#define SID_V3_CTRL     0x12
#define SID_V3_AD       0x13
#define SID_V3_SR       0x14

// Filter Registers
#define SID_FC_LO       0x15    // Filter Cutoff Low (3 bits)
#define SID_FC_HI       0x16    // Filter Cutoff High (8 bits)
#define SID_RES_FILT    0x17    // Resonance / Filter Routing
#define SID_MODE_VOL    0x18    // Filter Mode / Master Volume

// Read-Only Registers
#define SID_POT_X       0x19    // Potentiometer X
#define SID_POT_Y       0x1A    // Potentiometer Y
#define SID_OSC3        0x1B    // Oscillator 3 Output
#define SID_ENV3        0x1C    // Envelope 3 Output

// =============================================================================
// Control Register Bits (Register 04/0B/12)
// =============================================================================

#define SID_CTRL_GATE   0x01    // Bit 0: Gate (trigger envelope)
#define SID_CTRL_SYNC   0x02    // Bit 1: Oscillator Sync
#define SID_CTRL_RING   0x04    // Bit 2: Ring Modulation
#define SID_CTRL_TEST   0x08    // Bit 3: Test (reset oscillator & hold)
#define SID_CTRL_TRI    0x10    // Bit 4: Triangle Waveform
#define SID_CTRL_SAW    0x20    // Bit 5: Sawtooth Waveform
#define SID_CTRL_PULSE  0x40    // Bit 6: Pulse Waveform
#define SID_CTRL_NOISE  0x80    // Bit 7: Noise Waveform

// =============================================================================
// Filter/Mode Register Bits (Register 17/18)
// =============================================================================

#define SID_FILT_V1     0x01    // Route Voice 1 through filter
#define SID_FILT_V2     0x02    // Route Voice 2 through filter
#define SID_FILT_V3     0x04    // Route Voice 3 through filter
#define SID_FILT_EXT    0x08    // Route External input through filter

#define SID_MODE_LP     0x10    // Low-Pass filter output
#define SID_MODE_BP     0x20    // Band-Pass filter output
#define SID_MODE_HP     0x40    // High-Pass filter output
#define SID_MODE_3OFF   0x80    // Disconnect Voice 3 from mixer

// =============================================================================
// Envelope Generator States
// The SID actually only has 3 states: ATTACK, DECAY_SUSTAIN, RELEASE
// The distinction between DECAY and SUSTAIN is just the envelope level
// =============================================================================

typedef enum {
    ENV_ATTACK,         // Rising from current level to peak (0xFF)
    ENV_DECAY,          // Falling from peak toward sustain level (legacy)
    ENV_SUSTAIN,        // At sustain level (legacy - actually DECAY checks level)
    ENV_RELEASE,        // Falling from current level to zero
    ENV_IDLE            // Legacy - not used in hardware
} envelope_state_t;

// =============================================================================
// Voice Structure - One per voice (3 total)
// Based on reSID reverse-engineering
// =============================================================================

typedef struct {
    // Oscillator (reSID: 24-bit accumulator, frequency added to lower 16 bits)
    uint16_t frequency;         // 16-bit frequency register
    uint16_t pulse_width;       // 12-bit pulse width register
    uint32_t accumulator;       // 24-bit phase accumulator
    
    // Noise Generator - 23-bit LFSR
    // reSID: Clocked when bit 19 of accumulator goes high
    // 2-cycle pipeline delay for shift register clocking
    uint32_t shift_register;    // 23-bit LFSR state
    uint32_t prev_bit19;        // Previous bit 19 for edge detection
    uint8_t  shift_pipeline;    // 2-cycle pipeline: bit0=clock1, bit1=clock2
    
    // Control Register
    uint8_t control;            // Full control byte
    
    // ADSR values (4-bit each, stored as index into rate table)
    uint8_t attack;             // Attack rate index (0-15)
    uint8_t decay;              // Decay rate index (0-15)
    uint8_t sustain;            // Sustain level (0-15)
    uint8_t release;            // Release rate index (0-15)
    
    // Envelope Generator
    // reSID: 15-bit rate counter, resets when reaching period value
    envelope_state_t env_state; // Current envelope state
    uint16_t env_counter;       // 15-bit rate counter (renamed from rate_counter)
    uint16_t env_rate;          // Current rate counter period
    uint8_t  exp_counter;       // Exponential counter (0-255)
    uint8_t  exp_period;        // Exponential counter period (1,2,4,8,16,30)
    uint8_t  env_level;         // 8-bit envelope output (0-255)
    bool     hold_zero;         // Envelope frozen at zero until gate on
    
    // Sync/Ring state
    bool sync_bit;              // MSB of accumulator (bit 23)
    bool prev_sync_bit;         // Previous MSB for edge detection
    
    // Voice output after envelope
    int16_t output;             // Final voice output (waveform * envelope)
} SID_Voice;

// =============================================================================
// Filter Structure - Two-integrator-loop biquadratic filter
// Based on reSID filter implementation
// =============================================================================

typedef struct {
    // Filter Registers
    uint16_t cutoff;            // 11-bit cutoff frequency (FC_LO:3 + FC_HI:8)
    uint8_t  resonance;         // 4-bit resonance (0-15)
    uint8_t  filter_voices;     // Voice filter routing bits (FILT1, FILT2, FILT3, FILTEX)
    uint8_t  mode;              // Filter mode bits (LP, BP, HP, 3OFF)
    
    // State Variable Filter State
    // Using 32-bit for internal precision, output is 16-bit
    int32_t Vhp;                // High-pass output (summer output)
    int32_t Vbp;                // Band-pass output (first integrator)
    int32_t Vlp;                // Low-pass output (second integrator)
    
    // Filter Coefficients (precomputed from cutoff/resonance)
    // Using fixed-point for efficiency
    int32_t w0;                 // Cutoff frequency coefficient (scaled)
    int32_t q_1024;             // 1024/Q coefficient for resonance
} SID_Filter;

// =============================================================================
// Main SID Structure
// =============================================================================

typedef struct {
    // Voices
    SID_Voice voice[3];
    
    // Filter
    SID_Filter filter;
    
    // Global
    uint8_t volume;             // 4-bit master volume (0-15)
    uint8_t pot_x;              // Potentiometer X value
    uint8_t pot_y;              // Potentiometer Y value
    
    // Register shadow (for write-only register reads/debugging)
    uint8_t registers[SID_REG_COUNT];
    
    // Clock and sample generation
    uint32_t clock_rate;        // System clock (PAL: 985248, NTSC: 1022727)
    uint32_t sample_rate;       // Audio output sample rate (e.g., 44100)
    uint32_t cycles_per_sample; // Clock cycles per audio sample (fixed-point)
    uint32_t cycle_count;       // Cycle accumulator for sample timing
    
    // External audio input
    int16_t ext_input;          // External audio input for filter
    
    // Audio buffer (for batch processing)
    int16_t *audio_buffer;      // Pointer to output buffer
    uint32_t buffer_size;       // Buffer capacity in samples
    uint32_t buffer_pos;        // Current write position
} SID;

// =============================================================================
// Public API Functions
// =============================================================================

/**
 * Initialize SID chip
 * @param sid Pointer to SID structure
 * @param clock_rate System clock frequency (Hz)
 * @param sample_rate Audio output sample rate (Hz)
 */
void sid_init(SID *sid, uint32_t clock_rate, uint32_t sample_rate);

/**
 * Reset SID chip to power-on state
 * @param sid Pointer to SID structure
 */
void sid_reset(SID *sid);

/**
 * Write to SID register
 * @param sid Pointer to SID structure
 * @param addr Register address (0x00-0x1C)
 * @param data Data byte to write
 */
void sid_write(SID *sid, uint16_t addr, uint8_t data);

/**
 * Read from SID register
 * @param sid Pointer to SID structure
 * @param addr Register address
 * @return Data byte (only valid for read registers 0x19-0x1C)
 */
uint8_t sid_read(SID *sid, uint16_t addr);

/**
 * Clock SID for specified number of cycles
 * @param sid Pointer to SID structure
 * @param cycles Number of clock cycles to execute
 */
void sid_clock(SID *sid, uint32_t cycles);

/**
 * Generate single audio sample
 * @param sid Pointer to SID structure
 * @return 16-bit signed audio sample
 */
int16_t sid_output(SID *sid);

/**
 * Set external audio buffer for batch processing
 * @param sid Pointer to SID structure
 * @param buffer Pointer to audio buffer
 * @param size Buffer size in samples
 */
void sid_set_audio_buffer(SID *sid, int16_t *buffer, uint32_t size);

/**
 * Get number of samples generated in buffer
 * @param sid Pointer to SID structure
 * @return Number of samples in buffer
 */
uint32_t sid_get_samples(SID *sid);

// =============================================================================
// Internal Functions (exposed for testing)
// =============================================================================

/**
 * Generate waveform output for voice
 * @param voice Pointer to voice
 * @param sync_source Pointer to sync source voice (voice N-1, wrapping)
 * @param ring_mod Ring modulation enabled
 * @return 12-bit waveform output (0-4095)
 */
uint16_t sid_oscillator(SID_Voice *voice, SID_Voice *sync_source, bool ring_mod);

/**
 * Clock envelope generator for one cycle
 * Uses reSID rate counter periods and exponential decay
 * @param voice Pointer to voice structure
 */
void sid_envelope_clock(SID_Voice *voice);

/**
 * Process filter with input signal
 * @param sid Pointer to SID structure
 * @param input Filter input signal (sum of filtered voices)
 * @return Filtered output signal
 */
int16_t sid_filter_output(SID *sid, int32_t input);

#endif // SID6581_H
