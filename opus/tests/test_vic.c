/**
 * test_vic.c - VIC-II tests
 * 
 * Tests raster timing, bad lines, interrupts, and register behavior.
 */

#include "test_framework.h"
#include "../src/c64.h"

static C64System sys;

static void setup(void) {
    c64_init(&sys);
    mem_reset(&sys.mem);
    vic_reset(&sys.vic);
    cpu_reset(&sys.cpu);
}

// Advance VIC by N cycles
static void vic_cycles(int n) {
    for (int i = 0; i < n; i++) {
        vic_step(&sys.vic);
    }
}

// Advance to specific raster line and ensure cycle 1 (IRQ check) has run
static void advance_to_line(u16 line) {
    // First, advance until we reach the target line
    while (sys.vic.raster_line != line) {
        vic_step(&sys.vic);
    }
    // Now advance to at least cycle 2 to ensure cycle 1's IRQ check has run
    while (sys.vic.raster_cycle < 2) {
        vic_step(&sys.vic);
    }
}

// ============================================================================
// Raster Counter Tests
// ============================================================================

TEST(raster_initial_state) {
    setup();
    
    ASSERT_EQ(0, sys.vic.raster_line);
    ASSERT_EQ(1, sys.vic.raster_cycle);
    PASS();
}

TEST(raster_line_increment) {
    setup();
    
    // Advance through one complete line (63 cycles PAL)
    vic_cycles(PAL_CYCLES_PER_LINE);
    
    ASSERT_EQ(1, sys.vic.raster_line);
    ASSERT_EQ(1, sys.vic.raster_cycle);
    PASS();
}

TEST(raster_frame_wrap) {
    setup();
    
    // Advance through entire frame
    vic_cycles(PAL_CYCLES_PER_LINE * PAL_RASTER_LINES);
    
    ASSERT_EQ(0, sys.vic.raster_line);
    ASSERT_EQ(1, sys.vic.frame_count);
    PASS();
}

TEST(raster_register_d012) {
    setup();
    
    // Advance to line 100
    advance_to_line(100);
    
    u8 d012 = vic_read(&sys.vic, VIC_RASTER);
    ASSERT_EQ(100, d012);
    PASS();
}

TEST(raster_register_d011_bit7) {
    setup();
    
    // Advance to line 256 (bit 8 set)
    advance_to_line(256);
    
    u8 d011 = vic_read(&sys.vic, VIC_CR1);
    ASSERT_TRUE(d011 & 0x80);  // Bit 7 = raster bit 8
    
    u8 d012 = vic_read(&sys.vic, VIC_RASTER);
    ASSERT_EQ(0x00, d012);  // Lower 8 bits = 0
    PASS();
}

TEST(raster_high_line_300) {
    setup();
    
    // Advance to line 300 (PAL has 312 lines)
    advance_to_line(300);
    
    u8 d011 = vic_read(&sys.vic, VIC_CR1);
    u8 d012 = vic_read(&sys.vic, VIC_RASTER);
    
    u16 raster = ((d011 & 0x80) << 1) | d012;
    ASSERT_EQ(300, raster);
    PASS();
}

// ============================================================================
// Raster IRQ Tests (per documentation)
// ============================================================================

TEST(raster_irq_set_target) {
    setup();
    
    // Set raster IRQ line to 100
    vic_write(&sys.vic, VIC_RASTER, 100);
    
    ASSERT_EQ(100, sys.vic.raster_irq_line);
    PASS();
}

TEST(raster_irq_set_target_high) {
    setup();
    
    // Set raster IRQ line to 256 (requires D011 bit 7)
    vic_write(&sys.vic, VIC_CR1, 0x80 | 0x1B);  // Bit 7 set
    vic_write(&sys.vic, VIC_RASTER, 0x00);
    
    ASSERT_EQ(256, sys.vic.raster_irq_line);
    PASS();
}

TEST(raster_irq_trigger) {
    setup();
    
    // Set raster IRQ for line 50
    vic_write(&sys.vic, VIC_RASTER, 50);
    // Enable raster IRQ
    vic_write(&sys.vic, VIC_IMR, 0x01);
    
    // Advance to line 50
    advance_to_line(50);
    
    // IRQ should be pending
    u8 irr = vic_read(&sys.vic, VIC_IRR);
    ASSERT_TRUE(irr & 0x01);  // Raster IRQ flag
    ASSERT_TRUE(irr & 0x80);  // IRQ occurred
    ASSERT_TRUE(sys.vic.irq_pending);
    PASS();
}

