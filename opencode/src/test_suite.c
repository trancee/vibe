#include "test_suite.h"
#include "cpu.h"
#include <math.h>

// Global test state
TestResults g_test_results = {0};
bool g_verbose = false;

static const TestCase* registered_tests[256];
static int num_registered_tests = 0;

// Test framework implementation
void test_init(void) {
    memset(&g_test_results, 0, sizeof(TestResults));
    num_registered_tests = 0;
    memset(registered_tests, 0, sizeof(registered_tests));
}

void test_register(const TestCase* test_case) {
    if (num_registered_tests < 256) {
        registered_tests[num_registered_tests++] = test_case;
    }
}

void test_report(void) {
    printf("\n==================================================\n");
    printf("TEST SUMMARY\n");
    printf("==================================================\n");
    printf("Total:  %d\n", g_test_results.total);
    printf("Passed: %d\n", g_test_results.passed);
    printf("Failed: %d\n", g_test_results.failed);
    printf("Skipped: %d\n", g_test_results.skipped);
    
    if (g_test_results.failed == 0) {
        printf("\n🎉 ALL TESTS PASSED!\n");
    } else {
        printf("\n❌ %d TESTS FAILED\n", g_test_results.failed);
    }
    printf("==================================================\n");
}

static void run_single_test(const TestCase* test) {
    printf("\n🧪 Running: %s (%s)\n", test->name, test->description);
    g_test_results.total++;
    
    bool result = test->test_func();
    if (result) {
        g_test_results.passed++;
        if (g_verbose) printf("✅ %s PASSED\n", test->name);
    } else {
        g_test_results.failed++;
        printf("❌ %s FAILED\n", test->name);
    }
}

void test_run_all(void) {
    printf("🚀 Running all tests...\n");
    
    for (int i = 0; i < num_registered_tests; i++) {
        run_single_test(registered_tests[i]);
    }
    
    test_report();
}

void test_run_category(const char* category) {
    printf("🚀 Running %s tests...\n", category);
    
    for (int i = 0; i < num_registered_tests; i++) {
        if (strcmp(registered_tests[i]->category, category) == 0) {
            run_single_test(registered_tests[i]);
        }
    }
    
    test_report();
}

// Helper functions for C64 testing
void c64_test_init(C64* sys) {
    memset(sys, 0, sizeof(C64));
    c64_init(sys);
}

void c64_test_cleanup(C64* sys) {
    c64_cleanup(sys);
}

void cpu_set_registers(C64CPU* cpu, u16 pc, u8 a, u8 x, u8 y, u8 sp, u8 status) {
    cpu->pc = pc;
    cpu->a = a;
    cpu->x = x;
    cpu->y = y;
    cpu->sp = sp;
    cpu->status = status;
}

bool cpu_verify_registers(C64CPU* cpu, u16 pc, u8 a, u8 x, u8 y, u8 sp, u8 status) {
    if (pc != cpu->pc) {
        printf("PC register mismatch: expected $%04X, got $%04X\n", pc, cpu->pc);
        return false;
    }
    if (a != cpu->a) {
        printf("A register mismatch: expected $%02X, got $%02X\n", a, cpu->a);
        return false;
    }
    if (x != cpu->x) {
        printf("X register mismatch: expected $%02X, got $%02X\n", x, cpu->x);
        return false;
    }
    if (y != cpu->y) {
        printf("Y register mismatch: expected $%02X, got $%02X\n", y, cpu->y);
        return false;
    }
    if (sp != cpu->sp) {
        printf("SP register mismatch: expected $%02X, got $%02X\n", sp, cpu->sp);
        return false;
    }
    if (status != cpu->status) {
        printf("Status register mismatch: expected $%02X, got $%02X\n", status, cpu->status);
        return false;
    }
    return true;
}

void memory_write_test_pattern(C64Memory* mem, u16 start, u16 size) {
    for (u16 i = 0; i < size; i++) {
        u16 addr = start + i;
        if (addr < sizeof(mem->ram)) {
            mem->ram[addr] = (u8)(i & 0xFF);
        }
    }
}

bool memory_verify_test_pattern(C64Memory* mem, u16 start, u16 size) {
    for (u16 i = 0; i < size; i++) {
        u16 addr = start + i;
        if (addr < sizeof(mem->ram)) {
            if (mem->ram[addr] != (u8)(i & 0xFF)) {
                printf("Memory mismatch at $%04X: expected $%02X, got $%02X\n",
                       addr, (u8)(i & 0xFF), mem->ram[addr]);
                return false;
            }
        }
    }
    return true;
}

