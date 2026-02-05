/**
 * memory.h - Memory subsystem header
 */

#ifndef C64_MEMORY_H
#define C64_MEMORY_H

#include "types.h"

// Forward declaration
typedef struct C64 C64;

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
    bool has_basic;
    bool has_kernal;
    bool has_charom;

    // Reference to system
    C64 *sys;
} MEM;

// Memory functions
void mem_init(MEM *mem, C64 *sys);
void mem_reset(MEM *mem);

// Memory access (calls c64_tick internally)
u8   mem_read(MEM *mem, u16 addr);
void mem_write(MEM *mem, u16 addr, u8 value);

// Direct memory access (no tick, for VIC/DMA)
u8   mem_read_raw(MEM *mem, u16 addr);
void mem_write_raw(MEM *mem, u16 addr, u8 value);

// ROM loading
bool mem_load_rom(MEM *mem, const char *filename, u8 *dest, size_t size);
bool mem_load_roms(MEM *mem, const char *rom_path);

// VIC memory access (uses VIC bank from CIA2)
u8   mem_vic_read(MEM *mem, u16 vic_addr);

void mem_dump(MEM *mem, u16 addr);

#endif // C64_MEMORY_H
