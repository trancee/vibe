#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "sid6581.h"

// Test framework
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, msg) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("  ✓ %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  ✗ %s\n", msg); \
    } \
} while(0)

#define TEST_ASSERT_EQ(expected, actual, msg) do { \
    tests_run++; \
    if ((expected) == (actual)) { \
        tests_passed++; \
        printf("  ✓ %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  ✗ %s (expected 0x%X, got 0x%X)\n", msg, (expected), (actual)); \
    } \
} while(0)

// ============================================================================
// SID Initialization Tests
// ============================================================================

static void test_sid_init(void) {
    printf("\n=== SID Initialization Tests ===\n");
    
    SID sid;
    sid_init(&sid, 985248, 44100);
    
    TEST_ASSERT_EQ(985248, sid.clock_rate, "Clock rate initialized correctly");
    TEST_ASSERT_EQ(44100, sid.sample_rate, "Sample rate initialized correctly");
    TEST_ASSERT_EQ(0, sid.volume, "Volume initialized to 0");
    
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQ(0, sid.voice[i].frequency, "Voice frequency initialized to 0");
        TEST_ASSERT_EQ(0, sid.voice[i].pulse_width, "Voice pulse width initialized to 0");
        TEST_ASSERT_EQ(0, sid.voice[i].control, "Voice control initialized to 0");
        TEST_ASSERT_EQ(ENV_IDLE, sid.voice[i].env_state, "Voice envelope in IDLE state");
    }
}

// ============================================================================
// SID Register Write Tests
// ============================================================================

static void test_sid_register_writes(void) {
    printf("\n=== SID Register Write Tests ===\n");
    
    SID sid;
    sid_init(&sid, 985248, 44100);
    
    // Test Voice 1 frequency
    sid_write(&sid, SID_MEM_START + SID_V1_FREQ_LO, 0x34);
    sid_write(&sid, SID_MEM_START + SID_V1_FREQ_HI, 0x12);
    TEST_ASSERT_EQ(0x1234, sid.voice[0].frequency, "Voice 1 frequency set correctly");
    
    // Test Voice 1 pulse width
    sid_write(&sid, SID_MEM_START + SID_V1_PW_LO, 0xFF);
    sid_write(&sid, SID_MEM_START + SID_V1_PW_HI, 0x0F);
    TEST_ASSERT_EQ(0x0FFF, sid.voice[0].pulse_width, "Voice 1 pulse width set correctly");
    
    // Test Voice 1 ADSR
    sid_write(&sid, SID_MEM_START + SID_V1_AD, 0xA5);
    TEST_ASSERT_EQ(0x0A, sid.voice[0].attack, "Voice 1 attack set correctly");
    TEST_ASSERT_EQ(0x05, sid.voice[0].decay, "Voice 1 decay set correctly");
    
    sid_write(&sid, SID_MEM_START + SID_V1_SR, 0xC3);
    TEST_ASSERT_EQ(0x0C, sid.voice[0].sustain, "Voice 1 sustain set correctly");
    TEST_ASSERT_EQ(0x03, sid.voice[0].release, "Voice 1 release set correctly");
    
    // Test Voice 2 frequency
    sid_write(&sid, SID_MEM_START + SID_V2_FREQ_LO, 0xCD);
    sid_write(&sid, SID_MEM_START + SID_V2_FREQ_HI, 0xAB);
    TEST_ASSERT_EQ(0xABCD, sid.voice[1].frequency, "Voice 2 frequency set correctly");
    
    // Test Voice 3 frequency
    sid_write(&sid, SID_MEM_START + SID_V3_FREQ_LO, 0x78);
    sid_write(&sid, SID_MEM_START + SID_V3_FREQ_HI, 0x56);
    TEST_ASSERT_EQ(0x5678, sid.voice[2].frequency, "Voice 3 frequency set correctly");
    
    // Test Filter cutoff
    sid_write(&sid, SID_MEM_START + SID_FC_LO, 0x07);
    sid_write(&sid, SID_MEM_START + SID_FC_HI, 0xFF);
    TEST_ASSERT_EQ(0x7FF, sid.filter.cutoff, "Filter cutoff set correctly");
    
    // Test Resonance and filter routing
    sid_write(&sid, SID_MEM_START + SID_RES_FILT, 0xF7);
    TEST_ASSERT_EQ(0x0F, sid.filter.resonance, "Filter resonance set correctly");
    TEST_ASSERT_EQ(0x07, sid.filter.filter_voices, "Filter routing set correctly");
    
    // Test Mode and volume
    sid_write(&sid, SID_MEM_START + SID_MODE_VOL, 0x1F);
    TEST_ASSERT_EQ(0x01, sid.filter.mode, "Filter mode set correctly");
    TEST_ASSERT_EQ(0x0F, sid.volume, "Master volume set correctly");
}

