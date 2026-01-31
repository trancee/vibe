/**
 * test_cpu.c - CPU (6510) comprehensive tests
 * 
 * Tests all legal opcodes, addressing modes, flags, interrupts,
 * and CPU port behavior as documented.
 */

#include "test_framework.h"
#include "../src/c64.h"

static C64System sys;

// Helper to setup a minimal test environment
static void setup(void) {
    c64_init(&sys);
    mem_reset(&sys.mem);
    cpu_reset(&sys.cpu);
    // Set PC to a known location in RAM for testing
    sys.cpu.PC = 0x0800;
    // Disable interrupts by default
    sys.cpu.P |= FLAG_I;
}

// Helper to load code at current PC
static void load_code(const u8 *code, size_t len) {
    for (size_t i = 0; i < len; i++) {
        mem_write_raw(&sys.mem, sys.cpu.PC + i, code[i]);
    }
}

// Execute N instructions
static void exec_n(int n) {
    for (int i = 0; i < n; i++) {
        cpu_step(&sys.cpu);
    }
}

// ============================================================================
// LDA Tests
// ============================================================================

TEST(lda_immediate) {
    setup();
    u8 code[] = {0xA9, 0x42};  // LDA #$42
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x42, sys.cpu.A);
    ASSERT_EQ(2, cycles);
    ASSERT_FALSE(sys.cpu.P & FLAG_Z);
    ASSERT_FALSE(sys.cpu.P & FLAG_N);
    PASS();
}

TEST(lda_zero_flag) {
    setup();
    u8 code[] = {0xA9, 0x00};  // LDA #$00
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x00, sys.cpu.A);
    ASSERT_TRUE(sys.cpu.P & FLAG_Z);
    ASSERT_FALSE(sys.cpu.P & FLAG_N);
    PASS();
}

TEST(lda_negative_flag) {
    setup();
    u8 code[] = {0xA9, 0x80};  // LDA #$80
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x80, sys.cpu.A);
    ASSERT_FALSE(sys.cpu.P & FLAG_Z);
    ASSERT_TRUE(sys.cpu.P & FLAG_N);
    PASS();
}

TEST(lda_zeropage) {
    setup();
    mem_write_raw(&sys.mem, 0x42, 0x77);
    u8 code[] = {0xA5, 0x42};  // LDA $42
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x77, sys.cpu.A);
    ASSERT_EQ(3, cycles);
    PASS();
}

TEST(lda_zeropage_x) {
    setup();
    sys.cpu.X = 0x05;
    mem_write_raw(&sys.mem, 0x47, 0x88);
    u8 code[] = {0xB5, 0x42};  // LDA $42,X
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x88, sys.cpu.A);
    ASSERT_EQ(4, cycles);
    PASS();
}

TEST(lda_zeropage_x_wrap) {
    setup();
    sys.cpu.X = 0xFF;
    mem_write_raw(&sys.mem, 0x41, 0x99);  // $42 + $FF wraps to $41
    u8 code[] = {0xB5, 0x42};  // LDA $42,X
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x99, sys.cpu.A);
    PASS();
}

TEST(lda_absolute) {
    setup();
    mem_write_raw(&sys.mem, 0x1234, 0xAB);
    u8 code[] = {0xAD, 0x34, 0x12};  // LDA $1234
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(0xAB, sys.cpu.A);
    ASSERT_EQ(4, cycles);
    PASS();
}

TEST(lda_absolute_x_no_page_cross) {
    setup();
    sys.cpu.X = 0x05;
    mem_write_raw(&sys.mem, 0x1239, 0xCD);
    u8 code[] = {0xBD, 0x34, 0x12};  // LDA $1234,X
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(0xCD, sys.cpu.A);
    ASSERT_EQ(4, cycles);  // No page cross
    PASS();
}

TEST(lda_absolute_x_page_cross) {
    setup();
    sys.cpu.X = 0xFF;
    mem_write_raw(&sys.mem, 0x1333, 0xEF);  // $1234 + $FF = $1333, crosses page
    u8 code[] = {0xBD, 0x34, 0x12};  // LDA $1234,X
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(0xEF, sys.cpu.A);
    ASSERT_EQ(5, cycles);  // +1 for page cross
    PASS();
}

TEST(lda_absolute_y_page_cross) {
    setup();
    sys.cpu.Y = 0xFF;
    mem_write_raw(&sys.mem, 0x1333, 0x11);
    u8 code[] = {0xB9, 0x34, 0x12};  // LDA $1234,Y
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x11, sys.cpu.A);
    ASSERT_EQ(5, cycles);  // +1 for page cross
    PASS();
}

TEST(lda_indirect_x) {
    setup();
    sys.cpu.X = 0x04;
    // Pointer at $44 (= $40 + $04) points to $1234
    mem_write_raw(&sys.mem, 0x44, 0x34);
    mem_write_raw(&sys.mem, 0x45, 0x12);
    mem_write_raw(&sys.mem, 0x1234, 0x22);
    u8 code[] = {0xA1, 0x40};  // LDA ($40,X)
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x22, sys.cpu.A);
    ASSERT_EQ(6, cycles);
    PASS();
}

