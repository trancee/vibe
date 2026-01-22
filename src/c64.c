#include <stdlib.h>

#include "c64.h"

uint8_t c64_read(void *c64, uint16_t addr)
{
    return c64_read_byte((C64 *)c64, addr);
}
void c64_write(void *c64, uint16_t addr, uint8_t data)
{
    c64_write_byte((C64 *)c64, addr, data);
}

void c64_init(C64 *c64, bool debug)
{
    cpu_init(&c64->cpu, debug, c64_read, c64_write);

    /* Load ROMs */
    c64_load_rom(c64, "roms/basic.901226-01.bin", 0xA000, 0x2000);
    c64_load_rom(c64, "roms/characters.906143-02.bin", 0xD000, 0x1000);
    c64_load_rom(c64, "roms/kernal.901227-03.bin", 0xE000, 0x2000);

    /* Reset chips */
    c64_reset(c64);
}

void c64_reset(C64 *c64)
{
    cpu_reset(&c64->cpu);

    cia_reset(&c64->cia1);
    cia_reset(&c64->cia2);

    vic_reset(&c64->vic, c64->cpu.memory);

    // sid_reset(SAMPLE_RATE);
}
void c64_reset_pc(C64 *c64, uint16_t addr)
{
    cpu_reset_pc(&c64->cpu, addr);
}

uint16_t c64_get_pc(C64 *c64)
{
    return c64->cpu.pc;
}
void c64_set_pc(C64 *c64, uint16_t addr)
{
    c64->cpu.pc = addr;
}

uint8_t c64_read_byte(C64 *c64, uint16_t addr)
{
    // printf("C64 READ #$%04X\n", addr);
    if (addr >= VIC_MEM_START && addr <= VIC_MEM_END)
    {
        return vic_read(&c64->vic, addr);
    }

    return cpu_read(&c64->cpu, addr);
}
uint16_t c64_read_word(C64 *c64, uint16_t addr)
{
    return cpu_read_word(&c64->cpu, addr);
}

void c64_write_byte(C64 *c64, uint16_t addr, uint8_t data)
{
    // printf("C64 WRITE #$%04X = $%02X\n", addr, data);
    if (addr >= VIC_MEM_START && addr <= VIC_MEM_END)
    {
        return vic_write(&c64->vic, addr, data);
    }

    cpu_write(&c64->cpu, addr, data);
}
void c64_write_word(C64 *c64, uint16_t addr, uint16_t data)
{
    cpu_write_word(&c64->cpu, addr, data);
}

void c64_write_data(C64 *c64, uint16_t addr, uint8_t data[], size_t size)
{
    cpu_write_data(&c64->cpu, addr, data, size);
}

bool c64_trap(C64 *c64, uint16_t addr, handler_t handler)
{
    return cpu_trap(&c64->cpu, addr, handler);
}

uint8_t c64_step(C64 *c64)
{
    uint8_t steps = cpu_step(&c64->cpu);

    vic_clock(&c64->vic /*, CYCLES_PER_FRAME*/);
    vic_clock(&c64->vic /*, CYCLES_PER_FRAME*/);

    cia_clock(&c64->cia1 /*, CYCLES_PER_FRAME*/);
    cia_clock(&c64->cia2 /*, CYCLES_PER_FRAME*/);

    // sid_clock(&sid, CYCLES_PER_FRAME);

    return steps;
}

/* ============================================================
   ROM Loading
   ============================================================ */

void c64_load_rom(C64 *c64, const char *path, uint16_t addr, size_t size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        printf("Missing ROM: %s\n", path);
        exit(1);
    }
    fread(&c64->cpu.memory[addr], 1, size, f);
    fclose(f);
}
