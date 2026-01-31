/**
 * test_cia.c - CIA 6526 tests
 * 
 * Tests timers, ICR, TOD clock, and keyboard matrix.
 * Covers edge cases from Lorenz test suite findings.
 */

#include "test_framework.h"
#include "../src/c64.h"

static C64System sys;

static void setup(void) {
    c64_init(&sys);
    mem_reset(&sys.mem);
    cia_reset(&sys.cia1);
    cia_reset(&sys.cia2);
    cpu_reset(&sys.cpu);
}

// Clock CIA N times
static void cia_cycles(C64Cia *cia, int n) {
    for (int i = 0; i < n; i++) {
        cia_clock(cia);
    }
}

// ============================================================================
// Timer A Basic Tests
// ============================================================================

TEST(timer_a_initial_state) {
    setup();
    
    // Timer should start at $FFFF
    ASSERT_EQ(0xFF, cia_read(&sys.cia1, CIA_TALO));
    ASSERT_EQ(0xFF, cia_read(&sys.cia1, CIA_TAHI));
    PASS();
}

TEST(timer_a_load_latch) {
    setup();
    
    // Write latch values
    cia_write(&sys.cia1, CIA_TALO, 0x34);
    cia_write(&sys.cia1, CIA_TAHI, 0x12);
    
    // Writing high byte loads timer (when not running)
    ASSERT_EQ(0x34, cia_read(&sys.cia1, CIA_TALO));
    ASSERT_EQ(0x12, cia_read(&sys.cia1, CIA_TAHI));
    PASS();
}

TEST(timer_a_countdown) {
    setup();
    
    // Set timer to 10
    cia_write(&sys.cia1, CIA_TALO, 0x0A);
    cia_write(&sys.cia1, CIA_TAHI, 0x00);
    
    // Start timer (phi2 mode)
    cia_write(&sys.cia1, CIA_CRA, CIA_CR_START);
    
    // Account for pipeline delay (2 cycles)
    cia_cycles(&sys.cia1, 2);
    
    // Timer should have started counting
    cia_cycles(&sys.cia1, 5);
    
    u16 timer = cia_read(&sys.cia1, CIA_TALO) | (cia_read(&sys.cia1, CIA_TAHI) << 8);
    ASSERT_EQ(5, timer);  // 10 - 5 = 5
    PASS();
}

TEST(timer_a_underflow) {
    setup();
    
    // Set timer to 5
    cia_write(&sys.cia1, CIA_TALO, 0x05);
    cia_write(&sys.cia1, CIA_TAHI, 0x00);
    
    // Set latch to 10 for reload
    cia_write(&sys.cia1, CIA_TALO, 0x0A);
    cia_write(&sys.cia1, CIA_TAHI, 0x00);
    
    // Force load and start
    cia_write(&sys.cia1, CIA_CRA, CIA_CR_START | CIA_CR_LOAD);
    
    // Clear any previous ICR
    cia_read(&sys.cia1, CIA_ICR);
    
    // Wait for pipeline + countdown to 0
    cia_cycles(&sys.cia1, 2 + 10 + 1);  // delay + count + underflow
    
    // Timer should reload and set ICR flag
    u8 icr = cia_read(&sys.cia1, CIA_ICR);
    ASSERT_TRUE(icr & CIA_ICR_TA);
    PASS();
}

TEST(timer_a_continuous_mode) {
    setup();
    
    // Set timer to 3
    cia_write(&sys.cia1, CIA_TALO, 0x03);
    cia_write(&sys.cia1, CIA_TAHI, 0x00);
    
    // Start in continuous mode (RUNMODE=0)
    cia_write(&sys.cia1, CIA_CRA, CIA_CR_START);
    
    // Clear ICR
    cia_read(&sys.cia1, CIA_ICR);
    
    // Run through multiple underflows
    cia_cycles(&sys.cia1, 2 + 4 + 4 + 4);  // delay + 3 underflows
    
    // Timer should still be running
    ASSERT_TRUE(sys.cia1.cra & CIA_CR_START);
    PASS();
}