TEST(lda_indirect_y_no_page_cross) {
    setup();
    sys.cpu.Y = 0x10;
    // Pointer at $40 points to $1200
    mem_write_raw(&sys.mem, 0x40, 0x00);
    mem_write_raw(&sys.mem, 0x41, 0x12);
    mem_write_raw(&sys.mem, 0x1210, 0x33);  // $1200 + $10
    u8 code[] = {0xB1, 0x40};  // LDA ($40),Y
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x33, sys.cpu.A);
    ASSERT_EQ(5, cycles);
    PASS();
}

TEST(lda_indirect_y_page_cross) {
    setup();
    sys.cpu.Y = 0xFF;
    // Pointer at $40 points to $1234
    mem_write_raw(&sys.mem, 0x40, 0x34);
    mem_write_raw(&sys.mem, 0x41, 0x12);
    mem_write_raw(&sys.mem, 0x1333, 0x44);  // $1234 + $FF crosses page
    u8 code[] = {0xB1, 0x40};  // LDA ($40),Y
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x44, sys.cpu.A);
    ASSERT_EQ(6, cycles);  // +1 for page cross
    PASS();
}

// ============================================================================
// LDX/LDY Tests
// ============================================================================

TEST(ldx_immediate) {
    setup();
    u8 code[] = {0xA2, 0x55};  // LDX #$55
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x55, sys.cpu.X);
    PASS();
}

TEST(ldy_immediate) {
    setup();
    u8 code[] = {0xA0, 0x66};  // LDY #$66
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x66, sys.cpu.Y);
    PASS();
}

TEST(ldx_absolute_y_page_cross) {
    setup();
    sys.cpu.Y = 0xFF;
    mem_write_raw(&sys.mem, 0x1333, 0xAA);
    u8 code[] = {0xBE, 0x34, 0x12};  // LDX $1234,Y
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(0xAA, sys.cpu.X);
    ASSERT_EQ(5, cycles);  // +1 for page cross
    PASS();
}

// ============================================================================
// STA/STX/STY Tests
// ============================================================================

TEST(sta_zeropage) {
    setup();
    sys.cpu.A = 0x42;
    u8 code[] = {0x85, 0x30};  // STA $30
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_MEM_EQ(0x30, 0x42);
    ASSERT_EQ(3, cycles);
    PASS();
}

TEST(sta_absolute) {
    setup();
    sys.cpu.A = 0x77;
    u8 code[] = {0x8D, 0x00, 0x20};  // STA $2000
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_MEM_EQ(0x2000, 0x77);
    ASSERT_EQ(4, cycles);
    PASS();
}

TEST(sta_absolute_x_no_extra_cycle) {
    setup();
    sys.cpu.A = 0x88;
    sys.cpu.X = 0xFF;
    u8 code[] = {0x9D, 0x34, 0x12};  // STA $1234,X
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_MEM_EQ(0x1333, 0x88);
    // STA abs,X is always 5 cycles (no +1 for page cross on writes)
    ASSERT_EQ(5, cycles);
    PASS();
}

TEST(stx_zeropage) {
    setup();
    sys.cpu.X = 0x99;
    u8 code[] = {0x86, 0x40};  // STX $40
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_MEM_EQ(0x40, 0x99);
    PASS();
}

TEST(sty_zeropage) {
    setup();
    sys.cpu.Y = 0xAA;
    u8 code[] = {0x84, 0x50};  // STY $50
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_MEM_EQ(0x50, 0xAA);
    PASS();
}

// ============================================================================
// Transfer Instructions
// ============================================================================

TEST(tax) {
    setup();
    sys.cpu.A = 0x42;
    u8 code[] = {0xAA};  // TAX
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x42, sys.cpu.X);
    ASSERT_EQ(2, cycles);
    PASS();
}

TEST(tay) {
    setup();
    sys.cpu.A = 0x43;
    u8 code[] = {0xA8};  // TAY
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x43, sys.cpu.Y);
    PASS();
}

TEST(txa) {
    setup();
    sys.cpu.X = 0x44;
    u8 code[] = {0x8A};  // TXA
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x44, sys.cpu.A);
    PASS();
}

TEST(tya) {
    setup();
    sys.cpu.Y = 0x45;
    u8 code[] = {0x98};  // TYA
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x45, sys.cpu.A);
    PASS();
}

TEST(tsx) {
    setup();
    sys.cpu.SP = 0xAB;
    u8 code[] = {0xBA};  // TSX
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0xAB, sys.cpu.X);
    PASS();
}

TEST(txs) {
    setup();
    sys.cpu.X = 0xCD;
    u8 code[] = {0x9A};  // TXS
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0xCD, sys.cpu.SP);
    // TXS does NOT affect flags
    PASS();
}

// ============================================================================
// Stack Operations
// ============================================================================

TEST(pha_pla) {
    setup();
    sys.cpu.A = 0x42;
    sys.cpu.SP = 0xFF;
    u8 code[] = {0x48, 0xA9, 0x00, 0x68};  // PHA, LDA #$00, PLA
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);  // PHA
    ASSERT_EQ(0xFE, sys.cpu.SP);
    ASSERT_MEM_EQ(0x01FF, 0x42);
    
    cpu_step(&sys.cpu);  // LDA #$00
    ASSERT_EQ(0x00, sys.cpu.A);
    
    cpu_step(&sys.cpu);  // PLA
    ASSERT_EQ(0x42, sys.cpu.A);
    ASSERT_EQ(0xFF, sys.cpu.SP);
    PASS();
}

