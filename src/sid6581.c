#include "sid6581.h"
#include <string.h>
#include <stdlib.h>

// Attack rate lookup table (in cycles)
// Values from C64 Programmer's Reference Guide
static const uint32_t attack_rates[] = {
    2,      // 2ms
    8,      // 8ms
    16,     // 16ms
    24,     // 24ms
    38,     // 38ms
    56,     // 56ms
    68,     // 68ms
    80,     // 80ms
    100,    // 100ms
    250,    // 250ms
    500,    // 500ms
    800,    // 800ms
    1000,   // 1s
    3000,   // 3s
    5000,   // 5s
    8000    // 8s
};

// Decay/Release rate lookup table (in cycles)
static const uint32_t decay_release_rates[] = {
    6,      // 6ms
    24,     // 24ms
    48,     // 48ms
    72,     // 72ms
    114,    // 114ms
    168,    // 168ms
    204,    // 204ms
    240,    // 240ms
    300,    // 300ms
    750,    // 750ms
    1500,   // 1.5s
    2400,   // 2.4s
    3000,   // 3s
    9000,   // 9s
    15000,  // 15s
    24000   // 24s
};

// Exponential counter period for decay/release
// The envelope uses an exponential curve during decay and release
// Periods: 0xFF-0x5D: 1, 0x5C-0x36: 2, 0x35-0x1A: 4, 0x19-0x0E: 8, 0x0D-0x06: 16, 0x05-0x00: 30

// Get exponential counter period based on envelope level
static uint8_t get_exp_period(uint8_t level) {
    if (level >= 0x5D) return 1;
    if (level >= 0x36) return 2;
    if (level >= 0x1A) return 4;
    if (level >= 0x0E) return 8;
    if (level >= 0x06) return 16;
    return 30;
}

// Calculate rate counter period from ADSR value and clock rate
static uint32_t calc_rate_period(uint32_t time_ms, uint32_t clock_rate) {
    // Convert milliseconds to clock cycles for full envelope sweep
    // Rate counter increments, period = total_cycles / 256 levels
    return (time_ms * clock_rate) / (1000 * 256);
}

void sid_init(SID *sid, uint32_t clock_rate, uint32_t sample_rate) {
    memset(sid, 0, sizeof(SID));
    
    sid->clock_rate = clock_rate;
    sid->sample_rate = sample_rate;
    sid->cycles_per_sample = clock_rate / sample_rate;
    
    // Initialize noise shift registers
    for (int i = 0; i < 3; i++) {
        sid->voice[i].shift_register = 0x7FFFF8;  // Initial LFSR state
    }
    
    sid_reset(sid);
}

void sid_reset(SID *sid) {
    // Clear all registers
    memset(sid->registers, 0, sizeof(sid->registers));
    
    // Reset voices
    for (int i = 0; i < 3; i++) {
        SID_Voice *v = &sid->voice[i];
        v->frequency = 0;
        v->pulse_width = 0;
        v->control = 0;
        v->attack = 0;
        v->decay = 0;
        v->sustain = 0;
        v->release = 0;
        v->accumulator = 0;
        v->shift_register = 0x7FFFF8;
        v->env_state = ENV_IDLE;
        v->env_counter = 0;
        v->env_level = 0;
        v->env_rate = 0;
        v->exp_counter = 0;
        v->sync_bit = false;
        v->prev_sync_bit = false;
        v->output = 0;
    }
    
    // Reset filter
    sid->filter.cutoff = 0;
    sid->filter.resonance = 0;
    sid->filter.filter_voices = 0;
    sid->filter.mode = 0;
    sid->filter.Vhp = 0;
    sid->filter.Vbp = 0;
    sid->filter.Vlp = 0;
    
    // Reset volume
    sid->volume = 0;
    
    // Reset cycle counter
    sid->cycle_count = 0;
    sid->buffer_pos = 0;
}

