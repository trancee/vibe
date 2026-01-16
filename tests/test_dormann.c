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

void load_test(uint8_t memory, char *test_name)
{
    char test_path[256];
    snprintf(test_path, sizeof(test_path), "tests/dormann/%s.bin", test_name);

    FILE *ptr = fopen(test_path, "rb");
    if (ptr == NULL)
    {
        fprintf(stderr, "Error: Could not open test binary.\n");
        exit(1);
    }

    fread(memory, sizeof(memory), 1, ptr);

    fclose(ptr);
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
        MOS6510 *cpu = setup_cpu();
        load_test(cpu->memory, opcode_tests[i].test_name);

        mos6510_set_pc(cpu, opcode_tests[i].start_address);
        uint8_t cycles = mos6510_step(cpu);

        teardown_cpu(cpu);
    }

    return 0;
}
