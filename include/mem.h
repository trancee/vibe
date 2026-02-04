#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include <stdbool.h>

#define MEM_START 0x0000
#define MEM_END (MEM_START + MEM_SIZE - 1)
#define MEM_SIZE 0x10000

// Memory structure
typedef struct
{
    uint8_t ram[MEM_SIZE];           // 64KB RAM
} MEM;

void mem_init(MEM *mem);
void mem_reset(MEM *mem);

uint8_t mem_read(MEM *mem, uint16_t addr);
uint8_t mem_read_byte(MEM *mem, uint16_t addr);
uint16_t mem_read_word(MEM *mem, uint16_t addr);
uint16_t mem_read_word_zp(MEM *mem, uint16_t addr);

void mem_write(MEM *mem, uint16_t addr, uint8_t data);
void mem_write_byte(MEM *mem, uint16_t addr, uint8_t data);
void mem_write_word(MEM *mem, uint16_t addr, uint16_t data);

void mem_write_data(MEM *mem, uint16_t addr, uint8_t data[], size_t size);

void mem_dump(MEM *mem, uint16_t addr);

#endif // MEM_H