TEST(php_plp) {
    setup();
    sys.cpu.P = FLAG_C | FLAG_Z | FLAG_U;
    sys.cpu.SP = 0xFF;
    u8 code[] = {0x08, 0xA9, 0x80, 0x28};  // PHP, LDA #$80 (sets N), PLP
    load_code(code, sizeof(code));
    
    u8 expected_push = sys.cpu.P | FLAG_B | FLAG_U;  // PHP pushes with B set
    
    cpu_step(&sys.cpu);  // PHP
    ASSERT_MEM_EQ(0x01FF, expected_push);
    
    cpu_step(&sys.cpu);  // LDA #$80
    ASSERT_TRUE(sys.cpu.P & FLAG_N);
    
    cpu_step(&sys.cpu);  // PLP
    ASSERT_TRUE(sys.cpu.P & FLAG_C);
    ASSERT_TRUE(sys.cpu.P & FLAG_Z);
    ASSERT_FALSE(sys.cpu.P & FLAG_N);
    PASS();
}

// ============================================================================
// ADC Tests (Binary Mode)
// ============================================================================

TEST(adc_binary_no_carry) {
    setup();
    sys.cpu.A = 0x10;
    sys.cpu.P &= ~FLAG_C;
    u8 code[] = {0x69, 0x20};  // ADC #$20
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x30, sys.cpu.A);
    ASSERT_FALSE(sys.cpu.P & FLAG_C);
    ASSERT_FALSE(sys.cpu.P & FLAG_V);
    PASS();
}

TEST(adc_binary_with_carry_in) {
    setup();
    sys.cpu.A = 0x10;
    sys.cpu.P |= FLAG_C;
    u8 code[] = {0x69, 0x20};  // ADC #$20
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x31, sys.cpu.A);  // 0x10 + 0x20 + 1 = 0x31
    PASS();
}

TEST(adc_binary_carry_out) {
    setup();
    sys.cpu.A = 0xFF;
    sys.cpu.P &= ~FLAG_C;
    u8 code[] = {0x69, 0x01};  // ADC #$01
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x00, sys.cpu.A);
    ASSERT_TRUE(sys.cpu.P & FLAG_C);
    ASSERT_TRUE(sys.cpu.P & FLAG_Z);
    PASS();
}

TEST(adc_binary_overflow_positive) {
    setup();
    sys.cpu.A = 0x7F;  // +127
    sys.cpu.P &= ~FLAG_C;
    u8 code[] = {0x69, 0x01};  // ADC #$01 = +128 (overflow!)
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x80, sys.cpu.A);
    ASSERT_TRUE(sys.cpu.P & FLAG_V);  // Overflow: positive + positive = negative
    ASSERT_TRUE(sys.cpu.P & FLAG_N);
    PASS();
}

TEST(adc_binary_overflow_negative) {
    setup();
    sys.cpu.A = 0x80;  // -128
    sys.cpu.P &= ~FLAG_C;
    u8 code[] = {0x69, 0xFF};  // ADC #$FF (-1) = -129 (overflow!)
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x7F, sys.cpu.A);  // Wraps to +127
    ASSERT_TRUE(sys.cpu.P & FLAG_V);  // Overflow: negative + negative = positive
    ASSERT_TRUE(sys.cpu.P & FLAG_C);
    PASS();
}

// ============================================================================
// ADC Tests (Decimal Mode) - Per documentation
// ============================================================================

TEST(adc_decimal_simple) {
    setup();
    sys.cpu.A = 0x12;
    sys.cpu.P |= FLAG_D;
    sys.cpu.P &= ~FLAG_C;
    u8 code[] = {0x69, 0x34};  // ADC #$34 (BCD)
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x46, sys.cpu.A);  // 12 + 34 = 46 (BCD)
    ASSERT_FALSE(sys.cpu.P & FLAG_C);
    PASS();
}

TEST(adc_decimal_carry) {
    setup();
    sys.cpu.A = 0x58;
    sys.cpu.P |= FLAG_D;
    sys.cpu.P &= ~FLAG_C;
    u8 code[] = {0x69, 0x46};  // ADC #$46 (BCD)
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x04, sys.cpu.A);  // 58 + 46 = 104, wraps to 04 with carry
    ASSERT_TRUE(sys.cpu.P & FLAG_C);
    PASS();
}

TEST(adc_decimal_99_plus_1) {
    setup();
    sys.cpu.A = 0x99;
    sys.cpu.P |= FLAG_D;
    sys.cpu.P &= ~FLAG_C;
    u8 code[] = {0x69, 0x01};  // ADC #$01 (BCD)
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x00, sys.cpu.A);  // 99 + 1 = 100, wraps to 00
    ASSERT_TRUE(sys.cpu.P & FLAG_C);
    PASS();
}

// ============================================================================
// SBC Tests (Binary Mode)
// ============================================================================

TEST(sbc_binary_no_borrow) {
    setup();
    sys.cpu.A = 0x50;
    sys.cpu.P |= FLAG_C;  // No borrow (C=1 means no borrow)
    u8 code[] = {0xE9, 0x20};  // SBC #$20
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x30, sys.cpu.A);
    ASSERT_TRUE(sys.cpu.P & FLAG_C);  // No borrow
    PASS();
}