void sid_write(SID *sid, uint16_t addr, uint8_t data) {
    // Map address to register offset
    uint8_t reg = addr & 0x1F;
    
    // Ignore writes to read-only registers
    if (reg >= SID_POT_X) {
        return;
    }
    
    // Store in register shadow
    if (reg < SID_REG_COUNT) {
        sid->registers[reg] = data;
    }
    
    // Update internal state based on register
    switch (reg) {
        // Voice 1
        case SID_V1_FREQ_LO:
            sid->voice[0].frequency = (sid->voice[0].frequency & 0xFF00) | data;
            break;
        case SID_V1_FREQ_HI:
            sid->voice[0].frequency = (sid->voice[0].frequency & 0x00FF) | (data << 8);
            break;
        case SID_V1_PW_LO:
            sid->voice[0].pulse_width = (sid->voice[0].pulse_width & 0x0F00) | data;
            break;
        case SID_V1_PW_HI:
            sid->voice[0].pulse_width = (sid->voice[0].pulse_width & 0x00FF) | ((data & 0x0F) << 8);
            break;
        case SID_V1_CTRL:
            // Check for gate transition
            if ((data & SID_CTRL_GATE) && !(sid->voice[0].control & SID_CTRL_GATE)) {
                // Gate on - start attack
                sid->voice[0].env_state = ENV_ATTACK;
                sid->voice[0].env_rate = calc_rate_period(attack_rates[sid->voice[0].attack], sid->clock_rate);
            } else if (!(data & SID_CTRL_GATE) && (sid->voice[0].control & SID_CTRL_GATE)) {
                // Gate off - start release
                sid->voice[0].env_state = ENV_RELEASE;
                sid->voice[0].env_rate = calc_rate_period(decay_release_rates[sid->voice[0].release], sid->clock_rate);
            }
            // TEST bit resets oscillator
            if (data & SID_CTRL_TEST) {
                sid->voice[0].accumulator = 0;
                sid->voice[0].shift_register = 0x7FFFF8;
            }
            sid->voice[0].control = data;
            break;
        case SID_V1_AD:
            sid->voice[0].attack = (data >> 4) & 0x0F;
            sid->voice[0].decay = data & 0x0F;
            break;
        case SID_V1_SR:
            sid->voice[0].sustain = (data >> 4) & 0x0F;
            sid->voice[0].release = data & 0x0F;
            break;
            
        // Voice 2
        case SID_V2_FREQ_LO:
            sid->voice[1].frequency = (sid->voice[1].frequency & 0xFF00) | data;
            break;
        case SID_V2_FREQ_HI:
            sid->voice[1].frequency = (sid->voice[1].frequency & 0x00FF) | (data << 8);
            break;
        case SID_V2_PW_LO:
            sid->voice[1].pulse_width = (sid->voice[1].pulse_width & 0x0F00) | data;
            break;
        case SID_V2_PW_HI:
            sid->voice[1].pulse_width = (sid->voice[1].pulse_width & 0x00FF) | ((data & 0x0F) << 8);
            break;
        case SID_V2_CTRL:
            if ((data & SID_CTRL_GATE) && !(sid->voice[1].control & SID_CTRL_GATE)) {
                sid->voice[1].env_state = ENV_ATTACK;
                sid->voice[1].env_rate = calc_rate_period(attack_rates[sid->voice[1].attack], sid->clock_rate);
            } else if (!(data & SID_CTRL_GATE) && (sid->voice[1].control & SID_CTRL_GATE)) {
                sid->voice[1].env_state = ENV_RELEASE;
                sid->voice[1].env_rate = calc_rate_period(decay_release_rates[sid->voice[1].release], sid->clock_rate);
            }
            if (data & SID_CTRL_TEST) {
                sid->voice[1].accumulator = 0;
                sid->voice[1].shift_register = 0x7FFFF8;
            }
            sid->voice[1].control = data;
            break;
        case SID_V2_AD:
            sid->voice[1].attack = (data >> 4) & 0x0F;
            sid->voice[1].decay = data & 0x0F;
            break;
        case SID_V2_SR:
            sid->voice[1].sustain = (data >> 4) & 0x0F;
            sid->voice[1].release = data & 0x0F;
            break;
            
        // Voice 3
        case SID_V3_FREQ_LO:
            sid->voice[2].frequency = (sid->voice[2].frequency & 0xFF00) | data;
            break;
        case SID_V3_FREQ_HI:
            sid->voice[2].frequency = (sid->voice[2].frequency & 0x00FF) | (data << 8);
            break;
        case SID_V3_PW_LO:
            sid->voice[2].pulse_width = (sid->voice[2].pulse_width & 0x0F00) | data;
            break;
        case SID_V3_PW_HI:
            sid->voice[2].pulse_width = (sid->voice[2].pulse_width & 0x00FF) | ((data & 0x0F) << 8);
            break;
        case SID_V3_CTRL:
            if ((data & SID_CTRL_GATE) && !(sid->voice[2].control & SID_CTRL_GATE)) {
                sid->voice[2].env_state = ENV_ATTACK;
                sid->voice[2].env_rate = calc_rate_period(attack_rates[sid->voice[2].attack], sid->clock_rate);
            } else if (!(data & SID_CTRL_GATE) && (sid->voice[2].control & SID_CTRL_GATE)) {
                sid->voice[2].env_state = ENV_RELEASE;
                sid->voice[2].env_rate = calc_rate_period(decay_release_rates[sid->voice[2].release], sid->clock_rate);
            }
            if (data & SID_CTRL_TEST) {
                sid->voice[2].accumulator = 0;
                sid->voice[2].shift_register = 0x7FFFF8;
            }
            sid->voice[2].control = data;
            break;
        case SID_V3_AD:
            sid->voice[2].attack = (data >> 4) & 0x0F;
            sid->voice[2].decay = data & 0x0F;
            break;
        case SID_V3_SR:
            sid->voice[2].sustain = (data >> 4) & 0x0F;
            sid->voice[2].release = data & 0x0F;
            break;
            
        // Filter
        case SID_FC_LO:
            sid->filter.cutoff = (sid->filter.cutoff & 0x7F8) | (data & 0x07);
            break;
        case SID_FC_HI:
            sid->filter.cutoff = (sid->filter.cutoff & 0x007) | (data << 3);
            break;
        case SID_RES_FILT:
            sid->filter.resonance = (data >> 4) & 0x0F;
            sid->filter.filter_voices = data & 0x0F;
            break;
        case SID_MODE_VOL:
            sid->filter.mode = (data >> 4) & 0x0F;
            sid->volume = data & 0x0F;
            break;
    }
}

