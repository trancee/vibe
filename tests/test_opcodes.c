#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mos6510.h"

// Test framework
static int tests_passed = 0;
static int tests_failed = 0;
static int total_tests = 0;

#define TEST_ASSERT(condition, message) \
    do                                  \
    {                                   \
        total_tests++;                  \
        if (condition)                  \
        {                               \
            tests_passed++;             \
            printf("✓ %s\n", message);  \
        }                               \
        else                            \
        {                               \
            tests_failed++;             \
            printf("✗ %s\n", message);  \
        }                               \
    } while (0)

#define TEST_ASSERT_EQ(expected, actual, message) \
    do                                            \
    {                                             \
        total_tests++;                            \
        if ((expected) == (actual))               \
        {                                         \
            tests_passed++;                       \
            printf("✓ %s\n", message);            \
        }                                         \
        else                                      \
        {                                         \
            tests_failed++;                       \
            printf("✗ %s (expected: 0x%02X, actual: 0x%02X)\n", message, (unsigned)(expected), (unsigned)(actual)); \
        }                                         \
    } while (0)

#define TEST_ASSERT_EQ16(expected, actual, message) \
    do                                              \
    {                                               \
        total_tests++;                              \
        if ((expected) == (actual))                 \
        {                                           \
            tests_passed++;                         \
            printf("✓ %s\n", message);              \
        }                                           \
        else                                        \
        {                                           \
            tests_failed++;                         \
            printf("✗ %s (expected: 0x%04X, actual: 0x%04X)\n", message, (unsigned)(expected), (unsigned)(actual)); \
        }                                           \
    } while (0)

// Helper functions
static CPU *setup_cpu()
{
    CPU *cpu = malloc(sizeof(CPU));
    cpu_init(cpu, false);
    return cpu;
}

static void teardown_cpu(CPU *cpu)
{
    free(cpu);
}

static void write_instruction(CPU *cpu, uint16_t addr, const uint8_t *bytes, int len)
{
    for (int i = 0; i < len; i++)
    {
        cpu->memory[addr + i] = bytes[i];
    }
    cpu_set_pc(cpu, addr);
}

// Test data structures
typedef struct
{
    uint8_t opcode;
    char name[4];
    void (*test_func)(CPU *);
} test_case_t;

// ============================================================================
// ADC - Add with Carry Tests
// ============================================================================

static void test_ADC_imm(CPU *cpu)
{
    cpu->A = 0x50;
    uint8_t instr[] = {0x69, 0x30}; // ADC #$30
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->A, "ADC immediate sets accumulator");
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "ADC immediate clears carry when no overflow");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "ADC immediate sets negative flag when result is negative");
    TEST_ASSERT_EQ(true, get_flag_overflow(cpu), "ADC immediate sets overflow when sign changes");
}

static void test_ADC_imm_with_carry(CPU *cpu)
{
    set_flag_carry(cpu, true);
    cpu->A = 0xFF;
    uint8_t instr[] = {0x69, 0x01}; // ADC #$01
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x01, cpu->A, "ADC immediate with carry wraps around");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ADC immediate sets carry on overflow");
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "ADC immediate clears zero flag");
    TEST_ASSERT_EQ(false, get_flag_negative(cpu), "ADC immediate clears negative flag");
}

static void test_ADC_zero_result(CPU *cpu)
{
    set_flag_carry(cpu, true);
    cpu->A = 0xFF;
    uint8_t instr[] = {0x69, 0x00}; // ADC #$00
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "ADC with carry produces zero");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "ADC sets zero flag when result is zero");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ADC sets carry on wrap");
}

static void test_ADC_overflow_positive(CPU *cpu)
{
    // Adding two positive numbers that result in negative (overflow)
    set_flag_carry(cpu, false);
    cpu->A = 0x7F; // 127
    uint8_t instr[] = {0x69, 0x01}; // ADC #$01
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->A, "ADC 127+1=128 (signed overflow)");
    TEST_ASSERT_EQ(true, get_flag_overflow(cpu), "ADC sets overflow when positive+positive=negative");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "ADC sets negative flag");
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "ADC no carry on signed overflow");
}

static void test_ADC_overflow_negative(CPU *cpu)
{
    // Adding two negative numbers that result in positive (overflow)
    set_flag_carry(cpu, false);
    cpu->A = 0x80; // -128
    uint8_t instr[] = {0x69, 0x80}; // ADC #$80 (-128)
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "ADC -128+-128=0 (signed overflow with wrap)");
    TEST_ASSERT_EQ(true, get_flag_overflow(cpu), "ADC sets overflow when negative+negative=positive");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ADC sets carry on wrap");
}

static void test_ADC_no_overflow(CPU *cpu)
{
    // Adding positive and negative that doesn't overflow
    set_flag_carry(cpu, false);
    cpu->A = 0x50; // 80
    uint8_t instr[] = {0x69, 0xB0}; // ADC #$B0 (-80)
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "ADC 80+-80=0");
    TEST_ASSERT_EQ(false, get_flag_overflow(cpu), "ADC no overflow when positive+negative");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ADC sets carry");
}

// ADC Decimal mode tests (per 64doc.txt)
static void test_ADC_decimal_simple(CPU *cpu)
{
    set_flag_decimal(cpu, true);
    set_flag_carry(cpu, false);
    cpu->A = 0x09;
    uint8_t instr[] = {0x69, 0x01}; // ADC #$01
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x10, cpu->A, "ADC decimal 09+01=10 (BCD)");
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "ADC decimal no carry");
}

static void test_ADC_decimal_carry(CPU *cpu)
{
    set_flag_decimal(cpu, true);
    set_flag_carry(cpu, false);
    cpu->A = 0x99;
    uint8_t instr[] = {0x69, 0x01}; // ADC #$01
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "ADC decimal 99+01=00 with carry");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ADC decimal sets carry on overflow");
}

static void test_ADC_decimal_with_carry_in(CPU *cpu)
{
    set_flag_decimal(cpu, true);
    set_flag_carry(cpu, true);
    cpu->A = 0x58;
    uint8_t instr[] = {0x69, 0x46}; // ADC #$46
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x05, cpu->A, "ADC decimal 58+46+1=105 (BCD 05 with carry)");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ADC decimal sets carry");
}

static void test_ADC_decimal_zero_flag(CPU *cpu)
{
    // Per 64doc.txt: Z flag is set before BCD fixup (binary mode logic)
    set_flag_decimal(cpu, true);
    set_flag_carry(cpu, true);
    cpu->A = 0x99;
    uint8_t instr[] = {0x69, 0x66}; // ADC #$66
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // Binary: 0x99 + 0x66 + 1 = 0x100 -> 0x00, so Z should be set
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "ADC decimal Z flag set based on binary result");
}

// ADC addressing mode tests
static void test_ADC_zp(CPU *cpu)
{
    set_flag_carry(cpu, false);
    cpu->A = 0x10;
    cpu->memory[0x42] = 0x20;
    uint8_t instr[] = {0x65, 0x42}; // ADC $42
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x30, cpu->A, "ADC zero page");
}

static void test_ADC_zpx(CPU *cpu)
{
    set_flag_carry(cpu, false);
    cpu->A = 0x10;
    cpu->X = 0x05;
    cpu->memory[0x47] = 0x20;
    uint8_t instr[] = {0x75, 0x42}; // ADC $42,X
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x30, cpu->A, "ADC zero page,X");
}

static void test_ADC_zpx_wrap(CPU *cpu)
{
    // Zero page indexed wraps at page boundary (per 64doc.txt)
    set_flag_carry(cpu, false);
    cpu->A = 0x10;
    cpu->X = 0x10;
    cpu->memory[0x0F] = 0x25; // $FF + $10 wraps to $0F
    uint8_t instr[] = {0x75, 0xFF}; // ADC $FF,X
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x35, cpu->A, "ADC zero page,X wraps at page boundary");
}

static void test_ADC_abs(CPU *cpu)
{
    set_flag_carry(cpu, false);
    cpu->A = 0x10;
    cpu->memory[0x2000] = 0x30;
    uint8_t instr[] = {0x6D, 0x00, 0x20}; // ADC $2000
    write_instruction(cpu, 0x1000, instr, 3);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x40, cpu->A, "ADC absolute");
}

static void test_ADC_absx(CPU *cpu)
{
    set_flag_carry(cpu, false);
    cpu->A = 0x10;
    cpu->X = 0x10;
    cpu->memory[0x2010] = 0x30;
    uint8_t instr[] = {0x7D, 0x00, 0x20}; // ADC $2000,X
    write_instruction(cpu, 0x1000, instr, 3);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x40, cpu->A, "ADC absolute,X");
}

static void test_ADC_absy(CPU *cpu)
{
    set_flag_carry(cpu, false);
    cpu->A = 0x10;
    cpu->Y = 0x10;
    cpu->memory[0x2010] = 0x30;
    uint8_t instr[] = {0x79, 0x00, 0x20}; // ADC $2000,Y
    write_instruction(cpu, 0x1000, instr, 3);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x40, cpu->A, "ADC absolute,Y");
}

static void test_ADC_indx(CPU *cpu)
{
    set_flag_carry(cpu, false);
    cpu->A = 0x10;
    cpu->X = 0x04;
    cpu->memory[0x24] = 0x00;  // Low byte of address
    cpu->memory[0x25] = 0x20;  // High byte of address
    cpu->memory[0x2000] = 0x30;
    uint8_t instr[] = {0x61, 0x20}; // ADC ($20,X)
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x40, cpu->A, "ADC indexed indirect");
}

static void test_ADC_indy(CPU *cpu)
{
    set_flag_carry(cpu, false);
    cpu->A = 0x10;
    cpu->Y = 0x10;
    cpu->memory[0x20] = 0x00;  // Low byte of base address
    cpu->memory[0x21] = 0x20;  // High byte of base address
    cpu->memory[0x2010] = 0x30;
    uint8_t instr[] = {0x71, 0x20}; // ADC ($20),Y
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x40, cpu->A, "ADC indirect indexed");
}

// ============================================================================
// SBC - Subtract with Carry (Borrow) Tests
// ============================================================================

