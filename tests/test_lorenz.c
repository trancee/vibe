#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "c64.h"

#define DEBUG false

#define TESTCASE "start" // "cpuport"
#define MAX_STEPS 1300000000

uint16_t load_testcase(C64 *c64, const char *testcase);

char petscii_to_ascii(uint8_t c)
{
    if (c >= 0xC1 && c <= 0xDA)
        return (char)(c - 0xC1 + 65);
    else if (c >= 0x41 && c <= 0x5A)
        return (char)(c - 0x41 + 97);
    else if (c < 32 || c >= 127)
    {
        printf("PETSCII #$%02X\n", c);
        abort();
    }
    else
        return (char)c;
}

// https://www.softwolves.com/arkiv/cbm-hackers/7/7114.html

void dump(C64 *c64, uint16_t addr)
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

        printf("%02X ", c64_read_byte(c64, addr + i));
    }
    printf("\n");
}

void setup_c64(C64 *c64)
{
    c64_init(c64);
    c64_set_debug(c64, DEBUG, NULL);

    // Enable CIA1 Timer A interrupt (like KERNAL does)
    // Write $81 to $DC0D: bit 7 = SET, bit 0 = Timer A
    // c64_write(c64, 0xDC0D, 0x81);
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

void reset(C64 *c64, uint16_t addr, uint8_t data[], size_t size)
{
    c64_reset(c64);

    // // Set up CPU port
    // c64_write_byte(c64, D6510, 0x2F); // Data Direction Register
    // c64_write_byte(c64, R6510, 0x37); // Data Register

    // c64_write_byte(c64, UNUSED, 0x00);

    // c64_write_word(c64, WARM, 0x8000); // 0xA002
    // c64_write_word(c64, PC, 0x7FFF);   // 0x01FE

    // c64_write_word(c64, NMI, 0x8000);   // 0xFFFA
    // c64_write_word(c64, RESET, 0x0800); // 0xFFFC
    // c64_write_word(c64, IRQ, 0xFF48);   // 0xFFFE

    // c64_write_word(c64, ISTOP, 0x0290); // 0x0328

    // // Put RTSes in some of the stubbed calls
    // c64_write_byte(c64, CHROUT, 0x60);  // 0xFFD2
    // c64_write_byte(c64, CARTROM, 0x60); // 0x8000
    // c64_write_byte(c64, READY, 0x60);   // 0xA474

    // // NOP the loading routine
    // c64_write_byte(c64, 0xE16F, 0xEA);

    // // scan keyboard is LDA #3: RTS
    // c64_write_data(c64, GETIN, (uint8_t[]){0xA9, 0x03, 0x60}, 3); // 0xFFE4

    // c64_write_data(c64, PULS, irq_handler, sizeof(irq_handler));

    c64_write_data(c64, addr, data, size);

    c64_reset_pc(c64, addr);
    // cpu_push16(&c64->cpu, 0x7FFF); // Return to WARM trap

    // c64->cpu.P = 0x04; // Interrupt Disable
}

uint16_t load_testcase(C64 *c64, const char *testcase)
{
    char test_path[256];
    snprintf(test_path, sizeof(test_path), "tests/lorenz/%s", testcase);

    FILE *stream = fopen(test_path, "rb");
    if (stream == NULL)
    {
        fprintf(stderr, "Error: could not open \"%s\" testcase.\n", testcase);
        abort();
    }

    fseek(stream, 0L, SEEK_END);
    size_t size = ftell(stream) - 2;
    fseek(stream, 0L, SEEK_SET);

    uint16_t addr = fgetc(stream) | fgetc(stream) << 8;

    uint8_t data[size];

    size_t read = fread(data, 1, sizeof(data), stream);
    fclose(stream);

    assert(read == size);

    reset(c64, addr, data, sizeof(data));

    return addr;
}

int main()
{
    printf("=== MOS 6510 Lorenz Test Suite ===\n");

    C64 c64;
    setup_c64(&c64);

    load_testcase(&c64, TESTCASE);

    size_t cycles = 0;

    size_t step = 1;
    uint16_t pc;
    do
    {
        pc = c64_get_pc(&c64);

        uint8_t mem_config = c64_mem_config(&c64);

        // Check if KERNAL ROM is visible (HIRAM bit set in port data)
        bool KERNAL_ROM = (mem_config & MEM_HIRAM) != 0;

        // Check for trap addresses BEFORE executing
        // Only trap KERNAL calls when KERNAL ROM is actually visible
        if (KERNAL_ROM)
        {
            // CHROUT ($FFD2) - print character
            if (pc == CHROUT)
            {
                // Set $030C = 0
                // Print PETSCII character corresponding to value of A
                // Pop return address from stack
                // Set PC to return address
                // Re-start the CPU

                c64_write_byte(&c64, SAREG, 0x00); // Storage for 6502 .A Register

                uint8_t a = c64.cpu.A;

                switch (a)
                {
                case 10:
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
                uint16_t ret_addr = cpu_pop16(&c64.cpu); // Pop return address from stack
                c64_set_pc(&c64, ret_addr + 1);           // Set PC to return address

                continue;
            }

            // GETIN ($FFE4) - get key, return non-zero to not wait forever
            else if (pc == GETIN)
            {
                c64.cpu.A = 0x03; // Return 3 (RUN/STOP key) - non-zero so loops exit

                set_flag_zero(&c64.cpu, false); // Clear Z so BEQ fails

                // RTS
                uint16_t ret_addr = cpu_pop16(&c64.cpu); // Pop return address from stack
                c64_set_pc(&c64, ret_addr);               // Set PC to return address

                continue;
            }

            // LOAD ($E16F) - load next test (we skip this for single tests)
            else if (pc == 0xE16F)
            {
                // $BB is PETSCII filename address, low byte
                // $BC is PETSCII filename address, high byte
                // $B7 is PETSCII filename length
                // Load the file
                // Pop return address from stack
                // Set PC to $0816
                // Re-start the CPU

                uint16_t addr = c64_read_word(&c64, 0xBB);
                uint8_t size = c64_read_byte(&c64, 0xB7);

                char testcase[size + 1];
                for (size_t i = 0; i < size; i++)
                    testcase[i] = petscii_to_ascii(c64_read_byte(&c64, addr + i));
                testcase[size] = 0;

                load_testcase(&c64, testcase);

                // RTS
                uint16_t ret_addr = cpu_pop16(&c64.cpu); // Pop return address from stack
                c64_set_pc(&c64, 0x0816);

                continue;
            }
        }

        // WARM/CARTROM ($8000) - test passed
        if (pc == 0x8000)
        {
            printf("\033[31;1;6m");
            printf("WARM");
            printf("\033[0m");
            printf("\n");

            break;
        }

        // READY ($A474) - return to BASIC (error)
        if (pc == 0xA474)
        {
            printf("\033[31;1;6m");
            printf("READY");
            printf("\033[0m");
            printf("\n");

            break;
        }

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