TEST(raster_irq_not_enabled) {
    setup();
    
    // Set raster IRQ for line 50 but don't enable it
    vic_write(&sys.vic, VIC_RASTER, 50);
    vic_write(&sys.vic, VIC_IMR, 0x00);  // No IRQ enabled
    
    advance_to_line(50);
    
    // IRQ flag set but no IRQ triggered
    u8 irr = vic_read(&sys.vic, VIC_IRR);
    ASSERT_TRUE(irr & 0x01);   // Flag set
    ASSERT_FALSE(irr & 0x80);  // No IRQ bit
    ASSERT_FALSE(sys.vic.irq_pending);
    PASS();
}

TEST(raster_irq_acknowledge) {
    setup();
    
    // Trigger a raster IRQ
    vic_write(&sys.vic, VIC_RASTER, 50);
    vic_write(&sys.vic, VIC_IMR, 0x01);
    advance_to_line(50);
    
    ASSERT_TRUE(sys.vic.irq_pending);
    
    // Acknowledge by writing 1 to IRR
    vic_write(&sys.vic, VIC_IRR, 0x01);
    
    u8 irr = vic_read(&sys.vic, VIC_IRR);
    ASSERT_FALSE(irr & 0x01);
    ASSERT_FALSE(irr & 0x80);
    ASSERT_FALSE(sys.vic.irq_pending);
    PASS();
}

// ============================================================================
// Bad Line Tests (per documentation)
// ============================================================================

TEST(bad_line_condition) {
    setup();
    
    // Bad line occurs when (raster & 7) == YSCROLL, lines 48-247
    // Default YSCROLL is 3
    sys.vic.regs[VIC_CR1] = 0x1B;  // DEN=1, YSCROLL=3
    
    // Line 48 should NOT be a bad line (48 & 7 = 0 != 3)
    advance_to_line(48);
    vic_cycles(12);  // Bad line latches at cycle 12
    ASSERT_FALSE(sys.vic.bad_line_active);
    
    // Line 51 SHOULD be a bad line (51 & 7 = 3)
    advance_to_line(51);
    vic_cycles(12);
    ASSERT_TRUE(sys.vic.bad_line_condition);
    PASS();
}

TEST(bad_line_ba_low) {
    setup();
    
    sys.vic.regs[VIC_CR1] = 0x1B;  // DEN=1, YSCROLL=3
    
    // First, advance past the display area to reset bad line state
    while (sys.vic.raster_line < 10) {
        vic_step(&sys.vic);
    }
    
    // Now advance to line 51 (a bad line: 51 & 7 = 3 == yscroll)
    while (sys.vic.raster_line != 51) {
        vic_step(&sys.vic);
    }
    
    // We're now at line 51, cycle 1. Bad line is detected at cycle 12.
    // BA goes low at cycle 12 during a bad line
    ASSERT_EQ(1, sys.vic.raster_cycle);
    
    // Before cycle 12, BA should not be low (bad line not yet latched)
    // Note: the bad_line_condition might already be true, but ba_low 
    // only goes low at cycle 12 when bad_line_active is set
    
    // Advance to cycle 11
    while (sys.vic.raster_cycle < 11) vic_step(&sys.vic);
    
    // At cycle 11, BA should still be false
    ASSERT_FALSE(sys.vic.ba_low);
    
    // Advance through cycle 12 and 13
    vic_step(&sys.vic);  // Now at cycle 12
    vic_step(&sys.vic);  // Process cycle 12, now at cycle 13
    
    // BA should now be low (cycles 12-54)
    ASSERT_TRUE(sys.vic.ba_low);
    
    // Advance to after cycle 54
    while (sys.vic.raster_cycle <= 55 && sys.vic.raster_line == 51) {
        vic_step(&sys.vic);
    }
    
    // After cycle 54, BA goes high
    ASSERT_FALSE(sys.vic.ba_low);
    PASS();
}

