#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mos6510.h"
#include "cia6526.h"

// Test context for CIA1
typedef struct {
    uint8_t keyboard_matrix[8][8];
    uint8_t joystick1_state;  // Joystick 1 on Port B (active low)
    uint8_t joystick2_state;  // Joystick 2 on Port A (active low)
    uint8_t port_a_output;
    uint8_t port_b_output;
} cia1_context_t;

// CIA1 port A callback (keyboard rows + joystick 2)
uint8_t cia1_port_a_read(void *context) {
    cia1_context_t *ctx = (cia1_context_t*)context;
    // Return joystick 2 state (bits 0-4) combined with port A output
    return (ctx->port_a_output & 0xE0) | (ctx->joystick2_state & 0x1F);
}

void cia1_port_a_write(void *context, uint8_t data) {
    cia1_context_t *ctx = (cia1_context_t*)context;
    ctx->port_a_output = data;
}

// CIA1 port B callback (keyboard columns + joystick 1)
uint8_t cia1_port_b_read(void *context) {
    cia1_context_t *ctx = (cia1_context_t*)context;
    // Scan keyboard matrix - check which rows are active (low) in port_a_output
    uint8_t result = 0xFF;  // All columns inactive by default (no keys pressed)
    
    for (int row = 0; row < 8; row++) {
        if ((ctx->port_a_output & (1 << row)) == 0) {  // Row active (low)
            // For each column, check if a key is set at keyboard_matrix[row][col]
            for (int col = 0; col < 8; col++) {
                // Non-zero value means key is pressed at this position
                // The value itself indicates which bit should be cleared
                if (ctx->keyboard_matrix[row][col] != 0) {
                    result &= ctx->keyboard_matrix[row][col];
                }
            }
        }
    }
    
    // Combine with joystick 1 state (bits 0-4), default to 0xFF if not set
    if (ctx->joystick1_state == 0) {
        ctx->joystick1_state = 0xFF;
    }
    result &= ctx->joystick1_state;
    return result;
}

void cia1_port_b_write(void *context, uint8_t data) {
    cia1_context_t *ctx = (cia1_context_t*)context;
    ctx->port_b_output = data;
}

// Test functions
void test_cia1_init() {
    printf("Testing CIA1 initialization...\n");
    
    CIA cia1;
    cia1_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia1, CIA1_PRA);
    cia_set_io_callbacks(&cia1, &ctx,
                      cia1_port_a_read, cia1_port_a_write,
                      cia1_port_b_read, cia1_port_b_write);
    
    // Check initial state
    assert(cia1.base_address == CIA1_PRA);
    assert(cia1.ddra == 0xFF);
    assert(cia1.ddrb == 0xFF);
    assert(cia1.tod_running == true);
    assert(cia1.tod_50hz == true);
    assert(!cia_get_irq(&cia1));
    
    printf("✓ CIA1 initialization test passed\n\n");
}