static void test_SBC_imm(CPU *cpu)
{
    set_flag_carry(cpu, true); // No borrow
    cpu->A = 0x50;
    uint8_t instr[] = {0xE9, 0x30}; // SBC #$30
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x20, cpu->A, "SBC immediate performs subtraction");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "SBC immediate sets carry when no borrow");
    TEST_ASSERT_EQ(false, get_flag_negative(cpu), "SBC clears negative for positive result");
}

static void test_SBC_with_borrow(CPU *cpu)
{
    set_flag_carry(cpu, false); // Borrow
    cpu->A = 0x50;
    uint8_t instr[] = {0xE9, 0x30}; // SBC #$30
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x1F, cpu->A, "SBC with borrow subtracts extra 1");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "SBC sets carry when no borrow needed");
}

static void test_SBC_overflow_positive(CPU *cpu)
{
    // Subtracting negative from positive causing overflow
    set_flag_carry(cpu, true);
    cpu->A = 0x7F; // 127
    uint8_t instr[] = {0xE9, 0xFF}; // SBC #$FF (-1)
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->A, "SBC 127-(-1)=128 causes overflow");
    TEST_ASSERT_EQ(true, get_flag_overflow(cpu), "SBC sets overflow when crossing sign boundary");
}

static void test_SBC_overflow_negative(CPU *cpu)
{
    // Subtracting positive from negative causing overflow
    set_flag_carry(cpu, true);
    cpu->A = 0x80; // -128
    uint8_t instr[] = {0xE9, 0x01}; // SBC #$01
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x7F, cpu->A, "SBC -128-1=127 causes overflow");
    TEST_ASSERT_EQ(true, get_flag_overflow(cpu), "SBC sets overflow");
}

static void test_SBC_zero_result(CPU *cpu)
{
    set_flag_carry(cpu, true);
    cpu->A = 0x50;
    uint8_t instr[] = {0xE9, 0x50}; // SBC #$50
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "SBC produces zero");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "SBC sets zero flag");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "SBC sets carry (no borrow)");
}

static void test_SBC_negative_result(CPU *cpu)
{
    set_flag_carry(cpu, true);
    cpu->A = 0x30;
    uint8_t instr[] = {0xE9, 0x50}; // SBC #$50
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0xE0, cpu->A, "SBC produces negative");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "SBC sets negative flag");
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "SBC clears carry (borrow occurred)");
}

// SBC Decimal mode tests (per 64doc.txt - flags same as binary)
static void test_SBC_decimal_simple(CPU *cpu)
{
    set_flag_decimal(cpu, true);
    set_flag_carry(cpu, true);
    cpu->A = 0x20;
    uint8_t instr[] = {0xE9, 0x05}; // SBC #$05
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x15, cpu->A, "SBC decimal 20-05=15 (BCD)");
}

static void test_SBC_decimal_borrow(CPU *cpu)
{
    set_flag_decimal(cpu, true);
    set_flag_carry(cpu, true);
    cpu->A = 0x10;
    uint8_t instr[] = {0xE9, 0x05}; // SBC #$05
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x05, cpu->A, "SBC decimal 10-05=05 (BCD)");
}

static void test_SBC_decimal_underflow(CPU *cpu)
{
    set_flag_decimal(cpu, true);
    set_flag_carry(cpu, true);
    cpu->A = 0x00;
    uint8_t instr[] = {0xE9, 0x01}; // SBC #$01
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x99, cpu->A, "SBC decimal 00-01=99 with borrow");
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "SBC decimal clears carry on borrow");
}

// ============================================================================
// AND - Logical AND Tests
// ============================================================================

static void test_AND_imm(CPU *cpu)
{
    cpu->A = 0xFF;
    uint8_t instr[] = {0x29, 0x0F}; // AND #$0F
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x0F, cpu->A, "AND immediate performs bitwise AND");
    TEST_ASSERT_EQ(false, get_flag_negative(cpu), "AND immediate clears negative flag");
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "AND immediate clears zero flag");
}

static void test_AND_zero_result(CPU *cpu)
{
    cpu->A = 0xF0;
    uint8_t instr[] = {0x29, 0x0F}; // AND #$0F
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "AND produces zero");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "AND sets zero flag");
}

static void test_AND_negative_result(CPU *cpu)
{
    cpu->A = 0xFF;
    uint8_t instr[] = {0x29, 0x80}; // AND #$80
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->A, "AND with bit 7 set");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "AND sets negative flag");
}

// ============================================================================
// ORA - Logical OR Tests
// ============================================================================

static void test_ORA_imm(CPU *cpu)
{
    cpu->A = 0x30;
    uint8_t instr[] = {0x09, 0x81}; // ORA #$81
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0xB1, cpu->A, "ORA immediate performs bitwise OR");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "ORA immediate sets negative flag");
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "ORA immediate clears zero flag");
}

static void test_ORA_zero_operand(CPU *cpu)
{
    cpu->A = 0x00;
    uint8_t instr[] = {0x09, 0x00}; // ORA #$00
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "ORA with zeros stays zero");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "ORA sets zero flag");
}

// ============================================================================
// EOR - Exclusive OR Tests
// ============================================================================

static void test_EOR_imm(CPU *cpu)
{
    cpu->A = 0xFF;
    uint8_t instr[] = {0x49, 0x0F}; // EOR #$0F
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0xF0, cpu->A, "EOR immediate performs XOR");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "EOR sets negative flag");
}

static void test_EOR_same_value(CPU *cpu)
{
    cpu->A = 0x55;
    uint8_t instr[] = {0x49, 0x55}; // EOR #$55
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "EOR same value produces zero");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "EOR sets zero flag");
}

// ============================================================================
// ASL - Arithmetic Shift Left Tests
// ============================================================================

static void test_ASL_acc(CPU *cpu)
{
    cpu->A = 0x80;
    uint8_t instr[] = {0x0A}; // ASL A
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "ASL accumulator shifts left");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ASL accumulator sets carry on bit 7");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "ASL accumulator sets zero flag");
    TEST_ASSERT_EQ(false, get_flag_negative(cpu), "ASL clears negative when result is 0");
}

static void test_ASL_acc_no_carry(CPU *cpu)
{
    cpu->A = 0x40;
    uint8_t instr[] = {0x0A}; // ASL A
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->A, "ASL shifts 0x40 to 0x80");
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "ASL clears carry when bit 7 was 0");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "ASL sets negative when bit 7 becomes 1");
}

static void test_ASL_zp(CPU *cpu)
{
    cpu->memory[0x0010] = 0x01;
    uint8_t instr[] = {0x06, 0x10}; // ASL $10
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x02, cpu->memory[0x0010], "ASL zero page shifts memory left");
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "ASL zero page clears carry");
}

static void test_ASL_multiple_shifts(CPU *cpu)
{
    cpu->A = 0x01;
    // Shift 7 times to get carry
    for (int i = 0; i < 7; i++) {
        uint8_t instr[] = {0x0A}; // ASL A
        write_instruction(cpu, 0x1000, instr, 1);
        cpu_step(cpu);
    }
    TEST_ASSERT_EQ(0x80, cpu->A, "ASL 7 times moves bit 0 to bit 7");
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "ASL no carry yet");
    
    uint8_t instr[] = {0x0A}; // ASL A one more time
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "ASL 8 times produces zero");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ASL sets carry on final shift");
}

// ============================================================================
// LSR - Logical Shift Right Tests  
// ============================================================================

static void test_LSR_acc(CPU *cpu)
{
    cpu->A = 0x01;
    uint8_t instr[] = {0x4A}; // LSR A
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "LSR accumulator shifts right");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "LSR accumulator sets carry on bit 0");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "LSR accumulator sets zero flag");
    TEST_ASSERT_EQ(false, get_flag_negative(cpu), "LSR always clears negative");
}

static void test_LSR_acc_no_carry(CPU *cpu)
{
    cpu->A = 0x80;
    uint8_t instr[] = {0x4A}; // LSR A
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x40, cpu->A, "LSR shifts 0x80 to 0x40");
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "LSR clears carry when bit 0 was 0");
    TEST_ASSERT_EQ(false, get_flag_negative(cpu), "LSR always clears negative (bit 7 = 0)");
}

static void test_LSR_zp(CPU *cpu)
{
    cpu->memory[0x10] = 0x02;
    uint8_t instr[] = {0x46, 0x10}; // LSR $10
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x01, cpu->memory[0x10], "LSR zero page shifts memory right");
}

// ============================================================================
// ROL - Rotate Left Tests
// ============================================================================

static void test_ROL_acc_with_carry(CPU *cpu)
{
    set_flag_carry(cpu, true);
    cpu->A = 0x80;
    uint8_t instr[] = {0x2A}; // ROL A
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x01, cpu->A, "ROL rotates carry into bit 0");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ROL sets carry from old bit 7");
}

static void test_ROL_acc_without_carry(CPU *cpu)
{
    set_flag_carry(cpu, false);
    cpu->A = 0x80;
    uint8_t instr[] = {0x2A}; // ROL A
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "ROL with no carry produces zero");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ROL sets carry from old bit 7");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "ROL sets zero flag");
}

static void test_ROL_preserves_bit_pattern(CPU *cpu)
{
    // ROL is a 9-bit rotation (8 bits + carry). After 9 ROLs, original value restores.
    set_flag_carry(cpu, false);
    cpu->A = 0x80;
    
    // After 9 rotations, the original A and C should be restored
    for (int i = 0; i < 9; i++) {
        uint8_t instr[] = {0x2A}; // ROL A
        write_instruction(cpu, 0x1000, instr, 1);
        cpu_step(cpu);
    }
    TEST_ASSERT_EQ(0x80, cpu->A, "ROL 9 times restores value (9-bit rotation)");
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "ROL 9 times restores carry");
}

// ============================================================================
// ROR - Rotate Right Tests
// ============================================================================

static void test_ROR_acc_with_carry(CPU *cpu)
{
    set_flag_carry(cpu, true);
    cpu->A = 0x01;
    uint8_t instr[] = {0x6A}; // ROR A
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->A, "ROR rotates carry into bit 7");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ROR sets carry from old bit 0");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "ROR sets negative when carry rotates to bit 7");
}

