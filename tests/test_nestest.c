#include "../include/mos6510.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
static MOS6510 *setup_cpu()
{
    MOS6510 *cpu = malloc(sizeof(MOS6510));
    mos6510_init(cpu);
    return cpu;
}

static void teardown_cpu(MOS6510 *cpu)
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

    MOS6510 *cpu = setup_cpu();

    load_test(cpu->memory);

    for (size_t i = 0; i < 0x20; i++)
    {
        cpu->memory[0x4000 + i] = 0xFF;
    }
    cpu->memory[0xA9A9] = 0xA9;

    mos6510_push16(cpu, 0x07FF);

    mos6510_set_pc(cpu, START_ADDRESS);

    int cycles = 0;

    int step = 1;
    uint16_t pc;
    do
    {
        pc = mos6510_get_pc(cpu);
        // uint8_t opcode = cpu->memory[pc];

        cycles += mos6510_step(cpu);

        step++;
        if (step > 9000)
        { // Safety limit
            printf("Too many steps, stopping...\n");
            break;
        }
    } while (pc != mos6510_get_pc(cpu) && cycles < EXACT_CYCLES &&
             mos6510_get_pc(cpu) != END_ADDRESS);

    teardown_cpu(cpu);

    // printf("cycles: %d\nexpected %d\n", cycles, opcode_tests[0].exact_cycles);

    return 0;
}