TEST(bad_line_den_disabled) {
    setup();
    
    // DEN=0 should prevent bad lines
    sys.vic.regs[VIC_CR1] = 0x0B;  // DEN=0
    
    advance_to_line(51);
    vic_cycles(12);
    
    ASSERT_FALSE(sys.vic.bad_line_condition);
    ASSERT_FALSE(sys.vic.ba_low);
    PASS();
}

TEST(bad_line_outside_display) {
    setup();
    
    sys.vic.regs[VIC_CR1] = 0x1B;  // DEN=1, YSCROLL=3
    
    // Line 30 is before display area (48-247)
    advance_to_line(30);
    vic_cycles(12);
    ASSERT_FALSE(sys.vic.bad_line_condition);
    
    // Line 260 is after display area
    advance_to_line(260);
    vic_cycles(12);
    ASSERT_FALSE(sys.vic.bad_line_condition);
    PASS();
}

// ============================================================================
// Register Tests
// ============================================================================

TEST(vic_border_color) {
    setup();
    
    vic_write(&sys.vic, VIC_BORDER, 0x02);  // Red
    
    ASSERT_EQ(0x02, vic_read(&sys.vic, VIC_BORDER));
    PASS();
}

TEST(vic_background_colors) {
    setup();
    
    vic_write(&sys.vic, VIC_BG0, 0x01);
    vic_write(&sys.vic, VIC_BG1, 0x02);
    vic_write(&sys.vic, VIC_BG2, 0x03);
    vic_write(&sys.vic, VIC_BG3, 0x04);
    
    ASSERT_EQ(0x01, vic_read(&sys.vic, VIC_BG0));
    ASSERT_EQ(0x02, vic_read(&sys.vic, VIC_BG1));
    ASSERT_EQ(0x03, vic_read(&sys.vic, VIC_BG2));
    ASSERT_EQ(0x04, vic_read(&sys.vic, VIC_BG3));
    PASS();
}

TEST(vic_memory_pointer) {
    setup();
    
    // $D018: Memory pointers
    // Bits 4-7: Screen memory ($0400 steps)
    // Bits 1-3: Character memory ($0800 steps)
    
    vic_write(&sys.vic, VIC_MEMPTR, 0x14);  // Default: Screen=$0400, Char=$1000
    
    // Force update
    vic_step(&sys.vic);
    
    ASSERT_EQ(0x0400, sys.vic.video_matrix_base);
    ASSERT_EQ(0x1000, sys.vic.char_base);
    PASS();
}

TEST(vic_memory_pointer_high) {
    setup();
    
    // Screen at $3C00, Char at $3800
    vic_write(&sys.vic, VIC_MEMPTR, 0xFE);
    vic_step(&sys.vic);
    
    ASSERT_EQ(0x3C00, sys.vic.video_matrix_base);
    ASSERT_EQ(0x3800, sys.vic.char_base);
    PASS();
}

TEST(vic_control_register_1) {
    setup();
    
    // Test various CR1 settings
    vic_write(&sys.vic, VIC_CR1, 0x3B);  // YSCROLL=3, RSEL=1, DEN=1, BMM=0
    vic_step(&sys.vic);
    
    ASSERT_EQ(3, sys.vic.yscroll);
    ASSERT_TRUE(sys.vic.den);
    PASS();
}

TEST(vic_control_register_2) {
    setup();
    
    vic_write(&sys.vic, VIC_CR2, 0xC8);  // XSCROLL=0, CSEL=1, MCM=0
    vic_step(&sys.vic);
    
    ASSERT_EQ(0, sys.vic.xscroll);
    PASS();
}

TEST(vic_irr_unused_bits) {
    setup();
    
    // Bits 4-6 of IRR always read as 1
    u8 irr = vic_read(&sys.vic, VIC_IRR);
    ASSERT_EQ(0x70, irr & 0x70);
    PASS();
}

TEST(vic_imr_unused_bits) {
    setup();
    
    // Bits 4-7 of IMR always read as 1
    u8 imr = vic_read(&sys.vic, VIC_IMR);
    ASSERT_EQ(0xF0, imr & 0xF0);
    PASS();
}