static void test_ROR_acc_without_carry(CPU *cpu)
{
    set_flag_carry(cpu, false);
    cpu->A = 0x01;
    uint8_t instr[] = {0x6A}; // ROR A
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "ROR with no carry produces zero");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ROR sets carry from old bit 0");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "ROR sets zero flag");
}

// ============================================================================
// Branch Instructions Tests
// ============================================================================

static void test_BCC_taken(CPU *cpu)
{
    set_flag_carry(cpu, false);
    uint8_t instr[] = {0x90, 0x10}; // BCC $10
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x1012, cpu_get_pc(cpu), "BCC taken when carry clear");
}

static void test_BCC_not_taken(CPU *cpu)
{
    set_flag_carry(cpu, true);
    uint8_t instr[] = {0x90, 0x10}; // BCC $10
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x1002, cpu_get_pc(cpu), "BCC not taken when carry set");
}

static void test_BCS_taken(CPU *cpu)
{
    set_flag_carry(cpu, true);
    uint8_t instr[] = {0xB0, 0x10}; // BCS $10
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x1012, cpu_get_pc(cpu), "BCS taken when carry set");
}

static void test_BEQ_taken(CPU *cpu)
{
    set_flag_zero(cpu, true);
    uint8_t instr[] = {0xF0, 0x08}; // BEQ $08
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x100A, cpu_get_pc(cpu), "BEQ taken when zero set");
}

static void test_BEQ_not_taken(CPU *cpu)
{
    set_flag_zero(cpu, false);
    uint8_t instr[] = {0xF0, 0x08}; // BEQ $08
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x1002, cpu_get_pc(cpu), "BEQ not taken when zero clear");
}

static void test_BNE_taken(CPU *cpu)
{
    set_flag_zero(cpu, false);
    uint8_t instr[] = {0xD0, 0x08}; // BNE $08
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x100A, cpu_get_pc(cpu), "BNE taken when zero clear");
}

static void test_BMI_taken(CPU *cpu)
{
    set_flag_negative(cpu, true);
    uint8_t instr[] = {0x30, 0x08}; // BMI $08
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x100A, cpu_get_pc(cpu), "BMI taken when negative set");
}

static void test_BPL_taken(CPU *cpu)
{
    set_flag_negative(cpu, false);
    uint8_t instr[] = {0x10, 0x08}; // BPL $08
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x100A, cpu_get_pc(cpu), "BPL taken when negative clear");
}

static void test_BVC_taken(CPU *cpu)
{
    set_flag_overflow(cpu, false);
    uint8_t instr[] = {0x50, 0x08}; // BVC $08
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x100A, cpu_get_pc(cpu), "BVC taken when overflow clear");
}

static void test_BVS_taken(CPU *cpu)
{
    set_flag_overflow(cpu, true);
    uint8_t instr[] = {0x70, 0x08}; // BVS $08
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x100A, cpu_get_pc(cpu), "BVS taken when overflow set");
}

static void test_branch_backward(CPU *cpu)
{
    set_flag_carry(cpu, false);
    uint8_t instr[] = {0x90, 0xFC}; // BCC -4 (0xFC = -4 signed)
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x0FFE, cpu_get_pc(cpu), "Branch backward with negative offset");
}

static void test_branch_page_crossing(CPU *cpu)
{
    // Branch that crosses a page boundary
    set_flag_carry(cpu, false);
    uint8_t instr[] = {0x90, 0x7F}; // BCC +127
    write_instruction(cpu, 0x10F0, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x1171, cpu_get_pc(cpu), "Branch crosses page boundary correctly");
}

static void test_branch_backward_page_crossing(CPU *cpu)
{
    // Backward branch that crosses a page boundary
    set_flag_zero(cpu, true);
    uint8_t instr[] = {0xF0, 0x80}; // BEQ -128
    write_instruction(cpu, 0x1010, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x0F92, cpu_get_pc(cpu), "Backward branch crosses page boundary correctly");
}

// ============================================================================
// BIT - Bit Test
// ============================================================================

static void test_BIT_zp(CPU *cpu)
{
    cpu->A = 0x40;
    cpu->memory[0x0020] = 0xC0;
    uint8_t instr[] = {0x24, 0x20}; // BIT $20
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "BIT sets zero flag based on AND result");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "BIT copies bit 7 to negative flag");
    TEST_ASSERT_EQ(true, get_flag_overflow(cpu), "BIT copies bit 6 to overflow flag");
}

static void test_BIT_zero_result(CPU *cpu)
{
    cpu->A = 0x0F;
    cpu->memory[0x0020] = 0xF0;
    uint8_t instr[] = {0x24, 0x20}; // BIT $20
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "BIT sets zero when AND result is 0");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "BIT still copies bit 7 from memory");
    TEST_ASSERT_EQ(true, get_flag_overflow(cpu), "BIT still copies bit 6 from memory");
}

static void test_BIT_flags_from_memory(CPU *cpu)
{
    // Test that N and V flags come from memory, not AND result
    cpu->A = 0xFF;
    cpu->memory[0x0020] = 0x00; // Both bits 6 and 7 clear
    uint8_t instr[] = {0x24, 0x20}; // BIT $20
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "BIT zero when memory is 0");
    TEST_ASSERT_EQ(false, get_flag_negative(cpu), "BIT N=0 from memory bit 7");
    TEST_ASSERT_EQ(false, get_flag_overflow(cpu), "BIT V=0 from memory bit 6");
}

static void test_BIT_abs(CPU *cpu)
{
    cpu->A = 0xFF;
    cpu->memory[0x2000] = 0x80;
    uint8_t instr[] = {0x2C, 0x00, 0x20}; // BIT $2000
    write_instruction(cpu, 0x1000, instr, 3);
    cpu_step(cpu);
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "BIT absolute non-zero result");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "BIT absolute copies bit 7");
    TEST_ASSERT_EQ(false, get_flag_overflow(cpu), "BIT absolute copies bit 6 (0)");
}

// ============================================================================
// CMP/CPX/CPY - Compare Tests
// ============================================================================

static void test_CMP_imm_equal(CPU *cpu)
{
    cpu->A = 0x50;
    uint8_t instr[] = {0xC9, 0x50}; // CMP #$50
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "CMP sets carry when A >= operand");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "CMP sets zero when A == operand");
    TEST_ASSERT_EQ(false, get_flag_negative(cpu), "CMP clears negative when result >= 0");
}

static void test_CMP_greater(CPU *cpu)
{
    cpu->A = 0x80;
    uint8_t instr[] = {0xC9, 0x40}; // CMP #$40
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "CMP sets carry when A > operand");
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "CMP clears zero when A != operand");
}

static void test_CMP_less(CPU *cpu)
{
    cpu->A = 0x20;
    uint8_t instr[] = {0xC9, 0x40}; // CMP #$40
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "CMP clears carry when A < operand");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "CMP sets negative on subtraction");
}

static void test_CMP_unsigned(CPU *cpu)
{
    // Test unsigned comparison: 0xFF > 0x01
    cpu->A = 0xFF;
    uint8_t instr[] = {0xC9, 0x01}; // CMP #$01
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "CMP 0xFF >= 0x01 (unsigned)");
}

static void test_CPX_imm(CPU *cpu)
{
    cpu->X = 0x30;
    uint8_t instr[] = {0xE0, 0x30}; // CPX #$30
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "CPX sets carry when X >= operand");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "CPX sets zero when X == operand");
}

static void test_CPY_imm(CPU *cpu)
{
    cpu->Y = 0x30;
    uint8_t instr[] = {0xC0, 0x30}; // CPY #$30
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "CPY sets carry when Y >= operand");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "CPY sets zero when Y == operand");
}

// ============================================================================
// INC/DEC/INX/DEX/INY/DEY Tests
// ============================================================================

static void test_DEX(CPU *cpu)
{
    cpu->X = 0x01;
    uint8_t instr[] = {0xCA}; // DEX
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->X, "DEX decrements X register");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "DEX sets zero flag");
}

static void test_DEX_wrap(CPU *cpu)
{
    cpu->X = 0x00;
    uint8_t instr[] = {0xCA}; // DEX
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0xFF, cpu->X, "DEX wraps from 0 to 0xFF");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "DEX sets negative flag on wrap");
}

static void test_DEY(CPU *cpu)
{
    cpu->Y = 0x80;
    uint8_t instr[] = {0x88}; // DEY
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x7F, cpu->Y, "DEY decrements Y register");
    TEST_ASSERT_EQ(false, get_flag_negative(cpu), "DEY clears negative when result < 0x80");
}

static void test_INX(CPU *cpu)
{
    cpu->X = 0x7F;
    uint8_t instr[] = {0xE8}; // INX
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->X, "INX increments X register");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "INX sets negative flag");
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "INX clears zero flag");
}

static void test_INX_wrap(CPU *cpu)
{
    cpu->X = 0xFF;
    uint8_t instr[] = {0xE8}; // INX
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->X, "INX wraps from 0xFF to 0");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "INX sets zero flag on wrap");
}

static void test_INY(CPU *cpu)
{
    cpu->Y = 0x00;
    uint8_t instr[] = {0xC8}; // INY
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x01, cpu->Y, "INY increments Y register");
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "INY clears zero flag");
}

static void test_INC_zp(CPU *cpu)
{
    cpu->memory[0x10] = 0xFF;
    uint8_t instr[] = {0xE6, 0x10}; // INC $10
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->memory[0x10], "INC zero page wraps");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "INC sets zero flag");
}

static void test_DEC_zp(CPU *cpu)
{
    cpu->memory[0x10] = 0x01;
    uint8_t instr[] = {0xC6, 0x10}; // DEC $10
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->memory[0x10], "DEC zero page");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "DEC sets zero flag");
}

// ============================================================================
// JMP Tests (including 6502 indirect bug)
// ============================================================================

static void test_JMP_abs(CPU *cpu)
{
    uint8_t instr[] = {0x4C, 0x00, 0x20}; // JMP $2000
    write_instruction(cpu, 0x1000, instr, 3);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x2000, cpu_get_pc(cpu), "JMP absolute jumps to address");
}