TEST(sbc_binary_with_borrow) {
    setup();
    sys.cpu.A = 0x50;
    sys.cpu.P &= ~FLAG_C;  // Borrow (C=0 means borrow)
    u8 code[] = {0xE9, 0x20};  // SBC #$20
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x2F, sys.cpu.A);  // 0x50 - 0x20 - 1 = 0x2F
    PASS();
}

TEST(sbc_binary_underflow) {
    setup();
    sys.cpu.A = 0x00;
    sys.cpu.P |= FLAG_C;
    u8 code[] = {0xE9, 0x01};  // SBC #$01
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0xFF, sys.cpu.A);
    ASSERT_FALSE(sys.cpu.P & FLAG_C);  // Borrow occurred
    PASS();
}

TEST(sbc_binary_overflow) {
    setup();
    sys.cpu.A = 0x80;  // -128
    sys.cpu.P |= FLAG_C;
    u8 code[] = {0xE9, 0x01};  // SBC #$01 = -129 (overflow!)
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x7F, sys.cpu.A);  // -128 - 1 = -129, wraps to +127
    ASSERT_TRUE(sys.cpu.P & FLAG_V);
    PASS();
}

// ============================================================================
// SBC Tests (Decimal Mode)
// ============================================================================

TEST(sbc_decimal_simple) {
    setup();
    sys.cpu.A = 0x46;
    sys.cpu.P |= FLAG_D | FLAG_C;
    u8 code[] = {0xE9, 0x12};  // SBC #$12 (BCD)
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x34, sys.cpu.A);  // 46 - 12 = 34 (BCD)
    PASS();
}

TEST(sbc_decimal_borrow) {
    setup();
    sys.cpu.A = 0x10;
    sys.cpu.P |= FLAG_D | FLAG_C;
    u8 code[] = {0xE9, 0x20};  // SBC #$20 (BCD)
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x90, sys.cpu.A);  // 10 - 20 = -10, wraps to 90
    ASSERT_FALSE(sys.cpu.P & FLAG_C);  // Borrow
    PASS();
}

// ============================================================================
// Logical Operations
// ============================================================================

TEST(and_immediate) {
    setup();
    sys.cpu.A = 0xFF;
    u8 code[] = {0x29, 0x0F};  // AND #$0F
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x0F, sys.cpu.A);
    PASS();
}

TEST(ora_immediate) {
    setup();
    sys.cpu.A = 0xF0;
    u8 code[] = {0x09, 0x0F};  // ORA #$0F
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0xFF, sys.cpu.A);
    PASS();
}

TEST(eor_immediate) {
    setup();
    sys.cpu.A = 0xFF;
    u8 code[] = {0x49, 0xAA};  // EOR #$AA
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x55, sys.cpu.A);
    PASS();
}

TEST(bit_zeropage) {
    setup();
    sys.cpu.A = 0x0F;
    mem_write_raw(&sys.mem, 0x42, 0xC0);  // Bits 7,6 set
    u8 code[] = {0x24, 0x42};  // BIT $42
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_TRUE(sys.cpu.P & FLAG_N);   // Bit 7 of memory
    ASSERT_TRUE(sys.cpu.P & FLAG_V);   // Bit 6 of memory
    ASSERT_TRUE(sys.cpu.P & FLAG_Z);   // A AND mem == 0
    PASS();
}

// ============================================================================
// Compare Instructions
// ============================================================================

TEST(cmp_equal) {
    setup();
    sys.cpu.A = 0x42;
    u8 code[] = {0xC9, 0x42};  // CMP #$42
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_TRUE(sys.cpu.P & FLAG_Z);
    ASSERT_TRUE(sys.cpu.P & FLAG_C);  // A >= M
    ASSERT_FALSE(sys.cpu.P & FLAG_N);
    PASS();
}

TEST(cmp_greater) {
    setup();
    sys.cpu.A = 0x50;
    u8 code[] = {0xC9, 0x42};  // CMP #$42
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_FALSE(sys.cpu.P & FLAG_Z);
    ASSERT_TRUE(sys.cpu.P & FLAG_C);  // A >= M
    PASS();
}

TEST(cmp_less) {
    setup();
    sys.cpu.A = 0x30;
    u8 code[] = {0xC9, 0x42};  // CMP #$42
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_FALSE(sys.cpu.P & FLAG_Z);
    ASSERT_FALSE(sys.cpu.P & FLAG_C);  // A < M
    PASS();
}

TEST(cpx_immediate) {
    setup();
    sys.cpu.X = 0x42;
    u8 code[] = {0xE0, 0x42};  // CPX #$42
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_TRUE(sys.cpu.P & FLAG_Z);
    ASSERT_TRUE(sys.cpu.P & FLAG_C);
    PASS();
}

TEST(cpy_immediate) {
    setup();
    sys.cpu.Y = 0x42;
    u8 code[] = {0xC0, 0x42};  // CPY #$42
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_TRUE(sys.cpu.P & FLAG_Z);
    ASSERT_TRUE(sys.cpu.P & FLAG_C);
    PASS();
}

