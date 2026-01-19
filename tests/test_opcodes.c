#include "../include/mos6510.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test framework
static int tests_passed = 0;
static int tests_failed = 0;
static int total_tests = 0;

#define TEST_ASSERT(condition, message) do { \
    total_tests++; \
    if (condition) { \
        tests_passed++; \
        printf("✓ %s\n", message); \
    } else { \
        tests_failed++; \
        printf("✗ %s\n", message); \
    } \
} while(0)

#define TEST_ASSERT_EQ(expected, actual, message) TEST_ASSERT((expected) == (actual), message)

// Helper functions
static MOS6510* setup_cpu() {
    MOS6510* cpu = malloc(sizeof(MOS6510));
    mos6510_init(cpu, false);
    return cpu;
}

static void teardown_cpu(MOS6510* cpu) {
    free(cpu);
}

static void write_instruction(MOS6510* cpu, uint16_t addr, const uint8_t* bytes, int len) {
    for (int i = 0; i < len; i++) {
        cpu->memory[addr + i] = bytes[i];
    }
    mos6510_set_pc(cpu, addr);
}

// Test data structures
typedef struct {
    uint8_t opcode;
    char name[4];
    void (*test_func)(MOS6510*);
} test_case_t;

// Individual opcode tests
static void test_ADC_imm(MOS6510* cpu) {
    cpu->A = 0x50;
    uint8_t instr[] = {0x69, 0x30};  // ADC #$30
    write_instruction(cpu, 0x1000, instr, 2);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->A, "ADC immediate sets accumulator");
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "ADC immediate clears carry when no overflow");
}

static void test_ADC_imm_with_carry(MOS6510* cpu) {
    set_flag_carry(cpu, true);
    cpu->A = 0xFF;
    uint8_t instr[] = {0x69, 0x01};  // ADC #$01
    write_instruction(cpu, 0x1000, instr, 2);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x01, cpu->A, "ADC immediate with carry wraps around");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ADC immediate sets carry on overflow");
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "ADC immediate clears zero flag");
    TEST_ASSERT_EQ(false, get_flag_negative(cpu), "ADC immediate clears negative flag");
}

static void test_AND_imm(MOS6510* cpu) {
    cpu->A = 0xFF;
    uint8_t instr[] = {0x29, 0x0F};  // AND #$0F
    write_instruction(cpu, 0x1000, instr, 2);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x0F, cpu->A, "AND immediate performs bitwise AND");
    TEST_ASSERT_EQ(false, get_flag_negative(cpu), "AND immediate clears negative flag");
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "AND immediate clears zero flag");
}

static void test_ASL_acc(MOS6510* cpu) {
    cpu->A = 0x80;
    uint8_t instr[] = {0x0A};  // ASL A
    write_instruction(cpu, 0x1000, instr, 1);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "ASL accumulator shifts left");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "ASL accumulator sets carry on bit 7");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "ASL accumulator sets zero flag");
}

static void test_ASL_zp(MOS6510* cpu) {
    cpu->memory[0x0010] = 0x01;
    uint8_t instr[] = {0x06, 0x10};  // ASL $10
    write_instruction(cpu, 0x1000, instr, 2);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x02, cpu->memory[0x0010], "ASL zero page shifts memory left");
    TEST_ASSERT_EQ(false, get_flag_carry(cpu), "ASL zero page clears carry");
}

static void test_BCC_taken(MOS6510* cpu) {
    set_flag_carry(cpu, false);
    uint8_t instr[] = {0x90, 0x10};  // BCC $10
    write_instruction(cpu, 0x1000, instr, 2);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x1012, mos6510_get_pc(cpu), "BCC taken when carry clear");
}

static void test_BCC_not_taken(MOS6510* cpu) {
    set_flag_carry(cpu, true);
    uint8_t instr[] = {0x90, 0x10};  // BCC $10
    write_instruction(cpu, 0x1000, instr, 2);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x1002, mos6510_get_pc(cpu), "BCC not taken when carry set");
}

static void test_BEQ_taken(MOS6510* cpu) {
    set_flag_zero(cpu, true);
    uint8_t instr[] = {0xF0, 0x08};  // BEQ $08
    write_instruction(cpu, 0x1000, instr, 2);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x100A, mos6510_get_pc(cpu), "BEQ taken when zero set");
}