uint8_t sid_read(SID *sid, uint16_t addr) {
    uint8_t reg = addr & 0x1F;
    
    switch (reg) {
        case SID_POT_X:
            return sid->pot_x;
        case SID_POT_Y:
            return sid->pot_y;
        case SID_OSC3:
            // Return upper 8 bits of oscillator 3 output
            return (sid->voice[2].accumulator >> 16) & 0xFF;
        case SID_ENV3:
            return sid->voice[2].env_level;
        default:
            // Write-only registers return 0 or last bus value
            return 0;
    }
}

// Clock a single voice oscillator
static void clock_oscillator(SID_Voice *voice) {
    // Save previous sync bit
    voice->prev_sync_bit = voice->sync_bit;
    
    // Accumulate frequency
    voice->accumulator += voice->frequency;
    voice->accumulator &= 0xFFFFFF;  // 24-bit wrap
    
    // Update sync bit (MSB)
    voice->sync_bit = (voice->accumulator & 0x800000) != 0;
    
    // Clock noise LFSR when bit 19 goes high
    static uint32_t prev_bit19[3] = {0, 0, 0};
    uint32_t bit19 = voice->accumulator & 0x080000;
    
    // We need to track this per-voice, using a simple approach here
    if (bit19 && !prev_bit19[0]) {  // This is simplified
        // LFSR feedback: bit 22 XOR bit 17
        uint32_t bit22 = (voice->shift_register >> 22) & 1;
        uint32_t bit17 = (voice->shift_register >> 17) & 1;
        voice->shift_register = ((voice->shift_register << 1) | (bit22 ^ bit17)) & 0x7FFFFF;
    }
}