TEST(timer_a_one_shot_mode) {
    setup();
    
    // Set timer to 3
    cia_write(&sys.cia1, CIA_TALO, 0x03);
    cia_write(&sys.cia1, CIA_TAHI, 0x00);
    
    // Start in one-shot mode (RUNMODE=1)
    cia_write(&sys.cia1, CIA_CRA, CIA_CR_START | CIA_CR_RUNMODE);
    
    // Run through underflow
    cia_cycles(&sys.cia1, 2 + 4);
    
    // Timer should have stopped
    ASSERT_FALSE(sys.cia1.cra & CIA_CR_START);
    PASS();
}

// ============================================================================
// Timer B Tests
// ============================================================================

TEST(timer_b_basic) {
    setup();
    
    cia_write(&sys.cia1, CIA_TBLO, 0x20);
    cia_write(&sys.cia1, CIA_TBHI, 0x00);
    
    cia_write(&sys.cia1, CIA_CRB, CIA_CR_START);
    
    cia_cycles(&sys.cia1, 2 + 10);
    
    u16 timer = cia_read(&sys.cia1, CIA_TBLO) | (cia_read(&sys.cia1, CIA_TBHI) << 8);
    ASSERT(timer < 0x20);
    PASS();
}

TEST(timer_b_count_timer_a) {
    setup();
    
    // Timer A: counts phi2, set to 2
    cia_write(&sys.cia1, CIA_TALO, 0x02);
    cia_write(&sys.cia1, CIA_TAHI, 0x00);
    
    // Timer B: counts Timer A underflows, set to 1
    cia_write(&sys.cia1, CIA_TBLO, 0x01);
    cia_write(&sys.cia1, CIA_TBHI, 0x00);
    
    // Start both timers, Timer B counts Timer A (INMODE bits 5-6 = 10)
    cia_write(&sys.cia1, CIA_CRA, CIA_CR_START);
    cia_write(&sys.cia1, CIA_CRB, CIA_CR_START | (2 << 5));
    
    // Clear ICR
    cia_read(&sys.cia1, CIA_ICR);
    
    // Timer A with value 2: needs 3 counting cycles to underflow (2->1->0->underflow)
    // Timer B with value 1: needs 2 Timer A underflows (1->0->underflow)
    // Each Timer A underflow: 2 (delay first time) + 3 = 5 cycles, then 3 cycles after
    // Run enough cycles for Timer B to underflow
    cia_cycles(&sys.cia1, 2 + 3 + 3 + 2);  // TA delay + 2 TA underflows + margin
    
    // Timer B should have underflowed
    u8 icr = cia_read(&sys.cia1, CIA_ICR);
    ASSERT_TRUE(icr & CIA_ICR_TB);
    PASS();
}

// ============================================================================
// ICR Tests (per documentation findings)
// ============================================================================

TEST(icr_timer_flag) {
    setup();
    
    cia_write(&sys.cia1, CIA_TALO, 0x02);
    cia_write(&sys.cia1, CIA_TAHI, 0x00);
    cia_write(&sys.cia1, CIA_CRA, CIA_CR_START | CIA_CR_RUNMODE);
    
    cia_read(&sys.cia1, CIA_ICR);  // Clear
    
    // Run to underflow
    cia_cycles(&sys.cia1, 2 + 3);
    
    u8 icr = cia_read(&sys.cia1, CIA_ICR);
    ASSERT_TRUE(icr & CIA_ICR_TA);
    PASS();
}

TEST(icr_clear_on_read) {
    setup();
    
    // Set up a timer to underflow
    cia_write(&sys.cia1, CIA_TALO, 0x01);
    cia_write(&sys.cia1, CIA_TAHI, 0x00);
    cia_write(&sys.cia1, CIA_CRA, CIA_CR_START | CIA_CR_RUNMODE);
    
    cia_cycles(&sys.cia1, 2 + 2);
    
    u8 icr1 = cia_read(&sys.cia1, CIA_ICR);
    u8 icr2 = cia_read(&sys.cia1, CIA_ICR);
    
    ASSERT_TRUE(icr1 & CIA_ICR_TA);   // First read sees flag
    ASSERT_FALSE(icr2 & CIA_ICR_TA);  // Second read cleared
    PASS();
}

