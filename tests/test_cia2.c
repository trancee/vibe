#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mos6510.h"
#include "cia6526.h"

// Test context for CIA2
typedef struct {
    uint8_t serial_bus_data;
    bool serial_clock_enabled;
    bool serial_data_enabled;
    bool cassette_motor_on;
    bool cassette_write_enabled;
    uint8_t rs232_tx_data;
    uint8_t port_a_output;
    uint8_t port_b_output;
    uint8_t vic_bank;
} cia2_context_t;

// CIA2 port A callback (VIC bank control + serial bus)
uint8_t cia2_port_a_read(void *context) {
    cia2_context_t *ctx = (cia2_context_t*)context;
    return ctx->port_a_output;
}

void cia2_port_a_write(void *context, uint8_t data) {
    cia2_context_t *ctx = (cia2_context_t*)context;
    ctx->port_a_output = data;
    
    // Update VIC bank
    ctx->vic_bank = (~data & 0x03); // Bits 0-1 are active low
}

// CIA2 port B callback (serial bus)
uint8_t cia2_port_b_read(void *context) {
    cia2_context_t *ctx = (cia2_context_t*)context;
    uint8_t result = 0xFF;
    
    // Simulate serial bus state
    if (ctx->serial_clock_enabled) {
        result &= ~0x40; // Clear clock bit
    }
    if (ctx->serial_data_enabled) {
        result &= ~0x80; // Clear data bit
    }
    
    return result;
}

void cia2_port_b_write(void *context, uint8_t data) {
    cia2_context_t *ctx = (cia2_context_t*)context;
    ctx->port_b_output = data;
}

// Test functions
void test_cia2_init() {
    printf("Testing CIA2 initialization...\n");
    
    CIA cia2;
    cia2_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia2, CIA2_PRA);
    cia_set_io_callbacks(&cia2, &ctx,
                      cia2_port_a_read, cia2_port_a_write,
                      cia2_port_b_read, cia2_port_b_write);
    
    // Check initial state
    assert(cia2.base_address == CIA2_PRA);
    assert(cia2.ddra == 0xFF);
    assert(cia2.ddrb == 0xFF);
    assert(cia2.tod_running == true);
    assert(cia2.tod_50hz == true);
    assert(!cia_get_irq(&cia2));
    
    printf("✓ CIA2 initialization test passed\n\n");
}

void test_cia2_register_access() {
    printf("Testing CIA2 register access...\n");
    
    CIA cia2;
    cia2_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia2, CIA2_PRA);
    cia_set_io_callbacks(&cia2, &ctx,
                      cia2_port_a_read, cia2_port_a_write,
                      cia2_port_b_read, cia2_port_b_write);
    
    // Test port registers
    cia_write(&cia2, CIA2_PRA, 0x33);
    assert(cia_read(&cia2, CIA2_PRA) == 0x33);
    
    cia_write(&cia2, CIA2_PRB, 0xCC);
    assert(cia_read(&cia2, CIA2_PRB) == 0xCC);
    
    cia_write(&cia2, CIA2_DDRA, 0x55);
    assert(cia_read(&cia2, CIA2_DDRA) == 0x55);
    
    cia_write(&cia2, CIA2_DDRB, 0xAA);
    assert(cia_read(&cia2, CIA2_DDRB) == 0xAA);
    
    // Test timer registers
    cia_write(&cia2, CIA2_TALO, 0x66);
    cia_write(&cia2, CIA2_TAHI, 0x77);
    assert(cia_read(&cia2, CIA2_TALO) == 0x66);
    assert(cia_read(&cia2, CIA2_TAHI) == 0x77);
    assert(cia2.timer_a_latch == 0x7766);
    
    cia_write(&cia2, CIA2_TBLO, 0x88);
    cia_write(&cia2, CIA2_TBHI, 0x99);
    assert(cia_read(&cia2, CIA2_TBLO) == 0x88);
    assert(cia_read(&cia2, CIA2_TBHI) == 0x99);
    assert(cia2.timer_b_latch == 0x9988);
    
    // Test control registers
    cia_write(&cia2, CIA2_CRA, 0x3F);
    assert(cia_read(&cia2, CIA2_CRA) == 0x3F);
    
    cia_write(&cia2, CIA2_CRB, 0x7F);
    assert(cia_read(&cia2, CIA2_CRB) == 0x7F);
    
    printf("✓ CIA2 register access test passed\n\n");
}

