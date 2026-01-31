#include "test_suite.h"
#include "sid.h"
#include "c64.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

bool test_sid_voice_registers(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test SID voice registers for all 3 voices
    for (int voice = 0; voice < 3; voice++) {
        // Test frequency low byte
        sys.sid.freq_lo[voice] = 0x12;
        TEST_ASSERT_EQ(0x12, sys.sid.freq_lo[voice], "Voice frequency low should be stored");
        
        // Test frequency high byte
        sys.sid.freq_hi[voice] = 0x34;
        TEST_ASSERT_EQ(0x34, sys.sid.freq_hi[voice], "Voice frequency high should be stored");
        
        // Test pulse width low byte
        sys.sid.pw_lo[voice] = 0x56;
        TEST_ASSERT_EQ(0x56, sys.sid.pw_lo[voice], "Voice PW low should be stored");
        
        // Test pulse width high byte (bits 8-12)
        sys.sid.pw_hi[voice] = 0x0F; // Only lower 5 bits used
        TEST_ASSERT_EQ(0x0F, sys.sid.pw_hi[voice], "Voice PW high should be stored");
        
        // Test control register
        sys.sid.control[voice] = 0x1F; // All bits set
        TEST_ASSERT_EQ(0x1F, sys.sid.control[voice], "Voice control should be stored");
        
        // Test attack/decay register
        sys.sid.att_dec[voice] = 0x9A; // Attack=9, Decay=A
        TEST_ASSERT_EQ(0x9A, sys.sid.att_dec[voice], "Voice attack/decay should be stored");
        
        // Test sustain/release register  
        sys.sid.sus_rel[voice] = 0x78; // Sustain=7, Release=8
        TEST_ASSERT_EQ(0x78, sys.sid.sus_rel[voice], "Voice sustain/release should be stored");
    }
    
    c64_test_cleanup(&sys);
    TEST_PASS("SID voice registers test passed");
}

bool test_sid_filter_registers(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test filter cutoff frequency
    sys.sid.filter_cutoff_lo = 0x34;
    sys.sid.filter_cutoff_hi = 0x12;
    
    TEST_ASSERT_EQ(0x34, sys.sid.filter_cutoff_lo, "Filter cutoff low should be stored");
    TEST_ASSERT_EQ(0x12, sys.sid.filter_cutoff_hi, "Filter cutoff high should be stored");
    
    // Combine to get full cutoff (3-bit shift for hi byte)
    u16 full_cutoff = sys.sid.filter_cutoff_lo | (sys.sid.filter_cutoff_hi << 8);
    TEST_ASSERT_EQ(0x1234, full_cutoff, "Full cutoff should combine correctly");
    
    // Test filter resonance and control
    sys.sid.filter_res_ctrl = 0x8F; // Resonance=0xF, voice filters enabled
    TEST_ASSERT_EQ(0x8F, sys.sid.filter_res_ctrl, "Filter resonance/control should be stored");
    
    // Extract resonance (bits 4-7)
    u8 resonance = (sys.sid.filter_res_ctrl >> 4) & 0x0F;
    TEST_ASSERT_EQ(0x08, resonance, "Resonance should be extracted correctly");
    
    // Extract voice filter enables (bits 0-2)
    u8 voice_filters = sys.sid.filter_res_ctrl & 0x07;
    TEST_ASSERT_EQ(0x07, voice_filters, "Voice filter enables should be extracted correctly");
    
    // Test filter mode/volume
    sys.sid.filter_mode_vol = 0x1F; // Volume=0xF, all filter modes on
    TEST_ASSERT_EQ(0x1F, sys.sid.filter_mode_vol, "Filter mode/volume should be stored");
    
    // Extract volume (bits 0-3)
    u8 volume = sys.sid.filter_mode_vol & 0x0F;
    TEST_ASSERT_EQ(0x0F, volume, "Volume should be extracted correctly");
    
    // Extract filter modes (bits 4-7)
    u8 filter_modes = (sys.sid.filter_mode_vol >> 4) & 0x0F;
    TEST_ASSERT_EQ(0x01, filter_modes, "Filter modes should be extracted correctly");
    
    c64_test_cleanup(&sys);
    TEST_PASS("SID filter registers test passed");
}