// ============================================================================
// Increment/Decrement
// ============================================================================

TEST(inc_zeropage) {
    setup();
    mem_write_raw(&sys.mem, 0x42, 0x7F);
    u8 code[] = {0xE6, 0x42};  // INC $42
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_MEM_EQ(0x42, 0x80);
    ASSERT_TRUE(sys.cpu.P & FLAG_N);
    ASSERT_EQ(5, cycles);
    PASS();
}

TEST(inc_wrap) {
    setup();
    mem_write_raw(&sys.mem, 0x42, 0xFF);
    u8 code[] = {0xE6, 0x42};  // INC $42
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_MEM_EQ(0x42, 0x00);
    ASSERT_TRUE(sys.cpu.P & FLAG_Z);
    PASS();
}

TEST(dec_zeropage) {
    setup();
    mem_write_raw(&sys.mem, 0x42, 0x80);
    u8 code[] = {0xC6, 0x42};  // DEC $42
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_MEM_EQ(0x42, 0x7F);
    ASSERT_FALSE(sys.cpu.P & FLAG_N);
    PASS();
}

TEST(dec_wrap) {
    setup();
    mem_write_raw(&sys.mem, 0x42, 0x00);
    u8 code[] = {0xC6, 0x42};  // DEC $42
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_MEM_EQ(0x42, 0xFF);
    ASSERT_TRUE(sys.cpu.P & FLAG_N);
    PASS();
}

TEST(inx_iny_dex_dey) {
    setup();
    sys.cpu.X = 0x7F;
    sys.cpu.Y = 0x01;
    u8 code[] = {0xE8, 0x88, 0xCA, 0xC8};  // INX, DEY, DEX, INY
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);  // INX
    ASSERT_EQ(0x80, sys.cpu.X);
    ASSERT_TRUE(sys.cpu.P & FLAG_N);
    
    cpu_step(&sys.cpu);  // DEY
    ASSERT_EQ(0x00, sys.cpu.Y);
    ASSERT_TRUE(sys.cpu.P & FLAG_Z);
    
    cpu_step(&sys.cpu);  // DEX
    ASSERT_EQ(0x7F, sys.cpu.X);
    
    cpu_step(&sys.cpu);  // INY
    ASSERT_EQ(0x01, sys.cpu.Y);
    PASS();
}

// ============================================================================
// Shift/Rotate Operations
// ============================================================================

TEST(asl_accumulator) {
    setup();
    sys.cpu.A = 0x81;
    u8 code[] = {0x0A};  // ASL A
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x02, sys.cpu.A);
    ASSERT_TRUE(sys.cpu.P & FLAG_C);   // Bit 7 shifted to carry
    ASSERT_FALSE(sys.cpu.P & FLAG_N);
    PASS();
}

TEST(lsr_accumulator) {
    setup();
    sys.cpu.A = 0x81;
    u8 code[] = {0x4A};  // LSR A
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x40, sys.cpu.A);
    ASSERT_TRUE(sys.cpu.P & FLAG_C);   // Bit 0 shifted to carry
    ASSERT_FALSE(sys.cpu.P & FLAG_N);  // LSR always clears N
    PASS();
}

TEST(rol_accumulator) {
    setup();
    sys.cpu.A = 0x80;
    sys.cpu.P |= FLAG_C;  // Carry set
    u8 code[] = {0x2A};  // ROL A
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x01, sys.cpu.A);  // Carry rotated in from right
    ASSERT_TRUE(sys.cpu.P & FLAG_C);   // Bit 7 rotated to carry
    PASS();
}

TEST(ror_accumulator) {
    setup();
    sys.cpu.A = 0x01;
    sys.cpu.P |= FLAG_C;  // Carry set
    u8 code[] = {0x6A};  // ROR A
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x80, sys.cpu.A);  // Carry rotated in from left
    ASSERT_TRUE(sys.cpu.P & FLAG_C);   // Bit 0 rotated to carry
    PASS();
}

TEST(asl_memory) {
    setup();
    mem_write_raw(&sys.mem, 0x42, 0x40);
    u8 code[] = {0x06, 0x42};  // ASL $42
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_MEM_EQ(0x42, 0x80);
    ASSERT_FALSE(sys.cpu.P & FLAG_C);
    ASSERT_EQ(5, cycles);
    PASS();
}

// ============================================================================
// Jump/Branch Instructions
// ============================================================================

TEST(jmp_absolute) {
    setup();
    u8 code[] = {0x4C, 0x00, 0x20};  // JMP $2000
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x2000, sys.cpu.PC);
    ASSERT_EQ(3, cycles);
    PASS();
}

TEST(jmp_indirect) {
    setup();
    mem_write_raw(&sys.mem, 0x1000, 0x34);
    mem_write_raw(&sys.mem, 0x1001, 0x12);
    u8 code[] = {0x6C, 0x00, 0x10};  // JMP ($1000)
    load_code(code, sizeof(code));
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x1234, sys.cpu.PC);
    ASSERT_EQ(5, cycles);
    PASS();
}

