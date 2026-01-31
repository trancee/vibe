#include "test_suite.h"
#include "cpu.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>

// CPU instruction test implementation
bool test_cpu_adc_immediate(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test ADC #$40 + #$30 = #$70 (no carry)
    sys.mem.ram[0x1000] = 0x69; // ADC #$40
    sys.mem.ram[0x1001] = 0x40;
    sys.mem.ram[0x1002] = 0x69; // ADC #$30
    sys.mem.ram[0x1003] = 0x30;
    
    sys.cpu.pc = 0x1000;
    sys.cpu.a = 0x00;
    sys.cpu.status &= ~FLAG_C; // Clear carry
    
    cpu_step(&sys); // ADC #$40
    TEST_ASSERT_EQ(0x40, sys.cpu.a, "A should be $40 after first ADC");
    TEST_ASSERT(!(sys.cpu.status & FLAG_C), "Carry should be clear");
    TEST_ASSERT(!(sys.cpu.status & FLAG_Z), "Zero should be clear");
    TEST_ASSERT(!(sys.cpu.status & FLAG_N), "Negative should be clear");
    TEST_ASSERT(!(sys.cpu.status & FLAG_V), "Overflow should be clear");
    
    cpu_step(&sys); // ADC #$30
    TEST_ASSERT_EQ(0x70, sys.cpu.a, "A should be $70 after second ADC");
    TEST_ASSERT(!(sys.cpu.status & FLAG_C), "Carry should be clear");
    TEST_ASSERT(!(sys.cpu.status & FLAG_Z), "Zero should be clear");
    TEST_ASSERT(!(sys.cpu.status & FLAG_N), "Negative should be clear");
    TEST_ASSERT(!(sys.cpu.status & FLAG_V), "Overflow should be clear");
    
    c64_test_cleanup(&sys);
    TEST_PASS("ADC immediate instruction test passed");
}

bool test_cpu_branch_timing(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test BNE taken (no page cross)
    u64 start_cycles = sys.mem.cycle_count;
    
    sys.mem.ram[0x1000] = 0xD0; // BNE
    sys.mem.ram[0x1001] = 0x04; // +4
    
    sys.cpu.pc = 0x1000;
    sys.cpu.status &= ~FLAG_Z; // Clear zero flag so branch is taken
    
    cpu_step(&sys);
    
    u64 end_cycles = sys.mem.cycle_count;
    u64 elapsed = end_cycles - start_cycles;
    
    TEST_ASSERT_EQ(0x1006, sys.cpu.pc, "PC should be $1006 after taken branch");
    TEST_ASSERT_EQ(3, elapsed, "Taken branch should take 3 cycles");
    
    // Test BNE not taken
    start_cycles = sys.mem.cycle_count;
    sys.cpu.pc = 0x1000;
    sys.cpu.status |= FLAG_Z; // Set zero flag so branch is not taken
    
    cpu_step(&sys);
    
    end_cycles = sys.mem.cycle_count;
    elapsed = end_cycles - start_cycles;
    
    TEST_ASSERT_EQ(0x1002, sys.cpu.pc, "PC should be $1002 after not taken branch");
    TEST_ASSERT_EQ(2, elapsed, "Not taken branch should take 2 cycles");
    
    c64_test_cleanup(&sys);
    TEST_PASS("Branch timing test passed");
}

