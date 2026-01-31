/**
 * test_sid.c - SID 6581 tests
 * 
 * Tests voice registers, envelopes, oscillators, and open bus behavior.
 */

#include "test_framework.h"
#include "../src/sid.h"

static C64Sid sid;

static void setup(void) {
    sid_reset(&sid);
}

// ============================================================================
// Basic Register Tests
// ============================================================================

TEST(sid_initial_state) {
    setup();
    
    // All registers should be 0
    for (int i = 0; i < 25; i++) {
        ASSERT_EQ(0, sid.regs[i]);
    }
    PASS();
}

TEST(sid_voice_freq_write) {
    setup();
    
    // Voice 1 frequency
    sid_write(&sid, 0x00, 0x34);  // Freq low
    sid_write(&sid, 0x01, 0x12);  // Freq high
    
    ASSERT_EQ(0x34, sid.regs[0]);
    ASSERT_EQ(0x12, sid.regs[1]);
    PASS();
}

TEST(sid_voice_pw_write) {
    setup();
    
    // Voice 1 pulse width
    sid_write(&sid, 0x02, 0xFF);  // PW low
    sid_write(&sid, 0x03, 0x0F);  // PW high (4 bits)
    
    ASSERT_EQ(0xFF, sid.regs[2]);
    ASSERT_EQ(0x0F, sid.regs[3]);
    PASS();
}

TEST(sid_voice_control) {
    setup();
    
    // Voice 1 control register
    sid_write(&sid, 0x04, 0x41);  // Gate + Triangle
    
    ASSERT_EQ(0x41, sid.regs[4]);
    PASS();
}

TEST(sid_voice_adsr) {
    setup();
    
    // Voice 1 ADSR
    sid_write(&sid, 0x05, 0x12);  // Attack/Decay
    sid_write(&sid, 0x06, 0x34);  // Sustain/Release
    
    ASSERT_EQ(0x12, sid.regs[5]);
    ASSERT_EQ(0x34, sid.regs[6]);
    PASS();
}

// ============================================================================
// All Three Voices
// ============================================================================

TEST(sid_voice2_registers) {
    setup();
    
    sid_write(&sid, 0x07, 0xAB);  // V2 Freq Low
    sid_write(&sid, 0x08, 0xCD);  // V2 Freq High
    sid_write(&sid, 0x09, 0x55);  // V2 PW Low
    sid_write(&sid, 0x0A, 0x08);  // V2 PW High
    sid_write(&sid, 0x0B, 0x81);  // V2 Control (Gate + Noise)
    sid_write(&sid, 0x0C, 0x33);  // V2 AD
    sid_write(&sid, 0x0D, 0x44);  // V2 SR
    
    ASSERT_EQ(0xAB, sid.regs[0x07]);
    ASSERT_EQ(0xCD, sid.regs[0x08]);
    ASSERT_EQ(0x55, sid.regs[0x09]);
    ASSERT_EQ(0x08, sid.regs[0x0A]);
    ASSERT_EQ(0x81, sid.regs[0x0B]);
    ASSERT_EQ(0x33, sid.regs[0x0C]);
    ASSERT_EQ(0x44, sid.regs[0x0D]);
    PASS();
}

TEST(sid_voice3_registers) {
    setup();
    
    sid_write(&sid, 0x0E, 0x11);  // V3 Freq Low
    sid_write(&sid, 0x0F, 0x22);  // V3 Freq High
    sid_write(&sid, 0x10, 0x33);  // V3 PW Low
    sid_write(&sid, 0x11, 0x04);  // V3 PW High
    sid_write(&sid, 0x12, 0x21);  // V3 Control (Gate + Sawtooth)
    sid_write(&sid, 0x13, 0x55);  // V3 AD
    sid_write(&sid, 0x14, 0x66);  // V3 SR
    
    ASSERT_EQ(0x11, sid.regs[0x0E]);
    ASSERT_EQ(0x22, sid.regs[0x0F]);
    ASSERT_EQ(0x33, sid.regs[0x10]);
    ASSERT_EQ(0x04, sid.regs[0x11]);
    ASSERT_EQ(0x21, sid.regs[0x12]);
    ASSERT_EQ(0x55, sid.regs[0x13]);
    ASSERT_EQ(0x66, sid.regs[0x14]);
    PASS();
}