static void test_BIT_zp(MOS6510* cpu) {
    cpu->A = 0x40;
    cpu->memory[0x0020] = 0xC0;
    uint8_t instr[] = {0x24, 0x20};  // BIT $20
    write_instruction(cpu, 0x1000, instr, 2);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "BIT sets zero flag based on AND result");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "BIT copies bit 7 to negative flag");
    TEST_ASSERT_EQ(true, get_flag_overflow(cpu), "BIT copies bit 6 to overflow flag");
}

static void test_CMP_imm_equal(MOS6510* cpu) {
    cpu->A = 0x50;
    uint8_t instr[] = {0xC9, 0x50};  // CMP #$50
    write_instruction(cpu, 0x1000, instr, 2);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "CMP sets carry when A >= operand");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "CMP sets zero when A == operand");
    TEST_ASSERT_EQ(false, get_flag_negative(cpu), "CMP clears negative when result >= 0");
}

static void test_DEX(MOS6510* cpu) {
    cpu->X = 0x01;
    uint8_t instr[] = {0xCA};  // DEX
    write_instruction(cpu, 0x1000, instr, 1);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->X, "DEX decrements X register");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "DEX sets zero flag");
}

static void test_INX(MOS6510* cpu) {
    cpu->X = 0x7F;
    uint8_t instr[] = {0xE8};  // INX
    write_instruction(cpu, 0x1000, instr, 1);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->X, "INX increments X register");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "INX sets negative flag");
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "INX clears zero flag");
}

static void test_JMP_abs(MOS6510* cpu) {
    uint8_t instr[] = {0x4C, 0x00, 0x20};  // JMP $2000
    write_instruction(cpu, 0x1000, instr, 3);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x2000, mos6510_get_pc(cpu), "JMP absolute jumps to address");
}

static void test_JSR(MOS6510* cpu) {
    cpu->SP = 0xFF;
    uint8_t instr[] = {0x20, 0x34, 0x12};  // JSR $1234
    write_instruction(cpu, 0x1000, instr, 3);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x1234, mos6510_get_pc(cpu), "JSR jumps to subroutine");
    TEST_ASSERT_EQ(0xFD, cpu->SP, "JSR pushes return address to stack");
    TEST_ASSERT_EQ(0x10, cpu->memory[0x01FF], "JSR pushes high byte of return address");
    TEST_ASSERT_EQ(0x02, cpu->memory[0x01FE], "JSR pushes low byte of return address");
}

static void test_LDA_imm(MOS6510* cpu) {
    uint8_t instr[] = {0xA9, 0x42};  // LDA #$42
    write_instruction(cpu, 0x1000, instr, 2);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x42, cpu->A, "LDA immediate loads accumulator");
    TEST_ASSERT_EQ(false, get_flag_negative(cpu), "LDA immediate clears negative flag");
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "LDA immediate clears zero flag");
}

static void test_LDX_imm(MOS6510* cpu) {
    uint8_t instr[] = {0xA2, 0x80};  // LDX #$80
    write_instruction(cpu, 0x1000, instr, 2);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->X, "LDX immediate loads X register");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "LDX immediate sets negative flag");
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "LDX immediate clears zero flag");
}

static void test_LDY_imm(MOS6510* cpu) {
    uint8_t instr[] = {0xA0, 0x00};  // LDY #$00
    write_instruction(cpu, 0x1000, instr, 2);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->Y, "LDY immediate loads Y register");
    TEST_ASSERT_EQ(false, get_flag_negative(cpu), "LDY immediate clears negative flag");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "LDY immediate sets zero flag");
}

static void test_LSR_acc(MOS6510* cpu) {
    cpu->A = 0x01;
    uint8_t instr[] = {0x4A};  // LSR A
    write_instruction(cpu, 0x1000, instr, 1);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x00, cpu->A, "LSR accumulator shifts right");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "LSR accumulator sets carry on bit 0");
    TEST_ASSERT_EQ(true, get_flag_zero(cpu), "LSR accumulator sets zero flag");
}

static void test_NOP(MOS6510* cpu) {
    uint16_t pc = 0x1000;
    mos6510_set_pc(cpu, pc);
    uint8_t instr[] = {0xEA};  // NOP
    write_instruction(cpu, pc, instr, 1);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(pc + 1, mos6510_get_pc(cpu), "NOP increments PC");
}

static void test_ORA_imm(MOS6510* cpu) {
    cpu->A = 0x30;
    uint8_t instr[] = {0x09, 0x81};  // ORA #$81
    write_instruction(cpu, 0x1000, instr, 2);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0xB1, cpu->A, "ORA immediate performs bitwise OR");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "ORA immediate sets negative flag");
    TEST_ASSERT_EQ(false, get_flag_zero(cpu), "ORA immediate clears zero flag");
}