// Generate waveform output for a voice
uint16_t sid_oscillator(SID_Voice *voice, SID_Voice *sync_source, bool ring_mod) {
    uint8_t ctrl = voice->control;
    uint32_t acc = voice->accumulator;
    uint16_t output = 0;
    uint16_t pw = voice->pulse_width << 4;  // 12-bit to 16-bit
    
    // Handle sync
    if ((ctrl & SID_CTRL_SYNC) && sync_source) {
        // Reset accumulator when sync source MSB goes from 0 to 1
        if (sync_source->sync_bit && !sync_source->prev_sync_bit) {
            voice->accumulator = 0;
            acc = 0;
        }
    }
    
    // Get MSB for ring modulation
    bool ring_bit = false;
    if (ring_mod && sync_source) {
        ring_bit = sync_source->sync_bit;
    }
    
    // Generate waveform(s) - upper 12 bits of accumulator
    uint16_t sawtooth = (acc >> 8) & 0xFFF;
    
    // Triangle (XOR with ring mod)
    uint16_t triangle;
    if (acc & 0x800000) {
        triangle = ((~acc) >> 11) & 0xFFF;
    } else {
        triangle = (acc >> 11) & 0xFFF;
    }
    if (ring_bit && (ctrl & SID_CTRL_TRI)) {
        triangle ^= 0xFFF;
    }
    
    // Pulse
    uint16_t pulse = ((acc >> 12) >= (pw >> 4)) ? 0xFFF : 0x000;
    
    // Noise (from LFSR)
    uint16_t noise = ((voice->shift_register & 0x400000) >> 11) |
                     ((voice->shift_register & 0x100000) >> 10) |
                     ((voice->shift_register & 0x010000) >> 7) |
                     ((voice->shift_register & 0x002000) >> 5) |
                     ((voice->shift_register & 0x000800) >> 4) |
                     ((voice->shift_register & 0x000080) >> 1) |
                     ((voice->shift_register & 0x000010) << 1) |
                     ((voice->shift_register & 0x000004) << 2);
    
    // Select waveform(s) - if multiple selected, they are ANDed
    bool any_selected = false;
    output = 0xFFF;
    
    if (ctrl & SID_CTRL_TRI) {
        output &= triangle;
        any_selected = true;
    }
    if (ctrl & SID_CTRL_SAW) {
        output &= sawtooth;
        any_selected = true;
    }
    if (ctrl & SID_CTRL_PULSE) {
        output &= pulse;
        any_selected = true;
    }
    if (ctrl & SID_CTRL_NOISE) {
        output &= noise;
        any_selected = true;
    }
    
    if (!any_selected) {
        output = 0;
    }
    
    return output;
}

// Clock envelope generator
void sid_envelope_clock(SID_Voice *voice) {
    if (voice->env_rate == 0) {
        return;
    }
    
    voice->env_counter++;
    
    if (voice->env_counter >= voice->env_rate) {
        voice->env_counter = 0;
        
        switch (voice->env_state) {
            case ENV_ATTACK:
                voice->env_level++;
                if (voice->env_level >= 0xFF) {
                    voice->env_level = 0xFF;
                    voice->env_state = ENV_DECAY;
                    voice->env_rate = calc_rate_period(
                        decay_release_rates[voice->decay], 
                        1000000  // Approximate clock rate
                    );
                    voice->exp_counter = 0;
                }
                break;
                
            case ENV_DECAY:
                // Exponential decay
                voice->exp_counter++;
                if (voice->exp_counter >= get_exp_period(voice->env_level)) {
                    voice->exp_counter = 0;
                    if (voice->env_level > 0) {
                        voice->env_level--;
                    }
                    // Check if we've reached sustain level
                    uint8_t sustain_level = voice->sustain | (voice->sustain << 4);
                    if (voice->env_level <= sustain_level) {
                        voice->env_level = sustain_level;
                        voice->env_state = ENV_SUSTAIN;
                    }
                }
                break;
                
            case ENV_SUSTAIN:
                // Hold at sustain level (do nothing)
                break;
                
            case ENV_RELEASE:
                // Exponential release
                voice->exp_counter++;
                if (voice->exp_counter >= get_exp_period(voice->env_level)) {
                    voice->exp_counter = 0;
                    if (voice->env_level > 0) {
                        voice->env_level--;
                    } else {
                        voice->env_state = ENV_IDLE;
                    }
                }
                break;
                
            case ENV_IDLE:
                // Do nothing
                break;
        }
    }
}