bool test_cpu_page_cross_timing(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test LDA absolute X with page cross
    sys.mem.ram[0x10FF] = 0xBD; // LDA $1000,X
    sys.mem.ram[0x1100] = 0x00;
    sys.mem.ram[0x1101] = 0x10;
    sys.mem.ram[0x1001] = 0x42; // Target value at correct page crossed address
    
    u64 start_cycles = sys.mem.cycle_count;
    
    sys.cpu.pc = 0x10FF;
    sys.cpu.x = 0x01; // Will cause page cross from $1001 to $2000
    
    cpu_step(&sys);
    
    u64 end_cycles = sys.mem.cycle_count;
    u64 elapsed = end_cycles - start_cycles;
    
    TEST_ASSERT_EQ(0x42, sys.cpu.a, "A should load value from page crossed address");
    TEST_ASSERT_EQ(5, elapsed, "LDA abs,X with page cross should take 5 cycles");
    
    // Test LDA absolute X without page cross
    start_cycles = sys.mem.cycle_count;
    
    sys.mem.ram[0x1200] = 0xBD; // LDA $1000,X
    sys.mem.ram[0x1201] = 0x00;
    sys.mem.ram[0x1202] = 0x10;
    sys.mem.ram[0x10F0] = 0x24; // Target value
    
    sys.cpu.pc = 0x1200;
    sys.cpu.x = 0xF0; // No page cross from $10F0 to $10F0
    
    cpu_step(&sys);
    
    end_cycles = sys.mem.cycle_count;
    elapsed = end_cycles - start_cycles;
    
    TEST_ASSERT_EQ(0x24, sys.cpu.a, "A should load value from non-page crossed address");
    TEST_ASSERT_EQ(4, elapsed, "LDA abs,X without page cross should take 4 cycles");
    
    c64_test_cleanup(&sys);
    TEST_PASS("Page cross timing test passed");
}

bool test_cpu_stack_operations(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Initialize stack pointer to middle of stack
    sys.cpu.sp = 0xFF;
    
    // Test PHA/PLA
    sys.cpu.a = 0x42;
    
    sys.mem.ram[0x1000] = 0x48; // PHA
    sys.cpu.pc = 0x1000;
    
    cpu_step(&sys);
    TEST_ASSERT_EQ(0xFE, sys.cpu.sp, "SP should decrement after PHA");
    TEST_ASSERT_EQ(0x42, sys.mem.ram[0x01FF], "Value should be pushed to stack");
    
    sys.mem.ram[0x1001] = 0x68; // PLA
    sys.cpu.pc = 0x1001;
    sys.cpu.a = 0x00;
    
    cpu_step(&sys);
    TEST_ASSERT_EQ(0xFF, sys.cpu.sp, "SP should increment after PLA");
    TEST_ASSERT_EQ(0x42, sys.cpu.a, "Value should be pulled from stack");
    
    // Test JSR/RTS
    sys.mem.ram[0x1000] = 0x20; // JSR $2000
    sys.mem.ram[0x1001] = 0x00;
    sys.mem.ram[0x1002] = 0x20;
    sys.mem.ram[0x2000] = 0x60; // RTS
    
    sys.cpu.pc = 0x1000;
    u64 start_cycles = sys.mem.cycle_count;
    
    cpu_step(&sys); // JSR
    TEST_ASSERT_EQ(0x2000, sys.cpu.pc, "PC should jump to subroutine");
    TEST_ASSERT_EQ(0xFD, sys.cpu.sp, "SP should decrement by 2 after JSR");
    
    cpu_step(&sys); // RTS
    u64 end_cycles = sys.mem.cycle_count;
    
    TEST_ASSERT_EQ(0x1004, sys.cpu.pc, "PC should return after RTS (return address + 1)");
    TEST_ASSERT_EQ(0xFF, sys.cpu.sp, "SP should be restored after RTS");
    TEST_ASSERT_EQ(12, end_cycles - start_cycles, "JSR+RTS should take 12 cycles total");
    
    c64_test_cleanup(&sys);
    TEST_PASS("Stack operations test passed");
}