static void test_JMP_ind(CPU *cpu)
{
    cpu->memory[0x2000] = 0x34;
    cpu->memory[0x2001] = 0x12;
    uint8_t instr[] = {0x6C, 0x00, 0x20}; // JMP ($2000)
    write_instruction(cpu, 0x1000, instr, 3);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x1234, cpu_get_pc(cpu), "JMP indirect reads address from memory");
}

// Test the famous 6502 JMP indirect bug per 64doc.txt:
// "If the parameter's low byte is $FF, the effective address wraps
// around and the CPU fetches high byte from $xx00 instead of $xx00+$0100"
static void test_JMP_ind_page_boundary_bug(CPU *cpu)
{
    // JMP ($20FF) should read low byte from $20FF and high byte from $2000 (not $2100)
    cpu->memory[0x20FF] = 0x34;  // Low byte of target
    cpu->memory[0x2000] = 0x12;  // High byte (wrapped, not from $2100)
    cpu->memory[0x2100] = 0x56;  // This should NOT be used
    uint8_t instr[] = {0x6C, 0xFF, 0x20}; // JMP ($20FF)
    write_instruction(cpu, 0x1000, instr, 3);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x1234, cpu_get_pc(cpu), "JMP indirect bug: high byte from $xx00 not $xx00+$100");
}

static void test_JMP_ind_no_bug_when_not_at_boundary(CPU *cpu)
{
    // Normal case: JMP ($2000) should work correctly
    cpu->memory[0x2000] = 0xCD;
    cpu->memory[0x2001] = 0xAB;
    uint8_t instr[] = {0x6C, 0x00, 0x20}; // JMP ($2000)
    write_instruction(cpu, 0x1000, instr, 3);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0xABCD, cpu_get_pc(cpu), "JMP indirect normal case");
}

// ============================================================================
// JSR/RTS Tests
// ============================================================================

static void test_JSR(CPU *cpu)
{
    cpu->SP = 0xFF;
    uint8_t instr[] = {0x20, 0x34, 0x12}; // JSR $1234
    write_instruction(cpu, 0x1000, instr, 3);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x1234, cpu_get_pc(cpu), "JSR jumps to subroutine");
    TEST_ASSERT_EQ(0xFD, cpu->SP, "JSR pushes return address to stack");
    TEST_ASSERT_EQ(0x10, cpu->memory[0x01FF], "JSR pushes high byte of return address");
    TEST_ASSERT_EQ(0x02, cpu->memory[0x01FE], "JSR pushes low byte of return address");
}

static void test_JSR_RTS_round_trip(CPU *cpu)
{
    cpu->SP = 0xFF;
    
    // JSR to $2000
    uint8_t instr_jsr[] = {0x20, 0x00, 0x20}; // JSR $2000
    write_instruction(cpu, 0x1000, instr_jsr, 3);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x2000, cpu_get_pc(cpu), "JSR jumps to subroutine");
    
    // RTS to return
    uint8_t instr_rts[] = {0x60}; // RTS
    write_instruction(cpu, 0x2000, instr_rts, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ16(0x1003, cpu_get_pc(cpu), "RTS returns to instruction after JSR");
    TEST_ASSERT_EQ(0xFF, cpu->SP, "RTS restores stack pointer");
}

// ============================================================================
// LDA/LDX/LDY Tests
// ============================================================================

static void test_LDA_imm(CPU *cpu)
{
    uint8_t instr[] = {0xA9, 0x42}; // LDA #$42
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x42, cpu->A, "LDA immediate loads accumulator");
    TEST_ASSERT_EQ(false, get_flag_negative(cpu), "LDA immediate clears negative flag");
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "LDA immediate clears zero flag");
}

static void test_LDA_zero(CPU *cpu)
{
    uint8_t instr[] = {0xA9, 0x00}; // LDA #$00
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "LDA loads zero");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "LDA sets zero flag");
}

static void test_LDA_negative(CPU *cpu)
{
    uint8_t instr[] = {0xA9, 0x80}; // LDA #$80
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->A, "LDA loads negative value");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "LDA sets negative flag");
}

static void test_LDX_imm(CPU *cpu)
{
    uint8_t instr[] = {0xA2, 0x80}; // LDX #$80
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->X, "LDX immediate loads X register");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "LDX immediate sets negative flag");
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "LDX immediate clears zero flag");
}

static void test_LDY_imm(CPU *cpu)
{
    uint8_t instr[] = {0xA0, 0x00}; // LDY #$00
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->Y, "LDY immediate loads Y register");
    TEST_ASSERT_EQ(false, get_flag_negative(cpu), "LDY immediate clears negative flag");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "LDY immediate sets zero flag");
}

static void test_NOP(CPU *cpu)
{
    uint16_t pc = 0x1000;
    cpu_set_pc(cpu, pc);
    uint8_t instr[] = {0xEA}; // NOP
    write_instruction(cpu, pc, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(pc + 1, cpu_get_pc(cpu), "NOP increments PC");
}

static void test_PHA_PLA(CPU *cpu)
{
    cpu->A = 0x42;
    cpu->SP = 0xFF;

    // Test PHA
    uint8_t instr_pha[] = {0x48}; // PHA
    write_instruction(cpu, 0x1000, instr_pha, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0xFE, cpu->SP, "PHA decrements stack pointer");
    TEST_ASSERT_EQ(0x42, cpu->memory[0x01FF], "PHA stores accumulator on stack");

    // Test PLA
    cpu->A = 0x00;
    uint8_t instr_pla[] = {0x68}; // PLA
    write_instruction(cpu, 0x1001, instr_pla, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0xFF, cpu->SP, "PLA increments stack pointer");
    TEST_ASSERT_EQ(0x42, cpu->A, "PLA restores accumulator from stack");
}

static void test_RTS(CPU *cpu)
{
    cpu->SP = 0xFD;
    cpu->memory[0x01FF] = 0x10;
    cpu->memory[0x01FE] = 0x02;

    uint8_t instr[] = {0x60}; // RTS
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x1003, cpu_get_pc(cpu), "RTS pulls return address and adds 1");
    TEST_ASSERT_EQ(0xFF, cpu->SP, "RTS increments stack pointer");
}

static void test_STA_zp(CPU *cpu)
{
    cpu->A = 0x42;
    uint8_t instr[] = {0x85, 0x20}; // STA $20
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x42, cpu->memory[0x0020], "STA zero page stores accumulator");
}

static void test_TAX_TAY(CPU *cpu)
{
    cpu->A = 0x80;

    // Test TAX
    uint8_t instr_tax[] = {0xAA}; // TAX
    write_instruction(cpu, 0x1000, instr_tax, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->X, "TAX transfers A to X");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "TAX sets negative flag");

    // Test TAY
    uint8_t instr_tay[] = {0xA8}; // TAY
    write_instruction(cpu, 0x1001, instr_tay, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->Y, "TAY transfers A to Y");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "TAY sets negative flag");
}

static void test_TSX_TXS(CPU *cpu)
{
    cpu->SP = 0x42;

    // Test TSX
    uint8_t instr_tsx[] = {0xBA}; // TSX
    write_instruction(cpu, 0x1000, instr_tsx, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x42, cpu->X, "TSX transfers SP to X");

    // Test TXS
    cpu->X = 0xFF;
    uint8_t instr_txs[] = {0x9A}; // TXS
    write_instruction(cpu, 0x1001, instr_txs, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0xFF, cpu->SP, "TXS transfers X to SP");
}

static void test_BRK(CPU *cpu)
{
    cpu->P = FLAG_RESERVED;

    cpu_write_data(cpu, IRQ, (uint8_t[]){0xCD, 0xAB}, 2);

    // $C000 BRK
    cpu_write_byte(cpu, 0xC000, 0x00); // BRK opcode

    cpu_set_pc(cpu, 0xC000);

    TEST_ASSERT_EQ(7, cpu_step(cpu), "BRK takes 7 cycles");
    TEST_ASSERT_EQ(0xABCD, cpu_get_pc(cpu), "BRK sets PC to IRQ vector");
    TEST_ASSERT_EQ(0xFC, cpu->SP, "BRK pushes PC and P to stack");
    TEST_ASSERT_EQ(0x02, cpu->memory[PCL], "BRK pushes low byte of PC");
    TEST_ASSERT_EQ(0xC0, cpu->memory[PCH], "BRK pushes high byte of PC");
    TEST_ASSERT_EQ(0xC002, cpu_read_word(cpu, 0x01FE), "BRK pushes correct PC onto stack");
    TEST_ASSERT_EQ(FLAG_RESERVED | FLAG_BREAK, cpu->memory[0x01FD], "BRK pushes P with BREAK flag set");
    TEST_ASSERT_EQ(FLAG_RESERVED | FLAG_INTERRUPT_DISABLE, cpu->P, "BRK sets INTERRUPT flag in P");
}

static void test_IRQ(CPU *cpu)
{
    cpu->P = FLAG_RESERVED;

    cpu_write_data(cpu, NMI, (uint8_t[]){0x88, 0x77}, 2);
    cpu_write_data(cpu, IRQ, (uint8_t[]){0xCD, 0xAB}, 2);

    cpu_set_pc(cpu, 0xC123);

    cpu->irq = true;

    TEST_ASSERT_EQ(7, cpu_step(cpu), "IRQ takes 7 cycles");
    TEST_ASSERT_EQ(0xABCD, cpu_get_pc(cpu), "IRQ sets PC to IRQ vector");
    TEST_ASSERT_EQ(0xFC, cpu->SP, "IRQ pushes PC and P to stack");
    TEST_ASSERT_EQ(0x23, cpu->memory[PCL], "IRQ pushes low byte of PC");
    TEST_ASSERT_EQ(0xC1, cpu->memory[PCH], "IRQ pushes high byte of PC");
    TEST_ASSERT_EQ(0xC123, cpu_read_word(cpu, 0x01FE), "IRQ pushes correct PC onto stack");
    TEST_ASSERT_EQ(FLAG_RESERVED, cpu->memory[0x01FD], "IRQ pushes P without BREAK flag set");
    TEST_ASSERT_EQ(FLAG_RESERVED | FLAG_INTERRUPT_DISABLE, cpu->P, "IRQ sets INTERRUPT flag in P");
}

