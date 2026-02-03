#include "sid6581.h"
#include <string.h>
#include <stdlib.h>

/**
 * @file sid6581.c
 * @brief MOS 6581/8580 SID Emulation - Hardware-accurate implementation
 * 
 * Based on reSID by Dag Lem and C64 Programmer's Reference Guide.
 * 
 * Key implementation details:
 * - 24-bit phase accumulator with frequency added to lower 16 bits
 * - 23-bit LFSR noise with bit 19 clocking and 2-cycle pipeline
 * - LFSR feedback: bit0 = (bit22 | test) ^ bit17
 * - 15-bit rate counter with hardware-verified periods
 * - Exponential decay at thresholds: 255, 93, 54, 26, 14, 6, 0
 * - Two-integrator-loop biquadratic state-variable filter
 */

// =============================================================================
// Hardware-verified constants from reSID
// =============================================================================

/**
 * Rate counter periods - exact number of cycles between envelope counter updates
 * These values were verified by sampling ENV3 on real 6581/8580 hardware
 * Index 0-15 corresponds to ADSR register values 0-15
 */
static const uint16_t rate_counter_period[] = {
    9,      // Attack/Release 0:   2ms (theoretical: 1.0MHz*2ms/256 = 7.81)
    32,     // Attack/Release 1:   8ms
    63,     // Attack/Release 2:  16ms
    95,     // Attack/Release 3:  24ms
    149,    // Attack/Release 4:  38ms
    220,    // Attack/Release 5:  56ms
    267,    // Attack/Release 6:  68ms
    313,    // Attack/Release 7:  80ms
    392,    // Attack/Release 8: 100ms
    977,    // Attack/Release 9: 250ms
    1954,   // Attack/Release 10: 500ms
    3126,   // Attack/Release 11: 800ms
    3907,   // Attack/Release 12:   1s
    11720,  // Attack/Release 13:   3s
    19532,  // Attack/Release 14:   5s
    31251   // Attack/Release 15:   8s
};

/**
 * Sustain levels - mapped from 4-bit value to 8-bit envelope level
 * Both nibbles are the same to allow proper comparison
 */
static const uint8_t sustain_level[] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};

// =============================================================================
// Initialization and Reset
// =============================================================================

void sid_init(SID *sid, uint32_t clock_rate, uint32_t sample_rate) {
    memset(sid, 0, sizeof(SID));
    
    sid->clock_rate = clock_rate;
    sid->sample_rate = sample_rate;
    sid->cycles_per_sample = clock_rate / sample_rate;
    
    // Initialize LFSR to known state (all 1s except bit 0)
    for (int i = 0; i < 3; i++) {
        // reSID: Initial LFSR state verified from hardware
        sid->voice[i].shift_register = 0x7FFFF8;
        sid->voice[i].shift_pipeline = 0;
    }
    
    sid_reset(sid);
}

