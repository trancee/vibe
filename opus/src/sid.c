/**
 * sid.c - SID 6581/8580 sound chip emulation
 * 
 * Implements basic SID register access and open bus behavior.
 * Audio generation is stubbed for now.
 */

#include "sid.h"
#include "c64.h"
#include <string.h>
#include <math.h>

// Attack/Decay/Release rate table (in cycles)
static const u32 rate_table[16] = {
    9, 32, 63, 95, 149, 220, 267, 313,
    392, 977, 1954, 3126, 3907, 11720, 19532, 31251
};

// Sustain level table
static const u8 sustain_table[16] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};

void sid_init(C64Sid *sid, C64System *sys) {
    memset(sid, 0, sizeof(C64Sid));
    sid->sys = sys;
    
    // Initialize noise LFSR
    for (int i = 0; i < 3; i++) {
        sid->voice[i].shift_reg = 0x7FFFFF;
    }
}

void sid_reset(C64Sid *sid) {
    memset(sid->regs, 0, sizeof(sid->regs));
    
    for (int i = 0; i < 3; i++) {
        SidVoice *v = &sid->voice[i];
        memset(v, 0, sizeof(SidVoice));
        v->shift_reg = 0x7FFFFF;
        v->env_state = ENV_RELEASE;
    }
    
    memset(&sid->filter, 0, sizeof(SidFilter));
    sid->filter_ctrl = 0;
    sid->mode_vol = 0;
    sid->potx = 0xFF;
    sid->poty = 0xFF;
    sid->last_write_addr = 0;
}

// Update filter coefficients
static void update_filter(C64Sid *sid) {
    // Get 11-bit cutoff value
    u16 fc = (sid->regs[SID_FC_LO] & 0x07) | (sid->regs[SID_FC_HI] << 3);
    
    // Get resonance (4-bit)
    u8 res = (sid->filter_ctrl >> 4) & 0x0F;
    
    // Convert to filter coefficients (simplified model)
    // Real SID filter is more complex, this is a basic approximation
    sid->filter.cutoff = fc;
    sid->filter.resonance = res;
}

// Clock one voice oscillator
static void clock_oscillator(SidVoice *v) {
    // Save previous MSB for sync detection
    v->prev_msb = (v->accumulator >> 23) & 1;
    
    // Accumulate frequency
    if (!(v->ctrl & SID_CTRL_TEST)) {
        v->accumulator += v->freq;
        v->accumulator &= 0xFFFFFF;
    }
    
    // Clock noise LFSR on accumulator bit 19 transition
    if ((v->accumulator & 0x080000) && !(v->prev_msb)) {
        u32 bit = ((v->shift_reg >> 22) ^ (v->shift_reg >> 17)) & 1;
        v->shift_reg = (v->shift_reg << 1) | bit;
        v->shift_reg &= 0x7FFFFF;
    }
}

// Clock one voice envelope
static void clock_envelope(SidVoice *v) {
    // Check gate edge
    bool gate = v->ctrl & SID_CTRL_GATE;
    
    if (gate && !v->prev_gate) {
        // Gate on: start attack
        v->env_state = ENV_ATTACK;
    } else if (!gate && v->prev_gate) {
        // Gate off: start release
        v->env_state = ENV_RELEASE;
    }
    v->prev_gate = gate;
    
    // Rate counter
    u32 rate;
    switch (v->env_state) {
        case ENV_ATTACK:
            rate = rate_table[v->attack];
            break;
        case ENV_DECAY:
            rate = rate_table[v->decay];
            break;
        case ENV_SUSTAIN:
            rate = 0;  // No change
            break;
        case ENV_RELEASE:
        default:
            rate = rate_table[v->release];
            break;
    }
    
    if (rate == 0) return;
    
    v->rate_counter++;
    if (v->rate_counter >= rate) {
        v->rate_counter = 0;
        
        switch (v->env_state) {
            case ENV_ATTACK:
                if (v->env_counter < 255) {
                    v->env_counter++;
                    if (v->env_counter == 255) {
                        v->env_state = ENV_DECAY;
                    }
                }
                break;
            
            case ENV_DECAY:
                if (v->env_counter > sustain_table[v->sustain]) {
                    v->env_counter--;
                } else {
                    v->env_state = ENV_SUSTAIN;
                }
                break;
            
            case ENV_SUSTAIN:
                // Stay at sustain level
                break;
            
            case ENV_RELEASE:
                if (v->env_counter > 0) {
                    v->env_counter--;
                }
                break;
        }
    }
}

void sid_clock(C64Sid *sid) {
    // Clock all three voices
    for (int i = 0; i < 3; i++) {
        clock_oscillator(&sid->voice[i]);
        clock_envelope(&sid->voice[i]);
    }
}

u8 sid_read(C64Sid *sid, u8 reg) {
    switch (reg) {
        case SID_POTX:
            return sid->potx;
        
        case SID_POTY:
            return sid->poty;
        
        case SID_OSC3:
            // Read oscillator 3 output (upper 8 bits)
            return (sid->voice[2].accumulator >> 16) & 0xFF;
        
        case SID_ENV3:
            // Read envelope 3 output
            return sid->voice[2].env_counter;
        
        default:
            // Write-only registers: return open bus (high byte of address)
            return (0xD400 + reg) >> 8;
    }
}