static void test_PHA_PLA(MOS6510* cpu) {
    cpu->A = 0x42;
    cpu->SP = 0xFF;
    
    // Test PHA
    uint8_t instr_pha[] = {0x48};  // PHA
    write_instruction(cpu, 0x1000, instr_pha, 1);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0xFE, cpu->SP, "PHA decrements stack pointer");
    TEST_ASSERT_EQ(0x42, cpu->memory[0x01FF], "PHA stores accumulator on stack");
    
    // Test PLA
    cpu->A = 0x00;
    uint8_t instr_pla[] = {0x68};  // PLA
    write_instruction(cpu, 0x1001, instr_pla, 1);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0xFF, cpu->SP, "PLA increments stack pointer");
    TEST_ASSERT_EQ(0x42, cpu->A, "PLA restores accumulator from stack");
}

static void test_RTS(MOS6510* cpu) {
    cpu->SP = 0xFD;
    cpu->memory[0x01FF] = 0x10;
    cpu->memory[0x01FE] = 0x02;
    
    uint8_t instr[] = {0x60};  // RTS
    write_instruction(cpu, 0x1000, instr, 1);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x1003, mos6510_get_pc(cpu), "RTS pulls return address and adds 1");
    TEST_ASSERT_EQ(0xFF, cpu->SP, "RTS increments stack pointer");
}

static void test_SBC_imm(MOS6510* cpu) {
    set_flag_carry(cpu, true);
    cpu->A = 0x50;
    uint8_t instr[] = {0xE9, 0x30};  // SBC #$30
    write_instruction(cpu, 0x1000, instr, 2);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x20, cpu->A, "SBC immediate performs subtraction");
    TEST_ASSERT_EQ(true, get_flag_carry(cpu), "SBC immediate sets carry when no borrow");
}

static void test_STA_zp(MOS6510* cpu) {
    cpu->A = 0x42;
    uint8_t instr[] = {0x85, 0x20};  // STA $20
    write_instruction(cpu, 0x1000, instr, 2);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x42, cpu->memory[0x0020], "STA zero page stores accumulator");
}

static void test_TAX_TAY(MOS6510* cpu) {
    cpu->A = 0x80;
    
    // Test TAX
    uint8_t instr_tax[] = {0xAA};  // TAX
    write_instruction(cpu, 0x1000, instr_tax, 1);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->X, "TAX transfers A to X");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "TAX sets negative flag");
    
    // Test TAY
    uint8_t instr_tay[] = {0xA8};  // TAY
    write_instruction(cpu, 0x1001, instr_tay, 1);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x80, cpu->Y, "TAY transfers A to Y");
    TEST_ASSERT_EQ(true, get_flag_negative(cpu), "TAY sets negative flag");
}

static void test_TSX_TXS(MOS6510* cpu) {
    cpu->SP = 0x42;
    
    // Test TSX
    uint8_t instr_tsx[] = {0xBA};  // TSX
    write_instruction(cpu, 0x1000, instr_tsx, 1);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x42, cpu->X, "TSX transfers SP to X");
    
    // Test TXS
    cpu->X = 0xFF;
    uint8_t instr_txs[] = {0x9A};  // TXS
    write_instruction(cpu, 0x1001, instr_txs, 1);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0xFF, cpu->SP, "TXS transfers X to SP");
}

// Test array for all opcodes
static const test_case_t opcode_tests[] = {
    {0x69, "ADC", test_ADC_imm},
    {0x69, "ADC", test_ADC_imm_with_carry},
    {0x29, "AND", test_AND_imm},
    {0x0A, "ASL", test_ASL_acc},
    {0x06, "ASL", test_ASL_zp},
    {0x90, "BCC", test_BCC_taken},
    {0x90, "BCC", test_BCC_not_taken},
    {0xF0, "BEQ", test_BEQ_taken},
    {0x24, "BIT", test_BIT_zp},
    {0xC9, "CMP", test_CMP_imm_equal},
    {0xCA, "DEX", test_DEX},
    {0xE8, "INX", test_INX},
    {0x4C, "JMP", test_JMP_abs},
    {0x20, "JSR", test_JSR},
    {0xA9, "LDA", test_LDA_imm},
    {0xA2, "LDX", test_LDX_imm},
    {0xA0, "LDY", test_LDY_imm},
    {0x4A, "LSR", test_LSR_acc},
    {0xEA, "NOP", test_NOP},
    {0x09, "ORA", test_ORA_imm},
    {0x48, "PHA", test_PHA_PLA},
    {0x68, "PLA", test_PHA_PLA},
    {0x60, "RTS", test_RTS},
    {0xE9, "SBC", test_SBC_imm},
    {0x85, "STA", test_STA_zp},
    {0xAA, "TAX", test_TAX_TAY},
    {0xA8, "TAY", test_TAX_TAY},
    {0xBA, "TSX", test_TSX_TXS},
    {0x9A, "TXS", test_TSX_TXS},
};