// Simple state-variable filter
int16_t sid_filter_output(SID *sid, int32_t input) {
    // Calculate filter coefficients
    // Cutoff frequency: approximate mapping from 11-bit value
    // Real SID has complex cutoff curve, this is simplified
    int32_t w0 = sid->filter.cutoff * 6;  // Rough approximation
    if (w0 > 2047) w0 = 2047;
    
    // Resonance: Q = 1 / (1 - res/16)
    int32_t q = sid->filter.resonance;
    int32_t Q1024 = 1024 - (q * 64);  // Scaled by 1024
    if (Q1024 < 64) Q1024 = 64;  // Limit Q
    
    // State variable filter update
    // Vhp = input - Vlp - Q * Vbp
    // Vbp = Vbp + w * Vhp
    // Vlp = Vlp + w * Vbp
    
    sid->filter.Vhp = input - sid->filter.Vlp - ((sid->filter.Vbp * Q1024) >> 10);
    sid->filter.Vbp += (w0 * sid->filter.Vhp) >> 12;
    sid->filter.Vlp += (w0 * sid->filter.Vbp) >> 12;
    
    // Select output based on mode
    int32_t output = 0;
    if (sid->filter.mode & SID_MODE_LP) {
        output += sid->filter.Vlp;
    }
    if (sid->filter.mode & SID_MODE_BP) {
        output += sid->filter.Vbp;
    }
    if (sid->filter.mode & SID_MODE_HP) {
        output += sid->filter.Vhp;
    }
    
    // Clamp output
    if (output > 32767) output = 32767;
    if (output < -32768) output = -32768;
    
    return (int16_t)output;
}

// Clock SID for specified number of cycles
void sid_clock(SID *sid, uint32_t cycles) {
    for (uint32_t i = 0; i < cycles; i++) {
        // Clock oscillators
        for (int v = 0; v < 3; v++) {
            if (!(sid->voice[v].control & SID_CTRL_TEST)) {
                clock_oscillator(&sid->voice[v]);
            }
        }
        
        // Clock envelope generators (at a slower rate)
        // Real SID clocks envelopes at ~1MHz / 15
        if ((sid->cycle_count % 15) == 0) {
            for (int v = 0; v < 3; v++) {
                sid_envelope_clock(&sid->voice[v]);
            }
        }
        
        // Generate sample at sample rate
        sid->cycle_count++;
        if (sid->cycle_count >= sid->cycles_per_sample) {
            sid->cycle_count = 0;
            
            // Generate output sample
            int16_t sample = sid_output(sid);
            
            // Store in buffer if available
            if (sid->audio_buffer && sid->buffer_pos < sid->buffer_size) {
                sid->audio_buffer[sid->buffer_pos++] = sample;
            }
        }
    }
}

// Generate output sample
int16_t sid_output(SID *sid) {
    int32_t mixed_filtered = 0;
    int32_t mixed_direct = 0;
    
    // Get sync sources (voice N syncs to voice N-1, wrapping)
    SID_Voice *sync_sources[3] = {
        &sid->voice[2],  // Voice 0 syncs to Voice 2
        &sid->voice[0],  // Voice 1 syncs to Voice 0
        &sid->voice[1]   // Voice 2 syncs to Voice 1
    };
    
    for (int v = 0; v < 3; v++) {
        SID_Voice *voice = &sid->voice[v];
        
        // Get oscillator output (12-bit, unsigned)
        bool ring_mod = (voice->control & SID_CTRL_RING) != 0;
        uint16_t osc = sid_oscillator(voice, sync_sources[v], ring_mod);
        
        // Convert to signed and apply envelope
        int32_t output = ((int32_t)osc - 2048) * voice->env_level;
        output >>= 8;  // Scale down
        
        voice->output = (int16_t)output;
        
        // Check if voice 3 is muted
        if (v == 2 && (sid->filter.mode & SID_MODE_3OFF)) {
            continue;
        }
        
        // Route to filter or direct output
        if (sid->filter.filter_voices & (1 << v)) {
            mixed_filtered += output;
        } else {
            mixed_direct += output;
        }
    }
    
    // Apply filter to filtered voices
    int16_t filtered_output = sid_filter_output(sid, mixed_filtered);
    
    // Mix filtered and direct
    int32_t total = filtered_output + mixed_direct;
    
    // Apply master volume
    total = (total * sid->volume) >> 4;
    
    // Clamp to 16-bit range
    if (total > 32767) total = 32767;
    if (total < -32768) total = -32768;
    
    return (int16_t)total;
}

void sid_set_audio_buffer(SID *sid, int16_t *buffer, uint32_t size) {
    sid->audio_buffer = buffer;
    sid->buffer_size = size;
    sid->buffer_pos = 0;
}

uint32_t sid_get_samples(SID *sid) {
    return sid->buffer_pos;
}
