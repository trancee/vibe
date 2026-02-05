#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include <stdbool.h>

#define MEM_START 0x0000
#define MEM_END (MEM_START + MEM_SIZE - 1)
#define MEM_SIZE 0x10000

typedef uint8_t (*mem_read_fn)(void *ctx, uint16_t addr);
typedef void (*mem_write_fn)(void *ctx, uint16_t addr, uint8_t data);

// Memory structure
typedef struct
{
    uint8_t ram[MEM_SIZE]; // 64KB RAM

    mem_read_fn read_fn;   // Optional read callback
    mem_write_fn write_fn; // Optional write callback
    void *ctx;             // Context for callbacks
} MEM;

void mem_init(MEM *mem);
void mem_reset(MEM *mem);

void mem_set_read_write(MEM *mem, mem_read_fn read_fn, mem_write_fn write_fn, void *ctx);

uint8_t mem_read_raw(void *mem, uint16_t addr);
uint8_t mem_read(MEM *mem, uint16_t addr);
uint8_t mem_read_byte(MEM *mem, uint16_t addr);
uint16_t mem_read_word(MEM *mem, uint16_t addr);
uint16_t mem_read_word_zp(MEM *mem, uint16_t addr);

void mem_write_raw(void *mem, uint16_t addr, uint8_t data);
void mem_write(MEM *mem, uint16_t addr, uint8_t data);
void mem_write_byte(MEM *mem, uint16_t addr, uint8_t data);
void mem_write_word(MEM *mem, uint16_t addr, uint16_t data);

void mem_write_data(MEM *mem, uint16_t addr, uint8_t data[], size_t size);

void mem_dump(MEM *mem, uint16_t addr);

#endif // MEM_H