void test_cia1_register_access() {
    printf("Testing CIA1 register access...\n");
    
    CIA cia1;
    cia1_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia1, CIA1_PRA);
    cia_set_io_callbacks(&cia1, &ctx,
                      cia1_port_a_read, cia1_port_a_write,
                      cia1_port_b_read, cia1_port_b_write);
    
    // Test port registers
    cia_write(&cia1, CIA1_PRA, 0x55);
    assert(cia_read(&cia1, CIA1_PRA) == 0x55);
    
    cia_write(&cia1, CIA1_PRB, 0xAA);
    assert(cia_read(&cia1, CIA1_PRB) == 0xAA);
    
    cia_write(&cia1, CIA1_DDRA, 0x0F);
    assert(cia_read(&cia1, CIA1_DDRA) == 0x0F);
    
    cia_write(&cia1, CIA1_DDRB, 0xF0);
    assert(cia_read(&cia1, CIA1_DDRB) == 0xF0);
    
    // Test timer registers
    cia_write(&cia1, CIA1_TALO, 0x34);
    cia_write(&cia1, CIA1_TAHI, 0x12);
    assert(cia_read(&cia1, CIA1_TALO) == 0x34);
    assert(cia_read(&cia1, CIA1_TAHI) == 0x12);
    assert(cia1.timer_a_latch == 0x1234);
    
    cia_write(&cia1, CIA1_TBLO, 0x78);
    cia_write(&cia1, CIA1_TBHI, 0x56);
    assert(cia_read(&cia1, CIA1_TBLO) == 0x78);
    assert(cia_read(&cia1, CIA1_TBHI) == 0x56);
    assert(cia1.timer_b_latch == 0x5678);
    
    // Test control registers
    cia_write(&cia1, CIA1_CRA, 0x1F);
    assert(cia_read(&cia1, CIA1_CRA) == 0x1F);
    
    cia_write(&cia1, CIA1_CRB, 0x5F);
    assert(cia_read(&cia1, CIA1_CRB) == 0x5F);
    
    printf("✓ CIA1 register access test passed\n\n");
}

void test_cia1_timer_a() {
    printf("Testing CIA1 Timer A...\n");
    
    CIA cia1;
    cia1_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia1, CIA1_PRA);
    cia_set_io_callbacks(&cia1, &ctx,
                      cia1_port_a_read, cia1_port_a_write,
                      cia1_port_b_read, cia1_port_b_write);
    
    // Test timer loading
    cia_write(&cia1, CIA1_TALO, 0x00);
    cia_write(&cia1, CIA1_TAHI, 0x10);
    assert(cia1.timer_a_latch == 0x1000);
    assert(cia1.timer_a == 0x1000);
    
    // Enable Timer A interrupt (bit 7=1 for SET, bit 0 for TA)
    cia_write(&cia1, CIA1_ICR, CIA_ICR_SET | CIA_ICR_TA);
    assert((cia1.icr_mask & CIA_ICR_TA) != 0);
    
    // Test timer start
    cia_write(&cia1, CIA1_CRA, CIA_CR_START);
    assert(cia1.timer_a_running);
    assert((cia1.cra & CIA_CR_START) != 0);
    
    // Clock timer and check underflow (0x1001 clocks: 0x1000 decrements to 0, then 1 more to underflow)
    for (int i = 0; i < 0x1001; i++) {
        cia_clock(&cia1);
    }
    
    assert(cia1.timer_a_underflow);
    assert(cia_get_irq(&cia1));
    assert((cia1.icr_data & CIA_ICR_TA) != 0);
    
    // Read ICR to clear interrupt (reading clears icr_data)
    uint8_t icr_val = cia_read(&cia1, CIA1_ICR);
    assert((icr_val & CIA_ICR_TA) != 0);      // Timer A interrupt was set
    assert((icr_val & CIA_ICR_FLAG) != 0);    // IRQ flag was set
    assert(!cia_get_irq(&cia1));              // IRQ cleared after read
    assert(cia1.icr_data == 0);               // ICR data cleared after read
    
    // Test one-shot mode (default - bit 3 = 1 in RUNMODE)
    cia_write(&cia1, CIA1_TALO, 0x02);
    cia_write(&cia1, CIA1_TAHI, 0x00);
    cia_write(&cia1, CIA1_CRA, CIA_CR_START | CIA_CR_LOAD | CIA_CR_RUNMODE);
    assert(cia1.timer_a == 0x0002);
    cia_clock(&cia1);  // 2 -> 1
    cia_clock(&cia1);  // 1 -> 0
    cia_clock(&cia1);  // 0 -> underflow, reload to 2, stop timer
    assert(!cia1.timer_a_running);
    assert((cia1.cra & CIA_CR_START) == 0);
    
    printf("✓ CIA1 Timer A test passed\n\n");
}