TEST(jmp_indirect_page_boundary_bug) {
    setup();
    // The 6502 bug: JMP ($10FF) reads from $10FF and $1000 (not $1100)
    mem_write_raw(&sys.mem, 0x10FF, 0x34);
    mem_write_raw(&sys.mem, 0x1000, 0x12);  // Bug: reads from $1000, not $1100
    u8 code[] = {0x6C, 0xFF, 0x10};  // JMP ($10FF)
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x1234, sys.cpu.PC);
    PASS();
}

TEST(jsr_rts) {
    setup();
    sys.cpu.SP = 0xFF;
    // JSR $0900, then at $0900: RTS
    u8 code[] = {0x20, 0x00, 0x09};  // JSR $0900
    load_code(code, sizeof(code));
    mem_write_raw(&sys.mem, 0x0900, 0x60);  // RTS
    
    u16 return_addr = sys.cpu.PC + 2;  // Address of last byte of JSR
    
    cpu_step(&sys.cpu);  // JSR
    ASSERT_EQ(0x0900, sys.cpu.PC);
    ASSERT_EQ(0xFD, sys.cpu.SP);  // Pushed 2 bytes
    
    cpu_step(&sys.cpu);  // RTS
    ASSERT_EQ(return_addr + 1, sys.cpu.PC);  // RTS adds 1
    ASSERT_EQ(0xFF, sys.cpu.SP);
    PASS();
}

TEST(branch_not_taken) {
    setup();
    sys.cpu.P &= ~FLAG_Z;  // Z=0
    u8 code[] = {0xF0, 0x10};  // BEQ +16
    load_code(code, sizeof(code));
    
    u16 expected_pc = sys.cpu.PC + 2;
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(expected_pc, sys.cpu.PC);
    ASSERT_EQ(2, cycles);
    PASS();
}

TEST(branch_taken_no_page_cross) {
    setup();
    sys.cpu.P |= FLAG_Z;  // Z=1
    u8 code[] = {0xF0, 0x10};  // BEQ +16
    load_code(code, sizeof(code));
    
    u16 expected_pc = sys.cpu.PC + 2 + 0x10;
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(expected_pc, sys.cpu.PC);
    ASSERT_EQ(3, cycles);  // +1 for branch taken
    PASS();
}

TEST(branch_taken_page_cross) {
    setup();
    sys.cpu.PC = 0x08F0;  // Near page boundary
    sys.cpu.P |= FLAG_Z;  // Z=1
    u8 code[] = {0xF0, 0x20};  // BEQ +32 (crosses to $0912)
    load_code(code, sizeof(code));
    
    u16 expected_pc = 0x08F0 + 2 + 0x20;  // = 0x0912
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(expected_pc, sys.cpu.PC);
    ASSERT_EQ(4, cycles);  // +2 for branch taken with page cross
    PASS();
}

TEST(branch_backward) {
    setup();
    sys.cpu.PC = 0x0810;
    sys.cpu.P |= FLAG_N;  // N=1
    u8 code[] = {0x30, 0xFE};  // BMI -2 (loop to self)
    load_code(code, sizeof(code));
    
    u16 expected_pc = 0x0810;  // Back to start
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(expected_pc, sys.cpu.PC);
    PASS();
}

// ============================================================================
// Flag Instructions
// ============================================================================

TEST(flag_instructions) {
    setup();
    u8 code[] = {
        0x38,  // SEC
        0x18,  // CLC
        0x78,  // SEI
        0x58,  // CLI
        0xF8,  // SED
        0xD8,  // CLD
        0x38,  // SEC (set carry for CLV test)
        0xB8   // CLV
    };
    load_code(code, sizeof(code));
    
    // Set V flag for CLV test
    sys.cpu.P |= FLAG_V;
    
    cpu_step(&sys.cpu);  // SEC
    ASSERT_TRUE(sys.cpu.P & FLAG_C);
    
    cpu_step(&sys.cpu);  // CLC
    ASSERT_FALSE(sys.cpu.P & FLAG_C);
    
    cpu_step(&sys.cpu);  // SEI
    ASSERT_TRUE(sys.cpu.P & FLAG_I);
    
    cpu_step(&sys.cpu);  // CLI
    ASSERT_FALSE(sys.cpu.P & FLAG_I);
    
    cpu_step(&sys.cpu);  // SED
    ASSERT_TRUE(sys.cpu.P & FLAG_D);
    
    cpu_step(&sys.cpu);  // CLD
    ASSERT_FALSE(sys.cpu.P & FLAG_D);
    
    cpu_step(&sys.cpu);  // SEC
    cpu_step(&sys.cpu);  // CLV
    ASSERT_FALSE(sys.cpu.P & FLAG_V);
    PASS();
}

// ============================================================================
// NOP
// ============================================================================

TEST(nop) {
    setup();
    u8 code[] = {0xEA};  // NOP
    load_code(code, sizeof(code));
    
    u16 expected_pc = sys.cpu.PC + 1;
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(expected_pc, sys.cpu.PC);
    ASSERT_EQ(2, cycles);
    PASS();
}

// ============================================================================
// BRK and RTI
// ============================================================================