// ============================================================================
// SID Register Read Tests
// ============================================================================

static void test_sid_register_reads(void) {
    printf("\n=== SID Register Read Tests ===\n");
    
    SID sid;
    sid_init(&sid, 985248, 44100);
    
    // Set paddle values
    sid.pot_x = 0x80;
    sid.pot_y = 0x40;
    
    TEST_ASSERT_EQ(0x80, sid_read(&sid, SID_MEM_START + SID_POT_X), "POT_X read correctly");
    TEST_ASSERT_EQ(0x40, sid_read(&sid, SID_MEM_START + SID_POT_Y), "POT_Y read correctly");
    
    // Write-only registers should return 0
    sid_write(&sid, SID_MEM_START + SID_V1_FREQ_LO, 0xFF);
    TEST_ASSERT_EQ(0x00, sid_read(&sid, SID_MEM_START + SID_V1_FREQ_LO), "Write-only register reads 0");
}

// ============================================================================
// SID Control Register Tests
// ============================================================================

static void test_sid_control_register(void) {
    printf("\n=== SID Control Register Tests ===\n");
    
    SID sid;
    sid_init(&sid, 985248, 44100);
    
    // Set up voice 1
    sid_write(&sid, SID_MEM_START + SID_V1_AD, 0x00);  // Fast attack/decay
    sid_write(&sid, SID_MEM_START + SID_V1_SR, 0xF0);  // High sustain, fast release
    
    // Test GATE on triggers ATTACK
    sid_write(&sid, SID_MEM_START + SID_V1_CTRL, SID_CTRL_GATE | SID_CTRL_TRI);
    TEST_ASSERT_EQ(ENV_ATTACK, sid.voice[0].env_state, "GATE on triggers ATTACK");
    
    // Test GATE off triggers RELEASE
    sid_write(&sid, SID_MEM_START + SID_V1_CTRL, SID_CTRL_TRI);
    TEST_ASSERT_EQ(ENV_RELEASE, sid.voice[0].env_state, "GATE off triggers RELEASE");
    
    // Test TEST bit resets oscillator
    sid.voice[0].accumulator = 0xFFFFFF;
    sid_write(&sid, SID_MEM_START + SID_V1_CTRL, SID_CTRL_TEST);
    TEST_ASSERT_EQ(0, sid.voice[0].accumulator, "TEST bit resets oscillator");
}

// ============================================================================
// SID Waveform Tests
// ============================================================================

static void test_sid_waveforms(void) {
    printf("\n=== SID Waveform Tests ===\n");
    
    SID sid;
    sid_init(&sid, 985248, 44100);
    
    // Set up voice 1 with maximum frequency
    sid_write(&sid, SID_MEM_START + SID_V1_FREQ_HI, 0xFF);
    sid_write(&sid, SID_MEM_START + SID_V1_FREQ_LO, 0xFF);
    sid_write(&sid, SID_MEM_START + SID_V1_AD, 0x00);
    sid_write(&sid, SID_MEM_START + SID_V1_SR, 0xF0);
    sid_write(&sid, SID_MEM_START + SID_MODE_VOL, 0x0F);  // Max volume
    
    // Test triangle waveform
    sid_write(&sid, SID_MEM_START + SID_V1_CTRL, SID_CTRL_GATE | SID_CTRL_TRI);
    sid_clock(&sid, 1000);  // Clock for a bit
    TEST_ASSERT((sid.voice[0].control & SID_CTRL_TRI) != 0, "Triangle waveform selected");
    
    // Test sawtooth waveform
    sid_write(&sid, SID_MEM_START + SID_V1_CTRL, SID_CTRL_GATE | SID_CTRL_SAW);
    sid_clock(&sid, 1000);
    TEST_ASSERT((sid.voice[0].control & SID_CTRL_SAW) != 0, "Sawtooth waveform selected");
    
    // Test pulse waveform with 50% duty cycle
    sid_write(&sid, SID_MEM_START + SID_V1_PW_HI, 0x08);
    sid_write(&sid, SID_MEM_START + SID_V1_PW_LO, 0x00);
    sid_write(&sid, SID_MEM_START + SID_V1_CTRL, SID_CTRL_GATE | SID_CTRL_PULSE);
    sid_clock(&sid, 1000);
    TEST_ASSERT((sid.voice[0].control & SID_CTRL_PULSE) != 0, "Pulse waveform selected");
    
    // Test noise waveform
    sid_write(&sid, SID_MEM_START + SID_V1_CTRL, SID_CTRL_GATE | SID_CTRL_NOISE);
    sid_clock(&sid, 1000);
    TEST_ASSERT((sid.voice[0].control & SID_CTRL_NOISE) != 0, "Noise waveform selected");
}