static void test_NMI(CPU *cpu)
{
    cpu->P = FLAG_RESERVED;

    cpu_write_data(cpu, NMI, (uint8_t[]){0x88, 0x77}, 2);
    cpu_write_data(cpu, IRQ, (uint8_t[]){0xCD, 0xAB}, 2);

    cpu_set_pc(cpu, 0xC123);

    cpu->nmi = true;

    TEST_ASSERT_EQ(7, cpu_step(cpu), "NMI takes 7 cycles");
    TEST_ASSERT_EQ16(0x7788, cpu_get_pc(cpu), "NMI sets PC to NMI vector");
    TEST_ASSERT_EQ(0xFC, cpu->SP, "NMI pushes PC and P to stack");
    TEST_ASSERT_EQ(0x23, cpu->memory[PCL], "NMI pushes low byte of PC");
    TEST_ASSERT_EQ(0xC1, cpu->memory[PCH], "NMI pushes high byte of PC");
    TEST_ASSERT_EQ16(0xC123, cpu_read_word(cpu, 0x01FE), "NMI pushes correct PC onto stack");
    TEST_ASSERT_EQ(FLAG_RESERVED, cpu->memory[0x01FD], "NMI pushes P without BREAK flag set");
    TEST_ASSERT_EQ(FLAG_RESERVED | FLAG_INTERRUPT_DISABLE, cpu->P, "NMI sets INTERRUPT flag in P");
}

// ============================================================================
// PHP/PLP Tests (per 64doc.txt: PHP always pushes B=1)
// ============================================================================

static void test_PHP_pushes_break_flag(CPU *cpu)
{
    // Per 64doc.txt: PHP always pushes the Break (B) flag as '1' to the stack
    cpu->P = FLAG_RESERVED;  // Only reserved flag set
    cpu->SP = 0xFF;
    
    uint8_t instr[] = {0x08}; // PHP
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    
    TEST_ASSERT_EQ(FLAG_RESERVED | FLAG_BREAK, cpu->memory[0x01FF], "PHP pushes P with BREAK flag always set");
}

static void test_PLP_clears_break_flag(CPU *cpu)
{
    // PLP should ignore the B flag and always clear it
    cpu->SP = 0xFE;
    cpu->memory[0x01FF] = FLAG_RESERVED | FLAG_BREAK | FLAG_CARRY;
    
    uint8_t instr[] = {0x28}; // PLP
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    
    TEST_ASSERT_EQ(FLAG_RESERVED | FLAG_CARRY, cpu->P, "PLP clears BREAK flag");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "PLP restores carry flag");
}

static void test_PLP_sets_reserved_flag(CPU *cpu)
{
    // PLP should always set the reserved flag
    cpu->SP = 0xFE;
    cpu->memory[0x01FF] = 0x00;  // No flags set
    
    uint8_t instr[] = {0x28}; // PLP
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    
    TEST_ASSERT_EQ(FLAG_RESERVED, cpu->P, "PLP always sets reserved flag");
}

// ============================================================================
// RTI Tests
// ============================================================================

static void test_RTI(CPU *cpu)
{
    cpu->SP = 0xFC;
    cpu->memory[0x01FD] = FLAG_RESERVED | FLAG_CARRY;  // Status to restore
    cpu->memory[0x01FE] = 0x34;  // PC low
    cpu->memory[0x01FF] = 0x12;  // PC high
    
    uint8_t instr[] = {0x40}; // RTI
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    
    TEST_ASSERT_EQ16(0x1234, cpu_get_pc(cpu), "RTI restores PC");
    TEST_ASSERT_EQ(0xFF, cpu->SP, "RTI restores stack pointer");
    TEST_ASSERT_EQ(FLAG_RESERVED | FLAG_CARRY, cpu->P, "RTI restores status");
}

static void test_RTI_clears_break_flag(CPU *cpu)
{
    cpu->SP = 0xFC;
    cpu->memory[0x01FD] = FLAG_RESERVED | FLAG_BREAK | FLAG_CARRY;
    cpu->memory[0x01FE] = 0x00;
    cpu->memory[0x01FF] = 0x20;
    
    uint8_t instr[] = {0x40}; // RTI
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    
    TEST_ASSERT_EQ(FLAG_RESERVED | FLAG_CARRY, cpu->P, "RTI clears BREAK flag");
}

// ============================================================================
// Zero Page Wrapping Tests (per 64doc.txt)
// ============================================================================

static void test_zp_indexed_wrapping(CPU *cpu)
{
    // Per 64doc.txt: Indexed zero page addressing modes never fix the page address
    // on crossing the zero page boundary
    cpu->X = 0x10;
    cpu->memory[0x0F] = 0x42;  // $FF + $10 wraps to $0F
    
    uint8_t instr[] = {0xB5, 0xFF}; // LDA $FF,X
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    
    TEST_ASSERT_EQ(0x42, cpu->A, "Zero page,X wraps at page boundary");
}

static void test_zpy_indexed_wrapping(CPU *cpu)
{
    cpu->Y = 0x20;
    cpu->memory[0x1F] = 0x55;  // $FF + $20 wraps to $1F
    
    uint8_t instr[] = {0xB6, 0xFF}; // LDX $FF,Y
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    
    TEST_ASSERT_EQ(0x55, cpu->X, "Zero page,Y wraps at page boundary");
}

// ============================================================================
// Indexed Indirect Wrapping Tests (per 64doc.txt)
// ============================================================================

static void test_indexed_indirect_wrapping(CPU *cpu)
{
    // Per 64doc.txt: The effective address is always fetched from zero page,
    // i.e. the zero page boundary crossing is not handled.
    // E.g. LDX #$01 : LDA ($FF,X) loads the effective address from $00 and $01
    cpu->X = 0x01;
    cpu->memory[0x00] = 0x00;  // Low byte of address
    cpu->memory[0x01] = 0x20;  // High byte of address
    cpu->memory[0x2000] = 0x77;
    
    uint8_t instr[] = {0xA1, 0xFF}; // LDA ($FF,X)
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    
    TEST_ASSERT_EQ(0x77, cpu->A, "Indexed indirect wraps in zero page");
}

static void test_indirect_indexed_zp_wrapping(CPU *cpu)
{
    // The pointer fetch also wraps within zero page
    // LDA ($FF),Y fetches the base address from $FF and $00
    cpu->Y = 0x10;
    cpu->memory[0xFF] = 0x00;  // Low byte of base
    cpu->memory[0x00] = 0x20;  // High byte (from wrap)
    cpu->memory[0x2010] = 0x88;
    
    uint8_t instr[] = {0xB1, 0xFF}; // LDA ($FF),Y
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    
    TEST_ASSERT_EQ(0x88, cpu->A, "Indirect indexed pointer wraps in zero page");
}

// ============================================================================
// Flag Instruction Tests
// ============================================================================

static void test_CLC(CPU *cpu)
{
    set_flag_carry(cpu, true);
    uint8_t instr[] = {0x18}; // CLC
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "CLC clears carry flag");
}

static void test_SEC(CPU *cpu)
{
    set_flag_carry(cpu, false);
    uint8_t instr[] = {0x38}; // SEC
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "SEC sets carry flag");
}

static void test_CLD(CPU *cpu)
{
    set_flag_decimal(cpu, true);
    uint8_t instr[] = {0xD8}; // CLD
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(false, get_flag_decimal(cpu), "CLD clears decimal flag");
}

static void test_SED(CPU *cpu)
{
    set_flag_decimal(cpu, false);
    uint8_t instr[] = {0xF8}; // SED
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(true, get_flag_decimal(cpu), "SED sets decimal flag");
}

static void test_CLI(CPU *cpu)
{
    set_flag_interrupt(cpu, true);
    uint8_t instr[] = {0x58}; // CLI
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(false, get_flag_interrupt(cpu), "CLI clears interrupt disable flag");
}

static void test_SEI(CPU *cpu)
{
    set_flag_interrupt(cpu, false);
    uint8_t instr[] = {0x78}; // SEI
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(true, get_flag_interrupt(cpu), "SEI sets interrupt disable flag");
}

static void test_CLV(CPU *cpu)
{
    set_flag_overflow(cpu, true);
    uint8_t instr[] = {0xB8}; // CLV
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(false, get_flag_overflow(cpu), "CLV clears overflow flag");
}

// ============================================================================
// Transfer Instruction Tests
// ============================================================================

static void test_TXA(CPU *cpu)
{
    cpu->X = 0x80;
    cpu->A = 0x00;
    uint8_t instr[] = {0x8A}; // TXA
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->A, "TXA transfers X to A");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "TXA sets negative flag");
}

static void test_TYA(CPU *cpu)
{
    cpu->Y = 0x00;
    cpu->A = 0xFF;
    uint8_t instr[] = {0x98}; // TYA
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "TYA transfers Y to A");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "TYA sets zero flag");
}

static void test_TXS_no_flags(CPU *cpu)
{
    // TXS should NOT affect any flags (per 64doc.txt)
    cpu->X = 0x80;
    cpu->P = FLAG_RESERVED;  // Clear all flags
    uint8_t instr[] = {0x9A}; // TXS
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->SP, "TXS transfers X to SP");
    TEST_ASSERT_EQ(FLAG_RESERVED, cpu->P, "TXS does NOT affect flags");
}

static void test_TSX(CPU *cpu)
{
    cpu->SP = 0x00;
    uint8_t instr[] = {0xBA}; // TSX
    write_instruction(cpu, 0x1000, instr, 1);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->X, "TSX transfers SP to X");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "TSX sets zero flag");
}

// ============================================================================
// Store Instruction Tests
// ============================================================================

static void test_STX_zp(CPU *cpu)
{
    cpu->X = 0x42;
    uint8_t instr[] = {0x86, 0x20}; // STX $20
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x42, cpu->memory[0x20], "STX zero page stores X");
}

static void test_STY_zp(CPU *cpu)
{
    cpu->Y = 0x42;
    uint8_t instr[] = {0x84, 0x20}; // STY $20
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x42, cpu->memory[0x20], "STY zero page stores Y");
}

// ============================================================================
// Illegal Opcode Tests (per 64doc.txt)
// ============================================================================

