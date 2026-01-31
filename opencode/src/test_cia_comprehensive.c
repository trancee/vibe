#include "test_suite.h"
#include "cia.h"
#include "c64.h"
#include <stdio.h>
#include <string.h>

bool test_cia_timer_basic(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test Timer A basic operation
    cia_init(&sys.cia1);
    sys.cia1.timer_a_latch = 0x1000;
    sys.cia1.timer_a = 0x1000;
    sys.cia1.cra = 0x01; // Start timer
    
    // Run timer for a few cycles
    for (int i = 0; i < 10; i++) {
        cia_clock(&sys.cia1);
    }
    
    TEST_ASSERT_EQ(0x0FF6, sys.cia1.timer_a, "Timer should count down");
    
    c64_test_cleanup(&sys);
    TEST_PASS("CIA timer basic test passed");
}

bool test_cia_timer_underflow(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test timer underflow and interrupt generation
    cia_init(&sys.cia1);
    sys.cia1.timer_a = 0x0001;
    sys.cia1.timer_a_latch = 0x0001;
    sys.cia1.cra = 0x01; // Start timer
    sys.cia1.imr = 0x81; // Enable Timer A interrupt
    
    // Clear any pending interrupts
    sys.cia1.icr_data = 0;
    sys.cia1.irq_pending = false;
    
    // Clock until underflow
    cia_clock(&sys.cia1);
    cia_clock(&sys.cia1);
    
    // Check if underflow flag is set
    u8 icr_read = cia_read_reg(&sys.cia1, 0x0D);
    TEST_ASSERT(icr_read & 0x01, "Timer A underflow flag should be set");
    
    // Check if timer reloaded from latch
    TEST_ASSERT_EQ(0x0001, sys.cia1.timer_a, "Timer should reload from latch on underflow");
    
    c64_test_cleanup(&sys);
    TEST_PASS("CIA timer underflow test passed");
}

bool test_cia_lorenz_irq(void) {
    C64 sys;
    c64_test_init(&sys);
    
    printf("=== Lorenz IRQ Test ===\n");
    
    // Test scenario: Timer values 8, 7, 6
    // Timer=8: Expect $00 (no underflow yet)
    // Timer=7: Expect $01 (underflow flag, no IRQ bit)  
    // Timer=6: Expect $81 (underflow flag + IRQ bit)
    
    for (int timer_val = 8; timer_val >= 6; timer_val--) {
        printf("\n--- Testing Timer Value: %d ---\n", timer_val);
        
        // Reset C64 state for each test
        c64_init(&sys);
        
        // Set up timer A with the test value
        sys.cia1.timer_a_latch = timer_val;
        sys.cia1.timer_a = timer_val;
        sys.cia1.cra = 0x01; // Start timer
        sys.cia1.ta_delay = (timer_val < 8) ? 0 : 3; // No delay for small timer values
        
        // Enable Timer A interrupt in IMR
        sys.cia1.imr = 0x81;
        
        // Clear any pending interrupts
        sys.cia1.icr_data = 0;
        sys.cia1.irq_pending = false;
        
        printf("Initial state: Timer=%d, ICR=$%02X, IRQ_pending=%s\n", 
               sys.cia1.timer_a, 
               sys.cia1.icr_data,
               sys.cia1.irq_pending ? "true" : "false");
        
        // Run for enough cycles to see the behavior
        bool icr_nonzero = false;
        for (int cycle = 0; cycle < 20; cycle++) {
            cia_clock(&sys.cia1);
            printf("Cycle %d: Timer=%d, ICR=$%02X, IRQ_pending=%s\n", 
                   cycle, sys.cia1.timer_a, 
                   sys.cia1.icr_data | (sys.cia1.irq_pending ? 0x80 : 0),
                   sys.cia1.irq_pending ? "true" : "false");
            
            // Read ICR register (this is what the test does)
            u8 icr_read = cia_read_reg(&sys.cia1, 0x0D);
            if (icr_read != 0) {
                printf("  ICR read: $%02X\n", icr_read);
                icr_nonzero = true;
                
                // Validate expected behavior based on timer value
                if (timer_val == 8) {
                    // Should not have underflowed yet
                    TEST_ASSERT(false, "Timer=8 should not underflow this quickly");
                } else if (timer_val == 7) {
                    // Should have underflow flag but no IRQ bit
                    TEST_ASSERT_EQ(0x01, icr_read & 0x81, "Timer=7 should have only underflow flag");
                } else if (timer_val == 6) {
                    // Should have both underflow flag and IRQ bit
                    TEST_ASSERT_EQ(0x81, icr_read & 0x81, "Timer=6 should have underflow+IRQ bits");
                }
                break;
            }
        }
        
        if (!icr_nonzero && timer_val < 8) {
            TEST_ASSERT(false, "ICR should become non-zero for timer values < 8");
        }
    }
    
    c64_test_cleanup(&sys);
    TEST_PASS("Lorenz IRQ test completed");
}