TEST(vic_collision_clear_on_read) {
    setup();
    
    // Set collision registers
    sys.vic.regs[VIC_MM] = 0xFF;  // Sprite-sprite
    sys.vic.regs[VIC_MD] = 0xFF;  // Sprite-data
    
    // Reading should return value and clear it
    u8 mm = vic_read(&sys.vic, VIC_MM);
    u8 md = vic_read(&sys.vic, VIC_MD);
    
    ASSERT_EQ(0xFF, mm);
    ASSERT_EQ(0xFF, md);
    
    // Should be cleared now
    ASSERT_EQ(0x00, vic_read(&sys.vic, VIC_MM));
    ASSERT_EQ(0x00, vic_read(&sys.vic, VIC_MD));
    PASS();
}

TEST(vic_unused_registers) {
    setup();
    
    // Registers $2F-$3F should return $FF
    ASSERT_EQ(0xFF, vic_read(&sys.vic, 0x2F));
    ASSERT_EQ(0xFF, vic_read(&sys.vic, 0x3F));
    PASS();
}

// ============================================================================
// Sprite Tests
// ============================================================================

TEST(vic_sprite_positions) {
    setup();
    
    // Set sprite 0 position
    vic_write(&sys.vic, VIC_M0X, 0x80);
    vic_write(&sys.vic, VIC_M0Y, 0x64);
    
    ASSERT_EQ(0x80, vic_read(&sys.vic, VIC_M0X));
    ASSERT_EQ(0x64, vic_read(&sys.vic, VIC_M0Y));
    PASS();
}

TEST(vic_sprite_x_msb) {
    setup();
    
    // Set MSB of sprite 0 X position
    vic_write(&sys.vic, VIC_MSBX, 0x01);
    
    ASSERT_EQ(0x01, vic_read(&sys.vic, VIC_MSBX));
    PASS();
}

TEST(vic_sprite_enable) {
    setup();
    
    vic_write(&sys.vic, VIC_ME, 0xFF);  // Enable all sprites
    
    ASSERT_EQ(0xFF, vic_read(&sys.vic, VIC_ME));
    PASS();
}

// ============================================================================
// Frame Counter
// ============================================================================

TEST(vic_frame_counter) {
    setup();
    
    ASSERT_EQ(0, sys.vic.frame_count);
    
    // Run one complete frame
    vic_cycles(PAL_CYCLES_PER_LINE * PAL_RASTER_LINES);
    
    ASSERT_EQ(1, sys.vic.frame_count);
    
    // Run another frame
    vic_cycles(PAL_CYCLES_PER_LINE * PAL_RASTER_LINES);
    
    ASSERT_EQ(2, sys.vic.frame_count);
    PASS();
}

// ============================================================================
// Run all VIC tests
// ============================================================================

void run_vic_tests(void) {
    TEST_SUITE("VIC-II - Raster Counter");
    RUN_TEST(raster_initial_state);
    RUN_TEST(raster_line_increment);
    RUN_TEST(raster_frame_wrap);
    RUN_TEST(raster_register_d012);
    RUN_TEST(raster_register_d011_bit7);
    RUN_TEST(raster_high_line_300);
    
    TEST_SUITE("VIC-II - Raster IRQ");
    RUN_TEST(raster_irq_set_target);
    RUN_TEST(raster_irq_set_target_high);
    RUN_TEST(raster_irq_trigger);
    RUN_TEST(raster_irq_not_enabled);
    RUN_TEST(raster_irq_acknowledge);
    
    TEST_SUITE("VIC-II - Bad Lines");
    RUN_TEST(bad_line_condition);
    RUN_TEST(bad_line_ba_low);
    RUN_TEST(bad_line_den_disabled);
    RUN_TEST(bad_line_outside_display);
    
    TEST_SUITE("VIC-II - Registers");
    RUN_TEST(vic_border_color);
    RUN_TEST(vic_background_colors);
    RUN_TEST(vic_memory_pointer);
    RUN_TEST(vic_memory_pointer_high);
    RUN_TEST(vic_control_register_1);
    RUN_TEST(vic_control_register_2);
    RUN_TEST(vic_irr_unused_bits);
    RUN_TEST(vic_imr_unused_bits);
    RUN_TEST(vic_collision_clear_on_read);
    RUN_TEST(vic_unused_registers);
    
    TEST_SUITE("VIC-II - Sprites");
    RUN_TEST(vic_sprite_positions);
    RUN_TEST(vic_sprite_x_msb);
    RUN_TEST(vic_sprite_enable);
    
    TEST_SUITE("VIC-II - Frame Counter");
    RUN_TEST(vic_frame_counter);
}