bool test_cpu_flag_operations(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test CLC/SEC
    sys.cpu.status |= FLAG_C;
    sys.mem.ram[0x1000] = 0x18; // CLC
    sys.cpu.pc = 0x1000;
    
    cpu_step(&sys);
    TEST_ASSERT(!(sys.cpu.status & FLAG_C), "Carry should be cleared by CLC");
    
    sys.mem.ram[0x1001] = 0x38; // SEC
    sys.cpu.pc = 0x1001;
    
    cpu_step(&sys);
    TEST_ASSERT(sys.cpu.status & FLAG_C, "Carry should be set by SEC");
    
    // Test CLD/SED
    sys.cpu.status |= FLAG_D;
    sys.mem.ram[0x1002] = 0xD8; // CLD
    sys.cpu.pc = 0x1002;
    
    cpu_step(&sys);
    TEST_ASSERT(!(sys.cpu.status & FLAG_D), "Decimal should be cleared by CLD");
    
    sys.mem.ram[0x1003] = 0xF8; // SED
    sys.cpu.pc = 0x1003;
    
    cpu_step(&sys);
    TEST_ASSERT(sys.cpu.status & FLAG_D, "Decimal should be set by SED");
    
    // Test CLI/SEI
    sys.cpu.status |= FLAG_I;
    sys.mem.ram[0x1004] = 0x58; // CLI
    sys.cpu.pc = 0x1004;
    
    cpu_step(&sys);
    TEST_ASSERT(!(sys.cpu.status & FLAG_I), "Interrupt should be cleared by CLI");
    
    sys.mem.ram[0x1005] = 0x78; // SEI
    sys.cpu.pc = 0x1005;
    
    cpu_step(&sys);
    TEST_ASSERT(sys.cpu.status & FLAG_I, "Interrupt should be set by SEI");
    
    // Test CLV
    sys.cpu.status |= FLAG_V;
    sys.mem.ram[0x1006] = 0xB8; // CLV
    sys.cpu.pc = 0x1006;
    
    cpu_step(&sys);
    TEST_ASSERT(!(sys.cpu.status & FLAG_V), "Overflow should be cleared by CLV");
    
    c64_test_cleanup(&sys);
    TEST_PASS("Flag operations test passed");
}

bool test_cpu_compare_instructions(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test CMP immediate - equal
    sys.cpu.a = 0x42;
    sys.mem.ram[0x1000] = 0xC9; // CMP #$42
    sys.mem.ram[0x1001] = 0x42;
    sys.cpu.pc = 0x1000;
    
    cpu_step(&sys);
    TEST_ASSERT(sys.cpu.status & FLAG_C, "Carry should be set for equal compare");
    TEST_ASSERT(sys.cpu.status & FLAG_Z, "Zero should be set for equal compare");
    TEST_ASSERT(!(sys.cpu.status & FLAG_N), "Negative should be clear for equal compare");
    
    // Test CMP immediate - accumulator > memory
    sys.cpu.a = 0x50;
    sys.mem.ram[0x1002] = 0xC9; // CMP #$40
    sys.mem.ram[0x1003] = 0x40;
    sys.cpu.pc = 0x1002;
    
    cpu_step(&sys);
    TEST_ASSERT(sys.cpu.status & FLAG_C, "Carry should be set when A > memory");
    TEST_ASSERT(!(sys.cpu.status & FLAG_Z), "Zero should be clear when A > memory");
    
    // Test CMP immediate - accumulator < memory
    sys.cpu.a = 0x30;
    sys.mem.ram[0x1004] = 0xC9; // CMP #$40
    sys.mem.ram[0x1005] = 0x40;
    sys.cpu.pc = 0x1004;
    
    cpu_step(&sys);
    TEST_ASSERT(!(sys.cpu.status & FLAG_C), "Carry should be clear when A < memory");
    TEST_ASSERT(!(sys.cpu.status & FLAG_Z), "Zero should be clear when A < memory");
    TEST_ASSERT(sys.cpu.status & FLAG_N, "Negative should be set when A < memory");
    
    // Test CPX immediate
    sys.cpu.x = 0x42;
    sys.mem.ram[0x1006] = 0xE0; // CPX #$42
    sys.mem.ram[0x1007] = 0x42;
    sys.cpu.pc = 0x1006;
    
    cpu_step(&sys);
    TEST_ASSERT(sys.cpu.status & FLAG_C, "Carry should be set for CPX equal");
    TEST_ASSERT(sys.cpu.status & FLAG_Z, "Zero should be set for CPX equal");
    
    // Test CPY immediate
    sys.cpu.y = 0x42;
    sys.mem.ram[0x1008] = 0xC0; // CPY #$42
    sys.mem.ram[0x1009] = 0x42;
    sys.cpu.pc = 0x1008;
    
    cpu_step(&sys);
    TEST_ASSERT(sys.cpu.status & FLAG_C, "Carry should be set for CPY equal");
    TEST_ASSERT(sys.cpu.status & FLAG_Z, "Zero should be set for CPY equal");
    
    c64_test_cleanup(&sys);
    TEST_PASS("Compare instructions test passed");
}