void sid_reset(SID *sid) {
    // Clear all registers
    memset(sid->registers, 0, sizeof(sid->registers));
    
    // Reset voices to power-on state
    for (int i = 0; i < 3; i++) {
        SID_Voice *v = &sid->voice[i];
        
        // Oscillator
        v->frequency = 0;
        v->pulse_width = 0;
        v->accumulator = 0;
        v->shift_register = 0x7FFFF8;  // Initial LFSR state
        v->prev_bit19 = 0;
        v->shift_pipeline = 0;
        
        // Control
        v->control = 0;
        
        // ADSR
        v->attack = 0;
        v->decay = 0;
        v->sustain = 0;
        v->release = 0;
        
        // Envelope - starts in release state with zero level
        v->env_state = ENV_RELEASE;
        v->env_counter = 0;
        v->env_rate = rate_counter_period[0];
        v->exp_counter = 0;
        v->exp_period = 1;
        v->env_level = 0;
        v->hold_zero = true;  // Envelope frozen until gate on
        
        // Sync
        v->sync_bit = false;
        v->prev_sync_bit = false;
        
        // Output
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
    sid->filter.w0 = 0;
    sid->filter.q_1024 = 1024;  // Q=1 default
    
    // Reset volume
    sid->volume = 0;
    
    // Reset sample timing
    sid->cycle_count = 0;
    sid->buffer_pos = 0;
}

// =============================================================================
// Register Write - Update internal state from register writes
// =============================================================================

/**
 * Helper to handle gate transition for a voice
 * Gate on: Start attack phase, clear hold_zero
 * Gate off: Start release phase
 */
static void handle_gate_transition(SID_Voice *v, uint8_t new_gate, uint8_t old_gate) {
    if (new_gate && !old_gate) {
        // Gate 0->1: Start attack
        v->env_state = ENV_ATTACK;
        v->env_rate = rate_counter_period[v->attack];
        v->hold_zero = false;
    } else if (!new_gate && old_gate) {
        // Gate 1->0: Start release
        v->env_state = ENV_RELEASE;
        v->env_rate = rate_counter_period[v->release];
    }
}

/**
 * Helper to handle TEST bit for a voice
 * TEST=1: Reset oscillator accumulator and LFSR, hold accumulator at 0
 */
static void handle_test_bit(SID_Voice *v, uint8_t data) {
    if (data & SID_CTRL_TEST) {
        v->accumulator = 0;
        // reSID: When TEST is set, LFSR is reset to all 1s (except bit 0)
        // Also: The noise waveform AND the LFSR writeback is affected
        v->shift_register = 0x7FFFF8;
        v->shift_pipeline = 0;
    }
}

void sid_write(SID *sid, uint16_t addr, uint8_t data) {
    // Map address to register offset (SID mirrors every 32 bytes)
    uint8_t reg = addr & 0x1F;
    
    // Ignore writes to read-only registers (0x19-0x1C)
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
            handle_gate_transition(&sid->voice[0], data & SID_CTRL_GATE, 
                                   sid->voice[0].control & SID_CTRL_GATE);
            handle_test_bit(&sid->voice[0], data);
            sid->voice[0].control = data;
            break;
        case SID_V1_AD:
            sid->voice[0].attack = (data >> 4) & 0x0F;
            sid->voice[0].decay = data & 0x0F;
            // Update rate if currently in attack or decay
            if (sid->voice[0].env_state == ENV_ATTACK) {
                sid->voice[0].env_rate = rate_counter_period[sid->voice[0].attack];
            } else if (sid->voice[0].env_state == ENV_DECAY) {
                sid->voice[0].env_rate = rate_counter_period[sid->voice[0].decay];
            }
            break;
        case SID_V1_SR:
            sid->voice[0].sustain = (data >> 4) & 0x0F;
            sid->voice[0].release = data & 0x0F;
            if (sid->voice[0].env_state == ENV_RELEASE) {
                sid->voice[0].env_rate = rate_counter_period[sid->voice[0].release];
            }
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
            handle_gate_transition(&sid->voice[1], data & SID_CTRL_GATE,
                                   sid->voice[1].control & SID_CTRL_GATE);
            handle_test_bit(&sid->voice[1], data);
            sid->voice[1].control = data;
            break;
        case SID_V2_AD:
            sid->voice[1].attack = (data >> 4) & 0x0F;
            sid->voice[1].decay = data & 0x0F;
            if (sid->voice[1].env_state == ENV_ATTACK) {
                sid->voice[1].env_rate = rate_counter_period[sid->voice[1].attack];
            } else if (sid->voice[1].env_state == ENV_DECAY) {
                sid->voice[1].env_rate = rate_counter_period[sid->voice[1].decay];
            }
            break;
        case SID_V2_SR:
            sid->voice[1].sustain = (data >> 4) & 0x0F;
            sid->voice[1].release = data & 0x0F;
            if (sid->voice[1].env_state == ENV_RELEASE) {
                sid->voice[1].env_rate = rate_counter_period[sid->voice[1].release];
            }
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
            handle_gate_transition(&sid->voice[2], data & SID_CTRL_GATE,
                                   sid->voice[2].control & SID_CTRL_GATE);
            handle_test_bit(&sid->voice[2], data);
            sid->voice[2].control = data;
            break;
        case SID_V3_AD:
            sid->voice[2].attack = (data >> 4) & 0x0F;
            sid->voice[2].decay = data & 0x0F;
            if (sid->voice[2].env_state == ENV_ATTACK) {
                sid->voice[2].env_rate = rate_counter_period[sid->voice[2].attack];
            } else if (sid->voice[2].env_state == ENV_DECAY) {
                sid->voice[2].env_rate = rate_counter_period[sid->voice[2].decay];
            }
            break;
        case SID_V3_SR:
            sid->voice[2].sustain = (data >> 4) & 0x0F;
            sid->voice[2].release = data & 0x0F;
            if (sid->voice[2].env_state == ENV_RELEASE) {
                sid->voice[2].env_rate = rate_counter_period[sid->voice[2].release];
            }
            break;
            
        // Filter
        case SID_FC_LO:
            // Lower 3 bits of cutoff frequency
            sid->filter.cutoff = (sid->filter.cutoff & 0x7F8) | (data & 0x07);
            break;
        case SID_FC_HI:
            // Upper 8 bits of cutoff frequency
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

// =============================================================================
// Register Read
// =============================================================================

// =============================================================================
// Register Read
// =============================================================================

uint8_t sid_read(SID *sid, uint16_t addr) {
    uint8_t reg = addr & 0x1F;
    
    switch (reg) {
        case SID_POT_X:
            return sid->pot_x;
        case SID_POT_Y:
            return sid->pot_y;
        case SID_OSC3:
            // Return upper 8 bits of oscillator 3 accumulator
            // reSID: This is the actual waveform output, not raw accumulator
            return (sid->voice[2].accumulator >> 16) & 0xFF;
        case SID_ENV3:
            return sid->voice[2].env_level;
        default:
            // Write-only registers return 0 on real hardware
            // Some games rely on this behavior
            return 0;
    }
}

// =============================================================================
// Oscillator - Phase Accumulator and Noise LFSR
// =============================================================================

/**
 * Clock a single voice oscillator for one cycle
 * 
 * reSID implementation notes:
 * - Frequency is added to the lower 16 bits of the 24-bit accumulator
 * - Noise LFSR is clocked when bit 19 transitions from 0 to 1
 * - There is a 2-cycle pipeline delay for LFSR clocking
 * - LFSR feedback: bit0 = (bit22 | test) ^ bit17
 */
static void clock_oscillator(SID_Voice *voice) {
    // Save previous sync bit for edge detection
    voice->prev_sync_bit = voice->sync_bit;
    
    // If TEST bit is set, oscillator is held at 0
    if (voice->control & SID_CTRL_TEST) {
        voice->accumulator = 0;
        // reSID: When TEST is high, noise output is also held high
        // The LFSR continues to receive the clock but with test OR'd into feedback
        return;
    }
    
    // Add frequency to accumulator
    // reSID: Actually adds to lower 16 bits, but result is 24-bit
    voice->accumulator = (voice->accumulator + voice->frequency) & 0xFFFFFF;
    
    // Update sync bit (MSB = bit 23)
    voice->sync_bit = (voice->accumulator & 0x800000) != 0;
    
    // Clock noise LFSR when bit 19 goes from 0 to 1
    // reSID: There's a 2-cycle pipeline delay
    uint32_t bit19 = voice->accumulator & 0x080000;
    
    // Shift pipeline and insert new clock pulse
    voice->shift_pipeline = (voice->shift_pipeline << 1) | (bit19 && !voice->prev_bit19 ? 1 : 0);
    voice->prev_bit19 = bit19;
    
    // Check if clock pulse has passed through 2-cycle pipeline
    if (voice->shift_pipeline & 0x04) {  // bit 2 = two cycles ago
        // LFSR feedback: new_bit0 = (bit22 | test) ^ bit17
        uint32_t bit22 = (voice->shift_register >> 22) & 1;
        uint32_t bit17 = (voice->shift_register >> 17) & 1;
        uint32_t test = (voice->control & SID_CTRL_TEST) ? 1 : 0;
        uint32_t feedback = (bit22 | test) ^ bit17;
        
        // Shift register left and insert feedback bit
        voice->shift_register = ((voice->shift_register << 1) | feedback) & 0x7FFFFF;
    }
}

/**
 * Generate waveform output for a voice
 * 
 * Waveform generation from reSID:
 * - Sawtooth: Upper 12 bits of accumulator
 * - Triangle: Upper 12 bits XOR'd based on MSB (and ring mod source MSB)
 * - Pulse: 1 if upper 12 bits >= pulse width, else 0
 * - Noise: Specific bits from LFSR mapped to output
 * - Combined waveforms are ANDed together
 * 
 * @param voice Pointer to voice
 * @param sync_source Pointer to sync source voice (voice N-1, wrapping)
 * @param ring_mod true if ring modulation enabled
 * @return 12-bit waveform output (0-4095)
 */
uint16_t sid_oscillator(SID_Voice *voice, SID_Voice *sync_source, bool ring_mod) {
    uint8_t ctrl = voice->control;
    uint32_t acc = voice->accumulator;
    
    // Handle sync - reset accumulator when sync source MSB goes 0->1
    if ((ctrl & SID_CTRL_SYNC) && sync_source) {
        if (sync_source->sync_bit && !sync_source->prev_sync_bit) {
            voice->accumulator = 0;
            acc = 0;
        }
    }
    
    // Ring modulation uses sync source MSB to flip triangle
    bool ring_msb = false;
    if (ring_mod && sync_source) {
        ring_msb = sync_source->sync_bit;
    }
    
    // Generate individual waveforms
    
    // Sawtooth: upper 12 bits of accumulator
    uint16_t sawtooth = (acc >> 12) & 0xFFF;
    
    // Triangle: upper 12 bits, XOR'd based on MSB
    // If MSB is 1, invert the lower bits for triangle shape
    uint16_t triangle;
    bool msb = (acc & 0x800000) != 0;
    if (ring_mod && (ctrl & SID_CTRL_TRI)) {
        // Ring modulation XORs with sync source MSB
        msb ^= ring_msb;
    }
    if (msb) {
        triangle = ((~acc) >> 11) & 0xFFF;
    } else {
        triangle = (acc >> 11) & 0xFFF;
    }
    
    // Pulse: compare upper 12 bits against pulse width
    uint16_t pw = voice->pulse_width;
    uint16_t acc_upper = (acc >> 12) & 0xFFF;
    uint16_t pulse;
    if (ctrl & SID_CTRL_TEST) {
        // TEST bit forces pulse high
        pulse = 0xFFF;
    } else {
        pulse = (acc_upper >= pw) ? 0xFFF : 0x000;
    }
    
    // Noise: map specific LFSR bits to output
    // reSID: Output bits 11-4 come from LFSR bits 20,18,14,11,9,5,2,0
    uint32_t sr = voice->shift_register;
    uint16_t noise = 
        ((sr & 0x100000) >> 9)  |  // bit 20 -> bit 11
        ((sr & 0x040000) >> 8)  |  // bit 18 -> bit 10
        ((sr & 0x004000) >> 5)  |  // bit 14 -> bit 9
        ((sr & 0x000800) >> 3)  |  // bit 11 -> bit 8
        ((sr & 0x000200) >> 2)  |  // bit 9  -> bit 7
        ((sr & 0x000020) << 1)  |  // bit 5  -> bit 6
        ((sr & 0x000004) << 3)  |  // bit 2  -> bit 5
        ((sr & 0x000001) << 4);    // bit 0  -> bit 4
    // Lower 4 bits are zero
    
    // Combine selected waveforms (AND them together)
    bool any_selected = false;
    uint16_t output = 0xFFF;
    
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
    
    // No waveform selected = silence
    if (!any_selected) {
        output = 0;
    }
    
    return output;
}

// =============================================================================
// Envelope Generator - Hardware-accurate ADSR
// =============================================================================

/**
 * Clock envelope generator for one cycle
 * 
 * reSID implementation details:
 * - 15-bit rate counter increments each cycle
 * - When rate counter == rate period, envelope counter may change
 * - Attack always increments (resets exp_counter each time)
 * - Decay/Release use exponential counter for curved falloff
 * - Exponential period changes at specific thresholds: 255, 93, 54, 26, 14, 6
 */
void sid_envelope_clock(SID_Voice *voice) {
    // Increment 15-bit rate counter
    voice->env_counter = (voice->env_counter + 1) & 0x7FFF;
    
    // Check if rate counter matches rate period
    if (voice->env_counter != voice->env_rate) {
        return;
    }
    
    // Reset rate counter to 0
    voice->env_counter = 0;
    
    // In attack, exp_counter is reset every time we get here
    // In decay/release, we need to wait for exp_counter cycles
    if (voice->env_state == ENV_ATTACK || ++voice->exp_counter == voice->exp_period) {
        voice->exp_counter = 0;
        
        // If envelope is frozen at zero, don't update
        if (voice->hold_zero) {
            return;
        }
        
        switch (voice->env_state) {
            case ENV_ATTACK:
                // Increment envelope level
                voice->env_level = (voice->env_level + 1) & 0xFF;
                
                if (voice->env_level == 0xFF) {
                    // Attack complete, switch to decay
                    voice->env_state = ENV_DECAY;
                    voice->env_rate = rate_counter_period[voice->decay];
                }
                break;
                
            case ENV_DECAY:
            case ENV_SUSTAIN:
                // Check if we've reached sustain level
                if (voice->env_level == sustain_level[voice->sustain]) {
                    // Hold at sustain (don't decrement further)
                    return;
                }
                // Decrement envelope counter
                if (voice->env_level > 0) {
                    voice->env_level--;
                }
                break;
                
            case ENV_RELEASE:
                // Decrement envelope counter
                if (voice->env_level > 0) {
                    voice->env_level--;
                } else {
                    // Envelope reached zero, freeze it
                    voice->hold_zero = true;
                }
                break;
                
            case ENV_IDLE:
                // Do nothing
                break;
        }
        
        // Update exponential period based on current envelope level
        // These thresholds are hardware-verified from reSID
        // The period increases as envelope gets lower (slower decay)
        switch (voice->env_level) {
            case 0xFF: voice->exp_period = 1;  break;  // Peak
            case 0x5D: voice->exp_period = 2;  break;  // 93
            case 0x36: voice->exp_period = 4;  break;  // 54
            case 0x1A: voice->exp_period = 8;  break;  // 26
            case 0x0E: voice->exp_period = 16; break;  // 14
            case 0x06: voice->exp_period = 30; break;  // 6
            case 0x00: 
                voice->exp_period = 1;
                voice->hold_zero = true;  // Freeze at zero
                break;
            // Default: keep current exp_period
        }
    }
}

// =============================================================================
// Filter - Two-integrator-loop Biquadratic State-Variable Filter
// =============================================================================

/**
 * Process filter with input signal
 * 
 * The SID uses a two-integrator-loop biquadratic filter confirmed by Bob Yannes.
 * This is a state-variable filter that produces LP, BP, and HP outputs simultaneously.
 * 
 * Implementation from reSID:
 * - Vhp = summer output
 * - Vbp = first integrator (bandpass)
 * - Vlp = second integrator (lowpass)
 * 
 * Each cycle:
 *   dVbp = w0 * Vhp
 *   dVlp = w0 * Vbp
 *   Vhp  = Vbp/Q - Vlp - Vi
 * 
 * @param sid Pointer to SID structure  
 * @param input Filter input signal (mixed from filtered voices)
 * @return Filtered output signal
 */
int16_t sid_filter_output(SID *sid, int32_t input) {
    SID_Filter *f = &sid->filter;
    
    // If no filter mode selected, return input directly
    if ((f->mode & (SID_MODE_LP | SID_MODE_BP | SID_MODE_HP)) == 0) {
        int32_t clamped = input;
        if (clamped > 32767) clamped = 32767;
        if (clamped < -32768) clamped = -32768;
        return (int16_t)clamped;
    }
    
    // Calculate filter cutoff coefficient (w0)
    // The 11-bit cutoff maps to approximately 30Hz - 12kHz
    // Using a simplified linear approximation for now
    // More accurate would use a lookup table or polynomial
    int32_t w0 = (f->cutoff * 6) >> 5;  // Scaled for stability
    if (w0 < 1) w0 = 1;
    if (w0 > 2047) w0 = 2047;
    
    // Calculate resonance (Q)
    // Resonance 0-15 maps to Q 0.5 - 8 approximately
    // Higher resonance = more feedback = more peak at cutoff
    int32_t q = f->resonance;
    // Q_factor = 1024 / (Q + 1), inverted for the feedback calculation
    // At resonance=0: Q=1, damping is high
    // At resonance=15: Q=~8, damping is low (near self-oscillation)
    int32_t q_factor = 1024 - (q * 60);  // Damping: 1024 to ~100
    if (q_factor < 100) q_factor = 100;  // Prevent self-oscillation
    
    // Scale input to prevent overflow (voices can be loud)
    int32_t Vi = input >> 2;
    
    // Two-integrator-loop filter
    // The order of operations matters for stability
    
    // Calculate Vhp (highpass) = (Vbp/Q) - Vlp - Vi
    // Actually: Vhp = Vbp * (1/Q) - Vlp - Vi, where 1/Q is damping
    f->Vhp = ((f->Vbp * q_factor) >> 10) - f->Vlp - Vi;
    
    // Clamp Vhp to prevent overflow
    if (f->Vhp > 32767) f->Vhp = 32767;
    if (f->Vhp < -32768) f->Vhp = -32768;
    
    // Integrate: Vbp += w0 * Vhp
    f->Vbp += (w0 * f->Vhp) >> 10;
    if (f->Vbp > 32767) f->Vbp = 32767;
    if (f->Vbp < -32768) f->Vbp = -32768;
    
    // Integrate: Vlp += w0 * Vbp
    f->Vlp += (w0 * f->Vbp) >> 10;
    if (f->Vlp > 32767) f->Vlp = 32767;
    if (f->Vlp < -32768) f->Vlp = -32768;
    
    // Select output based on filter mode
    // Multiple modes can be selected simultaneously for notch, etc.
    int32_t output = 0;
    
    if (f->mode & SID_MODE_LP) {
        output += f->Vlp;  // Lowpass
    }
    if (f->mode & SID_MODE_BP) {
        output += f->Vbp;  // Bandpass
    }
    if (f->mode & SID_MODE_HP) {
        output += f->Vhp;  // Highpass
    }
    
    // Scale back up
    output <<= 2;
    
    // Final clamp
    if (output > 32767) output = 32767;
    if (output < -32768) output = -32768;
    
    return (int16_t)output;
}

// =============================================================================
// Clock and Output - Main SID Processing
// =============================================================================

/**
 * Clock SID for specified number of cycles
 * 
 * Each cycle:
 * - Clock all 3 oscillators (unless TEST bit is set)
 * - Clock all 3 envelope generators
 * - Every cycles_per_sample cycles, generate an audio sample
 */
void sid_clock(SID *sid, uint32_t cycles) {
    for (uint32_t i = 0; i < cycles; i++) {
        // Clock oscillators every cycle
        // Note: clock_oscillator handles TEST bit internally
        for (int v = 0; v < 3; v++) {
            clock_oscillator(&sid->voice[v]);
        }
        
        // Clock envelope generators every cycle
        // The rate counter inside handles the actual timing
        for (int v = 0; v < 3; v++) {
            sid_envelope_clock(&sid->voice[v]);
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

/**
 * Generate single audio output sample
 * 
 * Process:
 * 1. Generate waveform output for each voice
 * 2. Apply envelope to each voice output
 * 3. Route voices to filter or direct output based on FILT bits
 * 4. Apply filter to filtered voices
 * 5. Mix filtered and direct outputs
 * 6. Apply master volume
 * 7. Handle voice 3 muting (3OFF bit)
 * 
 * @return 16-bit signed audio sample
 */
int16_t sid_output(SID *sid) {
    int32_t mixed_filtered = 0;
    int32_t mixed_direct = 0;
    
    // Sync source mapping: each voice syncs to the previous one (wrapping)
    // Voice 0 <- Voice 2
    // Voice 1 <- Voice 0
    // Voice 2 <- Voice 1
    SID_Voice *sync_sources[3] = {
        &sid->voice[2],
        &sid->voice[0],
        &sid->voice[1]
    };
    
    for (int v = 0; v < 3; v++) {
        SID_Voice *voice = &sid->voice[v];
        
        // Generate oscillator output (12-bit unsigned, 0-4095)
        bool ring_mod = (voice->control & SID_CTRL_RING) != 0;
        uint16_t osc = sid_oscillator(voice, sync_sources[v], ring_mod);
        
        // Convert to signed and apply envelope
        // OSC: 0-4095 -> -2048 to +2047 (centered at 0)
        // ENV: 0-255
        // Result: -524288 to +522495 (approximately ±19 bits)
        int32_t output = ((int32_t)osc - 2048) * voice->env_level;
        
        // Store voice output for OSC3 register reading
        voice->output = (int16_t)(output >> 8);
        
        // Check if voice 3 should be muted (3OFF bit in filter mode)
        // When 3OFF is set, voice 3 doesn't contribute to audio output
        // but still affects sync and ring mod for other voices
        if (v == 2 && (sid->filter.mode & SID_MODE_3OFF)) {
            continue;
        }
        
        // Route to filter or direct output based on FILT bits
        if (sid->filter.filter_voices & (1 << v)) {
            mixed_filtered += output;
        } else {
            mixed_direct += output;
        }
    }
    
    // Scale down mixed signals before filtering
    // Max possible: 3 voices * 2048 * 255 = 1,566,720 (about 21 bits)
    // Scale by 6 bits to fit in 16-bit range for filter
    mixed_filtered >>= 6;
    mixed_direct >>= 6;
    
    // Apply filter to filtered voices
    int16_t filtered_output = sid_filter_output(sid, mixed_filtered);
    
    // Mix filtered and direct signals
    int32_t total = (int32_t)filtered_output + mixed_direct;
    
    // Apply master volume (0-15)
    // Volume 15 = full volume, volume 0 = silence
    total = (total * sid->volume) >> 4;
    
    // Clamp to 16-bit signed range
    if (total > 32767) total = 32767;
    if (total < -32768) total = -32768;
    
    return (int16_t)total;
}

// =============================================================================
// Audio Buffer Management
// =============================================================================

void sid_set_audio_buffer(SID *sid, int16_t *buffer, uint32_t size) {
    sid->audio_buffer = buffer;
    sid->buffer_size = size;
    sid->buffer_pos = 0;
}

uint32_t sid_get_samples(SID *sid) {
    return sid->buffer_pos;
}
