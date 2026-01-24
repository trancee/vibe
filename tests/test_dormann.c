#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mos6510.h"

#define DEBUG true

void reset_handler(CPU *cpu)
{
    printf("\x{1B}[31;1;6mRESET\x{1B}[0m");
    abort();
}
void nmi_handler(CPU *cpu)
{
    printf("\x{1B}[31;1;6mNMI\x{1B}[0m");
    abort();
}
void irq_handler(CPU *cpu)
{
    printf("\x{1B}[31;1;6mIRQ\x{1B}[0m");
    abort();
}

// Helper functions
static CPU *setup_cpu()
{
    CPU *cpu = malloc(sizeof(CPU));
    cpu_init(cpu, DEBUG, NULL, NULL);

    cpu_trap(cpu, RESET, reset_handler);
    cpu_trap(cpu, NMI, nmi_handler);
    cpu_trap(cpu, IRQ, irq_handler);

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

typedef int (*test_t)(CPU *, void *test_case);

typedef struct
{
    char test_name[32];
    uint16_t start_address;
    uint16_t end_address;
    // uint16_t test_case_address;
    test_t test_function;
    int exact_cycles;
} test_case_t;

uint8_t last_test = -1;
uint16_t test_case_address = 0x0200;

int functional_test(CPU *cpu, test_case_t test_case)
{
    uint8_t pc = cpu_get_pc(cpu);

    if (cpu->memory[test_case_address] != last_test)
    {
        last_test = cpu->memory[test_case_address];
        printf("test case #$%02X at $%04X\n", last_test, pc);
    }

    return cpu_step(cpu);
}

uint16_t feedback_address = 0xBFFC;
uint8_t irq_bit = 1 << 0;
uint8_t nmi_bit = 1 << 1;
uint16_t nmi_count_address = 0x0200;
uint16_t irq_count_address = 0x0201;
uint16_t brk_count_address = 0x0202;

int interrupt_test(CPU *cpu, test_case_t test_case)
{
    uint8_t pc = cpu_get_pc(cpu);

    if (pc == test_case.start_address)
    {
        // Reset counts
        cpu_write(cpu, nmi_count_address, 0x00);
        cpu_write(cpu, irq_count_address, 0x00);
        cpu_write(cpu, brk_count_address, 0x00);

        cpu_write(cpu, feedback_address, 0x00);
    }

    int cycles = cpu_step(cpu);

    uint8_t feedback = cpu_read(cpu, feedback_address);

    if ((feedback & nmi_bit))
    {
        printf("*** Triggering NMI\n");
        cpu_write(cpu, feedback_address, feedback & ~nmi_bit);
        cpu_nmi(cpu);
    }
    else if ((feedback & irq_bit))
    {
        printf("*** Triggering IRQ\n");
        cpu_write(cpu, feedback_address, feedback & ~irq_bit);
        cpu_irq(cpu);
    }

    if (cpu_get_pc(cpu) == pc) // End of test
    {
        printf("---\n");
        printf("NMI count: $%02X\n", cpu_read(cpu, nmi_count_address));
        printf("IRQ count: $%02X\n", cpu_read(cpu, irq_count_address));
        printf("BRK count: $%02X\n", cpu_read(cpu, brk_count_address));
    }

    return cycles;
}

int extended_opcodes_test(CPU *cpu, test_case_t test_case)
{
    uint16_t test_case_address = 0x0202;

    printf("65C02 Extended Opcodes Test Handler\n");
    abort();
}

static const test_case_t opcode_tests[] = {
    // {"6502_functional_test", 0x0400, 0x3469, functional_test, 96247422},              // 05-jan-2020
    {"6502_interrupt_test", 0x0400, 0x06F5, (void *)interrupt_test, 2929}, // 15-aug-2014
    // {"65C02_extended_opcodes_test", 0x0400, 0x24F1, extended_opcodes_test, 95340970}, // 04-dec-2017
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
        last_test = -1;
        do
        {
            pc = cpu_get_pc(cpu);

            cycles += test_case.test_function(cpu, &test_case);

            step++;
            if (step > 100000000)
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