TEST(icr_mask_enable) {
    setup();
    
    // Enable Timer A interrupt
    cia_write(&sys.cia1, CIA_ICR, 0x81);  // Set bit 7 + bit 0
    
    ASSERT_EQ(CIA_ICR_TA, sys.cia1.icr_mask);
    PASS();
}

TEST(icr_mask_disable) {
    setup();
    
    // Enable then disable Timer A interrupt
    cia_write(&sys.cia1, CIA_ICR, 0x81);  // Enable
    cia_write(&sys.cia1, CIA_ICR, 0x01);  // Disable (bit 7 = 0)
    
    ASSERT_EQ(0, sys.cia1.icr_mask);
    PASS();
}

TEST(icr_bit7_delay) {
    setup();
    
    // Per documentation: ICR bit 7 has 1-cycle delay
    cia_write(&sys.cia1, CIA_ICR, 0x81);  // Enable Timer A IRQ
    
    cia_write(&sys.cia1, CIA_TALO, 0x03);
    cia_write(&sys.cia1, CIA_TAHI, 0x00);
    cia_write(&sys.cia1, CIA_CRA, CIA_CR_START | CIA_CR_RUNMODE);
    
    cia_read(&sys.cia1, CIA_ICR);  // Clear
    
    // Run to just after underflow
    // Pipeline delay: 2 cycles
    // Timer counts: 3->2, 2->1, 1->0, then 0->underflow = 4 cycles
    // ICR delay: 1 cycle for bit 7
    // Total: 2 + 4 + 1 = 7 cycles
    cia_cycles(&sys.cia1, 2 + 4 + 1);
    
    // ICR should have bit 7 set (with delay processed)
    u8 icr = cia_read(&sys.cia1, CIA_ICR);
    ASSERT_TRUE(icr & CIA_ICR_IR);
    PASS();
}

TEST(icr_irq_pending) {
    setup();
    
    // Enable Timer A IRQ
    cia_write(&sys.cia1, CIA_ICR, 0x81);
    
    cia_write(&sys.cia1, CIA_TALO, 0x02);
    cia_write(&sys.cia1, CIA_TAHI, 0x00);
    cia_write(&sys.cia1, CIA_CRA, CIA_CR_START | CIA_CR_RUNMODE);
    
    cia_read(&sys.cia1, CIA_ICR);  // Clear
    
    // Timer=2: 2 (delay) + 3 (count 2,1,0,underflow) + 1 (IRQ delay) = 6 cycles
    cia_cycles(&sys.cia1, 2 + 3 + 1);
    
    ASSERT_TRUE(sys.cia1.irq_pending);
    
    // Reading ICR clears irq_pending
    cia_read(&sys.cia1, CIA_ICR);
    ASSERT_FALSE(sys.cia1.irq_pending);
    PASS();
}

// ============================================================================
// Timer Pipeline Delay Tests (per documentation)
// ============================================================================

TEST(timer_pipeline_delay) {
    setup();
    
    // Set timer to 5
    cia_write(&sys.cia1, CIA_TALO, 0x05);
    cia_write(&sys.cia1, CIA_TAHI, 0x00);
    
    // Start timer
    cia_write(&sys.cia1, CIA_CRA, CIA_CR_START);
    
    // Immediately after start, timer shouldn't have decremented yet
    // (due to pipeline delay)
    u16 timer = cia_read(&sys.cia1, CIA_TALO) | (cia_read(&sys.cia1, CIA_TAHI) << 8);
    ASSERT_EQ(5, timer);
    
    // After pipeline delay, timer should count
    cia_cycles(&sys.cia1, 3);
    timer = cia_read(&sys.cia1, CIA_TALO) | (cia_read(&sys.cia1, CIA_TAHI) << 8);
    ASSERT(timer < 5);
    PASS();
}

// ============================================================================
// TOD Clock Tests
// ============================================================================

TEST(tod_initial_state) {
    setup();
    
    ASSERT_EQ(0, cia_read(&sys.cia1, CIA_TOD_10));
    ASSERT_EQ(0, cia_read(&sys.cia1, CIA_TOD_S));
    ASSERT_EQ(0, cia_read(&sys.cia1, CIA_TOD_M));
    ASSERT_EQ(0, cia_read(&sys.cia1, CIA_TOD_H));
    PASS();
}

