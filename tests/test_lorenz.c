#include <stdio.h>
// #include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "c64.h"

#define DEBUG true

#define TESTCASE "irq"
#define MAX_STEPS 110000000

uint16_t load_testcase(CPU *cpu, const char *testcase);

#define petscii_to_ascii(c) (    \
    (c) >= 0xC1 && (c) <= 0xDA   \
        ? (c) - 0xC1 + 65        \
    : (c) >= 0x41 && (c) <= 0x5A \
        ? (c) - 0x41 + 97        \
    : (c) < 32 || (c) >= 127     \
        ? '.'                    \
        : (c))

// https://www.softwolves.com/arkiv/cbm-hackers/7/7114.html

void dump(CPU *cpu, uint16_t addr)
{
    printf("\n      ");
    for (size_t i = 0; i < 16; i++)
    {
        printf("%02lX ", i);
    }
    for (size_t i = 0; i <= 0xFF; i++)
    {
        if (i % 16 == 0)
            printf("\n%04lX  ", addr + i);

        printf("%02X ", cpu_read(cpu, addr + i));
    }
    printf("\n");
}

// Helper functions
void print_handler(CPU *cpu)
{
    // Set $030C = 0
    // Print PETSCII character corresponding to value of A
    // Pop return address from stack
    // Set PC to return address
    // Re-start the CPU

    cpu_write_byte(cpu, SAREG, 0x00); // Storage for 6502 .A Register

    uint8_t a = cpu->A;

    switch (a)
    {
    case 13:
        printf("\x{1B}[0m");
        printf("\n");
        break;
    case 14:
        printf("\x{1B}[33m");
        break;
    case 145:
        printf("\x{1B}[1A\x{1B}[1m"); // ↑ up arrow
        break;
    case 147:
        printf("\x{1B}c"); // clear
        break;
    default:
        printf("%c", petscii_to_ascii(a));
    }
}
void load_handler(CPU *cpu)
{
    // $BB is PETSCII filename address, low byte
    // $BC is PETSCII filename address, high byte
    // $B7 is PETSCII filename length
    // Load the file
    // Pop return address from stack
    // Set PC to $0816
    // Re-start the CPU

    uint16_t addr = cpu_read_word(cpu, 0xBB);
    uint8_t size = cpu_read_byte(cpu, 0xB7);

    char testcase[size + 1];
    for (size_t i = 0; i < size; i++)
        testcase[i] = petscii_to_ascii(cpu_read_byte(cpu, addr + i));
    testcase[size] = 0;

    load_testcase(cpu, testcase);
    cpu_set_pc(cpu, 0x0816);
}
void warm_handler(CPU *cpu)
{
    cpu_get_pc(cpu);

    printf("\x{1B}[31;1;6mWARM\x{1B}[0m\n");
    assert(false);
}
void ready_handler(CPU *cpu)
{
    cpu_get_pc(cpu);

    printf("\x{1B}[31;1;6mREADY\x{1B}[0m\n");
    assert(false);
}

void setup_c64(C64 *c64)
{
    c64_init(c64);

    c64_set_debug(&c64, DEBUG, NULL);

    // Enable CIA1 Timer A interrupt (like KERNAL does)
    // Write $81 to $DC0D: bit 7 = SET, bit 0 = Timer A
    // c64_write(c64, 0xDC0D, 0x81);

    // Print Character
    c64_trap(c64, 0xFFD2, print_handler);
    // Load
    c64_trap(c64, 0xE16F, load_handler);
    // Exit
    c64_trap(c64, 0x8000, warm_handler);
    c64_trap(c64, 0xA474, ready_handler);
}

uint8_t irq_handler[] = {
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
    0x03, // BEQ $03
    0x6C,
    0x16,
    0x03, // JMP ($0316)
    0x6C,
    0x14,
    0x03, // JMP ($0314)
};

void reset(CPU *cpu, uint16_t addr, uint8_t data[], size_t size)
{
    // cpu_write_byte(cpu, D6510, 0x2F);
    // cpu_write_byte(cpu, R6510, 0x04); // trap15 not working if commented out

    cpu_write_byte(cpu, UNUSED, 0x00);

    cpu_write_word(cpu, WARM, 0x8000); // 0xA002
    cpu_write_word(cpu, PC, 0x7FFF);   // 0x01FE
    cpu_write_word(cpu, IRQ, 0xFF48);  // 0xFFFE

    // Put RTSes in some of the stubbed calls
    cpu_write_byte(cpu, CHROUT, 0x60);  // 0xFFD2
    cpu_write_byte(cpu, CARTROM, 0x60); // 0x8000
    cpu_write_byte(cpu, READY, 0x60);   // 0xA474

    // NOP the loading routine
    cpu_write_byte(cpu, 0xE16F, 0xEA);

    // scan keyboard is LDA #3: RTS
    cpu_write_data(cpu, GETIN, (uint8_t[]){0xA9, 0x03, 0x60}, 3); // 0xFFE4

    cpu_write_data(cpu, PULS, irq_handler, sizeof(irq_handler));

    cpu_write_data(cpu, addr, data, size);

    cpu_reset_pc(cpu, addr);
    cpu_push16(cpu, 0x7FFF); // Return to WARM trap

    cpu->P = 0x04; // Interrupt Disable
}

uint16_t load_testcase(CPU *cpu, const char *testcase)
{
    char test_path[256];
    snprintf(test_path, sizeof(test_path), "tests/lorenz/%s", testcase);

    FILE *stream = fopen(test_path, "rb");
    if (stream == NULL)
    {
        fprintf(stderr, "Error: could not open \"%s\" testcase.\n", testcase);
        assert(false);
    }

    fseek(stream, 0L, SEEK_END);
    size_t size = ftell(stream) - 2;
    fseek(stream, 0L, SEEK_SET);

    // read load address (lo/hi)
    uint16_t addr = fgetc(stream) | fgetc(stream) << 8;
    // printf("addr=%04X\n", addr);

    uint8_t data[size];

    size_t read = fread(data, 1, sizeof(data), stream);
    assert(read == size);

    fclose(stream);

    reset(cpu, addr, data, sizeof(data));

    return addr;
}

int main()
{
    printf("=== MOS 6510 Lorenz Test Suite ===\n");

    C64 c64;
    setup_c64(&c64);

    load_testcase(&c64.cpu, TESTCASE);

    size_t cycles = 0;

    size_t step = 1;
    uint16_t pc;
    do
    {
        pc = c64_get_pc(&c64);

        cycles += c64_step(&c64);

        step++;
        if (step > MAX_STEPS)
        { // Safety limit
            printf("Too many steps, stopping...\n");
            break;
        }
    } while (pc != c64_get_pc(&c64) /* && cycles < 10000000000*/);

    printf("cycles: %ld\n", cycles);

    // dump(&c64.cpu, 0x0000);
    // dump(&c64.cpu, 0x0100);
    // dump(&c64.cpu, 0x0200);
    // dump(&c64.cpu, 0x0300);

    return 0;
}
