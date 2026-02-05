#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"

void mem_init(MEM *mem)
{
    memset(mem, 0x00, sizeof(MEM));

    mem->read_fn = mem_read_raw;
    mem->write_fn = mem_write_raw;
    mem->ctx = mem;
}

void mem_reset(MEM *mem)
{
    // Clear RAM (in reality it would have random values)
    memset(mem->ram, 0, sizeof(mem->ram));
}

/* ============================================================
   Memory Handler
   ============================================================ */

void mem_set_read_write(MEM *mem, mem_read_fn read_fn, mem_write_fn write_fn, void *ctx)
{
    mem->read_fn = read_fn;
    mem->write_fn = write_fn;
    mem->ctx = ctx;
}

/* ============================================================
   Memory Read
   ============================================================ */

size_t read_counter = 0;
uint8_t mem_read_raw(void *mem, uint16_t addr)
{
    printf("MEM #$%04X → $%02X\n", addr, ((MEM *)mem)->ram[addr]);
    if (read_counter++ > 10)
        abort();
    return ((MEM *)mem)->ram[addr];
}

uint8_t mem_read(MEM *mem, uint16_t addr)
{
    printf("MEM #$%04X → $%02X %p\n", addr, mem->ram[addr], mem->read_fn);

    return mem->read_fn(mem->ctx, addr);
}
uint8_t mem_read_byte(MEM *mem, uint16_t addr)
{
    return mem_read(mem, addr);
}
uint16_t mem_read_word(MEM *mem, uint16_t addr)
{
    return mem_read_byte(mem, addr) | (mem_read_byte(mem, addr + 1) << 8);
}
uint16_t mem_read_word_zp(MEM *mem, uint16_t addr)
{
    return mem_read_byte(mem, addr) | (mem_read_byte(mem, ((addr + 1) & 0x00FF) | (addr & 0xFF00)) << 8);
}

/* ============================================================
   Memory Write
   ============================================================ */

size_t write_counter = 0;
void mem_write_raw(void *mem, uint16_t addr, uint8_t data)
{
    printf("MEM #$%04X ← $%02X\n", addr, data);
    if (write_counter++ > 10)
        abort();
    ((MEM *)mem)->ram[addr] = data;
}

void mem_write(MEM *mem, uint16_t addr, uint8_t data)
{
    printf("MEM #$%04X ← $%02X %p\n", addr, data, mem->write_fn);

    mem->write_fn(mem->ctx, addr, data);
}
void mem_write_byte(MEM *mem, uint16_t addr, uint8_t data)
{
    mem_write(mem, addr, data);
}
void mem_write_word(MEM *mem, uint16_t addr, uint16_t data)
{
    mem_write_byte(mem, addr, data & 0xFF);
    mem_write_byte(mem, addr + 1, (data >> 8) & 0xFF);
}

void mem_write_data(MEM *mem, uint16_t addr, uint8_t data[], size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        mem_write(mem, addr + i, data[i]);
    }
}

/* ============================================================
   Memory Dump
   ============================================================ */

void mem_dump(MEM *mem, uint16_t addr)
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

        printf("%02X ", mem_read(mem, addr + i));
    }
    printf("\n");
}