// Comprehensive addressing mode tests
static void test_addressing_modes() {
    MOS6510* cpu = setup_cpu();
    
    // Test immediate addressing
    cpu->memory[0x1000] = 0xA9;  // LDA immediate
    cpu->memory[0x1001] = 0x42;
    mos6510_set_pc(cpu, 0x1000);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x42, cpu->A, "Immediate addressing loads correct value");
    
    // Test zero page addressing
    cpu->memory[0x1010] = 0xA5;  // LDA zero page
    cpu->memory[0x1011] = 0x50;
    cpu->memory[0x0050] = 0x84;
    mos6510_set_pc(cpu, 0x1010);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x84, cpu->A, "Zero page addressing loads correct value");
    
    // Test absolute addressing
    cpu->memory[0x1020] = 0xAD;  // LDA absolute
    cpu->memory[0x1021] = 0x00;
    cpu->memory[0x1022] = 0x20;
    cpu->memory[0x2000] = 0x96;
    mos6510_set_pc(cpu, 0x1020);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x96, cpu->A, "Absolute addressing loads correct value");
    
    // Test indexed addressing
    cpu->X = 0x10;
    cpu->memory[0x1030] = 0xB5;  // LDA zero page, X
    cpu->memory[0x1031] = 0x40;
    cpu->memory[0x0050] = 0x31;
    mos6510_set_pc(cpu, 0x1030);
    mos6510_step(cpu);
    TEST_ASSERT_EQ(0x31, cpu->A, "Zero page X addressing loads correct value");
    
    teardown_cpu(cpu);
}

// Stack tests
static void test_stack_operations() {
    MOS6510* cpu = setup_cpu();
    
    // Test stack push/pull cycle
    cpu->SP = 0xFF;
    for (int i = 0; i < 10; i++) {
        cpu->A = i + 0x30;
        cpu->memory[mos6510_get_pc(cpu)] = 0x48;  // PHA
        mos6510_step(cpu);
    }
    
    TEST_ASSERT_EQ(0xF5, cpu->SP, "Stack pointer decremented correctly");
    
    for (int i = 9; i >= 0; i--) {
        cpu->memory[mos6510_get_pc(cpu)] = 0x68;  // PLA
        mos6510_step(cpu);
        TEST_ASSERT_EQ(i + 0x30, cpu->A, "Stack pull returns correct value");
    }
    
    TEST_ASSERT_EQ(0xFF, cpu->SP, "Stack pointer restored correctly");
    
    teardown_cpu(cpu);
}

// Flag test comprehensive
static void test_flag_operations() {
    MOS6510* cpu = setup_cpu();
    
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
int main() {
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
    printf("\n");
    
    // Test individual opcodes
    printf("Testing Individual Opcodes:\n");
    for (size_t i = 0; i < sizeof(opcode_tests) / sizeof(opcode_tests[0]); i++) {
        MOS6510* cpu = setup_cpu();
        opcode_tests[i].test_func(cpu);
        teardown_cpu(cpu);
    }
    printf("\n");
    
    // Test comprehensive opcode coverage
    printf("Testing All 256 Opcodes:\n");
    MOS6510* cpu = setup_cpu();
    for (int opcode = 0; opcode < 256; opcode++) {
        cpu->memory[0x1000] = opcode;
        mos6510_set_pc(cpu, 0x1000);
        
        // Just make sure it doesn't crash
        uint8_t cycles = mos6510_step(cpu);
        TEST_ASSERT(cycles > 0, "Opcode executes without crashing");
    }
    teardown_cpu(cpu);
    
    // Print results
    printf("\n=== Test Results ===\n");
    printf("Total tests: %d\n", total_tests);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", (float)tests_passed / total_tests * 100);
    
    if (tests_failed == 0) {
        printf("🎉 All tests passed!\n");
        return 0;
    } else {
        printf("❌ Some tests failed.\n");
        return 1;
    }
}