void test_cia2_vic_bank_control() {
    printf("Testing CIA2 VIC bank control...\n");
    
    CIA cia2;
    cia2_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia2, CIA2_PRA);
    cia_set_io_callbacks(&cia2, &ctx,
                      cia2_port_a_read, cia2_port_a_write,
                      cia2_port_b_read, cia2_port_b_write);
    
    // Test all VIC bank settings
    for (int bank = 0; bank < 4; bank++) {
        cia2_set_vic_bank(&cia2, bank);
        uint8_t read_bank = cia2_get_vic_bank(&cia2);
        
        assert(read_bank == bank);
        
        // Verify port A reflects the bank setting (active low)
        uint8_t expected_port_a = (~bank) & 0x03;
        assert(ctx.port_a_output == expected_port_a);
    }
    
    // Test individual bank settings
    cia_write(&cia2, CIA2_PRA, 0xFF); // Set bank 0 (bits 0-1 = 11)
    assert(cia2_get_vic_bank(&cia2) == 0);
    
    cia_write(&cia2, CIA2_PRA, 0xFE); // Set bank 1 (bits 0-1 = 10)
    assert(cia2_get_vic_bank(&cia2) == 1);
    
    cia_write(&cia2, CIA2_PRA, 0xFD); // Set bank 2 (bits 0-1 = 01)
    assert(cia2_get_vic_bank(&cia2) == 2);
    
    cia_write(&cia2, CIA2_PRA, 0xFC); // Set bank 3 (bits 0-1 = 00)
    assert(cia2_get_vic_bank(&cia2) == 3);
    
    printf("✓ CIA2 VIC bank control test passed\n\n");
}

void test_cia2_serial_bus() {
    printf("Testing CIA2 serial bus control...\n");
    
    CIA cia2;
    cia2_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia2, CIA2_PRA);
    cia_set_io_callbacks(&cia2, &ctx,
                      cia2_port_a_read, cia2_port_a_write,
                      cia2_port_b_read, cia2_port_b_write);
    
    // Test serial clock control (bit 6 of port A)
    cia_write(&cia2, CIA2_PRA, 0xBF); // Clear bit 6
    assert(ctx.port_a_output == 0xBF);
    assert((ctx.port_a_output & 0x40) == 0); // Clock off
    
    cia_write(&cia2, CIA2_PRA, 0xFF); // Set bit 6
    assert(ctx.port_a_output == 0xFF);
    assert((ctx.port_a_output & 0x40) != 0); // Clock on
    
    // Test serial data control (bit 7 of port A)
    cia_write(&cia2, CIA2_PRA, 0x7F); // Clear bit 7
    assert(ctx.port_a_output == 0x7F);
    assert((ctx.port_a_output & 0x80) == 0); // Data off
    
    cia_write(&cia2, CIA2_PRA, 0xFF); // Set bit 7
    assert(ctx.port_a_output == 0xFF);
    assert((ctx.port_a_output & 0x80) != 0); // Data on
    
    // Test cassette control (bit 3 of port A)
    cia_write(&cia2, CIA2_PRA, 0xF7); // Clear bit 3
    assert(ctx.port_a_output == 0xF7);
    assert((ctx.port_a_output & 0x08) == 0); // Motor off
    
    cia_write(&cia2, CIA2_PRA, 0xFF); // Set bit 3
    assert(ctx.port_a_output == 0xFF);
    assert((ctx.port_a_output & 0x08) != 0); // Motor on
    
    // Test cassette write line (bit 4 of port A)
    cia_write(&cia2, CIA2_PRA, 0xEF); // Clear bit 4
    assert(ctx.port_a_output == 0xEF);
    assert((ctx.port_a_output & 0x10) == 0); // Write off
    
    cia_write(&cia2, CIA2_PRA, 0xFF); // Set bit 4
    assert(ctx.port_a_output == 0xFF);
    assert((ctx.port_a_output & 0x10) != 0); // Write on
    
    // Test RS-232 TXD (bit 5 of port A)
    cia_write(&cia2, CIA2_PRA, 0xDF); // Clear bit 5
    assert(ctx.port_a_output == 0xDF);
    assert((ctx.port_a_output & 0x20) == 0); // TXD off
    
    cia_write(&cia2, CIA2_PRA, 0xFF); // Set bit 5
    assert(ctx.port_a_output == 0xFF);
    assert((ctx.port_a_output & 0x20) != 0); // TXD on
    
    // Test port B serial bus reading
    cia_write(&cia2, CIA2_DDRB, 0x00); // Set port B as inputs
    ctx.serial_clock_enabled = true;
    ctx.serial_data_enabled = false;
    
    uint8_t port_b = cia_read(&cia2, CIA2_PRB);
    assert((port_b & 0x40) == 0); // Clock should be low
    assert((port_b & 0x80) != 0); // Data should be high
    
    ctx.serial_clock_enabled = false;
    ctx.serial_data_enabled = true;
    
    port_b = cia_read(&cia2, CIA2_PRB);
    assert((port_b & 0x40) != 0); // Clock should be high
    assert((port_b & 0x80) == 0); // Data should be low
    
    printf("✓ CIA2 serial bus control test passed\n\n");
}

