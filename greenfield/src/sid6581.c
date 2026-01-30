/*
 * C64 Emulator - SID 6581/8580 Implementation
 * 
 * Sound synthesis including:
 * - Oscillators: Triangle, Sawtooth, Pulse, Noise
 * - ADSR Envelope Generator
 * - State Variable Filter
 */

#include <string.h>
#include <stdlib.h>
#include "sid6581.h"

/* ADSR rate table (cycles per step) */
static const uint32_t adsr_rates[16] = {
    9,      /* 2ms */
    32,     /* 8ms */
    63,     /* 16ms */
    95,     /* 24ms */
    149,    /* 38ms */
    220,    /* 56ms */
    267,    /* 68ms */
    313,    /* 80ms */
    392,    /* 100ms */
    977,    /* 250ms */
    1954,   /* 500ms */
    3126,   /* 800ms */
    3907,   /* 1s */
    11720,  /* 3s */
    19532,  /* 5s */
    31251   /* 8s */
};

/* Exponential decay lookup for release/decay */
static const uint8_t exp_counter_period[256] = {
    1, 30, 30, 30, 30, 30, 30, 16, 16, 16, 16, 16, 16, 16, 16,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

static void voice_reset(SIDVoice *v)
{
    v->frequency = 0;
    v->pulse_width = 0;
    v->control = 0;
    v->attack = 0;
    v->decay = 0;
    v->sustain = 0;
    v->release = 0;
    v->accumulator = 0;
    v->noise_lfsr = 0x7FFFF8;  /* Initial LFSR state */
    v->env_state = ENV_IDLE;
    v->env_counter = 0;
    v->env_level = 0;
    v->exp_counter = 0;
    v->msb_rising = false;
    v->output = 0;
}

void sid_init(SID *sid, uint32_t clock_rate, uint32_t sample_rate)
{
    memset(sid, 0, sizeof(SID));
    sid->clock_rate = clock_rate;
    sid->sample_rate = sample_rate;
    sid->cycles_per_sample = clock_rate / sample_rate;
    sid_reset(sid);
}

void sid_reset(SID *sid)
{
    for (int i = 0; i < 3; i++) {
        voice_reset(&sid->voice[i]);
    }
    
    sid->filter.cutoff = 0;
    sid->filter.resonance = 0;
    sid->filter.filter_enable = 0;
    sid->filter.mode = 0;
    sid->filter.bp = 0;
    sid->filter.lp = 0;
    
    sid->volume = 0;
    sid->pot_x = 0xFF;
    sid->pot_y = 0xFF;
    
    memset(sid->registers, 0, sizeof(sid->registers));
    sid->cycle_count = 0;
}

uint8_t sid_read(SID *sid, uint16_t addr)
{
    uint8_t reg = addr & 0x1F;
    
    switch (reg) {
        case SID_POT_X:
            return sid->pot_x;
            
        case SID_POT_Y:
            return sid->pot_y;
            
        case SID_OSC3:
            /* Upper 8 bits of voice 3 oscillator */
            return (sid->voice[2].accumulator >> 16) & 0xFF;
            
        case SID_ENV3:
            /* Voice 3 envelope level */
            return sid->voice[2].env_level;
            
        default:
            /* Write-only registers return last value on data bus */
            /* In practice this is often the high byte of the address */
            return (addr >> 8) & 0xFF;
    }
}

void sid_write(SID *sid, uint16_t addr, uint8_t data)
{
    uint8_t reg = addr & 0x1F;
    sid->registers[reg] = data;
    
    /* Determine which voice (0-2) or if it's filter/volume */
    int voice_num = reg / 7;
    int voice_reg = reg % 7;
    
    if (reg < 21) {
        /* Voice registers */
        SIDVoice *v = &sid->voice[voice_num];
        
        switch (voice_reg) {
            case SID_FREQ_LO:
                v->frequency = (v->frequency & 0xFF00) | data;
                break;
                
            case SID_FREQ_HI:
                v->frequency = (v->frequency & 0x00FF) | (data << 8);
                break;
                
            case SID_PW_LO:
                v->pulse_width = (v->pulse_width & 0x0F00) | data;
                break;
                
            case SID_PW_HI:
                v->pulse_width = (v->pulse_width & 0x00FF) | ((data & 0x0F) << 8);
                break;
                
            case SID_CTRL:
                {
                    bool gate_was_off = !(v->control & SID_CTRL_GATE);
                    bool gate_is_on = data & SID_CTRL_GATE;
                    
                    v->control = data;
                    
                    /* Gate transition */
                    if (gate_was_off && gate_is_on) {
                        /* Gate on: start attack */
                        v->env_state = ENV_ATTACK;
                        v->env_counter = 0;
                    } else if (!gate_was_off && !gate_is_on) {
                        /* Gate off: start release */
                        v->env_state = ENV_RELEASE;
                    }
                    
                    /* Test bit resets noise LFSR */
                    if (data & SID_CTRL_TEST) {
                        v->noise_lfsr = 0;
                        v->accumulator = 0;
                    }
                }
                break;
                
            case SID_AD:
                v->attack = (data >> 4) & 0x0F;
                v->decay = data & 0x0F;
                break;
                
            case SID_SR:
                v->sustain = (data >> 4) & 0x0F;
                v->release = data & 0x0F;
                break;
        }
    } else {
        /* Filter and volume registers */
        switch (reg) {
            case SID_FC_LO:
                sid->filter.cutoff = (sid->filter.cutoff & 0x7F8) | (data & 0x07);
                break;
                
            case SID_FC_HI:
                sid->filter.cutoff = (sid->filter.cutoff & 0x007) | (data << 3);
                break;
                
            case SID_RES_FILT:
                sid->filter.resonance = (data >> 4) & 0x0F;
                sid->filter.filter_enable = data & 0x0F;
                break;
                
            case SID_MODE_VOL:
                sid->filter.mode = (data >> 4) & 0x0F;
                sid->volume = data & 0x0F;
                break;
        }
    }
}

/* Clock envelope generator */
static void clock_envelope(SIDVoice *v)
{
    uint32_t rate;
    
    switch (v->env_state) {
        case ENV_ATTACK:
            rate = adsr_rates[v->attack];
            v->env_counter++;
            if (v->env_counter >= rate) {
                v->env_counter = 0;
                v->env_level++;
                if (v->env_level == 0xFF) {
                    v->env_state = ENV_DECAY;
                }
            }
            break;
            
        case ENV_DECAY:
            rate = adsr_rates[v->decay];
            v->env_counter++;
            if (v->env_counter >= rate) {
                v->env_counter = 0;
                v->exp_counter++;
                if (v->exp_counter >= exp_counter_period[v->env_level]) {
                    v->exp_counter = 0;
                    if (v->env_level > (v->sustain << 4 | v->sustain)) {
                        v->env_level--;
                    } else {
                        v->env_state = ENV_SUSTAIN;
                    }
                }
            }
            break;
            
        case ENV_SUSTAIN:
            /* Maintain sustain level */
            v->env_level = v->sustain << 4 | v->sustain;
            break;
            
        case ENV_RELEASE:
            rate = adsr_rates[v->release];
            v->env_counter++;
            if (v->env_counter >= rate) {
                v->env_counter = 0;
                v->exp_counter++;
                if (v->exp_counter >= exp_counter_period[v->env_level]) {
                    v->exp_counter = 0;
                    if (v->env_level > 0) {
                        v->env_level--;
                    } else {
                        v->env_state = ENV_IDLE;
                    }
                }
            }
            break;
            
        case ENV_IDLE:
            v->env_level = 0;
            break;
    }
}

/* Clock oscillator and generate waveform */
static void clock_oscillator(SIDVoice *v, SIDVoice *sync_source)
{
    /* Save MSB for sync detection */
    bool old_msb = v->accumulator & 0x800000;
    
    /* Advance accumulator */
    v->accumulator = (v->accumulator + v->frequency) & 0xFFFFFF;
    
    /* Detect rising edge of MSB for sync */
    bool new_msb = v->accumulator & 0x800000;
    v->msb_rising = !old_msb && new_msb;
    
    /* Handle sync */
    if (sync_source && (v->control & SID_CTRL_SYNC) && sync_source->msb_rising) {
        v->accumulator = 0;
    }
    
    /* Clock noise LFSR on bit 19 transitions */
    static bool old_bit19 = false;
    bool new_bit19 = v->accumulator & 0x080000;
    if (!old_bit19 && new_bit19) {
        /* LFSR feedback: XOR bits 17 and 22 */
        uint32_t bit0 = ((v->noise_lfsr >> 17) ^ (v->noise_lfsr >> 22)) & 1;
        v->noise_lfsr = (v->noise_lfsr << 1) | bit0;
    }
    old_bit19 = new_bit19;
    
    /* Generate waveform output (12-bit) */
    int16_t wave = 0;
    uint16_t acc_upper = (v->accumulator >> 12) & 0xFFF;
    
    if (v->control & SID_CTRL_TEST) {
        /* Test bit forces output to max */
        wave = 0xFFF;
    } else {
        if (v->control & SID_CTRL_TRI) {
            /* Triangle: XOR upper bit to fold waveform */
            uint16_t tri = acc_upper;
            if (acc_upper & 0x800) {
                tri ^= 0x7FF;
            }
            tri <<= 1;
            wave = tri;
            
            /* Ring modulation with sync source */
            if ((v->control & SID_CTRL_RING) && sync_source) {
                if (sync_source->accumulator & 0x800000) {
                    wave ^= 0xFFF;
                }
            }
        }
        
        if (v->control & SID_CTRL_SAW) {
            wave = acc_upper;
        }
        
        if (v->control & SID_CTRL_PULSE) {
            if (acc_upper >= v->pulse_width) {
                wave = 0xFFF;
            } else {
                wave = 0;
            }
        }
        
        if (v->control & SID_CTRL_NOISE) {
            /* Noise from LFSR bits */
            wave = ((v->noise_lfsr >> 4) & 0x800) |
                   ((v->noise_lfsr >> 1) & 0x400) |
                   ((v->noise_lfsr << 2) & 0x200) |
                   ((v->noise_lfsr << 5) & 0x100) |
                   ((v->noise_lfsr << 6) & 0x080) |
                   ((v->noise_lfsr << 9) & 0x040) |
                   ((v->noise_lfsr << 13) & 0x020) |
                   ((v->noise_lfsr << 16) & 0x010);
        }
    }
    
    /* Apply envelope and convert to signed */
    v->output = ((wave - 0x800) * v->env_level) >> 8;
}

void sid_clock(SID *sid, uint32_t cycles)
{
    for (uint32_t i = 0; i < cycles; i++) {
        /* Clock all three voices */
        for (int v = 0; v < 3; v++) {
            clock_envelope(&sid->voice[v]);
            
            /* Sync source is the previous voice (wraps around) */
            SIDVoice *sync_src = &sid->voice[(v + 2) % 3];
            clock_oscillator(&sid->voice[v], sync_src);
        }
        
        sid->cycle_count++;
        
        /* Generate sample if we have enough cycles */
        if (sid->cycle_count >= sid->cycles_per_sample) {
            sid->cycle_count -= sid->cycles_per_sample;
            
            /* Write to audio buffer if available */
            if (sid->audio_buffer && sid->buffer_pos < sid->buffer_size) {
                sid->audio_buffer[sid->buffer_pos++] = sid_output(sid);
            }
        }
    }
}

int16_t sid_output(SID *sid)
{
    int32_t output = 0;
    int32_t filter_input = 0;
    
    /* Mix voices */
    for (int v = 0; v < 3; v++) {
        int16_t voice_out = sid->voice[v].output;
        
        /* Voice 3 can be muted */
        if (v == 2 && (sid->filter.mode & SID_FMODE_3OFF)) {
            continue;
        }
        
        /* Route to filter or direct output */
        if (sid->filter.filter_enable & (1 << v)) {
            filter_input += voice_out;
        } else {
            output += voice_out;
        }
    }
    
    /* Apply filter (simplified state variable filter) */
    if (sid->filter.mode & (SID_FMODE_LP | SID_FMODE_BP | SID_FMODE_HP)) {
        /* Calculate filter coefficients */
        /* Cutoff frequency: approximately (cutoff * 5.8) + 30 Hz */
        int32_t fc = sid->filter.cutoff;
        int32_t w0 = (fc * fc) >> 8;  /* Simplified frequency calculation */
        if (w0 < 1) w0 = 1;
        
        /* Resonance: 0-15 maps to Q factor */
        int32_t q = (15 - sid->filter.resonance) + 1;
        
        /* Update filter state */
        int32_t hp = filter_input - sid->filter.lp - (sid->filter.bp * q / 16);
        sid->filter.bp += (hp * w0) >> 12;
        sid->filter.lp += (sid->filter.bp * w0) >> 12;
        
        /* Select filter output */
        int32_t filter_output = 0;
        if (sid->filter.mode & SID_FMODE_LP) filter_output += sid->filter.lp;
        if (sid->filter.mode & SID_FMODE_BP) filter_output += sid->filter.bp;
        if (sid->filter.mode & SID_FMODE_HP) filter_output += hp;
        
        output += filter_output;
    } else {
        /* No filter, pass through */
        output += filter_input;
    }
    
    /* Apply master volume (0-15) */
    output = (output * sid->volume) >> 4;
    
    /* Clamp to 16-bit range */
    if (output > 32767) output = 32767;
    if (output < -32768) output = -32768;
    
    return (int16_t)output;
}

void sid_set_audio_buffer(SID *sid, int16_t *buffer, uint32_t size)
{
    sid->audio_buffer = buffer;
    sid->buffer_size = size;
    sid->buffer_pos = 0;
}

uint32_t sid_get_samples(SID *sid)
{
    return sid->buffer_pos;
}