void sid_write(C64Sid *sid, u8 reg, u8 value) {
    sid->regs[reg] = value;
    sid->last_write_addr = 0xD400 + reg;
    
    // Decode register writes to voice structures
    if (reg <= SID_V1_SR) {
        // Voice 1
        SidVoice *v = &sid->voice[0];
        switch (reg) {
            case SID_V1_FREQ_LO:
                v->freq = (v->freq & 0xFF00) | value;
                break;
            case SID_V1_FREQ_HI:
                v->freq = (v->freq & 0x00FF) | (value << 8);
                break;
            case SID_V1_PW_LO:
                v->pw = (v->pw & 0x0F00) | value;
                break;
            case SID_V1_PW_HI:
                v->pw = (v->pw & 0x00FF) | ((value & 0x0F) << 8);
                break;
            case SID_V1_CTRL:
                v->ctrl = value;
                break;
            case SID_V1_AD:
                v->attack = (value >> 4) & 0x0F;
                v->decay = value & 0x0F;
                break;
            case SID_V1_SR:
                v->sustain = (value >> 4) & 0x0F;
                v->release = value & 0x0F;
                break;
        }
    }
    else if (reg >= SID_V2_FREQ_LO && reg <= SID_V2_SR) {
        // Voice 2
        SidVoice *v = &sid->voice[1];
        u8 vreg = reg - SID_V2_FREQ_LO;
        switch (vreg) {
            case 0: v->freq = (v->freq & 0xFF00) | value; break;
            case 1: v->freq = (v->freq & 0x00FF) | (value << 8); break;
            case 2: v->pw = (v->pw & 0x0F00) | value; break;
            case 3: v->pw = (v->pw & 0x00FF) | ((value & 0x0F) << 8); break;
            case 4: v->ctrl = value; break;
            case 5: v->attack = (value >> 4) & 0x0F; v->decay = value & 0x0F; break;
            case 6: v->sustain = (value >> 4) & 0x0F; v->release = value & 0x0F; break;
        }
    }
    else if (reg >= SID_V3_FREQ_LO && reg <= SID_V3_SR) {
        // Voice 3
        SidVoice *v = &sid->voice[2];
        u8 vreg = reg - SID_V3_FREQ_LO;
        switch (vreg) {
            case 0: v->freq = (v->freq & 0xFF00) | value; break;
            case 1: v->freq = (v->freq & 0x00FF) | (value << 8); break;
            case 2: v->pw = (v->pw & 0x0F00) | value; break;
            case 3: v->pw = (v->pw & 0x00FF) | ((value & 0x0F) << 8); break;
            case 4: v->ctrl = value; break;
            case 5: v->attack = (value >> 4) & 0x0F; v->decay = value & 0x0F; break;
            case 6: v->sustain = (value >> 4) & 0x0F; v->release = value & 0x0F; break;
        }
    }
    else if (reg == SID_RESFILT) {
        sid->filter_ctrl = value;
        update_filter(sid);
    }
    else if (reg == SID_MODEVOL) {
        sid->mode_vol = value;
    }
    else if (reg == SID_FC_LO || reg == SID_FC_HI) {
        update_filter(sid);
    }
}

// Generate audio output (returns signed 16-bit sample)
i16 sid_output(C64Sid *sid) {
    // Simplified audio output - just return 0 for now
    // Full implementation would mix all three voices through the filter
    
    i32 output = 0;
    u8 vol = sid->mode_vol & 0x0F;
    
    for (int i = 0; i < 3; i++) {
        SidVoice *v = &sid->voice[i];
        
        // Get oscillator output based on waveform
        i32 osc = 0;
        u8 phase = (v->accumulator >> 16) & 0xFF;
        
        if (v->ctrl & SID_CTRL_TRI) {
            // Triangle wave
            if (v->accumulator & 0x800000) {
                osc = 0xFF - phase;
            } else {
                osc = phase;
            }
        }
        if (v->ctrl & SID_CTRL_SAW) {
            // Sawtooth wave
            osc = phase;
        }
        if (v->ctrl & SID_CTRL_PULSE) {
            // Pulse wave
            u8 pw_compare = (v->pw >> 4) & 0xFF;
            osc = (phase >= pw_compare) ? 0xFF : 0x00;
        }
        if (v->ctrl & SID_CTRL_NOISE) {
            // Noise
            osc = (v->shift_reg >> 15) & 0xFF;
        }
        
        // Apply envelope
        osc = (osc * v->env_counter) >> 8;
        
        // Convert to signed
        osc -= 128;
        
        output += osc;
    }
    
    // Apply master volume
    output = (output * vol) / 15;
    
    // Scale to 16-bit
    output *= 256;
    
    // Clamp
    if (output > 32767) output = 32767;
    if (output < -32768) output = -32768;
    
    return (i16)output;
}
