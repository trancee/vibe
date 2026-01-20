#include "../include/mos6510.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define DEBUG true

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
    {"6502_functional_test", 0x0400, 0x3469, 0x0200, 96247422},        // 05-jan-2020
    {"65C02_extended_opcodes_test", 0x0400, 0x24F1, 0x0202, 95340970}, // 04-dec-2017
};

int main()
{
    printf("=== MOS 6510 Dormann Test Suite ===\n");

    for (size_t i = 0; i < sizeof(opcode_tests) / sizeof(opcode_tests[0]); i++)
    {
        test_case_t test_case = opcode_tests[i];
        printf("\n--- %s\n", test_case.test_name);

        CPU *cpu = setup_cpu();
        load_test(cpu->memory, test_case.test_name);

        cpu_set_pc(cpu, test_case.start_address);

        int cycles = 0;

        int step = 1;
        uint16_t pc;
        uint8_t last_test = -1;
        do
        {
            pc = cpu_get_pc(cpu);
            if (cpu->memory[test_case.test_case_address] != last_test)
            {
                last_test = cpu->memory[test_case.test_case_address];
                printf("test case #$%02X at $%04X\n", last_test, pc);
            }

            uint8_t opcode = cpu->memory[pc];

            cycles += cpu_step(cpu);

            step++;
            if (step > 50000)
            { // Safety limit
                printf("Too many steps, stopping...\n");
                break;
            }
        } while (pc != cpu_get_pc(cpu) && cycles < test_case.exact_cycles &&
                 cpu_get_pc(cpu) != test_case.end_address);

        printf("---\n");
        if (cpu_get_pc(cpu) == test_case.end_address)
        {
            printf("test passed\n");
        }
        else
        {
            printf("test failed at $%04X\n", cpu_get_pc(cpu));
        }

        teardown_cpu(cpu);
    }

    return 0;
}