void test_cia2_timer_a() {
    printf("Testing CIA2 Timer A...\n");
    
    CIA cia2;
    cia2_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia2, CIA2_PRA);
    cia_set_io_callbacks(&cia2, &ctx,
                      cia2_port_a_read, cia2_port_a_write,
                      cia2_port_b_read, cia2_port_b_write);
    
    // Test timer loading
    cia_write(&cia2, CIA2_TALO, 0x11);
    cia_write(&cia2, CIA2_TAHI, 0x22);
    assert(cia2.timer_a_latch == 0x2211);
    assert(cia2.timer_a == 0x2211);
    
    // Enable Timer A interrupt
    cia_write(&cia2, CIA2_ICR, CIA_ICR_SET | CIA_ICR_TA);
    
    // Test timer start
    cia_write(&cia2, CIA2_CRA, CIA_CR_START);
    assert(cia2.timer_a_running);
    assert((cia2.cra & CIA_CR_START) != 0);
    
    // Clock timer and check underflow (need 0x2212 clocks)
    for (int i = 0; i < 0x2212; i++) {
        cia_clock(&cia2);
    }
    
    assert(cia2.timer_a_underflow);
    assert(cia_get_irq(&cia2));
    assert((cia2.icr_data & CIA_ICR_TA) != 0);
    
    // Read ICR to clear interrupt
    cia_read(&cia2, CIA2_ICR);
    assert(!cia_get_irq(&cia2));
    
    // Test continuous mode (no RUNMODE bit = continuous)
    cia_write(&cia2, CIA2_ICR, CIA_ICR_SET | CIA_ICR_TA);  // Re-enable Timer A interrupt
    cia_write(&cia2, CIA2_TALO, 0x02);
    cia_write(&cia2, CIA2_TAHI, 0x00);
    cia_write(&cia2, CIA2_CRA, CIA_CR_START | CIA_CR_LOAD);  // Start in continuous mode
    
    for (int i = 0; i < 3; i++) {  // Clock until first underflow
        cia_clock(&cia2);
    }
    
    assert(cia2.timer_a_running); // Should still be running in continuous mode
    assert(cia_get_irq(&cia2));
    
    printf("✓ CIA2 Timer A test passed\n\n");
}