void test_cia1_timer_b() {
    printf("Testing CIA1 Timer B...\n");
    
    CIA cia1;
    cia1_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia1, CIA1_PRA);
    cia_set_io_callbacks(&cia1, &ctx,
                      cia1_port_a_read, cia1_port_a_write,
                      cia1_port_b_read, cia1_port_b_write);
    
    // Test timer loading
    cia_write(&cia1, CIA1_TBLO, 0x00);
    cia_write(&cia1, CIA1_TBHI, 0x20);
    assert(cia1.timer_b_latch == 0x2000);
    assert(cia1.timer_b == 0x2000);
    
    // Enable Timer B interrupt (bit 7=1 for SET, bit 1 for TB)
    cia_write(&cia1, CIA1_ICR, CIA_ICR_SET | CIA_ICR_TB);
    assert((cia1.icr_mask & CIA_ICR_TB) != 0);
    
    // Test timer start
    cia_write(&cia1, CIA1_CRB, CIA_CRB_START);
    assert(cia1.timer_b_running);
    assert((cia1.crb & CIA_CRB_START) != 0);
    
    // Clock timer and check underflow (0x2001 clocks needed)
    for (int i = 0; i < 0x2001; i++) {
        cia_clock(&cia1);
    }
    
    assert(cia1.timer_b_underflow);
    assert(cia_get_irq(&cia1));
    assert((cia1.icr_data & CIA_ICR_TB) != 0);
    
    // Read ICR to clear interrupt
    uint8_t icr_val = cia_read(&cia1, CIA1_ICR);
    assert((icr_val & CIA_ICR_TB) != 0);
    assert(!cia_get_irq(&cia1));
    
    printf("✓ CIA1 Timer B test passed\n\n");
}

void test_cia1_tod() {
    printf("Testing CIA1 Time of Day...\n");
    
    CIA cia1;
    cia1_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia1, CIA1_PRA);
    cia_set_io_callbacks(&cia1, &ctx,
                      cia1_port_a_read, cia1_port_a_write,
                      cia1_port_b_read, cia1_port_b_write);
    
    // Set TOD
    cia_tod_set(&cia1, 12, 34, 56, 7);
    assert(cia1.tod_hr == 12);
    assert(cia1.tod_min == 34);
    assert(cia1.tod_sec == 56);
    assert(cia1.tod_10th == 7);
    
    // Test TOD registers
    assert(cia_read(&cia1, CIA1_TOD10TH) == 7);
    assert(cia_read(&cia1, CIA1_TODSEC) == 56);
    assert(cia_read(&cia1, CIA1_TODMIN) == 34);
    assert(cia_read(&cia1, CIA1_TODHR) == 12);
    
    // Test alarm setting
    cia_tod_set_alarm(&cia1, 13, 35, 57, 8);
    assert(cia1.tod_alarm_hr == 13);
    assert(cia1.tod_alarm_min == 35);
    assert(cia1.tod_alarm_sec == 57);
    assert(cia1.tod_alarm_10th == 8);
    
    // Clock TOD for a few cycles
    cia_clock_phi2(&cia1); // 10ths increment
    assert(cia1.tod_10th == 8);
    
    // Enable TOD interrupt
    cia_write(&cia1, CIA1_ICR, CIA_ICR_SET | CIA_ICR_TOD);
    
    // Simulate reaching alarm
    cia1.tod_hr = 13;
    cia1.tod_min = 35;
    cia1.tod_sec = 57;
    cia1.tod_10th = 8;
    cia_clock_phi2(&cia1);
    
    assert(cia_get_irq(&cia1));
    assert((cia1.icr_data & CIA_ICR_TOD) != 0);
    
    // Read ICR to clear TOD interrupt
    cia_read(&cia1, CIA1_ICR);
    assert(cia1.icr_data == 0);
    
    printf("✓ CIA1 Time of Day test passed\n\n");
}

