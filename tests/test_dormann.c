#include "../include/mos6510.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

void load_test(uint8_t *memory, const char *test_name)
{
    char test_path[256];
    snprintf(test_path, sizeof(test_path), "tests/dormann/%s.bin", test_name);

    FILE *stream = fopen(test_path, "rb");
    if (stream == NULL)
    {
        fprintf(stderr, "Error: Could not open test binary.\n");
        exit(1);
    }

    uint8_t buffer[65536];
    size_t read = fread(buffer, 1, sizeof(buffer), stream);

    for (size_t i = 0; i < read; i++)
    {
        memory[i] = buffer[i];
    }

    fclose(stream);
}

typedef struct
{
    char test_name[32];
    uint16_t start_address;
    uint16_t end_address;
    uint16_t test_case_address;
    int exact_cycles;
} test_case_t;

static const test_case_t opcode_tests[] = {
    {"6502_functional_test", 0x0400, 0x3469, 0x0200, 96247422}, // 05-jan-2020
};

int main()
{
    printf("=== MOS 6510 Dormann Test Suite ===\n\n");

    for (size_t i = 0; i < sizeof(opcode_tests) / sizeof(opcode_tests[0]); i++)
    {
        printf("--- %s\n", opcode_tests[i].test_name);

        MOS6510 *cpu = setup_cpu();
        load_test(cpu->memory, opcode_tests[i].test_name);

        mos6510_set_pc(cpu, opcode_tests[i].start_address);

        int cycles = 0;

        int step = 1;
        uint16_t pc;
        do
        {
            pc = mos6510_get_pc(cpu);
            uint8_t opcode = cpu->memory[pc];

            cycles += mos6510_step(cpu);

            step++;
            if (step > 50000)
            { // Safety limit
                printf("Too many steps, stopping...\n");
                break;
            }
        } while (pc != mos6510_get_pc(cpu) && cycles < opcode_tests[i].exact_cycles &&
                 mos6510_get_pc(cpu) != opcode_tests[i].end_address);

        teardown_cpu(cpu);

        // printf("cycles: %d\nexpected %d\n", cycles, opcode_tests[0].exact_cycles);
    }

    return 0;
}
