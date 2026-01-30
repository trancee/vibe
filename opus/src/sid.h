/**
 * sid.h - SID 6581/8580 sound chip emulation header
 */

#ifndef C64_SID_H
#define C64_SID_H

#include "types.h"

// Forward declaration
typedef struct C64System C64System;

// SID register offsets (from $D400)
// Voice 1
#define SID_V1_FREQ_LO   0x00
#define SID_V1_FREQ_HI   0x01
#define SID_V1_PW_LO     0x02
#define SID_V1_PW_HI     0x03
#define SID_V1_CTRL      0x04
#define SID_V1_AD        0x05
#define SID_V1_SR        0x06
// Voice 2
#define SID_V2_FREQ_LO   0x07
#define SID_V2_FREQ_HI   0x08
#define SID_V2_PW_LO     0x09
#define SID_V2_PW_HI     0x0A
#define SID_V2_CTRL      0x0B
#define SID_V2_AD        0x0C
#define SID_V2_SR        0x0D
// Voice 3
#define SID_V3_FREQ_LO   0x0E
#define SID_V3_FREQ_HI   0x0F
#define SID_V3_PW_LO     0x10
#define SID_V3_PW_HI     0x11
#define SID_V3_CTRL      0x12
#define SID_V3_AD        0x13
#define SID_V3_SR        0x14
// Filter/Mode
#define SID_FC_LO        0x15
#define SID_FC_HI        0x16
#define SID_RESFILT      0x17
#define SID_MODEVOL      0x18
// Read-only
#define SID_POTX         0x19
#define SID_POTY         0x1A
#define SID_OSC3         0x1B
#define SID_ENV3         0x1C

// Voice control bits
#define SID_CTRL_GATE    0x01
#define SID_CTRL_SYNC    0x02
#define SID_CTRL_RING    0x04
#define SID_CTRL_TEST    0x08
#define SID_CTRL_TRI     0x10
#define SID_CTRL_SAW     0x20
#define SID_CTRL_PULSE   0x40
#define SID_CTRL_NOISE   0x80

// Filter mode bits
#define SID_FILT_LP      0x10
#define SID_FILT_BP      0x20
#define SID_FILT_HP      0x40
#define SID_FILT_3OFF    0x80

// Envelope state
typedef enum {
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE
} SidEnvState;

// Voice structure
typedef struct {
    // Registers
    u16 freq;           // Frequency
    u16 pw;             // Pulse width (12-bit)
    u8  ctrl;           // Control register
    u8  attack;         // Attack rate (4-bit)
    u8  decay;          // Decay rate (4-bit)
    u8  sustain;        // Sustain level (4-bit)
    u8  release;        // Release rate (4-bit)

    // Oscillator state
    u32 accumulator;    // Phase accumulator (24-bit)
    u32 shift_reg;      // Noise LFSR (23-bit)
    u8  prev_msb;       // Previous MSB for sync detection

    // Envelope state
    SidEnvState env_state;
    u8  env_counter;    // Current envelope level (0-255)
    u16 rate_counter;   // Rate counter
    u8  exp_counter;    // Exponential counter

    // Gate edge detection
    bool prev_gate;
} SidVoice;

// Filter state (State Variable Filter)
typedef struct {
    i32 bp;     // Band-pass output
    i32 lp;     // Low-pass output
    i32 hp;     // High-pass output

    // Coefficients
    i32 cutoff;     // Filter cutoff (calculated)
    i32 resonance;  // Resonance (calculated)
} SidFilter;

// SID state structure
typedef struct {
    // Registers (for write-only detection)
    u8 regs[0x20];

    // Three voices
    SidVoice voice[3];

    // Filter
    SidFilter filter;
    u8 filter_ctrl;     // $D417 RESFILT
    u8 mode_vol;        // $D418 MODEVOL

    // Misc read values
    u8 potx;
    u8 poty;

    // Last write address (for open bus)
    u16 last_write_addr;

    // Reference to system
    C64System *sys;
} C64Sid;

// SID functions
void sid_init(C64Sid *sid, C64System *sys);
void sid_reset(C64Sid *sid);
void sid_clock(C64Sid *sid);  // Advance one phi2 cycle

// Register access
u8   sid_read(C64Sid *sid, u8 reg);
void sid_write(C64Sid *sid, u8 reg, u8 value);

// Audio output (for future audio implementation)
i16  sid_output(C64Sid *sid);

#endif // C64_SID_H
