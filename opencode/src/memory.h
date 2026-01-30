#ifndef MEMORY_H
#define MEMORY_H

#include "c64.h"
#include <stddef.h>

void mem_init(C64* sys);
int mem_load_rom(C64* sys, const char* filename, u8* buffer, size_t size);
u8 mem_read(C64* sys, u16 addr);
void mem_write(C64* sys, u16 addr, u8 data);

#endif