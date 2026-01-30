/*
 * C64 Emulator - SID 6581/8580 Sound Chip Header
 * 
 * Implements:
 * - 3 oscillators (triangle, sawtooth, pulse, noise)
 * - ADSR envelope generators
 * - State Variable Filter (LP/BP/HP)
 * - Register interface
 */

#ifndef SID6581_H
#define SID6581_H

#include <stdint.h>
#include <stdbool.h>

/* SID Memory Map */
#define SID_BASE        0xD400
#define SID_END         0xD7FF
#define SID_SIZE        0x0020  /* 32 registers (mirrored) */
#define SID_MEM_START   0x00    /* For offset-based addressing in tests */

/* Voice Register Offsets (add voice*7 for voice 1-3) */
#define SID_FREQ_LO     0x00    /* Frequency Low Byte */
#define SID_FREQ_HI     0x01    /* Frequency High Byte */
#define SID_PW_LO       0x02    /* Pulse Width Low Byte */
#define SID_PW_HI       0x03    /* Pulse Width High Nybble */
#define SID_CTRL        0x04    /* Control Register */
#define SID_AD          0x05    /* Attack/Decay */
#define SID_SR          0x06    /* Sustain/Release */

/* Voice 1 Registers (absolute offsets) */
#define SID_V1_FREQ_LO  0x00
#define SID_V1_FREQ_HI  0x01
#define SID_V1_PW_LO    0x02
#define SID_V1_PW_HI    0x03
#define SID_V1_CTRL     0x04
#define SID_V1_AD       0x05
#define SID_V1_SR       0x06

/* Voice 2 Registers (absolute offsets) */
#define SID_V2_FREQ_LO  0x07
#define SID_V2_FREQ_HI  0x08
#define SID_V2_PW_LO    0x09
#define SID_V2_PW_HI    0x0A
#define SID_V2_CTRL     0x0B
#define SID_V2_AD       0x0C
#define SID_V2_SR       0x0D

/* Voice 3 Registers (absolute offsets) */
#define SID_V3_FREQ_LO  0x0E
#define SID_V3_FREQ_HI  0x0F
#define SID_V3_PW_LO    0x10
#define SID_V3_PW_HI    0x11
#define SID_V3_CTRL     0x12
#define SID_V3_AD       0x13
#define SID_V3_SR       0x14

/* Filter/Volume Register Offsets */
#define SID_FC_LO       0x15    /* Filter Cutoff Low */
#define SID_FC_HI       0x16    /* Filter Cutoff High */
#define SID_RES_FILT    0x17    /* Resonance / Filter Enable */
#define SID_MODE_VOL    0x18    /* Filter Mode / Volume */

/* Read-Only Registers */
#define SID_POT_X       0x19    /* Potentiometer X */
#define SID_POT_Y       0x1A    /* Potentiometer Y */
#define SID_OSC3        0x1B    /* Oscillator 3 Output */
#define SID_ENV3        0x1C    /* Envelope 3 Output */

/* Control Register Bits */
#define SID_CTRL_GATE   0x01    /* Gate bit */
#define SID_CTRL_SYNC   0x02    /* Sync oscillator */
#define SID_CTRL_RING   0x04    /* Ring modulation */
#define SID_CTRL_TEST   0x08    /* Test bit */
#define SID_CTRL_TRI    0x10    /* Triangle waveform */
#define SID_CTRL_SAW    0x20    /* Sawtooth waveform */
#define SID_CTRL_PULSE  0x40    /* Pulse waveform */
#define SID_CTRL_NOISE  0x80    /* Noise waveform */