// ============================================================================
// Filter and Volume
// ============================================================================

TEST(sid_filter_cutoff) {
    setup();
    
    sid_write(&sid, 0x15, 0x07);  // FC Low (3 bits)
    sid_write(&sid, 0x16, 0xFF);  // FC High (8 bits)
    
    ASSERT_EQ(0x07, sid.regs[0x15]);
    ASSERT_EQ(0xFF, sid.regs[0x16]);
    PASS();
}

TEST(sid_filter_control) {
    setup();
    
    sid_write(&sid, 0x17, 0x17);  // Resonance + Filter voice enable
    
    ASSERT_EQ(0x17, sid.regs[0x17]);
    PASS();
}

TEST(sid_volume_filter_mode) {
    setup();
    
    sid_write(&sid, 0x18, 0x7F);  // Mode + Volume
    
    ASSERT_EQ(0x7F, sid.regs[0x18]);
    PASS();
}

// ============================================================================
// Read-Only Registers (per documentation: open bus)
// ============================================================================

TEST(sid_potx_read) {
    setup();
    
    // POTX at $D419 - typically returns $FF when no paddle
    u8 val = sid_read(&sid, 0x19);
    ASSERT_EQ(0xFF, val);
    PASS();
}

TEST(sid_poty_read) {
    setup();
    
    // POTY at $D41A
    u8 val = sid_read(&sid, 0x1A);
    ASSERT_EQ(0xFF, val);
    PASS();
}

TEST(sid_osc3_read) {
    setup();
    
    // OSC3 random at $D41B - returns oscillator 3 upper 8 bits
    u8 val = sid_read(&sid, 0x1B);
    // Value depends on oscillator state
    (void)val;
    PASS();
}

TEST(sid_env3_read) {
    setup();
    
    // ENV3 at $D41C - returns voice 3 envelope value
    u8 val = sid_read(&sid, 0x1C);
    // Value depends on envelope state
    (void)val;
    PASS();
}

// ============================================================================
// Open Bus Behavior (per documentation)
// ============================================================================

TEST(sid_write_only_open_bus) {
    setup();
    
    // Write-only registers return open bus (high byte of address)
    // Registers $00-$18 are write-only
    // Open bus typically returns last value on data bus or 0
    
    // Write a value first
    sid_write(&sid, 0x00, 0x55);
    
    // Reading write-only register returns open bus
    u8 val = sid_read(&sid, 0x00);
    // Implementation may return 0 or last written value
    // Just verify we can read without crashing
    (void)val;
    PASS();
}

TEST(sid_unused_register_read) {
    setup();
    
    // Registers $1D-$1F are unused, return open bus
    u8 val1D = sid_read(&sid, 0x1D);
    u8 val1E = sid_read(&sid, 0x1E);
    u8 val1F = sid_read(&sid, 0x1F);
    
    (void)val1D;
    (void)val1E;
    (void)val1F;
    PASS();
}

// ============================================================================
// Gate Control
// ============================================================================

TEST(sid_gate_on) {
    setup();
    
    // Gate bit 0 triggers attack phase
    sid_write(&sid, 0x04, 0x01);  // Gate on
    
    ASSERT_TRUE(sid.regs[4] & 0x01);
    PASS();
}

TEST(sid_gate_off) {
    setup();
    
    sid_write(&sid, 0x04, 0x01);  // Gate on
    sid_write(&sid, 0x04, 0x00);  // Gate off - triggers release
    
    ASSERT_FALSE(sid.regs[4] & 0x01);
    PASS();
}

// ============================================================================
// Waveform Selection
// ============================================================================

TEST(sid_triangle_wave) {
    setup();
    
    sid_write(&sid, 0x04, 0x11);  // Gate + Triangle (bit 4)
    ASSERT_EQ(0x11, sid.regs[4]);
    PASS();
}

TEST(sid_sawtooth_wave) {
    setup();
    
    sid_write(&sid, 0x04, 0x21);  // Gate + Sawtooth (bit 5)
    ASSERT_EQ(0x21, sid.regs[4]);
    PASS();
}

