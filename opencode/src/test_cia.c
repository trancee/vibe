#include "c64.h"
#include "cia.h"
#include <stdio.h>
#include <stdlib.h>

// Simple Lorenz irq test based on implementation plan details
int test_lorenz_irq(C64* sys) {
    printf("=== Lorenz IRQ Test ===\n");
    
    // Test scenario: Timer values 8, 7, 6
    // Timer=8: Expect $00 (no underflow yet)
    // Timer=7: Expect $01 (underflow flag, no IRQ bit)  
    // Timer=6: Expect $81 (underflow flag + IRQ bit)
    
    for (int timer_val = 8; timer_val >= 6; timer_val--) {
        printf("\n--- Testing Timer Value: %d ---\n", timer_val);
        
        // Reset C64 state for each test
        c64_init(sys);
        
        // Set up timer A with the test value
        sys->cia1.timer_a_latch = timer_val;
        sys->cia1.timer_a = timer_val;
        sys->cia1.cra = 0x01; // Start timer
        sys->cia1.ta_delay = 3; // Pipeline delay
        
        // Enable Timer A interrupt in IMR
        sys->cia1.imr = 0x81;
        
        // Clear any pending interrupts
        sys->cia1.icr_data = 0;
        sys->cia1.irq_pending = false;
        
        printf("Initial state: Timer=%d, ICR=$%02X, IRQ_pending=%s\n", 
               sys->cia1.timer_a, 
               sys->cia1.icr_data,
               sys->cia1.irq_pending ? "true" : "false");
        
        // Run for enough cycles to see the behavior
        for (int cycle = 0; cycle < 20; cycle++) {
            cia_clock(&sys->cia1);
            printf("Cycle %d: Timer=%d, ICR=$%02X, IRQ_pending=%s\n", 
                   cycle, sys->cia1.timer_a, 
                   sys->cia1.icr_data | (sys->cia1.irq_pending ? 0x80 : 0),
                   sys->cia1.irq_pending ? "true" : "false");
            
            // Read ICR register (this is what the test does)
            u8 icr_read = cia_read_reg(&sys->cia1, 0x0D);
            if (icr_read != 0) {
                printf("  ICR read: $%02X\n", icr_read);
                break;
            }
        }
    }
    
    return 0;
}

// main function removed - use test_runner instead