static void test_LAX(CPU *cpu)
{
    // LAX: Load A and X with the same value
    cpu->memory[0x20] = 0x42;
    uint8_t instr[] = {0xA7, 0x20}; // LAX $20
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x42, cpu->A, "LAX loads A");
    TEST_ASSERT_EQ(0x42, cpu->X, "LAX loads X with same value");
}

static void test_SAX(CPU *cpu)
{
    // SAX: Store A AND X
    cpu->A = 0xF0;
    cpu->X = 0x0F;
    uint8_t instr[] = {0x87, 0x20}; // SAX $20
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->memory[0x20], "SAX stores A AND X");
}

static void test_SAX_nonzero(CPU *cpu)
{
    cpu->A = 0xFF;
    cpu->X = 0x0F;
    uint8_t instr[] = {0x87, 0x20}; // SAX $20
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x0F, cpu->memory[0x20], "SAX stores A AND X (0xFF & 0x0F = 0x0F)");
}

static void test_DCP(CPU *cpu)
{
    // DCP: DEC then CMP
    cpu->A = 0x05;
    cpu->memory[0x20] = 0x06;
    uint8_t instr[] = {0xC7, 0x20}; // DCP $20
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x05, cpu->memory[0x20], "DCP decrements memory");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "DCP compares and sets zero when A == result");
}

static void test_ISC(CPU *cpu)
{
    // ISC (ISB): INC then SBC
    set_flag_carry(cpu, true);
    cpu->A = 0x10;
    cpu->memory[0x20] = 0x04;
    uint8_t instr[] = {0xE7, 0x20}; // ISC $20
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x05, cpu->memory[0x20], "ISC increments memory");
    TEST_ASSERT_EQ(0x0B, cpu->A, "ISC subtracts from A (0x10 - 0x05 = 0x0B)");
}

static void test_SLO(CPU *cpu)
{
    // SLO: ASL then ORA
    cpu->A = 0x0F;
    cpu->memory[0x20] = 0x40;
    uint8_t instr[] = {0x07, 0x20}; // SLO $20
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->memory[0x20], "SLO shifts memory left");
    TEST_ASSERT_EQ(0x8F, cpu->A, "SLO ORs result with A");
}

static void test_RLA(CPU *cpu)
{
    // RLA: ROL then AND
    set_flag_carry(cpu, true);
    cpu->A = 0xFF;
    cpu->memory[0x20] = 0x80;
    uint8_t instr[] = {0x27, 0x20}; // RLA $20
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x01, cpu->memory[0x20], "RLA rotates memory left");
    TEST_ASSERT_EQ(0x01, cpu->A, "RLA ANDs result with A");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "RLA sets carry from bit 7");
}

static void test_SRE(CPU *cpu)
{
    // SRE: LSR then EOR
    cpu->A = 0xFF;
    cpu->memory[0x20] = 0x02;
    uint8_t instr[] = {0x47, 0x20}; // SRE $20
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x01, cpu->memory[0x20], "SRE shifts memory right");
    TEST_ASSERT_EQ(0xFE, cpu->A, "SRE XORs result with A");
}

static void test_RRA(CPU *cpu)
{
    // RRA: ROR then ADC
    set_flag_carry(cpu, true);
    cpu->A = 0x10;
    cpu->memory[0x20] = 0x02;
    uint8_t instr[] = {0x67, 0x20}; // RRA $20
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x81, cpu->memory[0x20], "RRA rotates memory right");
    TEST_ASSERT_EQ(0x91, cpu->A, "RRA adds result to A");
}

static void test_ANC(CPU *cpu)
{
    // ANC: AND then copy N to C
    cpu->A = 0xFF;
    uint8_t instr[] = {0x0B, 0x80}; // ANC #$80
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->A, "ANC ANDs with immediate");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ANC copies bit 7 to carry");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "ANC sets negative flag");
}

static void test_ALR(CPU *cpu)
{
    // ALR: AND then LSR
    cpu->A = 0xFF;
    uint8_t instr[] = {0x4B, 0x03}; // ALR #$03
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x01, cpu->A, "ALR ANDs then shifts right (0xFF & 0x03 = 0x03, >> 1 = 0x01)");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ALR sets carry from bit 0 of AND result");
}

// ============================================================================
// ARR - AND then ROR with special flag behavior (per 64doc.txt)
// ============================================================================

static void test_ARR_binary_basic(CPU *cpu)
{
    // ARR in binary mode: AND then ROR, C from bit 6, V from bit6 XOR bit5
    set_flag_decimal(cpu, false);
    set_flag_carry(cpu, true);  // Carry will be rotated into bit 7
    cpu->A = 0xFF;
    uint8_t instr[] = {0x6B, 0xFF}; // ARR #$FF
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // A = (0xFF & 0xFF) >> 1 | (1 << 7) = 0x7F | 0x80 = 0xFF
    TEST_ASSERT_EQ(0xFF, cpu->A, "ARR binary: 0xFF AND 0xFF ROR with C=1 = 0xFF");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ARR binary: C = bit 6 of result (1)");
    TEST_ASSERT_EQ(false, get_flag_overflow(cpu), "ARR binary: V = bit6 XOR bit5 (1^1=0)");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "ARR binary: N = bit 7 of result");
}

static void test_ARR_binary_carry_from_bit6(CPU *cpu)
{
    // Test that carry comes from bit 6
    set_flag_decimal(cpu, false);
    set_flag_carry(cpu, false);
    cpu->A = 0x80;  // bit 7 set, bit 6 clear
    uint8_t instr[] = {0x6B, 0xFF}; // ARR #$FF
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // A = (0x80 & 0xFF) >> 1 | (0 << 7) = 0x40
    TEST_ASSERT_EQ(0x40, cpu->A, "ARR binary: 0x80 ROR with C=0 = 0x40");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ARR binary: C = bit 6 of result (0x40 has bit 6 set)");
}

static void test_ARR_binary_overflow_set(CPU *cpu)
{
    // V = bit6 XOR bit5, test case where they differ
    set_flag_decimal(cpu, false);
    set_flag_carry(cpu, false);
    cpu->A = 0x40;  // AND result will be 0x40, ROR = 0x20, bit6=0, bit5=1
    uint8_t instr[] = {0x6B, 0xFF}; // ARR #$FF
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // A = (0x40 & 0xFF) >> 1 | (0 << 7) = 0x20
    TEST_ASSERT_EQ(0x20, cpu->A, "ARR binary: 0x40 ROR with C=0 = 0x20");
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "ARR binary: C = bit 6 of 0x20 = 0");
    TEST_ASSERT_EQ(true, get_flag_overflow(cpu), "ARR binary: V = bit6(0) XOR bit5(1) = 1");
}

static void test_ARR_binary_zero_result(CPU *cpu)
{
    set_flag_decimal(cpu, false);
    set_flag_carry(cpu, false);
    cpu->A = 0x01;
    uint8_t instr[] = {0x6B, 0x01}; // ARR #$01
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // A = (0x01 & 0x01) >> 1 | 0 = 0x00
    TEST_ASSERT_EQ(0x00, cpu->A, "ARR binary: 0x01 AND 0x01 ROR = 0x00");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "ARR binary: Z set when result is 0");
}

static void test_ARR_decimal_basic(CPU *cpu)
{
    // ARR in decimal mode with no BCD fixup needed
    set_flag_decimal(cpu, true);
    set_flag_carry(cpu, true);
    cpu->A = 0x22;
    uint8_t instr[] = {0x6B, 0xFF}; // ARR #$FF
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // AND = 0x22, ROR = (0x22 >> 1) | 0x80 = 0x11 | 0x80 = 0x91
    // AL = 2, AH = 2, neither (nybble + (nybble & 1)) > 5
    TEST_ASSERT_EQ(0x91, cpu->A, "ARR decimal: 0x22 ROR with C=1, no fixup = 0x91");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "ARR decimal: N = initial carry (1)");
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "ARR decimal: C cleared when high nybble fixup not needed");
}

static void test_ARR_decimal_low_nybble_fixup(CPU *cpu)
{
    // Low nybble BCD fixup: AL + (AL & 1) > 5 means AL >= 5
    set_flag_decimal(cpu, true);
    set_flag_carry(cpu, false);
    cpu->A = 0x0A;  // Low nybble = 0xA, (0xA + 0) = 10 > 5
    uint8_t instr[] = {0x6B, 0xFF}; // ARR #$FF
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // AND = 0x0A, ROR = 0x05
    // AL = 0xA, AL + (AL & 1) = 10 + 0 = 10 > 5, so add 6 to low nybble
    // Result = 0x05 -> (0x05 + 6) & 0x0F = 0x0B
    TEST_ASSERT_EQ(0x0B, cpu->A, "ARR decimal: low nybble BCD fixup adds 6");
}

static void test_ARR_decimal_high_nybble_fixup(CPU *cpu)
{
    // High nybble BCD fixup: AH + (AH & 1) > 5
    set_flag_decimal(cpu, true);
    set_flag_carry(cpu, false);
    cpu->A = 0xA0;  // High nybble = 0xA
    uint8_t instr[] = {0x6B, 0xFF}; // ARR #$FF
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // AND = 0xA0, ROR = 0x50
    // AH = 0xA, AH + (AH & 1) = 10 + 0 = 10 > 5, so add 0x60 and set carry
    // Result = 0x50 + 0x60 = 0xB0
    TEST_ASSERT_EQ(0xB0, cpu->A, "ARR decimal: high nybble BCD fixup adds 0x60");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ARR decimal: C set when high nybble fixup applied");
}

static void test_ARR_decimal_v_flag(CPU *cpu)
{
    // V flag = bit 6 changed between AND and ROR
    set_flag_decimal(cpu, true);
    set_flag_carry(cpu, true);  // This will be rotated into bit 7
    cpu->A = 0x40;  // bit 6 = 1 in AND result
    uint8_t instr[] = {0x6B, 0xFF}; // ARR #$FF
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // AND = 0x40 (bit 6 = 1), ROR = 0xA0 (bit 6 = 0)
    // V = (0x40 ^ 0xA0) & 0x40 = 0xE0 & 0x40 = 0x40 (set)
    TEST_ASSERT_EQ(true, get_flag_overflow(cpu), "ARR decimal: V set when bit 6 changes");
}