bool test_cpu_transfer_instructions(void) {
    C64 sys;
    c64_test_init(&sys);
    
    // Test TAX
    sys.cpu.a = 0x42;
    sys.cpu.status = 0;
    sys.mem.ram[0x1000] = 0xAA; // TAX
    sys.cpu.pc = 0x1000;
    
    cpu_step(&sys);
    TEST_ASSERT_EQ(0x42, sys.cpu.x, "X should get value from A");
    TEST_ASSERT(!(sys.cpu.status & FLAG_Z), "Zero should be clear");
    TEST_ASSERT(!(sys.cpu.status & FLAG_N), "Negative should be clear");
    
    // Test TAY with zero
    sys.cpu.a = 0x00;
    sys.mem.ram[0x1001] = 0xA8; // TAY
    sys.cpu.pc = 0x1001;
    
    cpu_step(&sys);
    TEST_ASSERT_EQ(0x00, sys.cpu.y, "Y should get value from A");
    TEST_ASSERT(sys.cpu.status & FLAG_Z, "Zero should be set");
    TEST_ASSERT(!(sys.cpu.status & FLAG_N), "Negative should be clear");
    
    // Test TSX
    sys.cpu.sp = 0x40; // Use value that doesn't set negative flag
    sys.mem.ram[0x1002] = 0xBA; // TSX
    sys.cpu.pc = 0x1002;
    
    cpu_step(&sys);
    TEST_ASSERT_EQ(0x40, sys.cpu.x, "X should get value from SP");
    TEST_ASSERT(!(sys.cpu.status & FLAG_N), "Negative should be clear");
    TEST_ASSERT(!(sys.cpu.status & FLAG_Z), "Zero should be clear");
    
    // Test TXA
    sys.cpu.x = 0x7F;
    sys.mem.ram[0x1003] = 0x8A; // TXA
    sys.cpu.pc = 0x1003;
    
    cpu_step(&sys);
    TEST_ASSERT_EQ(0x7F, sys.cpu.a, "A should get value from X");
    TEST_ASSERT(!(sys.cpu.status & FLAG_N), "Negative should be clear");
    TEST_ASSERT(!(sys.cpu.status & FLAG_Z), "Zero should be clear");
    
    // Test TXS (doesn't affect flags)
    sys.cpu.x = 0x42;
    sys.cpu.status = 0;
    sys.mem.ram[0x1004] = 0x9A; // TXS
    sys.cpu.pc = 0x1004;
    
    cpu_step(&sys);
    TEST_ASSERT_EQ(0x42, sys.cpu.sp, "SP should get value from X");
    TEST_ASSERT_EQ(0, sys.cpu.status, "Flags should not be affected by TXS");
    
    // Test TYA
    sys.cpu.y = 0x80;
    sys.mem.ram[0x1005] = 0x98; // TYA
    sys.cpu.pc = 0x1005;
    
    cpu_step(&sys);
    TEST_ASSERT_EQ(0x80, sys.cpu.a, "A should get value from Y");
    TEST_ASSERT(sys.cpu.status & FLAG_N, "Negative should be set");
    TEST_ASSERT(!(sys.cpu.status & FLAG_Z), "Zero should be clear");
    
    c64_test_cleanup(&sys);
    TEST_PASS("Transfer instructions test passed");
}

bool test_cpu_illegal_opcodes(void) {
    // Test illegal opcodes like NOP variants, etc.
    // This is a placeholder for comprehensive illegal opcode testing
    printf("Illegal opcode tests not yet implemented\n");
    return true;
}

bool test_cpu_decimal_mode(void) {
    // Test decimal mode arithmetic (ADC/SBC with D flag set)
    // This is a placeholder for comprehensive decimal mode testing
    printf("Decimal mode tests not yet implemented\n");
    return true;
}

bool test_cpu_instructions(void) {
    return test_cpu_adc_immediate() && 
           test_cpu_branch_timing() && 
           test_cpu_page_cross_timing();
}

bool test_cpu_flags(void) {
    return test_cpu_flag_operations() && 
           test_cpu_compare_instructions() && 
           test_cpu_transfer_instructions();
}

bool test_cpu_interrupts(void) {
    // Placeholder for interrupt tests
    printf("CPU interrupt tests not yet implemented\n");
    return true;
}

bool test_cpu_timing(void) {
    return test_cpu_branch_timing() && 
           test_cpu_page_cross_timing() && 
           test_cpu_stack_operations();
}