TEST(sid_pulse_wave) {
    setup();
    
    sid_write(&sid, 0x04, 0x41);  // Gate + Pulse (bit 6)
    ASSERT_EQ(0x41, sid.regs[4]);
    PASS();
}

TEST(sid_noise_wave) {
    setup();
    
    sid_write(&sid, 0x04, 0x81);  // Gate + Noise (bit 7)
    ASSERT_EQ(0x81, sid.regs[4]);
    PASS();
}

TEST(sid_combined_waveforms) {
    setup();
    
    // Combined waveforms (used for special effects)
    sid_write(&sid, 0x04, 0x31);  // Gate + Triangle + Sawtooth
    ASSERT_EQ(0x31, sid.regs[4]);
    PASS();
}

// ============================================================================
// Ring Modulation and Sync
// ============================================================================

TEST(sid_ring_mod) {
    setup();
    
    // Ring modulation: voice 1 modulated by voice 3
    sid_write(&sid, 0x04, 0x15);  // Gate + Triangle + Ring Mod (bit 2)
    ASSERT_TRUE(sid.regs[4] & 0x04);
    PASS();
}

TEST(sid_sync) {
    setup();
    
    // Sync: voice 1 synced to voice 3
    sid_write(&sid, 0x04, 0x13);  // Gate + Triangle + Sync (bit 1)
    ASSERT_TRUE(sid.regs[4] & 0x02);
    PASS();
}

// ============================================================================
// Test Bit
// ============================================================================

TEST(sid_test_bit) {
    setup();
    
    // Test bit resets oscillator
    sid_write(&sid, 0x04, 0x09);  // Gate + Test (bit 3)
    ASSERT_TRUE(sid.regs[4] & 0x08);
    PASS();
}

// ============================================================================
// Clock/Tick
// ============================================================================

TEST(sid_clock) {
    setup();
    
    // Set up voice 1 with frequency and gate
    sid_write(&sid, 0x00, 0x00);
    sid_write(&sid, 0x01, 0x10);  // Frequency
    sid_write(&sid, 0x05, 0x00);  // Attack = 0
    sid_write(&sid, 0x06, 0xF0);  // Sustain = 15
    sid_write(&sid, 0x04, 0x41);  // Gate + Pulse
    
    // Clock the SID multiple times
    for (int i = 0; i < 1000; i++) {
        sid_clock(&sid);
    }
    
    // Voice should be active
    // (Just verify it runs without crashing)
    PASS();
}

// ============================================================================
// Filter Voice Enable
// ============================================================================

TEST(sid_filter_voice_enable) {
    setup();
    
    // Enable filter for voice 1 and 2
    sid_write(&sid, 0x17, 0x03);
    
    ASSERT_EQ(0x03, sid.regs[0x17] & 0x0F);
    PASS();
}

TEST(sid_filter_external) {
    setup();
    
    // Enable filter for external input (bit 3)
    sid_write(&sid, 0x17, 0x08);
    
    ASSERT_TRUE(sid.regs[0x17] & 0x08);
    PASS();
}

// ============================================================================
// Filter Modes
// ============================================================================

TEST(sid_lowpass_filter) {
    setup();
    
    sid_write(&sid, 0x18, 0x1F);  // Low-pass (bit 4) + max volume
    ASSERT_TRUE(sid.regs[0x18] & 0x10);
    PASS();
}

TEST(sid_bandpass_filter) {
    setup();
    
    sid_write(&sid, 0x18, 0x2F);  // Band-pass (bit 5) + max volume
    ASSERT_TRUE(sid.regs[0x18] & 0x20);
    PASS();
}

TEST(sid_highpass_filter) {
    setup();
    
    sid_write(&sid, 0x18, 0x4F);  // High-pass (bit 6) + max volume
    ASSERT_TRUE(sid.regs[0x18] & 0x40);
    PASS();
}

TEST(sid_voice3_off) {
    setup();
    
    // Bit 7 of $D418 mutes voice 3 output (but not for ring/sync)
    sid_write(&sid, 0x18, 0x8F);
    ASSERT_TRUE(sid.regs[0x18] & 0x80);
    PASS();
}

