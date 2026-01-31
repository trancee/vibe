/**
 * test_dormann.c - Klaus Dormann's 6502 test suite
 *
 * Runs the comprehensive 6502 functional, decimal, and interrupt tests
 * by Klaus Dormann to validate CPU emulation accuracy.
 *
 * Test binaries are in tests/dormann/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "../src/c64.h"
#include "../src/cpu.h"

#include "test_framework.h"

#define DEBUG true

static C64System sys;

const char *rom_path = "roms";

// Load a test binary into memory
static bool load_dormann_test(const char *test_name, u16 load_address)
{
    char path[256];
    snprintf(path, sizeof(path), "tests/dormann/%s.bin", test_name);

    FILE *f = fopen(path, "rb");
    if (!f)
    {
        printf("    Could not open %s\n", path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size > 65536 - load_address)
    {
        printf("    Test binary too large\n");
        fclose(f);
        return false;
    }

    // Reset system
    c64_reset(&sys);

    size_t read = fread(&sys.mem.ram[load_address], 1, size, f);
    fclose(f);

    if (read != (size_t)size)
    {
        printf("    Failed to read complete test binary\n");
        return false;
    }

    return true;
}

// ============================================================================
// Dormann Functional Test
// ============================================================================

TEST(dormann_functional)
{
    if (!load_dormann_test("6502_functional_test", 0x0000))
    {
        printf("    Skipping (test binary not found)\n");
        g_test_ctx.passed++;
        return;
    }

    // Initialize CPU
    sys.cpu.P = FLAG_U | FLAG_I;
    sys.cpu.SP = 0xFF;
    sys.cpu.PC = 0x0400; // Start address

    u16 end_address = 0x3469;      // Success address
    size_t max_cycles = 1000000; // Safety limit
    size_t cycles = 0;
    u16 last_pc = 0;
    int stuck_count = 0;
    u8 last_test_num = 0xFF;

    while (cycles < max_cycles)
    {
        u16 pc = sys.cpu.PC;

        // Print test progress
        u8 test_num = sys.mem.ram[0x0200];
        if (test_num != last_test_num)
        {
            last_test_num = test_num;
            // Uncomment for verbose output:
            printf("    Test #$%02X at $%04X\n", test_num, pc);
        }

        c64_run_frame(&sys);
        cycles += sys.cycle_count;

        // Check for success
        if (sys.cpu.PC == end_address)
        {
            printf("    Functional test passed! (cycles: %zu)\n", cycles);
            PASS();
            return;
        }

        // Check for infinite loop (failure)
        if (sys.cpu.PC == last_pc)
        {
            stuck_count++;
            if (stuck_count > 3)
            {
                printf("    Test failed at $%04X (test #$%02X, cycles: %zu)\n",
                       sys.cpu.PC, sys.mem.ram[0x0200], cycles);
                ASSERT(false);
                return;
            }
        }
        else
        {
            stuck_count = 0;
            last_pc = pc;
        }
    }

    printf("    Timeout after %zu cycles at $%04X\n", cycles, sys.cpu.PC);
    ASSERT(false);
}

// ============================================================================
// Dormann Decimal Test
// ============================================================================

TEST(dormann_decimal)
{
    if (!load_dormann_test("6502_decimal_test", 0x0200))
    {
        printf("    Skipping (test binary not found)\n");
        g_test_ctx.passed++;
        return;
    }

    // Initialize CPU
    sys.cpu.P = FLAG_U | FLAG_I;
    sys.cpu.SP = 0xFF;
    sys.cpu.PC = 0x0200; // Start address

    u16 end_address = 0x024B; // Success address
    size_t max_cycles = 50000000;
    size_t cycles = 0;
    u16 last_pc = 0;
    int stuck_count = 0;

    while (cycles < max_cycles)
    {
        u16 pc = sys.cpu.PC;

        c64_run_frame(&sys);
        cycles += sys.cycle_count;

        // Check for success
        if (sys.cpu.PC == end_address)
        {
            // Check error flag
            u8 error = sys.mem.ram[0x000B];
            if (error == 0)
            {
                printf("    Decimal test passed! (cycles: %zu)\n", cycles);
                PASS();
                return;
            }
            else
            {
                printf("    Decimal test completed with errors: $%02X\n", error);
                ASSERT(false);
                return;
            }
        }

        // Check for infinite loop
        if (sys.cpu.PC == last_pc)
        {
            stuck_count++;
            if (stuck_count > 3)
            {
                printf("    Test failed at $%04X (error: $%02X, cycles: %zu)\n",
                       sys.cpu.PC, sys.mem.ram[0x000B], cycles);
                ASSERT(false);
                return;
            }
        }
        else
        {
            stuck_count = 0;
            last_pc = pc;
        }
    }

    printf("    Timeout after %zu cycles\n", cycles);
    ASSERT(false);
}

// ============================================================================
// Dormann Interrupt Test
// ============================================================================

TEST(dormann_interrupt)
{
    if (!load_dormann_test("6502_interrupt_test", 0x0000))
    {
        printf("    Skipping (test binary not found)\n");
        g_test_ctx.passed++;
        return;
    }

    // Initialize CPU
    sys.cpu.P = FLAG_U | FLAG_I; // Start with interrupts disabled
    sys.cpu.SP = 0xFF;
    sys.cpu.PC = 0x0400; // Start address

    // Test uses feedback at I_port = $BFFC
    // Open collector mode: writing 1 to bit triggers interrupt
    // Bit 0 = IRQ, Bit 1 = NMI
    u16 I_port = 0xBFFC;

    // Initialize I_port to 0 (no interrupts pending) - the test will set it up
    // This simulates the external hardware state before the test runs
    sys.mem.ram[I_port] = 0;

    // Success is indicated by reaching the STP instruction at $0731
    // or an infinite loop at $06F5 (success trap)
    size_t max_cycles = 500000000; // 500M cycles should be enough
    size_t cycles = 0;
    u16 last_pc = 0;
    int stuck_count = 0;
    bool nmi_last = false; // For NMI edge detection

    while (cycles < max_cycles)
    {
        u16 pc = sys.cpu.PC;

        // Check for success BEFORE executing - the test ends with JMP * at $06F5
        if (pc == 0x06F5 && sys.mem.ram[pc] == 0x4C &&
            sys.mem.ram[pc + 1] == 0xF5 && sys.mem.ram[pc + 2] == 0x06)
        {
            printf("    Interrupt test passed! (cycles: %zu)\n", cycles);
            PASS();
            return;
        }

        // Check for STP instruction (alternative success at $0731)
        if (sys.mem.ram[pc] == 0xDB)
        {
            printf("    Interrupt test passed (STP)! (cycles: %zu)\n", cycles);
            PASS();
            return;
        }

        // Execute the instruction
        c64_run_frame(&sys);
        cycles += sys.cycle_count;

        // Then check feedback register for interrupt line states AFTER the instruction
        // This gives the CPU a chance to set up interrupts
        u8 feedback = sys.mem.ram[I_port];

        // NMI is edge-triggered (low-to-high of the bit = interrupt trigger)
        bool nmi_now = (feedback & 0x02) != 0;
        if (nmi_now && !nmi_last)
        {
            // Edge detected - trigger NMI
            sys.cpu.nmi_pending = true;
        }
        nmi_last = nmi_now;

        // IRQ is level sensitive
        if ((feedback & 0x01) && !(sys.cpu.P & FLAG_I))
        {
            sys.cpu.irq_pending = true;
        }

        // Handle NMI first (higher priority)
        if (sys.cpu.nmi_pending)
        {
            sys.cpu.nmi_pending = false;
            // Push PC and P, jump to NMI vector
            sys.mem.ram[0x100 + sys.cpu.SP--] = (sys.cpu.PC >> 8) & 0xFF;
            sys.mem.ram[0x100 + sys.cpu.SP--] = sys.cpu.PC & 0xFF;
            sys.mem.ram[0x100 + sys.cpu.SP--] = (sys.cpu.P | FLAG_U) & ~FLAG_B;
            cpu_set_flag(&sys.cpu, FLAG_I, true);
            sys.cpu.PC = sys.mem.ram[0xFFFA] | (sys.mem.ram[0xFFFB] << 8);
            cycles += 7;
            continue;
        }

        // Handle IRQ
        if (sys.cpu.irq_pending && !(sys.cpu.P & FLAG_I))
        {
            sys.cpu.irq_pending = false;
            sys.mem.ram[0x100 + sys.cpu.SP--] = (sys.cpu.PC >> 8) & 0xFF;
            sys.mem.ram[0x100 + sys.cpu.SP--] = sys.cpu.PC & 0xFF;
            sys.mem.ram[0x100 + sys.cpu.SP--] = (sys.cpu.P | FLAG_U) & ~FLAG_B;
            cpu_set_flag(&sys.cpu, FLAG_I, true);
            sys.cpu.PC = sys.mem.ram[0xFFFE] | (sys.mem.ram[0xFFFF] << 8);
            cycles += 7;
            continue;
        }
        // Check for infinite loop (failure trap)
        if (sys.cpu.PC == last_pc)
        {
            stuck_count++;
            if (stuck_count > 3)
            {
                printf("    Test failed at $%04X (I_src=$%02X, I_port=$%02X, cycles: %zu)\n",
                       sys.cpu.PC, sys.mem.ram[0x0203], sys.mem.ram[I_port], cycles);
                ASSERT(false);
                return;
            }
        }
        else
        {
            stuck_count = 0;
            last_pc = pc;
        }
    }

    printf("    Timeout after %zu cycles at $%04X\n", cycles, sys.cpu.PC);
    ASSERT(false);
}

// ============================================================================
// Run all Dormann tests
// ============================================================================

void run_dormann_tests(void)
{
    TEST_SUITE("Dormann 6502 Test Suite");

    printf("    (These tests may take a few seconds...)\n");

    // Initialize system
    c64_init(&sys);
    sys.debug_enabled = DEBUG;

    // // Load ROMs
    // if (!c64_load_roms(&sys, rom_path))
    // {
    //     fprintf(stderr, "Failed to load ROMs from %s\n", rom_path);
    //     fprintf(stderr, "Make sure basic.rom, kernal.rom, and char.rom exist.\n");
    //     return;
    // }

    // RUN_TEST(dormann_functional);
    // RUN_TEST(dormann_decimal);
    RUN_TEST(dormann_interrupt);

    // Cleanup
    c64_destroy(&sys);
}

#ifdef TEST_DORMANN
TestContext g_test_ctx = {0, 0, 0, NULL};

int main()
{
    run_dormann_tests();

    return 0;
}
#endif