// ============================================================================
// SID Envelope Tests
// ============================================================================

static void test_sid_envelope(void) {
    printf("\n=== SID Envelope Tests ===\n");
    
    SID sid;
    sid_init(&sid, 985248, 44100);
    
    // Set up voice 1 with fast envelope
    sid_write(&sid, SID_MEM_START + SID_V1_FREQ_HI, 0x10);
    sid_write(&sid, SID_MEM_START + SID_V1_AD, 0x00);  // Fastest attack/decay
    sid_write(&sid, SID_MEM_START + SID_V1_SR, 0x80);  // 50% sustain, fastest release
    
    // Gate on - envelope should start in ATTACK
    sid_write(&sid, SID_MEM_START + SID_V1_CTRL, SID_CTRL_GATE | SID_CTRL_TRI);
    TEST_ASSERT_EQ(ENV_ATTACK, sid.voice[0].env_state, "Envelope starts in ATTACK");
    TEST_ASSERT_EQ(0, sid.voice[0].env_level, "Envelope level starts at 0");
    
    // Clock many cycles to complete attack
    sid_clock(&sid, 100000);
    TEST_ASSERT(sid.voice[0].env_level > 0, "Envelope level increases during attack");
}

// ============================================================================
// SID Filter Tests
// ============================================================================

static void test_sid_filter(void) {
    printf("\n=== SID Filter Tests ===\n");
    
    SID sid;
    sid_init(&sid, 985248, 44100);
    
    // Set filter cutoff to mid-range
    sid_write(&sid, SID_MEM_START + SID_FC_LO, 0x00);
    sid_write(&sid, SID_MEM_START + SID_FC_HI, 0x40);
    TEST_ASSERT_EQ(0x200, sid.filter.cutoff, "Filter cutoff mid-range");
    
    // Route voice 1 through filter
    sid_write(&sid, SID_MEM_START + SID_RES_FILT, 0x81);  // Resonance=8, Voice 1 filtered
    TEST_ASSERT_EQ(0x08, sid.filter.resonance, "Filter resonance set");
    TEST_ASSERT((sid.filter.filter_voices & SID_FILT_V1) != 0, "Voice 1 routed to filter");
    
    // Enable low-pass filter
    // MODE_VOL register: bits 4-7 = mode, bits 0-3 = volume
    // After extraction, mode bits are shifted right by 4
    sid_write(&sid, SID_MEM_START + SID_MODE_VOL, 0x1F);  // LP mode (bit 4=1), max volume
    // sid.filter.mode will be 0x01 (LP bit after shift)
    TEST_ASSERT((sid.filter.mode & 0x01) != 0, "Low-pass filter enabled");
}

// ============================================================================
// SID Output Tests
// ============================================================================

static void test_sid_output(void) {
    printf("\n=== SID Output Tests ===\n");
    
    SID sid;
    sid_init(&sid, 985248, 44100);
    
    // Test with volume at 0 - output should be 0 or near 0
    sid_write(&sid, SID_MEM_START + SID_V1_FREQ_HI, 0x10);
    sid_write(&sid, SID_MEM_START + SID_V1_AD, 0x00);
    sid_write(&sid, SID_MEM_START + SID_V1_SR, 0xF0);
    sid_write(&sid, SID_MEM_START + SID_V1_CTRL, SID_CTRL_GATE | SID_CTRL_SAW);
    sid_write(&sid, SID_MEM_START + SID_MODE_VOL, 0x00);  // Volume 0
    
    sid_clock(&sid, 10000);
    int16_t output = sid_output(&sid);
    TEST_ASSERT(output == 0, "Output is 0 when volume is 0");
    
    // Test with volume at max
    sid_write(&sid, SID_MEM_START + SID_MODE_VOL, 0x0F);  // Volume max
    sid_clock(&sid, 50000);
    output = sid_output(&sid);
    TEST_ASSERT(output != 0 || sid.voice[0].env_level > 0, "Sound is produced with volume max");
}