static void test_SBX(CPU *cpu)
{
    // SBX: X = (A & X) - immediate (carry ignored, sets carry like CMP)
    cpu->A = 0xFF;
    cpu->X = 0x0F;
    uint8_t instr[] = {0xCB, 0x05}; // SBX #$05
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x0A, cpu->X, "SBX: X = (A & X) - imm = (0xFF & 0x0F) - 0x05 = 0x0A");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "SBX sets carry when no borrow");
}

// ============================================================================
// ANE/XAA - Unstable opcode $8B (per 64doc.txt)
// A = (A | #$EE) & X & #byte
// ============================================================================

static void test_ANE_basic(CPU *cpu)
{
    // ANE: A = (A | $EE) & X & imm
    cpu->A = 0x00;
    cpu->X = 0xFF;
    uint8_t instr[] = {0x8B, 0xFF}; // ANE #$FF
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // A = (0x00 | 0xEE) & 0xFF & 0xFF = 0xEE
    TEST_ASSERT_EQ(0xEE, cpu->A, "ANE: (0x00 | 0xEE) & 0xFF & 0xFF = 0xEE");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "ANE sets negative flag");
}

static void test_ANE_x_masks(CPU *cpu)
{
    // X register masks the result
    cpu->A = 0xFF;
    cpu->X = 0x0F;
    uint8_t instr[] = {0x8B, 0xFF}; // ANE #$FF
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // A = (0xFF | 0xEE) & 0x0F & 0xFF = 0xFF & 0x0F = 0x0F
    TEST_ASSERT_EQ(0x0F, cpu->A, "ANE: X masks result to low nibble");
}

static void test_ANE_imm_masks(CPU *cpu)
{
    // Immediate value masks the result
    cpu->A = 0xFF;
    cpu->X = 0xFF;
    uint8_t instr[] = {0x8B, 0x11}; // ANE #$11
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // A = (0xFF | 0xEE) & 0xFF & 0x11 = 0xFF & 0x11 = 0x11
    TEST_ASSERT_EQ(0x11, cpu->A, "ANE: immediate masks result");
}

static void test_ANE_a_contributes_low_bits(CPU *cpu)
{
    // Only bits NOT set in $EE (bits 0 and 4) come from A
    // $EE = 11101110, so bits 0 and 4 are 0 in the OR mask
    cpu->A = 0x11;  // bits 0 and 4 set
    cpu->X = 0xFF;
    uint8_t instr[] = {0x8B, 0xFF}; // ANE #$FF
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // A = (0x11 | 0xEE) & 0xFF & 0xFF = 0xFF
    TEST_ASSERT_EQ(0xFF, cpu->A, "ANE: A bits 0,4 combine with 0xEE to make 0xFF");
}

static void test_ANE_zero_result(CPU *cpu)
{
    cpu->A = 0x00;
    cpu->X = 0x00;
    uint8_t instr[] = {0x8B, 0xFF}; // ANE #$FF
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // A = (0x00 | 0xEE) & 0x00 & 0xFF = 0xEE & 0x00 = 0x00
    TEST_ASSERT_EQ(0x00, cpu->A, "ANE: X=0 produces zero result");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "ANE sets zero flag");
}

// ============================================================================
// LXA - Unstable opcode $AB (per 64doc.txt)
// A = X = (A | #$EE) & #byte
// ============================================================================

static void test_LXA_basic(CPU *cpu)
{
    // LXA: A = X = (A | $EE) & imm
    cpu->A = 0x00;
    uint8_t instr[] = {0xAB, 0xFF}; // LXA #$FF
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // A = X = (0x00 | 0xEE) & 0xFF = 0xEE
    TEST_ASSERT_EQ(0xEE, cpu->A, "LXA: (0x00 | 0xEE) & 0xFF = 0xEE");
    TEST_ASSERT_EQ(0xEE, cpu->X, "LXA: X = A");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "LXA sets negative flag");
}

static void test_LXA_imm_masks(CPU *cpu)
{
    // Immediate value masks the result
    cpu->A = 0xFF;
    uint8_t instr[] = {0xAB, 0x11}; // LXA #$11
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // A = X = (0xFF | 0xEE) & 0x11 = 0xFF & 0x11 = 0x11
    TEST_ASSERT_EQ(0x11, cpu->A, "LXA: immediate masks result");
    TEST_ASSERT_EQ(0x11, cpu->X, "LXA: X = A");
}

static void test_LXA_a_contributes_low_bits(CPU *cpu)
{
    // Only bits NOT set in $EE (bits 0 and 4) come from A
    cpu->A = 0x11;  // bits 0 and 4 set
    uint8_t instr[] = {0xAB, 0xFF}; // LXA #$FF
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // A = X = (0x11 | 0xEE) & 0xFF = 0xFF
    TEST_ASSERT_EQ(0xFF, cpu->A, "LXA: A bits 0,4 combine with 0xEE");
    TEST_ASSERT_EQ(0xFF, cpu->X, "LXA: X = A");
}

static void test_LXA_zero_result(CPU *cpu)
{
    cpu->A = 0x00;
    uint8_t instr[] = {0xAB, 0x00}; // LXA #$00
    write_instruction(cpu, 0x1000, instr, 2);
    cpu_step(cpu);
    // A = X = (0x00 | 0xEE) & 0x00 = 0x00
    TEST_ASSERT_EQ(0x00, cpu->A, "LXA: imm=0 produces zero result");
    TEST_ASSERT_EQ(0x00, cpu->X, "LXA: X = A");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "LXA sets zero flag");
}