TEST(brk) {
    setup();
    sys.cpu.SP = 0xFF;
    sys.cpu.P = FLAG_N | FLAG_U;  // Some flags set
    sys.cpu.P &= ~FLAG_I;  // Enable interrupts
    
    // Set up BRK vector
    mem_write_raw(&sys.mem, 0xFFFE, 0x00);
    mem_write_raw(&sys.mem, 0xFFFF, 0x10);  // Vector = $1000
    
    u8 code[] = {0x00};  // BRK
    load_code(code, sizeof(code));
    
    u16 return_addr = sys.cpu.PC + 2;  // BRK pushes PC+2
    
    int cycles = cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x1000, sys.cpu.PC);
    ASSERT_TRUE(sys.cpu.P & FLAG_I);  // I flag set
    ASSERT_EQ(0xFC, sys.cpu.SP);  // Pushed 3 bytes
    ASSERT_EQ(7, cycles);
    
    // Check pushed values
    u8 pushed_p = mem_read_raw(&sys.mem, 0x01FD);
    ASSERT_TRUE(pushed_p & FLAG_B);  // B flag set in pushed value
    PASS();
}

TEST(rti) {
    setup();
    sys.cpu.SP = 0xFC;
    // Push status and PC to stack
    mem_write_raw(&sys.mem, 0x01FD, FLAG_N | FLAG_C | FLAG_U);  // Status
    mem_write_raw(&sys.mem, 0x01FE, 0x34);  // PC low
    mem_write_raw(&sys.mem, 0x01FF, 0x12);  // PC high
    
    u8 code[] = {0x40};  // RTI
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x1234, sys.cpu.PC);
    ASSERT_TRUE(sys.cpu.P & FLAG_N);
    ASSERT_TRUE(sys.cpu.P & FLAG_C);
    ASSERT_EQ(0xFF, sys.cpu.SP);
    PASS();
}

// ============================================================================
// CPU Port Tests ($00/$01) - Per documentation
// ============================================================================

TEST(cpu_port_direction) {
    setup();
    // Write to direction register
    sys.cpu.port_dir = 0xFF;  // All outputs
    sys.cpu.port_data = 0xAA;
    
    u8 val = sys.cpu.port_data;
    ASSERT_EQ(0xAA, val);
    PASS();
}

TEST(cpu_port_floating_bits_6_7) {
    setup();
    // Bits 6-7 are not connected, they float
    sys.cpu.port_dir = 0x00;  // All inputs
    sys.cpu.port_data = 0xC0;  // Write to floating bits
    sys.cpu.cpu_port_floating = 0xC0;
    
    // When reading with direction=input, floating bits retain written value
    u8 code[] = {0xA5, 0x01};  // LDA $01
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    // Bits 0-4 have external pull-ups, bit 5 reads low when input
    // Bits 6-7 retain last written value
    ASSERT_EQ(0xDF, sys.cpu.A);  // 0x1F (pull-ups) | 0xC0 (floating)
    PASS();
}

TEST(cpu_port_bit5_behavior) {
    setup();
    // Bit 5 reads low when configured as input (6510 quirk)
    sys.cpu.port_dir = 0x00;  // All inputs
    sys.cpu.port_data = 0x20;  // Try to write bit 5
    
    u8 code[] = {0xA5, 0x01};  // LDA $01
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    // Bit 5 should be 0 when input
    ASSERT_FALSE(sys.cpu.A & 0x20);
    PASS();
}

TEST(cpu_port_output_readback) {
    setup();
    sys.cpu.port_dir = 0xFF;  // All outputs
    sys.cpu.port_data = 0x37;  // Standard C64 config
    
    u8 code[] = {0xA5, 0x01};  // LDA $01
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x37, sys.cpu.A);
    PASS();
}

// ============================================================================
// Interrupt Tests
// ============================================================================

TEST(irq_when_disabled) {
    setup();
    sys.cpu.P |= FLAG_I;  // Disable interrupts
    sys.cpu.irq_pending = true;
    
    u8 code[] = {0xEA};  // NOP
    load_code(code, sizeof(code));
    
    u16 expected_pc = sys.cpu.PC + 1;
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(expected_pc, sys.cpu.PC);  // IRQ ignored
    PASS();
}

TEST(irq_when_enabled) {
    setup();
    sys.cpu.SP = 0xFF;
    sys.cpu.P &= ~FLAG_I;  // Enable interrupts
    sys.cpu.irq_pending = true;
    
    // Set up IRQ vector
    mem_write_raw(&sys.mem, 0xFFFE, 0x00);
    mem_write_raw(&sys.mem, 0xFFFF, 0x20);  // Vector = $2000
    
    u8 code[] = {0xEA};  // NOP (won't execute)
    load_code(code, sizeof(code));
    
    cpu_step(&sys.cpu);
    
    ASSERT_EQ(0x2000, sys.cpu.PC);
    ASSERT_TRUE(sys.cpu.P & FLAG_I);  // I flag set
    PASS();
}

TEST(nmi_edge_triggered) {
    setup();
    sys.cpu.SP = 0xFF;
    
    // Set up NMI vector
    mem_write_raw(&sys.mem, 0xFFFA, 0x00);
    mem_write_raw(&sys.mem, 0xFFFB, 0x30);  // Vector = $3000
    
    u8 code[] = {0xEA, 0xEA};  // NOP, NOP
    load_code(code, sizeof(code));
    
    // Trigger NMI
    cpu_trigger_nmi(&sys.cpu);
    
    cpu_step(&sys.cpu);
    ASSERT_EQ(0x3000, sys.cpu.PC);
    
    // Second NMI trigger while edge flag is still set should be ignored
    cpu_trigger_nmi(&sys.cpu);
    ASSERT_FALSE(sys.cpu.nmi_pending);  // No new NMI
    
    PASS();
}

