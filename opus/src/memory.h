/**
 * memory.h - Memory subsystem header
 */

#ifndef C64_MEMORY_H
#define C64_MEMORY_H

#include "types.h"

// Forward declaration
typedef struct C64System C64System;

// Memory configuration bits (from $01)
#define MEM_LORAM   0x01  // BASIC ROM visible
#define MEM_HIRAM   0x02  // KERNAL ROM visible
#define MEM_CHAREN  0x04  // Char ROM visible (when I/O not mapped)

// Memory subsystem structure
typedef struct {
    u8 ram[C64_RAM_SIZE];           // 64KB RAM
    u8 color_ram[C64_COLOR_RAM_SIZE]; // 1KB Color RAM
    u8 basic_rom[C64_BASIC_SIZE];   // 8KB BASIC ROM
    u8 kernal_rom[C64_KERNAL_SIZE]; // 8KB KERNAL ROM
    u8 char_rom[C64_CHAR_SIZE];     // 4KB Character ROM

    // ROM loaded flags
    bool basic_loaded;
    bool kernal_loaded;
    bool char_loaded;

    // Reference to system
    C64System *sys;
} C64Memory;

// Memory functions
void mem_init(C64Memory *mem, C64System *sys);
void mem_reset(C64Memory *mem);

// Memory access (calls c64_tick internally)
u8   mem_read(C64Memory *mem, u16 addr);
void mem_write(C64Memory *mem, u16 addr, u8 value);

// Direct memory access (no tick, for VIC/DMA)
u8   mem_read_raw(C64Memory *mem, u16 addr);
void mem_write_raw(C64Memory *mem, u16 addr, u8 value);

// ROM loading
bool mem_load_rom(C64Memory *mem, const char *filename, u8 *dest, size_t size);
bool mem_load_roms(C64Memory *mem, const char *rom_path);

// VIC memory access (uses VIC bank from CIA2)
u8   mem_vic_read(C64Memory *mem, u16 vic_addr);

void mem_dump(C64Memory *mem, u16 addr);

#endif // C64_MEMORY_H