// Test array for all opcodes
static const test_case_t opcode_tests[] = {
    // ADC tests
    {0x69, "ADC", test_ADC_imm},
    {0x69, "ADC", test_ADC_imm_with_carry},
    {0x69, "ADC", test_ADC_zero_result},
    {0x69, "ADC", test_ADC_overflow_positive},
    {0x69, "ADC", test_ADC_overflow_negative},
    {0x69, "ADC", test_ADC_no_overflow},
    {0x69, "ADC", test_ADC_decimal_simple},
    {0x69, "ADC", test_ADC_decimal_carry},
    {0x69, "ADC", test_ADC_decimal_with_carry_in},
    {0x69, "ADC", test_ADC_decimal_zero_flag},
    {0x65, "ADC", test_ADC_zp},
    {0x75, "ADC", test_ADC_zpx},
    {0x75, "ADC", test_ADC_zpx_wrap},
    {0x6D, "ADC", test_ADC_abs},
    {0x7D, "ADC", test_ADC_absx},
    {0x79, "ADC", test_ADC_absy},
    {0x61, "ADC", test_ADC_indx},
    {0x71, "ADC", test_ADC_indy},
    
    // SBC tests
    {0xE9, "SBC", test_SBC_imm},
    {0xE9, "SBC", test_SBC_with_borrow},
    {0xE9, "SBC", test_SBC_overflow_positive},
    {0xE9, "SBC", test_SBC_overflow_negative},
    {0xE9, "SBC", test_SBC_zero_result},
    {0xE9, "SBC", test_SBC_negative_result},
    {0xE9, "SBC", test_SBC_decimal_simple},
    {0xE9, "SBC", test_SBC_decimal_borrow},
    {0xE9, "SBC", test_SBC_decimal_underflow},
    
    // Logical tests
    {0x29, "AND", test_AND_imm},
    {0x29, "AND", test_AND_zero_result},
    {0x29, "AND", test_AND_negative_result},
    {0x09, "ORA", test_ORA_imm},
    {0x09, "ORA", test_ORA_zero_operand},
    {0x49, "EOR", test_EOR_imm},
    {0x49, "EOR", test_EOR_same_value},
    
    // Shift tests
    {0x0A, "ASL", test_ASL_acc},
    {0x0A, "ASL", test_ASL_acc_no_carry},
    {0x06, "ASL", test_ASL_zp},
    {0x0A, "ASL", test_ASL_multiple_shifts},
    {0x4A, "LSR", test_LSR_acc},
    {0x4A, "LSR", test_LSR_acc_no_carry},
    {0x46, "LSR", test_LSR_zp},
    {0x2A, "ROL", test_ROL_acc_with_carry},
    {0x2A, "ROL", test_ROL_acc_without_carry},
    {0x2A, "ROL", test_ROL_preserves_bit_pattern},
    {0x6A, "ROR", test_ROR_acc_with_carry},
    {0x6A, "ROR", test_ROR_acc_without_carry},
    
    // Branch tests
    {0x90, "BCC", test_BCC_taken},
    {0x90, "BCC", test_BCC_not_taken},
    {0xB0, "BCS", test_BCS_taken},
    {0xF0, "BEQ", test_BEQ_taken},
    {0xF0, "BEQ", test_BEQ_not_taken},
    {0xD0, "BNE", test_BNE_taken},
    {0x30, "BMI", test_BMI_taken},
    {0x10, "BPL", test_BPL_taken},
    {0x50, "BVC", test_BVC_taken},
    {0x70, "BVS", test_BVS_taken},
    {0x90, "BCC", test_branch_backward},
    {0x90, "BCC", test_branch_page_crossing},
    {0xF0, "BEQ", test_branch_backward_page_crossing},
    
    // BIT tests
    {0x24, "BIT", test_BIT_zp},
    {0x24, "BIT", test_BIT_zero_result},
    {0x24, "BIT", test_BIT_flags_from_memory},
    {0x2C, "BIT", test_BIT_abs},
    
    // Compare tests
    {0xC9, "CMP", test_CMP_imm_equal},
    {0xC9, "CMP", test_CMP_greater},
    {0xC9, "CMP", test_CMP_less},
    {0xC9, "CMP", test_CMP_unsigned},
    {0xE0, "CPX", test_CPX_imm},
    {0xC0, "CPY", test_CPY_imm},
    
    // Inc/Dec tests
    {0xCA, "DEX", test_DEX},
    {0xCA, "DEX", test_DEX_wrap},
    {0x88, "DEY", test_DEY},
    {0xE8, "INX", test_INX},
    {0xE8, "INX", test_INX_wrap},
    {0xC8, "INY", test_INY},
    {0xE6, "INC", test_INC_zp},
    {0xC6, "DEC", test_DEC_zp},
    
    // JMP tests
    {0x4C, "JMP", test_JMP_abs},
    {0x6C, "JMP", test_JMP_ind},
    {0x6C, "JMP", test_JMP_ind_page_boundary_bug},
    {0x6C, "JMP", test_JMP_ind_no_bug_when_not_at_boundary},
    
    // JSR/RTS tests
    {0x20, "JSR", test_JSR},
    {0x20, "JSR", test_JSR_RTS_round_trip},
    {0x60, "RTS", test_RTS},
    
    // Load tests
    {0xA9, "LDA", test_LDA_imm},
    {0xA9, "LDA", test_LDA_zero},
    {0xA9, "LDA", test_LDA_negative},
    {0xA2, "LDX", test_LDX_imm},
    {0xA0, "LDY", test_LDY_imm},
    
    // Store tests
    {0x85, "STA", test_STA_zp},
    {0x86, "STX", test_STX_zp},
    {0x84, "STY", test_STY_zp},
    
    // NOP
    {0xEA, "NOP", test_NOP},
    
    // Stack tests
    {0x48, "PHA", test_PHA_PLA},
    {0x08, "PHP", test_PHP_pushes_break_flag},
    {0x28, "PLP", test_PLP_clears_break_flag},
    {0x28, "PLP", test_PLP_sets_reserved_flag},
    
    // RTI
    {0x40, "RTI", test_RTI},
    {0x40, "RTI", test_RTI_clears_break_flag},
    
    // Transfer tests
    {0xAA, "TAX", test_TAX_TAY},
    {0x8A, "TXA", test_TXA},
    {0x98, "TYA", test_TYA},
    {0x9A, "TXS", test_TXS_no_flags},
    {0xBA, "TSX", test_TSX},
    {0xBA, "TSX", test_TSX_TXS},
    
    // Flag tests
    {0x18, "CLC", test_CLC},
    {0x38, "SEC", test_SEC},
    {0xD8, "CLD", test_CLD},
    {0xF8, "SED", test_SED},
    {0x58, "CLI", test_CLI},
    {0x78, "SEI", test_SEI},
    {0xB8, "CLV", test_CLV},
    
    // Zero page wrapping tests
    {0xB5, "LDA", test_zp_indexed_wrapping},
    {0xB6, "LDX", test_zpy_indexed_wrapping},
    {0xA1, "LDA", test_indexed_indirect_wrapping},
    {0xB1, "LDA", test_indirect_indexed_zp_wrapping},
    
    // Interrupt tests
    {0x00, "BRK", test_BRK},
    {0x00, "IRQ", test_IRQ},
    {0x00, "NMI", test_NMI},
    
    // Illegal opcode tests
    {0xA7, "LAX", test_LAX},
    {0x87, "SAX", test_SAX},
    {0x87, "SAX", test_SAX_nonzero},
    {0xC7, "DCP", test_DCP},
    {0xE7, "ISC", test_ISC},
    {0x07, "SLO", test_SLO},
    {0x27, "RLA", test_RLA},
    {0x47, "SRE", test_SRE},
    {0x67, "RRA", test_RRA},
    {0x0B, "ANC", test_ANC},
    {0x4B, "ALR", test_ALR},
    {0x6B, "ARR", test_ARR_binary_basic},
    {0x6B, "ARR", test_ARR_binary_carry_from_bit6},
    {0x6B, "ARR", test_ARR_binary_overflow_set},
    {0x6B, "ARR", test_ARR_binary_zero_result},
    {0x6B, "ARR", test_ARR_decimal_basic},
    {0x6B, "ARR", test_ARR_decimal_low_nybble_fixup},
    {0x6B, "ARR", test_ARR_decimal_high_nybble_fixup},
    {0x6B, "ARR", test_ARR_decimal_v_flag},
    {0x8B, "ANE", test_ANE_basic},
    {0x8B, "ANE", test_ANE_x_masks},
    {0x8B, "ANE", test_ANE_imm_masks},
    {0x8B, "ANE", test_ANE_a_contributes_low_bits},
    {0x8B, "ANE", test_ANE_zero_result},
    {0xAB, "LXA", test_LXA_basic},
    {0xAB, "LXA", test_LXA_imm_masks},
    {0xAB, "LXA", test_LXA_a_contributes_low_bits},
    {0xAB, "LXA", test_LXA_zero_result},
    {0xCB, "SBX", test_SBX},
};

// Comprehensive addressing mode tests
static void test_addressing_modes()
{
    CPU *cpu = setup_cpu();

    // Test immediate addressing
    cpu->memory[0x1000] = 0xA9; // LDA immediate
    cpu->memory[0x1001] = 0x42;
    cpu_set_pc(cpu, 0x1000);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x42, cpu->A, "Immediate addressing loads correct value");

    // Test zero page addressing
    cpu->memory[0x1010] = 0xA5; // LDA zero page
    cpu->memory[0x1011] = 0x50;
    cpu->memory[0x0050] = 0x84;
    cpu_set_pc(cpu, 0x1010);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x84, cpu->A, "Zero page addressing loads correct value");

    // Test absolute addressing
    cpu->memory[0x1020] = 0xAD; // LDA absolute
    cpu->memory[0x1021] = 0x00;
    cpu->memory[0x1022] = 0x20;
    cpu->memory[0x2000] = 0x96;
    cpu_set_pc(cpu, 0x1020);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x96, cpu->A, "Absolute addressing loads correct value");

    // Test indexed addressing
    cpu->X = 0x10;
    cpu->memory[0x1030] = 0xB5; // LDA zero page, X
    cpu->memory[0x1031] = 0x40;
    cpu->memory[0x0050] = 0x31;
    cpu_set_pc(cpu, 0x1030);
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x31, cpu->A, "Zero page X addressing loads correct value");

    teardown_cpu(cpu);
}

// Stack tests
static void test_stack_operations()
{
    CPU *cpu = setup_cpu();

    // Test stack push/pull cycle
    cpu->SP = 0xFF;
    for (int i = 0; i < 10; i++)
    {
        cpu->A = i + 0x30;
        cpu->memory[cpu_get_pc(cpu)] = 0x48; // PHA
        cpu_step(cpu);
    }

    TEST_ASSERT_EQ(0xF5, cpu->SP, "Stack pointer decremented correctly");

    for (int i = 9; i >= 0; i--)
    {
        cpu->memory[cpu_get_pc(cpu)] = 0x68; // PLA
        cpu_step(cpu);
        TEST_ASSERT_EQ(i + 0x30, cpu->A, "Stack pull returns correct value");
    }

    TEST_ASSERT_EQ(0xFF, cpu->SP, "Stack pointer restored correctly");

    teardown_cpu(cpu);
}

// Stack wrap tests
static void test_stack_wrap()
{
    CPU *cpu = setup_cpu();
    
    // Test stack wrapping from 0x00 to 0xFF on pull
    cpu->SP = 0x00;
    cpu->memory[0x0101] = 0x42;
    cpu->memory[cpu_get_pc(cpu)] = 0x68; // PLA
    cpu_step(cpu);
    TEST_ASSERT_EQ(0x42, cpu->A, "PLA wraps stack from 0x00 to read from 0x0101");
    TEST_ASSERT_EQ(0x01, cpu->SP, "Stack pointer wraps correctly");
    
    teardown_cpu(cpu);
}

// Flag test comprehensive
static void test_flag_operations()
{
    CPU *cpu = setup_cpu();

    // Test all flag bits
    set_flag_carry(cpu, true);
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "Set carry flag");

    set_flag_zero(cpu, true);
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "Set zero flag");

    set_flag_interrupt(cpu, true);
    TEST_ASSERT_EQ(true, get_flag_interrupt(cpu), "Set interrupt disable flag");

    set_flag_decimal(cpu, true);
    TEST_ASSERT_EQ(true, get_flag_decimal(cpu), "Set decimal flag");

    set_flag_break(cpu, true);
    TEST_ASSERT_EQ(true, get_flag_break(cpu), "Set break flag");

    set_flag_overflow(cpu, true);
    TEST_ASSERT_EQ(true, get_flag_overflow(cpu), "Set overflow flag");

    set_flag_negative(cpu, true);
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "Set negative flag");

    // Test clearing flags
    set_flag_carry(cpu, false);
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "Clear carry flag");

    teardown_cpu(cpu);
}

// Run all tests
int main()
{
    printf("=== MOS 6510 Opcode Test Suite ===\n\n");

    // Test flag operations
    printf("Testing Flag Operations:\n");
    test_flag_operations();
    printf("\n");

    // Test addressing modes
    printf("Testing Addressing Modes:\n");
    test_addressing_modes();
    printf("\n");

    // Test stack operations
    printf("Testing Stack Operations:\n");
    test_stack_operations();
    test_stack_wrap();
    printf("\n");

    // Test individual opcodes
    printf("Testing Individual Opcodes:\n");
    for (size_t i = 0; i < sizeof(opcode_tests) / sizeof(opcode_tests[0]); i++)
    {
        CPU *cpu = setup_cpu();
        opcode_tests[i].test_func(cpu);
        teardown_cpu(cpu);
    }
    printf("\n");

    // Test comprehensive opcode coverage
    printf("Testing All 256 Opcodes:\n");
    CPU *cpu = setup_cpu();
    for (int opcode = 0; opcode < 256; opcode++)
    {
        cpu->memory[0x1000] = opcode;
        cpu_set_pc(cpu, 0x1000);

        // Just make sure it doesn't crash
        uint8_t cycles = cpu_step(cpu);
        TEST_ASSERT(cycles > 0, "Opcode executes without crashing");
    }
    teardown_cpu(cpu);

    // Print results
    printf("\n=== Test Results ===\n");
    printf("Total tests: %d\n", total_tests);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", (float)tests_passed / total_tests * 100);

    if (tests_failed == 0)
    {
        printf("🎉 All tests passed!\n");
        return 0;
    }
    else
    {
        printf("❌ Some tests failed.\n");
        return 1;
    }
}