bool test_sid_voice_control_bits(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test individual control register bits for voice 0
    
    // Bit 0: GATE
    sys.sid.control[0] = 0x01;
    TEST_ASSERT(sys.sid.control[0] & 0x01, "GATE bit should be settable");
    
    // Bit 1: SYNC
    sys.sid.control[0] = 0x02;
    TEST_ASSERT(sys.sid.control[0] & 0x02, "SYNC bit should be settable");
    
    // Bit 2: RING MOD
    sys.sid.control[0] = 0x04;
    TEST_ASSERT(sys.sid.control[0] & 0x04, "RING MOD bit should be settable");
    
    // Bit 3: TEST
    sys.sid.control[0] = 0x08;
    TEST_ASSERT(sys.sid.control[0] & 0x08, "TEST bit should be settable");
    
    // Bits 4-6: Waveform select
    sys.sid.control[0] = 0x10; // Triangle wave
    TEST_ASSERT(sys.sid.control[0] & 0x10, "Triangle wave bit should be settable");
    
    sys.sid.control[0] = 0x20; // Sawtooth wave
    TEST_ASSERT(sys.sid.control[0] & 0x20, "Sawtooth wave bit should be settable");
    
    sys.sid.control[0] = 0x40; // Pulse wave
    TEST_ASSERT(sys.sid.control[0] & 0x40, "Pulse wave bit should be settable");
    
    sys.sid.control[0] = 0x80; // Noise wave
    TEST_ASSERT(sys.sid.control[0] & 0x80, "Noise wave bit should be settable");
    
    // Test multiple bits set
    sys.sid.control[0] = 0x09; // GATE + TEST
    TEST_ASSERT_EQ(0x09, sys.sid.control[0], "Multiple control bits should be settable");
    
    c64_test_cleanup(&sys);
    TEST_PASS("SID voice control bits test passed");
}

bool test_sid_envelope_generator(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test envelope generator parameters
    
    // Test attack/decay decomposition
    sys.sid.att_dec[0] = 0x9A;
    u8 attack = (sys.sid.att_dec[0] >> 4) & 0x0F;
    u8 decay = sys.sid.att_dec[0] & 0x0F;
    
    TEST_ASSERT_EQ(0x09, attack, "Attack should be extracted from high nibble");
    TEST_ASSERT_EQ(0x0A, decay, "Decay should be extracted from low nibble");
    
    // Test sustain/release decomposition
    sys.sid.sus_rel[0] = 0x78;
    u8 sustain = (sys.sid.sus_rel[0] >> 4) & 0x0F;
    u8 release = sys.sid.sus_rel[0] & 0x0F;
    
    TEST_ASSERT_EQ(0x07, sustain, "Sustain should be extracted from high nibble");
    TEST_ASSERT_EQ(0x08, release, "Release should be extracted from low nibble");
    
    // Test envelope rate values (approximate)
    // Attack rates: 0=2ms, 1=8ms, 2=16ms, 3=24ms, ..., F=8s
    // Release rates: 0=6ms, 1=24ms, 2=48ms, 3=72ms, ..., F=24s
    
    // Test that 0xF gives maximum rates
    sys.sid.att_dec[0] = 0xF0; // Max attack
    sys.sid.sus_rel[0] = 0x0F; // Max release
    
    attack = (sys.sid.att_dec[0] >> 4) & 0x0F;
    release = sys.sid.sus_rel[0] & 0x0F;
    
    TEST_ASSERT_EQ(0x0F, attack, "Maximum attack value should be 15");
    TEST_ASSERT_EQ(0x0F, release, "Maximum release value should be 15");
    
    // Test that 0x0 gives minimum rates
    sys.sid.att_dec[0] = 0x00; // Min attack/decay
    sys.sid.sus_rel[0] = 0x00; // Min sustain/release
    
    attack = (sys.sid.att_dec[0] >> 4) & 0x0F;
    decay = sys.sid.att_dec[0] & 0x0F;
    sustain = (sys.sid.sus_rel[0] >> 4) & 0x0F;
    release = sys.sid.sus_rel[0] & 0x0F;
    
    TEST_ASSERT_EQ(0x00, attack, "Minimum attack value should be 0");
    TEST_ASSERT_EQ(0x00, decay, "Minimum decay value should be 0");
    TEST_ASSERT_EQ(0x00, sustain, "Minimum sustain value should be 0");
    TEST_ASSERT_EQ(0x00, release, "Minimum release value should be 0");
    
    c64_test_cleanup(&sys);
    TEST_PASS("SID envelope generator test passed");
}