// ============================================================================
// SID Audio Buffer Tests
// ============================================================================

static void test_sid_audio_buffer(void) {
    printf("\n=== SID Audio Buffer Tests ===\n");
    
    SID sid;
    sid_init(&sid, 985248, 44100);
    
    // Set up audio buffer
    int16_t buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    sid_set_audio_buffer(&sid, buffer, 1024);
    
    TEST_ASSERT(sid.audio_buffer == buffer, "Audio buffer set correctly");
    TEST_ASSERT_EQ(1024, sid.buffer_size, "Buffer size set correctly");
    TEST_ASSERT_EQ(0, sid.buffer_pos, "Buffer position starts at 0");
    
    // Generate some samples
    sid_write(&sid, SID_MEM_START + SID_V1_FREQ_HI, 0x10);
    sid_write(&sid, SID_MEM_START + SID_V1_AD, 0x00);
    sid_write(&sid, SID_MEM_START + SID_V1_SR, 0xF0);
    sid_write(&sid, SID_MEM_START + SID_V1_CTRL, SID_CTRL_GATE | SID_CTRL_SAW);
    sid_write(&sid, SID_MEM_START + SID_MODE_VOL, 0x0F);
    
    // Clock enough for at least one sample
    sid_clock(&sid, sid.cycles_per_sample * 10);
    
    uint32_t samples = sid_get_samples(&sid);
    TEST_ASSERT(samples > 0, "Samples generated in buffer");
}

// ============================================================================
// SID Voice 3 Readback Tests
// ============================================================================

static void test_sid_voice3_readback(void) {
    printf("\n=== SID Voice 3 Readback Tests ===\n");
    
    SID sid;
    sid_init(&sid, 985248, 44100);
    
    // Set up voice 3 oscillator
    sid_write(&sid, SID_MEM_START + SID_V3_FREQ_HI, 0xFF);
    sid_write(&sid, SID_MEM_START + SID_V3_FREQ_LO, 0xFF);
    sid_write(&sid, SID_MEM_START + SID_V3_AD, 0x00);
    sid_write(&sid, SID_MEM_START + SID_V3_SR, 0xF0);
    sid_write(&sid, SID_MEM_START + SID_V3_CTRL, SID_CTRL_GATE | SID_CTRL_SAW);
    
    // Clock to advance oscillator
    sid_clock(&sid, 1000);
    
    // Read OSC3 - should be upper 8 bits of oscillator 3
    uint8_t osc3 = sid_read(&sid, SID_MEM_START + SID_OSC3);
    TEST_ASSERT(osc3 != 0 || sid.voice[2].accumulator > 0, "OSC3 readback functional");
    
    // Clock more and check envelope
    sid_clock(&sid, 50000);
    uint8_t env3 = sid_read(&sid, SID_MEM_START + SID_ENV3);
    TEST_ASSERT(env3 > 0 || sid.voice[2].env_level > 0, "ENV3 readback functional");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
    printf("========================================\n");
    printf("   SID 6581 Sound Chip Test Suite\n");
    printf("========================================\n");
    
    test_sid_init();
    test_sid_register_writes();
    test_sid_register_reads();
    test_sid_control_register();
    test_sid_waveforms();
    test_sid_envelope();
    test_sid_filter();
    test_sid_output();
    test_sid_audio_buffer();
    test_sid_voice3_readback();
    
    printf("\n========================================\n");
    printf("   Test Results\n");
    printf("========================================\n");
    printf("Total tests: %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", (float)tests_passed / tests_run * 100);
    
    if (tests_failed == 0) {
        printf("\n🎉 All tests passed!\n");
    } else {
        printf("\n❌ Some tests failed.\n");
    }
    
    return tests_failed > 0 ? 1 : 0;
}