TEST(tod_write) {
    setup();
    
    // Write time (not alarm)
    sys.cia1.crb &= ~CIA_CR_TODIN;
    
    cia_write(&sys.cia1, CIA_TOD_10, 0x05);
    cia_write(&sys.cia1, CIA_TOD_S, 0x30);
    cia_write(&sys.cia1, CIA_TOD_M, 0x15);
    cia_write(&sys.cia1, CIA_TOD_H, 0x02);
    
    ASSERT_EQ(0x05, sys.cia1.tod_10ths);
    ASSERT_EQ(0x30, sys.cia1.tod_sec);
    ASSERT_EQ(0x15, sys.cia1.tod_min);
    ASSERT_EQ(0x02, sys.cia1.tod_hr);
    PASS();
}

TEST(tod_alarm_write) {
    setup();
    
    // Enable alarm write mode
    cia_write(&sys.cia1, CIA_CRB, CIA_CR_TODIN);
    
    cia_write(&sys.cia1, CIA_TOD_10, 0x09);
    cia_write(&sys.cia1, CIA_TOD_S, 0x59);
    cia_write(&sys.cia1, CIA_TOD_M, 0x59);
    cia_write(&sys.cia1, CIA_TOD_H, 0x11);
    
    ASSERT_EQ(0x09, sys.cia1.alarm_10ths);
    ASSERT_EQ(0x59, sys.cia1.alarm_sec);
    ASSERT_EQ(0x59, sys.cia1.alarm_min);
    ASSERT_EQ(0x11, sys.cia1.alarm_hr);
    PASS();
}

TEST(tod_latch_on_hours_read) {
    setup();
    
    // Set initial time
    sys.cia1.tod_10ths = 5;
    sys.cia1.tod_sec = 0x30;
    sys.cia1.tod_min = 0x15;
    sys.cia1.tod_hr = 0x02;
    
    // Reading hours latches all values
    u8 hr = cia_read(&sys.cia1, CIA_TOD_H);
    ASSERT_TRUE(sys.cia1.tod_latched);
    ASSERT_EQ(0x02, hr);
    
    // Change underlying values while latched
    sys.cia1.tod_10ths = 9;
    sys.cia1.tod_sec = 0x45;
    
    // Read seconds first (still latched) - should return original value
    u8 sec = cia_read(&sys.cia1, CIA_TOD_S);
    ASSERT_EQ(0x30, sec);
    
    // Still latched (unlatch happens on 10ths read)
    ASSERT_TRUE(sys.cia1.tod_latched);
    
    // Read 10ths - should return latched value and unlatch
    u8 tenths = cia_read(&sys.cia1, CIA_TOD_10);
    ASSERT_EQ(5, tenths);
    
    // Now unlatched
    ASSERT_FALSE(sys.cia1.tod_latched);
    PASS();
}

// ============================================================================
// Port Tests
// ============================================================================

TEST(port_data_direction) {
    setup();
    
    // Set port A as output
    cia_write(&sys.cia1, CIA_DDRA, 0xFF);
    cia_write(&sys.cia1, CIA_PRA, 0xAA);
    
    ASSERT_EQ(0xFF, sys.cia1.ddra);
    ASSERT_EQ(0xAA, sys.cia1.pra);
    PASS();
}

TEST(port_output_read) {
    setup();
    
    sys.cia2.ddra = 0xFF;  // All output
    cia_write(&sys.cia2, CIA_PRA, 0x55);
    
    u8 val = cia_read(&sys.cia2, CIA_PRA);
    ASSERT_EQ(0x55, val);
    PASS();
}

TEST(port_input_read) {
    setup();
    
    sys.cia2.ddra = 0x00;  // All input
    cia_write(&sys.cia2, CIA_PRA, 0x55);  // Written value
    
    // Input pins read external state (open = 0xFF)
    u8 val = cia_read(&sys.cia2, CIA_PRA);
    ASSERT_EQ(0xFF, val);
    PASS();
}