void test_cia1_interrupts() {
    printf("Testing CIA1 interrupts...\n");
    
    CIA cia1;
    cia1_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia1, CIA1_PRA);
    cia_set_io_callbacks(&cia1, &ctx,
                      cia1_port_a_read, cia1_port_a_write,
                      cia1_port_b_read, cia1_port_b_write);
    
    // Test interrupt enable/disable using icr_mask
    cia_write(&cia1, CIA1_ICR, CIA_ICR_SET | CIA_ICR_TA | CIA_ICR_TB | CIA_ICR_TOD | CIA_ICR_SDR);
    assert(cia1.icr_mask == (CIA_ICR_TA | CIA_ICR_TB | CIA_ICR_TOD | CIA_ICR_SDR));
    
    // Clear all interrupt enables
    cia_write(&cia1, CIA1_ICR, CIA_ICR_TA | CIA_ICR_TB | CIA_ICR_TOD | CIA_ICR_SDR);  // No SET bit = clear
    assert(cia1.icr_mask == 0);
    
    // Enable specific interrupts
    cia_write(&cia1, CIA1_ICR, CIA_ICR_SET | CIA_ICR_TA | CIA_ICR_TB);
    assert(cia1.icr_mask == (CIA_ICR_TA | CIA_ICR_TB));
    
    // Generate timer A interrupt
    cia_write(&cia1, CIA1_TALO, 0x02);
    cia_write(&cia1, CIA1_TAHI, 0x00);
    cia_write(&cia1, CIA1_CRA, CIA_CR_START | CIA_CR_LOAD);
    cia_clock(&cia1);  // 2 -> 1
    cia_clock(&cia1);  // 1 -> 0
    cia_clock(&cia1);  // 0 -> underflow
    
    assert(cia_get_irq(&cia1));
    uint8_t icr_val = cia_read(&cia1, CIA1_ICR);
    assert((icr_val & CIA_ICR_FLAG) != 0);
    assert((icr_val & CIA_ICR_TA) != 0);
    
    printf("✓ CIA1 interrupts test passed\n\n");
}

void test_cia1_keyboard() {
    printf("Testing CIA1 keyboard matrix...\n");
    
    CIA cia1;
    cia1_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    // Set up keyboard matrix - simulate 'A' key pressed (row 1, column 1)
    ctx.keyboard_matrix[1][1] = 0xFD; // Bit 1 cleared = key pressed
    
    cia_init(&cia1, CIA1_PRA);
    cia_set_io_callbacks(&cia1, &ctx,
                      cia1_port_a_read, cia1_port_a_write,
                      cia1_port_b_read, cia1_port_b_write);
    
    // Configure Port A as output (rows), Port B as input (columns)
    cia_write(&cia1, CIA1_DDRA, 0xFF);  // Port A all outputs (row select)
    cia_write(&cia1, CIA1_DDRB, 0x00);  // Port B all inputs (column read)
    
    // Activate row 1 (active low)
    cia_write(&cia1, CIA1_PRA, 0xFD); // Clear bit 1
    ctx.port_a_output = 0xFD;
    
    // Read port B to scan keyboard
    uint8_t result = cia_read(&cia1, CIA1_PRB);
    assert((result & 0x02) == 0); // Column 1 should show key pressed
    
    // Test multiple keys
    ctx.keyboard_matrix[1][2] = 0xFB; // Another key in same row, bit 2 cleared
    cia_write(&cia1, CIA1_PRA, 0xFC);
    ctx.port_a_output = 0xFC;
    
    result = cia_read(&cia1, CIA1_PRB);
    assert((result & 0x02) == 0); // Column 1 pressed
    assert((result & 0x04) == 0); // Column 2 pressed
    
    printf("✓ CIA1 keyboard matrix test passed\n\n");
}