// Timing test helpers
TimingInfo timing_start(const char* description) {
    TimingInfo info;
    info.description = description;
    info.start_cycle = 0; // Will be set by caller
    info.end_cycle = 0;
    return info;
}

void timing_end(TimingInfo* info, u64 expected_cycles) {
    u64 actual_cycles = info->end_cycle - info->start_cycle;
    printf("Timing: %s - Expected: %llu, Actual: %llu", 
           info->description, expected_cycles, actual_cycles);
    
    if (actual_cycles == expected_cycles) {
        printf(" ✅\n");
    } else {
        printf(" ❌ (diff: %lld)\n", (long long)(actual_cycles - expected_cycles));
    }
}

// CPU Instruction Table for comprehensive testing
const CPUInstructionInfo cpu_instruction_table[256] = {
    // ADC
    {0x69, "ADC #$imm", 2, 2, true, FLAG_C|FLAG_Z|FLAG_V|FLAG_N},
    {0x65, "ADC $zp",   3, 3, true, FLAG_C|FLAG_Z|FLAG_V|FLAG_N},
    {0x75, "ADC $zp,X", 4, 4, true, FLAG_C|FLAG_Z|FLAG_V|FLAG_N},
    {0x6D, "ADC $abs",  4, 4, true, FLAG_C|FLAG_Z|FLAG_V|FLAG_N},
    {0x7D, "ADC $abs,X",4, 5, true, FLAG_C|FLAG_Z|FLAG_V|FLAG_N}, // +1 page cross
    {0x79, "ADC $abs,Y",4, 5, true, FLAG_C|FLAG_Z|FLAG_V|FLAG_N}, // +1 page cross
    {0x61, "ADC ($zp,X)",6, 6, true, FLAG_C|FLAG_Z|FLAG_V|FLAG_N},
    {0x71, "ADC ($zp),Y",5, 6, true, FLAG_C|FLAG_Z|FLAG_V|FLAG_N}, // +1 page cross
    
    // AND
    {0x29, "AND #$imm", 2, 2, true, FLAG_Z|FLAG_N},
    {0x25, "AND $zp",   3, 3, true, FLAG_Z|FLAG_N},
    {0x35, "AND $zp,X", 4, 4, true, FLAG_Z|FLAG_N},
    {0x2D, "AND $abs",  4, 4, true, FLAG_Z|FLAG_N},
    {0x3D, "AND $abs,X",4, 5, true, FLAG_Z|FLAG_N}, // +1 page cross
    {0x39, "AND $abs,Y",4, 5, true, FLAG_Z|FLAG_N}, // +1 page cross
    {0x21, "AND ($zp,X)",6, 6, true, FLAG_Z|FLAG_N},
    {0x31, "AND ($zp),Y",5, 6, true, FLAG_Z|FLAG_N}, // +1 page cross
    
    // ASL
    {0x0A, "ASL A",     2, 2, true, FLAG_C|FLAG_Z|FLAG_N},
    {0x06, "ASL $zp",   5, 5, true, FLAG_C|FLAG_Z|FLAG_N},
    {0x16, "ASL $zp,X", 6, 6, true, FLAG_C|FLAG_Z|FLAG_N},
    {0x0E, "ASL $abs",  6, 6, true, FLAG_C|FLAG_Z|FLAG_N},
    {0x1E, "ASL $abs,X",7, 7, true, FLAG_C|FLAG_Z|FLAG_N},
    
    // BCC
    {0x90, "BCC",       2, 3, false, 0}, // +1 if branch taken, +1 if page cross
    
    // BCS
    {0xB0, "BCS",       2, 3, false, 0}, // +1 if branch taken, +1 if page cross
    
    // BEQ
    {0xF0, "BEQ",       2, 3, false, 0}, // +1 if branch taken, +1 if page cross
    
    // BIT
    {0x24, "BIT $zp",   3, 3, true, FLAG_Z|FLAG_V|FLAG_N},
    {0x2C, "BIT $abs",  4, 4, true, FLAG_Z|FLAG_V|FLAG_N},
    
    // BMI
    {0x30, "BMI",       2, 3, false, 0}, // +1 if branch taken, +1 if page cross
    
    // BNE
    {0xD0, "BNE",       2, 3, false, 0}, // +1 if branch taken, +1 if page cross
    
    // BPL
    {0x10, "BPL",       2, 3, false, 0}, // +1 if branch taken, +1 if page cross
    
    // BRK
    {0x00, "BRK",       7, 7, true, FLAG_B|FLAG_I},
    
    // BVC
    {0x50, "BVC",       2, 3, false, 0}, // +1 if branch taken, +1 if page cross
    
    // BVS
    {0x70, "BVS",       2, 3, false, 0}, // +1 if branch taken, +1 if page cross
    
    // CLC
    {0x18, "CLC",       2, 2, true, FLAG_C},
    
    // CLD
    {0xD8, "CLD",       2, 2, true, FLAG_D},
    
    // CLI
    {0x58, "CLI",       2, 2, true, FLAG_I},
    
    // CLV
    {0xB8, "CLV",       2, 2, true, FLAG_V},
    
    // CMP
    {0xC9, "CMP #$imm", 2, 2, true, FLAG_C|FLAG_Z|FLAG_N},
    {0xC5, "CMP $zp",   3, 3, true, FLAG_C|FLAG_Z|FLAG_N},
    {0xD5, "CMP $zp,X", 4, 4, true, FLAG_C|FLAG_Z|FLAG_N},
    {0xCD, "CMP $abs",  4, 4, true, FLAG_C|FLAG_Z|FLAG_N},
    {0xDD, "CMP $abs,X",4, 5, true, FLAG_C|FLAG_Z|FLAG_N}, // +1 page cross
    {0xD9, "CMP $abs,Y",4, 5, true, FLAG_C|FLAG_Z|FLAG_N}, // +1 page cross
    {0xC1, "CMP ($zp,X)",6, 6, true, FLAG_C|FLAG_Z|FLAG_N},
    {0xD1, "CMP ($zp),Y",5, 6, true, FLAG_C|FLAG_Z|FLAG_N}, // +1 page cross
    
    // CPX
    {0xE0, "CPX #$imm", 2, 2, true, FLAG_C|FLAG_Z|FLAG_N},
    {0xE4, "CPX $zp",   3, 3, true, FLAG_C|FLAG_Z|FLAG_N},
    {0xEC, "CPX $abs",  4, 4, true, FLAG_C|FLAG_Z|FLAG_N},
    
    // CPY
    {0xC0, "CPY #$imm", 2, 2, true, FLAG_C|FLAG_Z|FLAG_N},
    {0xC4, "CPY $zp",   3, 3, true, FLAG_C|FLAG_Z|FLAG_N},
    {0xCC, "CPY $abs",  4, 4, true, FLAG_C|FLAG_Z|FLAG_N},
    
    // DEC
    {0xCA, "DEX",       2, 2, true, FLAG_Z|FLAG_N},
    {0xDE, "DEC $abs,X",7, 7, true, FLAG_Z|FLAG_N},
    {0xC6, "DEC $zp",   5, 5, true, FLAG_Z|FLAG_N},
    {0xD6, "DEC $zp,X", 6, 6, true, FLAG_Z|FLAG_N},
    {0xCE, "DEC $abs",  6, 6, true, FLAG_Z|FLAG_N},
    {0x88, "DEY",       2, 2, true, FLAG_Z|FLAG_N},
    
    // EOR
    {0x49, "EOR #$imm", 2, 2, true, FLAG_Z|FLAG_N},
    {0x45, "EOR $zp",   3, 3, true, FLAG_Z|FLAG_N},
    {0x55, "EOR $zp,X", 4, 4, true, FLAG_Z|FLAG_N},
    {0x4D, "EOR $abs",  4, 4, true, FLAG_Z|FLAG_N},
    {0x5D, "EOR $abs,X",4, 5, true, FLAG_Z|FLAG_N}, // +1 page cross
    {0x59, "EOR $abs,Y",4, 5, true, FLAG_Z|FLAG_N}, // +1 page cross
    {0x41, "EOR ($zp,X)",6, 6, true, FLAG_Z|FLAG_N},
    {0x51, "EOR ($zp),Y",5, 6, true, FLAG_Z|FLAG_N}, // +1 page cross
    
    // INC
    {0xE8, "INX",       2, 2, true, FLAG_Z|FLAG_N},
    {0xF6, "INC $zp,X", 6, 6, true, FLAG_Z|FLAG_N},
    {0xEE, "INC $abs",  6, 6, true, FLAG_Z|FLAG_N},
    {0xE6, "INC $zp",   5, 5, true, FLAG_Z|FLAG_N},
    {0xFE, "INC $abs,X",7, 7, true, FLAG_Z|FLAG_N},
    {0xC8, "INY",       2, 2, true, FLAG_Z|FLAG_N},
    
    // JMP
    {0x4C, "JMP $abs",  3, 3, false, 0},
    {0x6C, "JMP ($abs)",5, 5, false, 0},
    
    // JSR
    {0x20, "JSR $abs",  6, 6, false, 0},
    
    // LDA
    {0xA9, "LDA #$imm", 2, 2, true, FLAG_Z|FLAG_N},
    {0xA5, "LDA $zp",   3, 3, true, FLAG_Z|FLAG_N},
    {0xB5, "LDA $zp,X", 4, 4, true, FLAG_Z|FLAG_N},
    {0xAD, "LDA $abs",  4, 4, true, FLAG_Z|FLAG_N},
    {0xBD, "LDA $abs,X",4, 5, true, FLAG_Z|FLAG_N}, // +1 page cross
    {0xB9, "LDA $abs,Y",4, 5, true, FLAG_Z|FLAG_N}, // +1 page cross
    {0xA1, "LDA ($zp,X)",6, 6, true, FLAG_Z|FLAG_N},
    {0xB1, "LDA ($zp),Y",5, 6, true, FLAG_Z|FLAG_N}, // +1 page cross
    
    // LDX
    {0xA2, "LDX #$imm", 2, 2, true, FLAG_Z|FLAG_N},
    {0xA6, "LDX $zp",   3, 3, true, FLAG_Z|FLAG_N},
    {0xB6, "LDX $zp,Y", 4, 4, true, FLAG_Z|FLAG_N},
    {0xAE, "LDX $abs",  4, 4, true, FLAG_Z|FLAG_N},
    {0xBE, "LDX $abs,Y",4, 5, true, FLAG_Z|FLAG_N}, // +1 page cross
    
    // LDY
    {0xA0, "LDY #$imm", 2, 2, true, FLAG_Z|FLAG_N},
    {0xA4, "LDY $zp",   3, 3, true, FLAG_Z|FLAG_N},
    {0xB4, "LDY $zp,X", 4, 4, true, FLAG_Z|FLAG_N},
    {0xAC, "LDY $abs",  4, 4, true, FLAG_Z|FLAG_N},
    {0xBC, "LDY $abs,X",4, 5, true, FLAG_Z|FLAG_N}, // +1 page cross
    
    // LSR
    {0x4A, "LSR A",     2, 2, true, FLAG_C|FLAG_Z|FLAG_N},
    {0x46, "LSR $zp",   5, 5, true, FLAG_C|FLAG_Z|FLAG_N},
    {0x56, "LSR $zp,X", 6, 6, true, FLAG_C|FLAG_Z|FLAG_N},
    {0x4E, "LSR $abs",  6, 6, true, FLAG_C|FLAG_Z|FLAG_N},
    {0x5E, "LSR $abs,X",7, 7, true, FLAG_C|FLAG_Z|FLAG_N},
    
    // NOP
    {0xEA, "NOP",       2, 2, false, 0},
    
    // ORA
    {0x09, "ORA #$imm", 2, 2, true, FLAG_Z|FLAG_N},
    {0x05, "ORA $zp",   3, 3, true, FLAG_Z|FLAG_N},
    {0x15, "ORA $zp,X", 4, 4, true, FLAG_Z|FLAG_N},
    {0x0D, "ORA $abs",  4, 4, true, FLAG_Z|FLAG_N},
    {0x1D, "ORA $abs,X",4, 5, true, FLAG_Z|FLAG_N}, // +1 page cross
    {0x19, "ORA $abs,Y",4, 5, true, FLAG_Z|FLAG_N}, // +1 page cross
    {0x01, "ORA ($zp,X)",6, 6, true, FLAG_Z|FLAG_N},
    {0x11, "ORA ($zp),Y",5, 6, true, FLAG_Z|FLAG_N}, // +1 page cross
    
    // PHA
    {0x48, "PHA",       3, 3, false, 0},
    
    // PHP
    {0x08, "PHP",       3, 3, false, 0},
    
    // PLA
    {0x68, "PLA",       4, 4, true, FLAG_Z|FLAG_N},
    
    // PLP
    {0x28, "PLP",       4, 4, true, 0xFF}, // All flags can change
    
    // ROL
    {0x2A, "ROL A",     2, 2, true, FLAG_C|FLAG_Z|FLAG_N},
    {0x26, "ROL $zp",   5, 5, true, FLAG_C|FLAG_Z|FLAG_N},
    {0x36, "ROL $zp,X", 6, 6, true, FLAG_C|FLAG_Z|FLAG_N},
    {0x2E, "ROL $abs",  6, 6, true, FLAG_C|FLAG_Z|FLAG_N},
    {0x3E, "ROL $abs,X",7, 7, true, FLAG_C|FLAG_Z|FLAG_N},
    
    // ROR
    {0x6A, "ROR A",     2, 2, true, FLAG_C|FLAG_Z|FLAG_N},
    {0x66, "ROR $zp",   5, 5, true, FLAG_C|FLAG_Z|FLAG_N},
    {0x76, "ROR $zp,X", 6, 6, true, FLAG_C|FLAG_Z|FLAG_N},
    {0x6E, "ROR $abs",  6, 6, true, FLAG_C|FLAG_Z|FLAG_N},
    {0x7E, "ROR $abs,X",7, 7, true, FLAG_C|FLAG_Z|FLAG_N},
    
    // RTI
    {0x40, "RTI",       6, 6, true, 0xFF}, // All flags can change
    
    // RTS
    {0x60, "RTS",       6, 6, false, 0},
    
    // SBC
    {0xE9, "SBC #$imm", 2, 2, true, FLAG_C|FLAG_Z|FLAG_V|FLAG_N},
    {0xE5, "SBC $zp",   3, 3, true, FLAG_C|FLAG_Z|FLAG_V|FLAG_N},
    {0xF5, "SBC $zp,X", 4, 4, true, FLAG_C|FLAG_Z|FLAG_V|FLAG_N},
    {0xED, "SBC $abs",  4, 4, true, FLAG_C|FLAG_Z|FLAG_V|FLAG_N},
    {0xFD, "SBC $abs,X",4, 5, true, FLAG_C|FLAG_Z|FLAG_V|FLAG_N}, // +1 page cross
    {0xF9, "SBC $abs,Y",4, 5, true, FLAG_C|FLAG_Z|FLAG_V|FLAG_N}, // +1 page cross
    {0xE1, "SBC ($zp,X)",6, 6, true, FLAG_C|FLAG_Z|FLAG_V|FLAG_N},
    {0xF1, "SBC ($zp),Y",5, 6, true, FLAG_C|FLAG_Z|FLAG_V|FLAG_N}, // +1 page cross
    
    // SEC
    {0x38, "SEC",       2, 2, true, FLAG_C},
    
    // SED
    {0xF8, "SED",       2, 2, true, FLAG_D},
    
    // SEI
    {0x78, "SEI",       2, 2, true, FLAG_I},
    
    // STA
    {0x85, "STA $zp",   3, 3, false, 0},
    {0x95, "STA $zp,X", 4, 4, false, 0},
    {0x8D, "STA $abs",  4, 4, false, 0},
    {0x9D, "STA $abs,X",5, 5, false, 0},
    {0x99, "STA $abs,Y",5, 5, false, 0},
    {0x81, "STA ($zp,X)",6, 6, false, 0},
    {0x91, "STA ($zp),Y",6, 6, false, 0},
    
    // STX
    {0x86, "STX $zp",   3, 3, false, 0},
    {0x96, "STX $zp,Y", 4, 4, false, 0},
    {0x8E, "STX $abs",  4, 4, false, 0},
    
    // STY
    {0x84, "STY $zp",   3, 3, false, 0},
    {0x94, "STY $zp,X", 4, 4, false, 0},
    {0x8C, "STY $abs",  4, 4, false, 0},
    
    // TAX
    {0xAA, "TAX",       2, 2, true, FLAG_Z|FLAG_N},
    
    // TAY
    {0xA8, "TAY",       2, 2, true, FLAG_Z|FLAG_N},
    
    // TSX
    {0xBA, "TSX",       2, 2, true, FLAG_Z|FLAG_N},
    
    // TXA
    {0x8A, "TXA",       2, 2, true, FLAG_Z|FLAG_N},
    
    // TXS
    {0x9A, "TXS",       2, 2, false, 0},
    
    // TYA
    {0x98, "TYA",       2, 2, true, FLAG_Z|FLAG_N},
    
    // Fill rest with illegal opcodes
    {0x00, "ILLEGAL", 0, 0, false, 0}
};