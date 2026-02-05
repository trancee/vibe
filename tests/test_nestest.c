#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mos6510.h"

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

MEM *mem;
CPU *cpu;

// Helper functions
static CPU *setup_cpu(FILE *stream)
{
    mem = malloc(sizeof(MEM));
    mem_init(mem);

    cpu = malloc(sizeof(CPU));
    cpu_init(cpu, mem);

    cpu_set_decimal_mode(cpu, false);
    cpu_set_debug(cpu, DEBUG, stream);

    return cpu;
}

static void teardown()
{
    free(mem);
    free(cpu);
}

void load_test()
{
    FILE *stream = fopen("tests/nestest/nestest.nes", "rb");
    if (stream == NULL)
    {
        fprintf(stderr, "Error: Could not open test binary.\n");
        exit(1);
    }

    uint8_t buffer[MEM_SIZE];
    size_t read = fread(buffer, 1, sizeof(buffer), stream);

    for (size_t i = 0; i < PAYLOAD_LENGTH; i++)
    {
        mem->ram[START_ADDRESS + i] = buffer[i + iNES_HEADER_SIZE];
    }

    fclose(stream);
}

int main()
{
    printf("=== 6502 NESTEST Test Suite ===\n\n");

    FILE *log = fopen("tests/nestest/nestest.log", "r");
    if (log == NULL)
    {
        fprintf(stderr, "Error: Could not open nestest.log\n");
        exit(1);
    }

    char *buffer = NULL;
    size_t buffer_size = 0;
    size_t buffer_position = 0;
    FILE *stream = open_memstream(&buffer, &buffer_size);

    CPU *cpu = setup_cpu(stream);

    load_test();

    for (size_t i = 0; i < 0x20; i++)
    {
        mem_write_byte(cpu->mem, 0x4000 + i, 0xFF);
    }
    mem_write_byte(cpu->mem, 0xA9A9, 0xA9);

    cpu_push16(cpu, 0x07FF);

    cpu_set_pc(cpu, START_ADDRESS);

    size_t cycles = 0;

    size_t step = 1;
    uint16_t pc;
    do
    {
        pc = cpu_get_pc(cpu);
        // uint8_t opcode = cpu->memory[pc];

        cycles += cpu_step(cpu);

        {
            char *line = NULL;
            size_t length = 0;
            size_t read;

            if ((read = getline(&line, &length, log)) != -1)
            {
                fflush(stream);

                if (strncmp(buffer + buffer_position, line, read - 1 - 1) != 0)
                {
                    printf("%s\x{1B}[31;1;6m✗ differ\x{1B}[0m\n", line);
                    // mem_dump(cpu->mem, 0x0000);
                    exit(1);
                }

                buffer_position += read - 1;
            }

            if (line)
                free(line);
        }

        step++;
        if (step > 9000)
        { // Safety limit
            printf("Too many steps, stopping...\n");
            break;
        }
    } while (pc != cpu_get_pc(cpu) && cycles < EXACT_CYCLES &&
             cpu_get_pc(cpu) != END_ADDRESS);

    fclose(stream);
    free(buffer);

    printf("---\n");
    printf("$02 #$%02X %s\n", mem_read_byte(cpu->mem, 0x02), mem_read_byte(cpu->mem, 0x02) == 0x00 ? "OK" : "FAIL");
    printf("$03 #$%02X %s\n", mem_read_byte(cpu->mem, 0x03), mem_read_byte(cpu->mem, 0x03) == 0x00 ? "OK" : "FAIL");

    teardown();

    fclose(log);

    return 0;
}