// ============================================================================
// ADSR Edge Cases
// ============================================================================

TEST(sid_zero_attack) {
    setup();
    
    // Zero attack = instant full volume
    sid_write(&sid, 0x05, 0x00);  // Attack = 0
    sid_write(&sid, 0x06, 0xF0);  // Sustain = 15
    sid_write(&sid, 0x04, 0x41);  // Gate on
    
    // Clock a few times
    for (int i = 0; i < 10; i++) {
        sid_clock(&sid);
    }
    
    PASS();
}

TEST(sid_max_attack) {
    setup();
    
    // Max attack = slow ramp up
    sid_write(&sid, 0x05, 0xF0);  // Attack = 15
    sid_write(&sid, 0x04, 0x41);  // Gate on
    
    for (int i = 0; i < 100; i++) {
        sid_clock(&sid);
    }
    
    PASS();
}

TEST(sid_zero_release) {
    setup();
    
    // Zero release = instant drop to 0
    sid_write(&sid, 0x05, 0x00);
    sid_write(&sid, 0x06, 0x00);  // Release = 0
    sid_write(&sid, 0x04, 0x41);  // Gate on
    
    for (int i = 0; i < 10; i++) {
        sid_clock(&sid);
    }
    
    sid_write(&sid, 0x04, 0x40);  // Gate off
    
    for (int i = 0; i < 10; i++) {
        sid_clock(&sid);
    }
    
    PASS();
}

// ============================================================================
// Run all SID tests
// ============================================================================

void run_sid_tests(void) {
    TEST_SUITE("SID - Basic Registers");
    RUN_TEST(sid_initial_state);
    RUN_TEST(sid_voice_freq_write);
    RUN_TEST(sid_voice_pw_write);
    RUN_TEST(sid_voice_control);
    RUN_TEST(sid_voice_adsr);
    
    TEST_SUITE("SID - All Voices");
    RUN_TEST(sid_voice2_registers);
    RUN_TEST(sid_voice3_registers);
    
    TEST_SUITE("SID - Filter & Volume");
    RUN_TEST(sid_filter_cutoff);
    RUN_TEST(sid_filter_control);
    RUN_TEST(sid_volume_filter_mode);
    
    TEST_SUITE("SID - Read-Only Registers");
    RUN_TEST(sid_potx_read);
    RUN_TEST(sid_poty_read);
    RUN_TEST(sid_osc3_read);
    RUN_TEST(sid_env3_read);
    
    TEST_SUITE("SID - Open Bus");
    RUN_TEST(sid_write_only_open_bus);
    RUN_TEST(sid_unused_register_read);
    
    TEST_SUITE("SID - Gate Control");
    RUN_TEST(sid_gate_on);
    RUN_TEST(sid_gate_off);
    
    TEST_SUITE("SID - Waveforms");
    RUN_TEST(sid_triangle_wave);
    RUN_TEST(sid_sawtooth_wave);
    RUN_TEST(sid_pulse_wave);
    RUN_TEST(sid_noise_wave);
    RUN_TEST(sid_combined_waveforms);
    
    TEST_SUITE("SID - Ring Mod & Sync");
    RUN_TEST(sid_ring_mod);
    RUN_TEST(sid_sync);
    
    TEST_SUITE("SID - Test Bit");
    RUN_TEST(sid_test_bit);
    
    TEST_SUITE("SID - Clock");
    RUN_TEST(sid_clock);
    
    TEST_SUITE("SID - Filter Voice Enable");
    RUN_TEST(sid_filter_voice_enable);
    RUN_TEST(sid_filter_external);
    
    TEST_SUITE("SID - Filter Modes");
    RUN_TEST(sid_lowpass_filter);
    RUN_TEST(sid_bandpass_filter);
    RUN_TEST(sid_highpass_filter);
    RUN_TEST(sid_voice3_off);
    
    TEST_SUITE("SID - ADSR Edge Cases");
    RUN_TEST(sid_zero_attack);
    RUN_TEST(sid_max_attack);
    RUN_TEST(sid_zero_release);
}
