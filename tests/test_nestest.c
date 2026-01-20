#include "../include/mos6510.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define DEBUG true

#define START_ADDRESS 0xC000
#define END_ADDRESS 0x0800
#define PAYLOAD_LENGTH 0x4000

#define EXACT_CYCLES 26553

#define iNES_HEADER_SIZE 16

#define TEST_ASSERT(condition, message) \
    do                                  \
    {                                   \
        if (condition)                  \
        {                               \
            printf("✓ %s\n", message);  \
        }                               \
        else                            \
        {                               \
            printf("✗ %s\n", message);  \
            exit(1);                    \
        }                               \
    } while (0)

// Helper functions
static CPU *setup_cpu()
{
    CPU *cpu = malloc(sizeof(CPU));
    cpu_init(cpu, DEBUG);
    return cpu;
}

static void teardown_cpu(CPU *cpu)
{
    free(cpu);
}

void load_test(uint8_t *memory)
{
    FILE *stream = fopen("tests/nestest/nestest.nes", "rb");
    if (stream == NULL)
    {
        fprintf(stderr, "Error: Could not open test binary.\n");
        exit(1);
    }

    uint8_t buffer[65536];
    size_t read = fread(buffer, 1, sizeof(buffer), stream);

    for (size_t i = 0; i < PAYLOAD_LENGTH; i++)
    {
        memory[START_ADDRESS + i] = buffer[i + iNES_HEADER_SIZE];
    }

    fclose(stream);
}

int main()
{
    printf("=== 6502 NESTEST Test Suite ===\n\n");

    CPU *cpu = setup_cpu();

    load_test(cpu->memory);

    for (size_t i = 0; i < 0x20; i++)
    {
        cpu->memory[0x4000 + i] = 0xFF;
    }
    cpu->memory[0xA9A9] = 0xA9;

    cpu_push16(cpu, 0x07FF);

    cpu_set_pc(cpu, START_ADDRESS);

    int cycles = 0;

    int step = 1;
    uint16_t pc;
    do
    {
        pc = cpu_get_pc(cpu);
        // uint8_t opcode = cpu->memory[pc];

        cycles += cpu_step(cpu);

        step++;
        if (step > 9000)
        { // Safety limit
            printf("Too many steps, stopping...\n");
            break;
        }
    } while (pc != cpu_get_pc(cpu) && cycles < EXACT_CYCLES &&
             cpu_get_pc(cpu) != END_ADDRESS);

    printf("---\n");
    printf("$02 #$%02X %s\n", cpu->memory[0x02], cpu->memory[0x02] == 0x00 ? "OK" : "FAIL");
    printf("$03 #$%02X %s\n", cpu->memory[0x03], cpu->memory[0x03] == 0x00 ? "OK" : "FAIL");

    teardown_cpu(cpu);

    return 0;
}
