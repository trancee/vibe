#include "cia.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    C64CIA cia1;
} TestSystem;

void test_cia_timing() {
    printf("=== Lorenz IRQ Test Simulation ===\n");
    
    TestSystem sys;
    memset(&sys, 0, sizeof(TestSystem));
    cia_init(&sys.cia1);
    
    // Test scenario: Timer values 8, 7, 6
    // Timer=8: Expect $00 (no underflow yet)
    // Timer=7: Expect $01 (underflow flag, no IRQ bit)  
    // Timer=6: Expect $81 (underflow flag + IRQ bit)
    
    for (int timer_val = 8; timer_val >= 6; timer_val--) {
        printf("\n--- Testing Timer Value: %d ---\n", timer_val);
        
        // Reset CIA state for each test
        cia_init(&sys.cia1);
        sys.cia1.icr_irq_flag_set = false; // Clear the ICR delay flag
        
        // Set up timer A with the test value
        sys.cia1.timer_a_latch = timer_val;
        sys.cia1.timer_a = timer_val;
        sys.cia1.cra = 0x01; // Start timer
        sys.cia1.ta_delay = 3; // Pipeline delay
        
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
        for (int cycle = 0; cycle < 15; cycle++) {
            cia_clock(&sys.cia1);
            printf("Cycle %d: Timer=%d, ICR=$%02X, IRQ_pending=%s\n", 
                   cycle, sys.cia1.timer_a, 
                   sys.cia1.icr_data | (sys.cia1.irq_pending ? 0x80 : 0),
                   sys.cia1.irq_pending ? "true" : "false");
            
            // Check ICR register (this is what the test does)
            u8 icr_value = cia_read_reg(&sys.cia1, 0x0D);
            if (icr_value != 0x00) {
                printf("  ICR read: $%02X\n", icr_value);
                break;
            }
        }
    }
}

#ifdef STANDALONE_MAIN
int main(void) {
    printf("CIA Timing Test Harness\n");
    test_cia_timing();
    return 0;
}
#endif