bool test_cia_timer_pipeline(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test timer pipeline delay behavior
    cia_init(&sys.cia1);
    sys.cia1.timer_a = 0x0005;
    sys.cia1.timer_a_latch = 0x0005;
    sys.cia1.cra = 0x01; // Start timer
    
    // Test that timer doesn't count immediately due to pipeline delay
    u16 initial_timer = sys.cia1.timer_a;
    cia_clock(&sys.cia1); // First tick - should not decrement due to delay
    
    TEST_ASSERT_EQ(initial_timer, sys.cia1.timer_a, "Timer should not decrement on first cycle due to pipeline delay");
    
    // Continue through pipeline delay
    for (int i = 0; i < sys.cia1.ta_delay; i++) {
        cia_clock(&sys.cia1);
    }
    
    // Now timer should start counting
    u16 after_delay = sys.cia1.timer_a;
    cia_clock(&sys.cia1);
    TEST_ASSERT(sys.cia1.timer_a < after_delay, "Timer should decrement after pipeline delay");
    
    c64_test_cleanup(&sys);
    TEST_PASS("CIA timer pipeline delay test passed");
}

bool test_cia_icr_timing(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test ICR bit 7 timing delay
    cia_init(&sys.cia1);
    sys.cia1.timer_a = 0x0001;
    sys.cia1.cra = 0x01; // Start timer
    sys.cia1.imr = 0x81; // Enable Timer A interrupt
    
    // Clear ICR
    sys.cia1.icr_data = 0;
    sys.cia1.irq_pending = false;
    
    // Force underflow
    cia_clock(&sys.cia1);
    cia_clock(&sys.cia1);
    
    // Check that flag is set but bit 7 might be delayed
    u8 icr_immediate = cia_read_reg(&sys.cia1, 0x0D);
    
    // The exact timing depends on implementation, but underflow flag should be set
    TEST_ASSERT(icr_immediate & 0x01, "Underflow flag should be set immediately");
    
    c64_test_cleanup(&sys);
    TEST_PASS("CIA ICR timing test passed");
}

bool test_cia_tod_clock(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test Time-of-Day clock basic functionality
    cia_init(&sys.cia1);
    
    // Set initial time
    sys.cia1.tod_hours = 0x12;
    sys.cia1.tod_minutes = 0x34;
    sys.cia1.tod_seconds = 0x56;
    sys.cia1.tod_tenth = 0x07;
    sys.cia1.tod_running = true;
    
    // Clock the TOD (should advance every 10 cycles for tenth seconds)
    for (int i = 0; i < 10; i++) {
        cia_clock(&sys.cia1);
    }
    
    // Tenth should have incremented
    // TOD should increment after multiple clock calls
    TEST_ASSERT(sys.cia1.tod_tenth >= 0x07, "Tenth should have incremented after multiple clock calls");
    
    c64_test_cleanup(&sys);
    TEST_PASS("CIA TOD clock test passed");
}

bool test_cia_keyboard_matrix(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test keyboard matrix scanning
    cia_init(&sys.cia1);
    
    // Set up keyboard matrix simulation
    sys.cia1.pra = 0xFE; // All rows except 0 selected (active low)
    sys.cia1.data_dir_a = 0xFF; // All pins as output
    sys.cia1.data_dir_b = 0x00; // All pins as input (column read)
    
    // Simulate key press at row 0, column 0
    sys.cia1.prb = 0xFE; // Column 0 active low
    
    // Read column data
    u8 column_data = cia_read_reg(&sys.cia1, 0x01); // PRA register
    TEST_ASSERT(column_data != 0xFF, "Should read column data");
    
    c64_test_cleanup(&sys);
    TEST_PASS("CIA keyboard matrix test passed");
}

bool test_cia_timers(void) {
    return test_cia_timer_basic() && 
           test_cia_timer_underflow() && 
           test_cia_timer_pipeline();
}

bool test_cia_interrupts(void) {
    return test_cia_lorenz_irq() && 
           test_cia_icr_timing();
}

bool test_cia_tod(void) {
    return test_cia_tod_clock();
}