void test_cia2_timer_b() {
    printf("Testing CIA2 Timer B...\n");
    
    CIA cia2;
    cia2_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia2, CIA2_PRA);
    cia_set_io_callbacks(&cia2, &ctx,
                      cia2_port_a_read, cia2_port_a_write,
                      cia2_port_b_read, cia2_port_b_write);
    
    // Test timer loading
    cia_write(&cia2, CIA2_TBLO, 0x05);
    cia_write(&cia2, CIA2_TBHI, 0x00);
    assert(cia2.timer_b_latch == 0x0005);
    assert(cia2.timer_b == 0x0005);
    
    // Enable Timer B interrupt
    cia_write(&cia2, CIA2_ICR, CIA_ICR_SET | CIA_ICR_TB);
    
    // Test timer start
    cia_write(&cia2, CIA2_CRB, CIA_CRB_START);
    assert(cia2.timer_b_running);
    assert((cia2.crb & CIA_CRB_START) != 0);
    
    // Clock timer and check underflow (need 6 clocks)
    for (int i = 0; i < 6; i++) {
        cia_clock(&cia2);
    }
    
    assert(cia2.timer_b_underflow);
    assert(cia_get_irq(&cia2));
    assert((cia2.icr_data & CIA_ICR_TB) != 0);
    
    // Read ICR to clear interrupt
    cia_read(&cia2, CIA2_ICR);
    assert(!cia_get_irq(&cia2));
    
    printf("✓ CIA2 Timer B test passed\n\n");
}

void test_cia2_tod() {
    printf("Testing CIA2 Time of Day...\n");
    
    CIA cia2;
    cia2_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia2, CIA2_PRA);
    cia_set_io_callbacks(&cia2, &ctx,
                      cia2_port_a_read, cia2_port_a_write,
                      cia2_port_b_read, cia2_port_b_write);
    
    // Test 50/60 Hz toggle (TODIN bit 7 of CRA)
    // TODIN = 1 means 50 Hz, TODIN = 0 means 60 Hz
    cia_write(&cia2, CIA2_CRA, CIA_CR_TODIN); // Set to 50 Hz
    assert(cia2.tod_50hz);
    
    cia_write(&cia2, CIA2_CRA, 0); // Set to 60 Hz
    assert(!cia2.tod_50hz);
    
    // Enable TOD interrupt
    cia_write(&cia2, CIA2_ICR, CIA_ICR_SET | CIA_ICR_TOD);
    
    // Test TOD alarm functionality
    cia_tod_set(&cia2, 23, 45, 12, 5);
    cia_tod_set_alarm(&cia2, 23, 45, 12, 5);
    
    // Clock to alarm time - alarm is checked BEFORE incrementing
    // So the clock will match alarm, trigger interrupt, then increment 10ths to 6
    cia_clock_phi2(&cia2);
    assert(cia_get_irq(&cia2));
    assert((cia2.icr_data & CIA_ICR_TOD) != 0);
    
    // Test TOD register reading (time was incremented after alarm match)
    assert(cia_read(&cia2, CIA2_TOD10TH) == 6);  // Was 5, now 6 after increment
    assert(cia_read(&cia2, CIA2_TODSEC) == 12);
    assert(cia_read(&cia2, CIA2_TODMIN) == 45);
    assert(cia_read(&cia2, CIA2_TODHR) == 23);
    
    printf("✓ CIA2 Time of Day test passed\n\n");
}

void test_cia2_interrupts() {
    printf("Testing CIA2 interrupts...\n");
    
    CIA cia2;
    cia2_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia2, CIA2_PRA);
    cia_set_io_callbacks(&cia2, &ctx,
                      cia2_port_a_read, cia2_port_a_write,
                      cia2_port_b_read, cia2_port_b_write);
    
    // Test interrupt control register (icr_mask)
    cia_write(&cia2, CIA2_ICR, CIA_ICR_SET | CIA_ICR_TA | CIA_ICR_TB);
    assert(cia2.icr_mask == (CIA_ICR_TA | CIA_ICR_TB));
    
    // Test interrupt flag behavior - no interrupt yet
    assert(!cia_get_irq(&cia2));
    
    // Generate Timer A interrupt
    cia_write(&cia2, CIA2_TALO, 0x02);
    cia_write(&cia2, CIA2_TAHI, 0x00);
    cia_write(&cia2, CIA2_CRA, CIA_CR_START | CIA_CR_LOAD);
    cia_clock(&cia2);  // 2 -> 1
    cia_clock(&cia2);  // 1 -> 0
    cia_clock(&cia2);  // 0 -> underflow
    
    assert(cia_get_irq(&cia2));
    uint8_t icr_val = cia_read(&cia2, CIA2_ICR);
    assert((icr_val & CIA_ICR_FLAG) != 0);
    assert((icr_val & CIA_ICR_TA) != 0);
    
    // After reading ICR, interrupt is cleared
    assert(!cia_get_irq(&cia2));
    
    printf("✓ CIA2 interrupts test passed\n\n");
}