// ============================================================================
// Run all CPU tests
// ============================================================================

void run_cpu_tests(void) {
    TEST_SUITE("CPU - Load/Store");
    RUN_TEST(lda_immediate);
    RUN_TEST(lda_zero_flag);
    RUN_TEST(lda_negative_flag);
    RUN_TEST(lda_zeropage);
    RUN_TEST(lda_zeropage_x);
    RUN_TEST(lda_zeropage_x_wrap);
    RUN_TEST(lda_absolute);
    RUN_TEST(lda_absolute_x_no_page_cross);
    RUN_TEST(lda_absolute_x_page_cross);
    RUN_TEST(lda_absolute_y_page_cross);
    RUN_TEST(lda_indirect_x);
    RUN_TEST(lda_indirect_y_no_page_cross);
    RUN_TEST(lda_indirect_y_page_cross);
    RUN_TEST(ldx_immediate);
    RUN_TEST(ldy_immediate);
    RUN_TEST(ldx_absolute_y_page_cross);
    RUN_TEST(sta_zeropage);
    RUN_TEST(sta_absolute);
    RUN_TEST(sta_absolute_x_no_extra_cycle);
    RUN_TEST(stx_zeropage);
    RUN_TEST(sty_zeropage);
    
    TEST_SUITE("CPU - Transfers");
    RUN_TEST(tax);
    RUN_TEST(tay);
    RUN_TEST(txa);
    RUN_TEST(tya);
    RUN_TEST(tsx);
    RUN_TEST(txs);
    
    TEST_SUITE("CPU - Stack");
    RUN_TEST(pha_pla);
    RUN_TEST(php_plp);
    
    TEST_SUITE("CPU - ADC (Binary)");
    RUN_TEST(adc_binary_no_carry);
    RUN_TEST(adc_binary_with_carry_in);
    RUN_TEST(adc_binary_carry_out);
    RUN_TEST(adc_binary_overflow_positive);
    RUN_TEST(adc_binary_overflow_negative);
    
    TEST_SUITE("CPU - ADC (Decimal)");
    RUN_TEST(adc_decimal_simple);
    RUN_TEST(adc_decimal_carry);
    RUN_TEST(adc_decimal_99_plus_1);
    
    TEST_SUITE("CPU - SBC (Binary)");
    RUN_TEST(sbc_binary_no_borrow);
    RUN_TEST(sbc_binary_with_borrow);
    RUN_TEST(sbc_binary_underflow);
    RUN_TEST(sbc_binary_overflow);
    
    TEST_SUITE("CPU - SBC (Decimal)");
    RUN_TEST(sbc_decimal_simple);
    RUN_TEST(sbc_decimal_borrow);
    
    TEST_SUITE("CPU - Logical");
    RUN_TEST(and_immediate);
    RUN_TEST(ora_immediate);
    RUN_TEST(eor_immediate);
    RUN_TEST(bit_zeropage);
    
    TEST_SUITE("CPU - Compare");
    RUN_TEST(cmp_equal);
    RUN_TEST(cmp_greater);
    RUN_TEST(cmp_less);
    RUN_TEST(cpx_immediate);
    RUN_TEST(cpy_immediate);
    
    TEST_SUITE("CPU - Increment/Decrement");
    RUN_TEST(inc_zeropage);
    RUN_TEST(inc_wrap);
    RUN_TEST(dec_zeropage);
    RUN_TEST(dec_wrap);
    RUN_TEST(inx_iny_dex_dey);
    
    TEST_SUITE("CPU - Shift/Rotate");
    RUN_TEST(asl_accumulator);
    RUN_TEST(lsr_accumulator);
    RUN_TEST(rol_accumulator);
    RUN_TEST(ror_accumulator);
    RUN_TEST(asl_memory);
    
    TEST_SUITE("CPU - Jump/Branch");
    RUN_TEST(jmp_absolute);
    RUN_TEST(jmp_indirect);
    RUN_TEST(jmp_indirect_page_boundary_bug);
    RUN_TEST(jsr_rts);
    RUN_TEST(branch_not_taken);
    RUN_TEST(branch_taken_no_page_cross);
    RUN_TEST(branch_taken_page_cross);
    RUN_TEST(branch_backward);
    
    TEST_SUITE("CPU - Flags");
    RUN_TEST(flag_instructions);
    RUN_TEST(nop);
    
    TEST_SUITE("CPU - BRK/RTI");
    RUN_TEST(brk);
    RUN_TEST(rti);
    
    TEST_SUITE("CPU - Port ($00/$01)");
    RUN_TEST(cpu_port_direction);
    RUN_TEST(cpu_port_floating_bits_6_7);
    RUN_TEST(cpu_port_bit5_behavior);
    RUN_TEST(cpu_port_output_readback);
    
    TEST_SUITE("CPU - Interrupts");
    RUN_TEST(irq_when_disabled);
    RUN_TEST(irq_when_enabled);
    RUN_TEST(nmi_edge_triggered);
}