void test_cia1_joystick() {
    printf("Testing CIA1 joystick reading...\n");
    
    CIA cia1;
    cia1_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.joystick1_state = 0xFF;  // All released initially
    ctx.joystick2_state = 0xFF;  // All released initially
    
    cia_init(&cia1, CIA1_PRA);
    cia_set_io_callbacks(&cia1, &ctx,
                      cia1_port_a_read, cia1_port_a_write,
                      cia1_port_b_read, cia1_port_b_write);
    
    // Set DDR for port B as input for joystick reading
    cia_write(&cia1, CIA1_DDRB, 0x00);  // All inputs
    
    // Set joystick 1 state (up + fire pressed)
    // Active low: 0 = pressed, 1 = released
    // Bit 0 = Up, Bit 1 = Down, Bit 2 = Left, Bit 3 = Right, Bit 4 = Fire
    ctx.joystick1_state = 0xEE;  // 11101110 = fire pressed (bit 4=0), up pressed (bit 0=0)
    
    uint8_t joy1 = cia1_joystick_read(&cia1, 1);
    assert((joy1 & 0x01) == 0);  // Up pressed (bit 0 = 0)
    assert((joy1 & 0x10) == 0);  // Fire pressed (bit 4 = 0)
    assert((joy1 & 0x02) != 0);  // Down not pressed
    assert((joy1 & 0x04) != 0);  // Left not pressed
    assert((joy1 & 0x08) != 0);  // Right not pressed
    
    // Set DDR for port A as input for joystick reading
    cia_write(&cia1, CIA1_DDRA, 0x00);  // All inputs
    
    // Test joystick 2 (right + fire pressed)
    ctx.joystick2_state = 0xE7;  // 11100111 = fire pressed (bit 4=0), right pressed (bit 3=0)
    
    uint8_t joy2 = cia1_joystick_read(&cia1, 2);
    assert((joy2 & 0x08) == 0);  // Right pressed (bit 3 = 0)
    assert((joy2 & 0x10) == 0);  // Fire pressed (bit 4 = 0)
    assert((joy2 & 0x01) != 0);  // Up not pressed
    assert((joy2 & 0x02) != 0);  // Down not pressed
    assert((joy2 & 0x04) != 0);  // Left not pressed
    
    printf("✓ CIA1 joystick reading test passed\n\n");
}

void test_cia1_serial_port() {
    printf("Testing CIA1 serial port...\n");
    
    CIA cia1;
    cia1_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    cia_init(&cia1, CIA1_PRA);
    cia_set_io_callbacks(&cia1, &ctx,
                      cia1_port_a_read, cia1_port_a_write,
                      cia1_port_b_read, cia1_port_b_write);
    
    // Test serial data register
    cia_write(&cia1, CIA1_SDR, 0x55);
    assert(cia1.sdr == 0x55);
    assert(cia_read(&cia1, CIA1_SDR) == 0x55);
    
    // Enable serial output mode and serial interrupt
    cia_write(&cia1, CIA1_CRA, CIA_CR_SPMODE);  // Set serial port output mode
    cia_write(&cia1, CIA1_ICR, CIA_ICR_SET | CIA_ICR_SDR);
    
    // Write to SDR to start transmission
    cia_write(&cia1, CIA1_SDR, 0xAA);
    
    // Enable serial output and clock 8 bits
    cia1.serial_output_enabled = true;
    for (int i = 0; i < 8; i++) {
        cia_clock_serial(&cia1);
    }
    
    assert(cia_get_irq(&cia1));
    assert((cia1.icr_data & CIA_ICR_SDR) != 0);
    
    // Clear by reading ICR
    cia_read(&cia1, CIA1_ICR);
    assert(cia1.icr_data == 0);
    
    printf("✓ CIA1 serial port test passed\n\n");
}

int main() {
    printf("=== CIA1 Comprehensive Test Suite ===\n\n");
    
    test_cia1_init();
    test_cia1_register_access();
    test_cia1_timer_a();
    test_cia1_timer_b();
    test_cia1_tod();
    test_cia1_interrupts();
    test_cia1_keyboard();
    test_cia1_joystick();
    test_cia1_serial_port();
    
    printf("=== All CIA1 Tests Passed! ===\n");
    return 0;
}