/* Filter Mode Bits (raw register values) */
#define SID_FILT_V1     0x01    /* Voice 1 through filter */
#define SID_FILT_V2     0x02    /* Voice 2 through filter */
#define SID_FILT_V3     0x04    /* Voice 3 through filter */
#define SID_FILT_EXT    0x08    /* External input through filter */
#define SID_MODE_LP     0x10    /* Low-pass filter (raw register bit) */
#define SID_MODE_BP     0x20    /* Band-pass filter (raw register bit) */
#define SID_MODE_HP     0x40    /* High-pass filter (raw register bit) */
#define SID_MODE_3OFF   0x80    /* Voice 3 off (raw register bit) */

/* Normalized filter mode bits (after extraction from register) */
#define SID_FMODE_LP    0x01    /* Low-pass filter */
#define SID_FMODE_BP    0x02    /* Band-pass filter */
#define SID_FMODE_HP    0x04    /* High-pass filter */
#define SID_FMODE_3OFF  0x08    /* Voice 3 off */

/* Envelope States */
typedef enum {
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE,
    ENV_IDLE
} EnvelopeState;

/* SID Voice Structure */
typedef struct {
    /* Frequency (16-bit) */
    uint16_t frequency;
    
    /* Pulse width (12-bit) */
    uint16_t pulse_width;
    
    /* Control register */
    uint8_t control;
    
    /* ADSR values (4-bit each) */
    uint8_t attack;
    uint8_t decay;
    uint8_t sustain;
    uint8_t release;
    
    /* Oscillator state */
    uint32_t accumulator;      /* 24-bit phase accumulator */
    uint32_t noise_lfsr;       /* 23-bit LFSR for noise */
    
    /* Envelope state */
    EnvelopeState env_state;
    uint32_t env_counter;      /* Rate counter */
    uint8_t env_level;         /* Current level (0-255) */
    uint8_t exp_counter;       /* Exponential decay counter */
    
    /* Sync state */
    bool msb_rising;           /* MSB transition for sync */
    
    /* Output */
    int16_t output;            /* Current waveform output */
} SIDVoice;

/* SID Filter Structure */
typedef struct {
    uint16_t cutoff;           /* 11-bit cutoff frequency */
    uint8_t resonance;         /* 4-bit resonance */
    uint8_t filter_enable;     /* Which voices through filter */
#define filter_voices filter_enable  /* Alias for test compatibility */
    uint8_t mode;              /* LP/BP/HP/3OFF */
    
    /* State variable filter state */
    int32_t bp;                /* Band-pass output */
    int32_t lp;                /* Low-pass output */
} SIDFilter;

/* SID Chip Structure */
typedef struct {
    /* Three voices */
    SIDVoice voice[3];
    
    /* Filter */
    SIDFilter filter;
    
    /* Master volume (4-bit) */
    uint8_t volume;
    
    /* Paddle/potentiometer inputs */
    uint8_t pot_x;
    uint8_t pot_y;
    
    /* Register shadow (for write-only registers) */
    uint8_t registers[SID_SIZE];
    
    /* Clock rate (PAL: 985248 Hz) */
    uint32_t clock_rate;
    
    /* Sample rate for output */
    uint32_t sample_rate;
    
    /* Cycle counter for sample generation */
    uint32_t cycle_count;
    uint32_t cycles_per_sample;
    
    /* Audio buffer (for test compatibility) */
    int16_t *audio_buffer;
    uint32_t buffer_size;
    uint32_t buffer_pos;
} SID;

/* SID Functions */
void sid_init(SID *sid, uint32_t clock_rate, uint32_t sample_rate);
void sid_reset(SID *sid);

uint8_t sid_read(SID *sid, uint16_t addr);
void sid_write(SID *sid, uint16_t addr, uint8_t data);

/* Clock the SID by one or more cycles */
void sid_clock(SID *sid, uint32_t cycles);

/* Generate audio sample */
int16_t sid_output(SID *sid);

/* Audio buffer functions (for test compatibility) */
void sid_set_audio_buffer(SID *sid, int16_t *buffer, uint32_t size);
uint32_t sid_get_samples(SID *sid);

#endif /* SID6581_H */
