/**
 * test_lorenz.c - Wolfgang Lorenz's C64 Test Suite
 *
 * Runs a selection of the Lorenz C64 test suite to validate
 * CPU instruction accuracy. The test files are in tests/lorenz/
 *
 * The Lorenz suite uses KERNAL traps for I/O:
 *   $FFD2 (CHROUT) - Print character
 *   $E16F (LOAD)   - Load next test
 *   $8000 (WARM)   - Test passed, continue to next
 *   $A474 (READY)  - All tests completed
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/c64.h"
#include "../src/cpu.h"

#include "test_framework.h"

#define DEBUG false

#define TESTCASE "start"

static C64System sys;

const char *rom_path = "roms";

// Test state
static bool lorenz_test_passed = false;
static bool lorenz_test_failed = false;
static const char *lorenz_test = TESTCASE;

// PETSCII to ASCII conversion
#define petscii_to_ascii(c) (    \
    (c) >= 0xC1 && (c) <= 0xDA   \
        ? (c) - 0xC1 + 65        \
    : (c) >= 0x41 && (c) <= 0x5A \
        ? (c) - 0x41 + 97        \
    : (c) < 32 || (c) >= 127     \
        ? '.'                    \
        : (c))

// Initialize memory with ROM-like vectors and stubs
static void lorenz_init_memory(void)
{
    // Set up CPU port
    mem_write_raw(&sys.mem, 0x0000, 0x2F); // Data direction
    mem_write_raw(&sys.mem, 0x0001, 0x37); // Default banking

    // Set up important vectors
    mem_write_raw(&sys.mem, 0xFFFA, 0x00);
    mem_write_raw(&sys.mem, 0xFFFB, 0x80); // NMI -> $8000
    mem_write_raw(&sys.mem, 0xFFFC, 0x00);
    mem_write_raw(&sys.mem, 0xFFFD, 0x08); // RESET -> $0800
    mem_write_raw(&sys.mem, 0xFFFE, 0x48);
    mem_write_raw(&sys.mem, 0xFFFF, 0xFF); // IRQ -> $FF48

    // KERNAL stubs - these are trap addresses we'll intercept
    mem_write_raw(&sys.mem, 0xFFD2, 0x60); // CHROUT - RTS (trapped)
    mem_write_raw(&sys.mem, 0xFFE4, 0xA9); // GETIN - LDA #3; RTS
    mem_write_raw(&sys.mem, 0xFFE5, 0x03);
    mem_write_raw(&sys.mem, 0xFFE6, 0x60);

    // Test harness addresses
    mem_write_raw(&sys.mem, 0x8000, 0x60); // WARM/CARTROM - RTS (trapped)
    mem_write_raw(&sys.mem, 0xA474, 0x60); // READY - RTS (trapped)
    mem_write_raw(&sys.mem, 0xE16F, 0xEA); // LOAD - NOP (trapped)

    // IRQ handler at $FF48 (standard location)
    static const u8 irq_handler[] = {
        0x48, // PHA
        0x8A, // TXA
        0x48, // PHA
        0x98, // TYA
        0x48, // PHA
        0xBA, // TSX
        0xBD,
        0x04,
        0x01, // LDA $0104,X
        0x29,
        0x10, // AND #$10
        0xF0,
        0x03, // BEQ +3
        0x6C,
        0x16,
        0x03, // JMP ($0316) - BRK vector
        0x6C,
        0x14,
        0x03, // JMP ($0314) - IRQ vector
    };
    memcpy(&sys.mem.ram[0xFF48], irq_handler, sizeof(irq_handler));

    // Default IRQ/BRK vectors
    mem_write_raw(&sys.mem, 0x0314, 0x31);
    mem_write_raw(&sys.mem, 0x0315, 0xEA); // IRQ -> $EA31
    mem_write_raw(&sys.mem, 0x0316, 0x66);
    mem_write_raw(&sys.mem, 0x0317, 0xFE); // BRK -> $FE66

    // RTI at common return points
    mem_write_raw(&sys.mem, 0xEA31, 0x40); // RTI
    mem_write_raw(&sys.mem, 0xFE66, 0x40); // RTI

    // WARM vector ($0302-$0303) points to $8000
    mem_write_raw(&sys.mem, 0x0302, 0x00);
    mem_write_raw(&sys.mem, 0x0303, 0x80);
    
    // Initialize CIA1 like KERNAL does
    // Enable Timer A interrupt (bit 0) - KERNAL uses this for keyboard scanning
    sys.cia1.icr_mask = 0x01;  // Timer A interrupt enabled
}

// Load a Lorenz test file
static bool lorenz_load_test(const char *testcase)
{
    char path[256];
    snprintf(path, sizeof(path), "tests/lorenz/%s", testcase);

    FILE *f = fopen(path, "rb");
    if (!f)
    {
        return false;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 3)
    {
        fclose(f);
        return false;
    }

    // Read 2-byte load address (little endian)
    u16 load_address = fgetc(f) | (fgetc(f) << 8);
    size -= 2;

    // Reset all components
    c64_reset(&sys);

    // Read program data
    size_t read = fread(&sys.mem.ram[load_address], 1, size, f);
    fclose(f);

    return (read == size);
}

// Run a single Lorenz test
static bool run_lorenz_test()
{
    if (!lorenz_load_test(lorenz_test))
        return false;

    // Initialize memory AFTER c64_reset (which is called in lorenz_load_test)
    lorenz_init_memory();

    lorenz_test_passed = false;
    lorenz_test_failed = false;

    // Initialize CPU
    // On a real C64, KERNAL initialization clears the I flag before running programs
    sys.cpu.P = FLAG_U;  // I flag clear (interrupts enabled)
    sys.cpu.SP = 0xFF;

    // Get start address from test file (already loaded)
    // Tests typically start at $0801 with BASIC stub, actual code at $0816
    sys.cpu.PC = 0x0816;

    // Push return address to WARM trap
    sys.mem.ram[0x100 + sys.cpu.SP--] = 0x7F; // High byte of $7FFF
    sys.mem.ram[0x100 + sys.cpu.SP--] = 0xFF; // Low byte

    size_t max_cycles = 100000000; // 100M cycles per test
    size_t cycles = 0;
    u16 last_pc = 0;
    int stuck_count = 0;

    while (cycles < max_cycles)
    {
        u16 pc = sys.cpu.PC;
        
        // Check if KERNAL ROM is visible (HIRAM bit set in port data)
        bool kernal_visible = (sys.cpu.port_data & 0x02) != 0;

        // Check for trap addresses BEFORE executing
        // Only trap KERNAL calls when KERNAL ROM is actually visible

        // CHROUT ($FFD2) - print character
        if (pc == 0xFFD2 && kernal_visible)
        {
            mem_write_raw(&sys.mem, 0x030C, 0x00); // Storage for 6502 .A Register

            u8 a = sys.cpu.A;

            switch (a)
            {
            case 13:
                printf("\033[0m");
                printf("\n");
                break;
            case 14:
                printf("\033[33m");
                break;
            case 17:
                // printf("↓");
                printf("\033[1B"); // cursor down
                printf("\033[1m");
                break;
            case 29:
                // printf("→");
                printf("\033[1C"); // cursor right
                printf("\033[1m");
                break;
            case 145:
                // printf("↑");
                printf("\033[1A"); // cursor up
                printf("\033[1m");
                break;
            case 147:
                printf("\033c"); // clear
                break;
            case 157:
                // printf("←");
                printf("\033[1D"); // cursor left
                printf("\033[1m");
                break;
            default:
                printf("%c", petscii_to_ascii(a));
            }

            // RTS
            u16 lo = sys.mem.ram[0x100 + (++sys.cpu.SP)];
            u16 hi = sys.mem.ram[0x100 + (++sys.cpu.SP)];
            sys.cpu.PC = (lo | (hi << 8)) + 1;
            continue;
        }

        // GETIN ($FFE4) - get key, return non-zero to not wait forever
        if (pc == 0xFFE4 && kernal_visible)
        {
            sys.cpu.A = 0x03; // Return 3 (RUN/STOP key) - non-zero so loops exit

            cpu_set_flag(&sys.cpu, FLAG_Z, false); // Clear Z so BEQ fails

            // RTS
            u16 lo = sys.mem.ram[0x100 + (++sys.cpu.SP)];
            u16 hi = sys.mem.ram[0x100 + (++sys.cpu.SP)];
            sys.cpu.PC = (lo | (hi << 8)) + 1;
            continue;
        }

        // WARM/CARTROM ($8000) - test passed
        if (pc == 0x8000)
        {
            printf("\033[31;1;6mWARM\033[0m\n");

            lorenz_test_passed = true;
            break;
        }

        // READY ($A474) - return to BASIC (error)
        if (pc == 0xA474)
        {
            printf("\033[31;1;6mREADY\033[0m\n");

            lorenz_test_failed = true;
            break;
        }

        // LOAD ($E16F) - load next test (we skip this for single tests)
        if (pc == 0xE16F && kernal_visible)
        {
            // Get filename from $BB/$BC (address) and $B7 (length)
            u16 name_addr = sys.mem.ram[0xBB] | (sys.mem.ram[0xBC] << 8);
            u8 name_len = sys.mem.ram[0xB7];

            static char next_name[30];
            for (int i = 0; i < name_len && i < 29; i++)
                next_name[i] = petscii_to_ascii(sys.mem.ram[name_addr + i]);

            next_name[name_len < 29 ? name_len : 29] = '\0';

            lorenz_test = next_name;
            lorenz_test_passed = true; // Current test passed, wants to load next
            break;
        }

        // Execute one CPU instruction (memory accesses tick the system)
        cycles += cpu_step(&sys.cpu);

        // Check for infinite loop
        if (sys.cpu.PC == last_pc)
        {
            stuck_count++;
            if (stuck_count > 10)
            {
                lorenz_test_failed = true;
                break;
            }
        }
        else
        {
            stuck_count = 0;
            last_pc = pc;
        }
    }

    if (cycles >= max_cycles)
        lorenz_test_failed = true;

    return lorenz_test_passed && !lorenz_test_failed;
}

TEST(lorenz_cpu_instructions)
{
    int passed = 0;
    int failed = 0;
    int skipped = 0;

    printf("\n");

    while (true) {
        if (run_lorenz_test()) {
            passed++;
        } else {
            // Check if test failed or if there's no more tests
            if (lorenz_test_failed) {
                failed++;
                break;  // Stop on first failure for now
            }

            break;  // No more tests to load
        }
    }

    printf("    Lorenz CPU tests: %d passed, %d failed, %d skipped\n",
           passed, failed, skipped);

    if (failed == 0)
        PASS();

    else
        ASSERT_EQ(failed, 0);
}

// ============================================================================
// Run all Lorenz tests
// ============================================================================

void run_lorenz_tests(void)
{
    TEST_SUITE("Lorenz Test Suite");

    printf("    (Running CPU instruction tests...)\n");

    // Initialize system
    c64_init(&sys);
    sys.debug = DEBUG;

    // Load ROMs
    if (!c64_load_roms(&sys, rom_path))
    {
        fprintf(stderr, "Failed to load ROMs from %s\n", rom_path);
        fprintf(stderr, "Make sure basic.rom, kernal.rom, and char.rom exist.\n");
        return;
    }

    RUN_TEST(lorenz_cpu_instructions);

    c64_destroy(&sys);
}

#ifdef TEST_LORENZ
TestContext g_test_ctx = {0, 0, 0, 0, NULL};

int main()
{
    run_lorenz_tests();

    return 0;
}
#endif