// ============================================================================
// CIA2 Specific Tests (NMI routing)
// ============================================================================

TEST(cia2_generates_nmi) {
    setup();
    
    // Enable Timer A interrupt on CIA2
    cia_write(&sys.cia2, CIA_ICR, 0x81);
    
    cia_write(&sys.cia2, CIA_TALO, 0x02);
    cia_write(&sys.cia2, CIA_TAHI, 0x00);
    cia_write(&sys.cia2, CIA_CRA, CIA_CR_START | CIA_CR_RUNMODE);
    
    // Timer=2: 2 (delay) + 3 (count 2,1,0,underflow) + 1 (IRQ delay) = 6 cycles
    cia_cycles(&sys.cia2, 2 + 3 + 1);
    
    ASSERT_TRUE(sys.cia2.irq_pending);
    // CIA2 IRQ routes to NMI
    PASS();
}

// ============================================================================
// Keyboard Matrix Tests (CIA1)
// ============================================================================

TEST(keyboard_no_keys) {
    setup();
    
    // Set port B as output (row select)
    cia_write(&sys.cia1, CIA_DDRB, 0xFF);
    cia_write(&sys.cia1, CIA_PRB, 0x00);  // All rows selected
    
    // Port A as input (column read)
    cia_write(&sys.cia1, CIA_DDRA, 0x00);
    
    // No keys pressed - should read $FF
    u8 cols = cia_read(&sys.cia1, CIA_PRA);
    ASSERT_EQ(0xFF, cols);
    PASS();
}

TEST(keyboard_key_pressed) {
    setup();
    
    // Press key at row 0, col 0
    cia_set_key(&sys.cia1, 0, 0, true);
    
    cia_write(&sys.cia1, CIA_DDRB, 0xFF);
    cia_write(&sys.cia1, CIA_PRB, 0xFE);  // Select row 0 (active low)
    cia_write(&sys.cia1, CIA_DDRA, 0x00);
    
    u8 cols = cia_read_keyboard(&sys.cia1);
    ASSERT_EQ(0xFE, cols);  // Column 0 pulled low
    
    // Release key
    cia_set_key(&sys.cia1, 0, 0, false);
    cols = cia_read_keyboard(&sys.cia1);
    ASSERT_EQ(0xFF, cols);
    PASS();
}

// ============================================================================
// Run all CIA tests
// ============================================================================

void run_cia_tests(void) {
    TEST_SUITE("CIA - Timer A Basic");
    RUN_TEST(timer_a_initial_state);
    RUN_TEST(timer_a_load_latch);
    RUN_TEST(timer_a_countdown);
    RUN_TEST(timer_a_underflow);
    RUN_TEST(timer_a_continuous_mode);
    RUN_TEST(timer_a_one_shot_mode);
    
    TEST_SUITE("CIA - Timer B");
    RUN_TEST(timer_b_basic);
    RUN_TEST(timer_b_count_timer_a);
    
    TEST_SUITE("CIA - ICR");
    RUN_TEST(icr_timer_flag);
    RUN_TEST(icr_clear_on_read);
    RUN_TEST(icr_mask_enable);
    RUN_TEST(icr_mask_disable);
    RUN_TEST(icr_bit7_delay);
    RUN_TEST(icr_irq_pending);
    
    TEST_SUITE("CIA - Timer Pipeline");
    RUN_TEST(timer_pipeline_delay);
    
    TEST_SUITE("CIA - TOD Clock");
    RUN_TEST(tod_initial_state);
    RUN_TEST(tod_write);
    RUN_TEST(tod_alarm_write);
    RUN_TEST(tod_latch_on_hours_read);
    
    TEST_SUITE("CIA - Ports");
    RUN_TEST(port_data_direction);
    RUN_TEST(port_output_read);
    RUN_TEST(port_input_read);
    
    TEST_SUITE("CIA - CIA2 NMI");
    RUN_TEST(cia2_generates_nmi);
    
    TEST_SUITE("CIA - Keyboard");
    RUN_TEST(keyboard_no_keys);
    RUN_TEST(keyboard_key_pressed);
}
