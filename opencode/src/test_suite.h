#ifndef TEST_SUITE_H
#define TEST_SUITE_H

#include "c64.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char* name;
    bool (*test_func)(void);
    const char* description;
    const char* category;
} TestCase;

typedef struct {
    uint32_t total;
    uint32_t passed;
    uint32_t failed;
    uint32_t skipped;
} TestResults;

extern TestResults g_test_results;
extern bool g_verbose;

// Enhanced test result macros
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("  ❌ FAIL: %s:%d - %s\n", __FILE__, __LINE__, message); \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            printf("  ❌ FAIL: %s:%d - %s (expected: %d, actual: %d)\n", \
                   __FILE__, __LINE__, message, (int)(expected), (int)(actual)); \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_EQ_HEX(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            printf("  ❌ FAIL: %s:%d - %s (expected: $%02X, actual: $%02X)\n", \
                   __FILE__, __LINE__, message, (unsigned)(expected), (unsigned)(actual)); \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_RANGE(min, max, value, message) \
    do { \
        if ((value) < (min) || (value) > (max)) { \
            printf("  ❌ FAIL: %s:%d - %s (value: %d, expected: %d-%d)\n", \
                   __FILE__, __LINE__, message, (int)(value), (int)(min), (int)(max)); \
            return false; \
        } \
    } while(0)

#define TEST_PASS(message) \
    do { \
        printf("  ✅ PASS: %s\n", message); \
        return true; \
    } while(0)

// Test framework functions
void test_init(void);
void test_report(void);
void test_register(const TestCase* test_case);
void test_run_all(void);
void test_run_category(const char* category);

// Helper functions for C64 testing
void c64_test_init(C64* sys);
void c64_test_cleanup(C64* sys);
void cpu_set_registers(C64CPU* cpu, u16 pc, u8 a, u8 x, u8 y, u8 sp, u8 status);
bool cpu_verify_registers(C64CPU* cpu, u16 pc, u8 a, u8 x, u8 y, u8 sp, u8 status);
void memory_write_test_pattern(C64Memory* mem, u16 start, u16 size);
bool memory_verify_test_pattern(C64Memory* mem, u16 start, u16 size);

// Timing test helpers
typedef struct {
    u64 start_cycle;
    u64 end_cycle;
    const char* description;
} TimingInfo;

TimingInfo timing_start(const char* description);
void timing_end(TimingInfo* info, u64 expected_cycles);

// CPU instruction test helpers
typedef struct {
    u8 opcode;
    const char* mnemonic;
    u8 cycles_min;
    u8 cycles_max;
    bool affects_flags;
    u8 flags_affected;
} CPUInstructionInfo;

extern const CPUInstructionInfo cpu_instruction_table[256];

// CPU test functions
bool test_cpu_instructions(void);
bool test_cpu_flags(void);
bool test_cpu_interrupts(void);
bool test_cpu_timing(void);
bool test_cpu_illegal_opcodes(void);
bool test_cpu_decimal_mode(void);
bool test_cpu_branch_timing(void);
bool test_cpu_page_cross_timing(void);

// CIA test functions  
bool test_cia_timers(void);
bool test_cia_interrupts(void);
bool test_cia_tod(void);
bool test_cia_lorenz_irq(void);
bool test_cia_timer_pipeline(void);
bool test_cia_icr_timing(void);

// VIC test functions
bool test_vic_timing(void);
bool test_vic_interrupts(void);
bool test_vic_rendering(void);
bool test_vic_bad_lines(void);
bool test_vic_sprite_dma(void);
bool test_vic_raster_interrupts(void);

// Memory test functions
bool test_memory_mapping(void);
bool test_memory_pla(void);
bool test_memory_color_ram(void);
bool test_memory_rom_loading(void);

// SID test functions
bool test_sid_registers(void);
bool test_sid_filter(void);
bool test_sid_audio_generation(void);

// Integration tests
bool test_kernal_boot(void);
bool test_basic_interpreter(void);
bool test_lorenz_suite_compliance(void);

// Test categories
#define CPU_CATEGORY      "CPU"
#define CIA_CATEGORY      "CIA" 
#define VIC_CATEGORY      "VIC"
#define MEMORY_CATEGORY   "Memory"
#define SID_CATEGORY      "SID"
#define INTEGRATION_CATEGORY "Integration"

#endif // TEST_SUITE_H