void test_cia2_serial_data_register() {
    printf("Testing CIA2 Serial Data Register...\n");
    
    CIA cia2;
    cia2_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia2, CIA2_PRA);
    cia_set_io_callbacks(&cia2, &ctx,
                      cia2_port_a_read, cia2_port_a_write,
                      cia2_port_b_read, cia2_port_b_write);
    
    // Test SDR access
    cia_write(&cia2, CIA2_SDR, 0x42);
    assert(cia2.sdr == 0x42);
    assert(cia_read(&cia2, CIA2_SDR) == 0x42);
    
    // Enable serial port output mode and SDR interrupt
    cia_write(&cia2, CIA2_CRA, CIA_CR_SPMODE);
    cia_write(&cia2, CIA2_ICR, CIA_ICR_SET | CIA_ICR_SDR);
    cia_write(&cia2, CIA2_SDR, 0x99);
    
    // Enable serial output and clock 8 bits
    cia2.serial_output_enabled = true;
    for (int i = 0; i < 8; i++) {
        cia_clock_serial(&cia2);
    }
    
    assert(cia_get_irq(&cia2));
    assert((cia2.icr_data & CIA_ICR_SDR) != 0);
    
    // Read ICR to clear interrupt
    cia_read(&cia2, CIA2_ICR);
    assert(cia2.icr_data == 0);
    
    printf("✓ CIA2 Serial Data Register test passed\n\n");
}

void test_cia2_port_data_direction() {
    printf("Testing CIA2 port data direction registers...\n");
    
    CIA cia2;
    cia2_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia2, CIA2_PRA);
    cia_set_io_callbacks(&cia2, &ctx,
                      cia2_port_a_read, cia2_port_a_write,
                      cia2_port_b_read, cia2_port_b_write);
    
    // Test data direction register A
    // DDRA bit = 1 means output, DDRA bit = 0 means input
    cia_write(&cia2, CIA2_DDRA, 0x55);  // Bits 0,2,4,6 are output
    assert(cia2.ddra == 0x55);
    
    // Writing to PRA - only output bits (where DDRA=1) are written to port
    cia_write(&cia2, CIA2_PRA, 0xFF);
    assert(ctx.port_a_output == 0x55);  // 0xFF & 0x55 = 0x55
    
    // Test data direction register B
    cia_write(&cia2, CIA2_DDRB, 0xAA);  // Bits 1,3,5,7 are output
    assert(cia2.ddrb == 0xAA);
    
    cia_write(&cia2, CIA2_PRB, 0xFF);
    assert(ctx.port_b_output == 0xAA);  // 0xFF & 0xAA = 0xAA
    
    // Test all inputs (DDRA = 0x00)
    cia_write(&cia2, CIA2_DDRA, 0x00);
    cia_write(&cia2, CIA2_PRA, 0xFF);
    assert(ctx.port_a_output == 0x00);  // 0xFF & 0x00 = 0x00
    
    // Test all outputs (DDRA = 0xFF)
    cia_write(&cia2, CIA2_DDRA, 0xFF);
    cia_write(&cia2, CIA2_PRA, 0x55);
    assert(ctx.port_a_output == 0x55);  // 0x55 & 0xFF = 0x55
    
    printf("✓ CIA2 port data direction test passed\n\n");
}

int main() {
    printf("=== CIA2 Comprehensive Test Suite ===\n\n");
    
    test_cia2_init();
    test_cia2_register_access();
    test_cia2_vic_bank_control();
    test_cia2_serial_bus();
    test_cia2_timer_a();
    test_cia2_timer_b();
    test_cia2_tod();
    test_cia2_interrupts();
    test_cia2_serial_data_register();
    test_cia2_port_data_direction();
    
    printf("=== All CIA2 Tests Passed! ===\n");
    return 0;
}