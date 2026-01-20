#include "../include/mos6510.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define DEBUG false

#define TESTCASE "start"

uint16_t load_testcase(CPU *cpu, const char *testcase);

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
        printf("\u{1B}[0m");
        printf("\n");
        break;
    case 14:
        printf("\u{1B}[33m");
        break;
    case 145:
        printf("\u{1B}[1A\u{1B}[1m"); // ↑ up arrow
        break;
    case 147:
        printf("\u{1B}c"); // clear
        break;
    default:
        printf("%c",
               (a >= 0xC1 && a <= 0xDA) ? //
                   a - 0xC1 + 65
                                        :     //
                   (a >= 0x41 && a <= 0x5A) ? //
                       a - 0x41 + 97
                                            : //
                       (a < 32 || a >= 127) ? //
                           '.'
                                            : //
                           a);
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

    // for (size_t i = 0; i <= 0xFF; i++)
    // {
    //     if (i % 16 == 0)
    //         printf("\n%04X  ", i);

    //     printf("%02X ", cpu_read_byte(cpu, 0x0D00 + i));
    // }
    // printf("\n");

    uint16_t addr = cpu_read_byte(cpu, 0xBB) | (cpu_read_byte(cpu, 0xBC) << 8);
    uint8_t size = cpu_read_word(cpu, 0xB7);

    char testcase[size + 1];
    for (size_t i = 0; i < size; i++)
        testcase[i] = cpu_read_byte(cpu, addr + i) - 0x41 + 97;
    testcase[size] = 0;

    load_testcase(cpu, testcase);
    cpu_reset_pc(cpu, 0x0816);
}
void warm_handler(CPU *cpu)
{
    printf("\u{1B}[31;1;6mWARM \u{1B}[0m\n");
    abort();
}
void ready_handler(CPU *cpu)
{
    printf("\u{1B}[31;1;6m[READY \u{1B}[0m\n");
    abort();
}

static CPU *setup_cpu()
{
    CPU *cpu = malloc(sizeof(CPU));
    cpu_init(cpu, DEBUG);

    // Print Character
    cpu_trap(cpu, 0xFFD2, print_handler);
    // Load
    cpu_trap(cpu, 0xE16F, load_handler);
    // Exit
    cpu_trap(cpu, 0x8000, warm_handler);
    cpu_trap(cpu, 0xA474, ready_handler);

    return cpu;
}

static void teardown_cpu(CPU *cpu)
{
    free(cpu);
}

uint8_t irq[] = {
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
    cpu_write_data(cpu, addr, data, size);

    cpu_write_data(cpu, 0xFF48, irq, sizeof(irq));

    cpu_write_byte(cpu, R6510, 0x04); // VIC
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
}

uint16_t load_testcase(CPU *cpu, const char *testcase)
{
    char test_path[256];
    snprintf(test_path, sizeof(test_path), "tests/lorenz/%s", testcase);

    FILE *stream = fopen(test_path, "rb");
    if (stream == NULL)
    {
        fprintf(stderr, "Error: Could not open test binary.\n");
        exit(1);
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

    CPU *cpu = setup_cpu();

    uint16_t addr = load_testcase(cpu, TESTCASE);
    cpu_reset_pc(cpu, addr);

    int cycles = 0;

    int step = 1;
    uint16_t pc;
    do
    {
        pc = cpu_get_pc(cpu);
        uint8_t opcode = cpu->memory[pc];

        cycles += cpu_step(cpu);

        step++;
        if (step > 1000000000)
        { // Safety limit
            printf("Too many steps, stopping...\n");
            break;
        }
    } while (pc != cpu_get_pc(cpu) && cycles < 10000000000);

    printf("cycles: %d\n", cycles);

    teardown_cpu(cpu);

    return 0;
}