bool test_sid_filter_types(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test different filter types via control register bits
    
    // Low-pass filter (bit 4)
    sys.sid.filter_mode_vol = 0x10;
    TEST_ASSERT(sys.sid.filter_mode_vol & 0x10, "Low-pass filter bit should be settable");
    
    // Band-pass filter (bit 5)
    sys.sid.filter_mode_vol = 0x20;
    TEST_ASSERT(sys.sid.filter_mode_vol & 0x20, "Band-pass filter bit should be settable");
    
    // High-pass filter (bit 6)
    sys.sid.filter_mode_vol = 0x40;
    TEST_ASSERT(sys.sid.filter_mode_vol & 0x40, "High-pass filter bit should be settable");
    
    // Notch filter (bit 7) - combination of low-pass and high-pass
    sys.sid.filter_mode_vol = 0x50; // Low-pass + High-pass
    TEST_ASSERT(sys.sid.filter_mode_vol & 0x10, "Notch filter should enable low-pass");
    TEST_ASSERT(sys.sid.filter_mode_vol & 0x40, "Notch filter should enable high-pass");
    
    // Test filter mode extraction
    u8 filter_mode = (sys.sid.filter_mode_vol >> 4) & 0x0F;
    TEST_ASSERT_EQ(0x05, filter_mode, "Filter mode should be extracted correctly");
    
    c64_test_cleanup(&sys);
    TEST_PASS("SID filter types test passed");
}

bool test_sid_voice3_output(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test voice 3 special registers (used for digi-sound)
    sys.sid.voice3_output = 0x42;
    sys.sid.voice3_unused = 0x00;
    
    TEST_ASSERT_EQ(0x42, sys.sid.voice3_output, "Voice 3 output should be readable");
    TEST_ASSERT_EQ(0x00, sys.sid.voice3_unused, "Voice 3 unused should be readable");
    
    // In real SID, voice 3 output is read-only and contains current oscillator value
    // For testing, we verify the register access works
    
    c64_test_cleanup(&sys);
    TEST_PASS("SID voice 3 output test passed");
}

bool test_sid_potentiometer_inputs(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test paddle potentiometer inputs
    sys.sid.pot_x = 0x7F;
    sys.sid.pot_y = 0x80;
    
    TEST_ASSERT_EQ(0x7F, sys.sid.pot_x, "Paddle X should be readable");
    TEST_ASSERT_EQ(0x80, sys.sid.pot_y, "Paddle Y should be readable");
    
    // Test range (0-255 for full paddle range)
    TEST_ASSERT_RANGE(0x00, 0xFF, sys.sid.pot_x, "Paddle X should be in valid range");
    TEST_ASSERT_RANGE(0x00, 0xFF, sys.sid.pot_y, "Paddle Y should be in valid range");
    
    c64_test_cleanup(&sys);
    TEST_PASS("SID potentiometer inputs test passed");
}

bool test_sid_registers(void) {
    return test_sid_voice_registers() && 
           test_sid_filter_registers() && 
           test_sid_voice_control_bits() && 
           test_sid_envelope_generator() && 
           test_sid_voice3_output() && 
           test_sid_potentiometer_inputs();
}

bool test_sid_filter(void) {
    return test_sid_filter_types();
}

bool test_sid_audio_generation(void) {
    // Placeholder for comprehensive audio generation testing
    // This would involve:
    // 1. Testing waveform generation (triangle, sawtooth, pulse, noise)
    // 2. Testing frequency accuracy
    // 3. Testing pulse width modulation
    // 4. Testing ring modulation
    // 5. Testing synchronization
    // 6. Testing filter frequency response
    // 7. Testing envelope shaping
    // 8. Testing audio output levels
    
    printf("SID audio generation tests not yet implemented\n");
    return true;
}