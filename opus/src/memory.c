/**
 * memory.c - C64 Memory subsystem
 *
 * Implements the PLA memory mapping logic, ROM loading,
 * and Color RAM handling.
 */

#include "memory.h"
#include "c64.h"
#include <stdio.h>
#include <string.h>

void mem_init(C64Memory *mem, C64System *sys)
{
    memset(mem, 0, sizeof(C64Memory));
    mem->sys = sys;
}

void mem_reset(C64Memory *mem)
{
    // Clear RAM (in reality it would have random values)
    memset(mem->ram, 0, sizeof(mem->ram));

    // Initialize Color RAM to light blue (default C64 color)
    memset(mem->color_ram, 0x0E, sizeof(mem->color_ram));
}

// Get current memory configuration from CPU port $01
static u8 get_mem_config(C64Memory *mem)
{
    // Bits 0-2 of $01 control memory mapping
    // We need the effective value, combining output bits from port_data
    // and input bits from external hardware (pullups on bits 0-2)
    u8 port_dir = mem->sys->cpu.port_dir;
    u8 port_data = mem->sys->cpu.port_data;
    
    // For output bits (DDR=1), use port_data
    // For input bits (DDR=0), use external state (bits 0-2 pulled high)
    u8 output_bits = port_dir;
    u8 input_bits = ~port_dir;
    u8 external = 0x07;  // Bits 0-2 pulled high by external resistors
    
    u8 effective = (port_data & output_bits) | (external & input_bits);
    return effective & 0x07;
}

// Check if BASIC ROM is visible
static bool basic_visible(u8 config)
{
    // BASIC visible when LORAM=1 and HIRAM=1
    return (config & MEM_LORAM) && (config & MEM_HIRAM);
}

// Check if KERNAL ROM is visible
static bool kernal_visible(u8 config)
{
    // KERNAL visible when HIRAM=1
    return (config & MEM_HIRAM);
}

// Check if I/O area is visible (vs Char ROM)
static bool io_visible(u8 config)
{
    // I/O visible when CHAREN=1 and (LORAM=1 or HIRAM=1)
    return (config & MEM_CHAREN) && ((config & MEM_LORAM) || (config & MEM_HIRAM));
}

// Check if Char ROM is visible
static bool char_visible(u8 config)
{
    // Char ROM visible when CHAREN=0 and (LORAM=1 or HIRAM=1)
    return !(config & MEM_CHAREN) && ((config & MEM_LORAM) || (config & MEM_HIRAM));
}

// Standard memory read (with tick)
u8 mem_read(C64Memory *mem, u16 addr)
{
    // Tick the system for this memory access
    c64_tick(mem->sys);

    // Check for BA stall (VIC-II has the bus)
    while (mem->sys->vic.ba_low)
        c64_tick(mem->sys);

    u8 result = mem_read_raw(mem, addr);

    if (mem->sys->debug)
        printf("MEM #$%04X → $%02X\n", addr, result);

    return result;
}

// Standard memory write (with tick)
void mem_write(C64Memory *mem, u16 addr, u8 value)
{
    // Tick the system
    c64_tick(mem->sys);

    // Check for BA stall
    while (mem->sys->vic.ba_low)
        c64_tick(mem->sys);

    if (mem->sys->debug)
        printf("MEM #$%04X ← $%02X\n", addr, value);

    mem_write_raw(mem, addr, value);
}

// Raw read without tick (for VIC-II access, initial setup)
u8 mem_read_raw(C64Memory *mem, u16 addr)
{
    u8 config = get_mem_config(mem);

    // Handle different memory regions

    // $A000-$BFFF: BASIC ROM or RAM
    if (addr >= 0xA000 && addr <= 0xBFFF &&
        basic_visible(config) && mem->has_basic)
        return mem->basic_rom[addr - 0xA000];

    // $D000-$DFFF: I/O, Char ROM, or RAM
    else if (addr >= 0xD000 && addr <= 0xDFFF)
    {
        if (io_visible(config))
        {
            // $D000-$D3FF: VIC-II (mirrored every 64 bytes)
            if (addr < 0xD400)
                return vic_read(&mem->sys->vic, addr & 0x3F);

            // $D400-$D7FF: SID (mirrored every 32 bytes)
            else if (addr < 0xD800)
                return sid_read(&mem->sys->sid, addr & 0x1F);

            // $D800-$DBFF: Color RAM
            else if (addr < 0xDC00)
                // Color RAM only stores 4 bits, upper nibble is open bus ($F)
                return 0xF0 | (mem->color_ram[addr - 0xD800] & 0x0F);

            // $DC00-$DCFF: CIA1
            else if (addr < 0xDD00)
                return cia_read(&mem->sys->cia1, addr & 0x0F);

            // $DD00-$DDFF: CIA2
            else if (addr < 0xDE00)
                return cia_read(&mem->sys->cia2, addr & 0x0F);

            // $DE00-$DFFF: I/O expansion area
            else
                return 0xFF; // Open bus
        }

        else if (char_visible(config) && mem->has_charom)
            return mem->char_rom[addr - 0xD000];
    }

    // $E000-$FFFF: KERNAL ROM or RAM
    else if (addr >= 0xE000 && addr <= 0xFFFF &&
             kernal_visible(config) && mem->has_kernal)
        return mem->kernal_rom[addr - 0xE000];

    // $0000-$9FFF: Always RAM
    // $C000-$CFFF: Always RAM
    return mem->ram[addr];
}

// Raw write without tick
void mem_write_raw(C64Memory *mem, u16 addr, u8 value)
{
    u8 config = get_mem_config(mem);

    // Writes always go to RAM underneath ROMs

    // $D000-$DFFF: I/O, or RAM
    if (addr >= 0xD000 && addr <= 0xDFFF && io_visible(config))
    {
        // VIC-II
        if (addr < 0xD400)
            vic_write(&mem->sys->vic, addr & 0x3F, value);

        // SID
        else if (addr < 0xD800)
            sid_write(&mem->sys->sid, addr & 0x1F, value);

        // Color RAM
        else if (addr < 0xDC00)
            mem->color_ram[addr - 0xD800] = value & 0x0F;

        // CIA1
        else if (addr < 0xDD00)
            cia_write(&mem->sys->cia1, addr & 0x0F, value);

        // CIA2
        else if (addr < 0xDE00)
            cia_write(&mem->sys->cia2, addr & 0x0F, value);

        // I/O expansion - ignore writes
    }
    else
        // $0000-$CFFF: RAM
        // $E000-$FFFF: RAM (under KERNAL)
        mem->ram[addr] = value;
}

// VIC memory read (uses VIC bank from CIA2)
u8 mem_vic_read(C64Memory *mem, u16 vic_addr)
{
    // VIC sees a 16KB window based on CIA2 port A bits 0-1
    u16 bank = c64_get_vic_bank(mem->sys);
    u16 addr = bank + (vic_addr & 0x3FFF);

    // VIC sees Character ROM at $1000-$1FFF and $9000-$9FFF within its bank
    // (only in banks 0 and 2)
    u16 bank_offset = vic_addr & 0x3FFF;
    if ((bank_offset >= 0x1000 && bank_offset < 0x2000) ||
        (bank_offset >= 0x9000 && bank_offset < 0xA000))
    {
        // Check if this maps to Character ROM
        // Banks 0 ($0000) and 2 ($8000) see Char ROM
        if (bank == 0x0000 || bank == 0x8000)
        {
            if (mem->has_charom)
                return mem->char_rom[bank_offset & 0x0FFF];
        }
    }

    return mem->ram[addr];
}

// Load a ROM file
bool mem_load_rom(C64Memory *mem, const char *filename, u8 *dest, size_t size)
{
    (void)mem; // Unused but kept for API consistency

    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        fprintf(stderr, "Failed to open ROM: %s\n", filename);
        return false;
    }

    size_t read = fread(dest, 1, size, f);
    fclose(f);

    if (read != size)
    {
        fprintf(stderr, "ROM file %s: expected %zu bytes, got %zu\n",
                filename, size, read);
        return false;
    }

    return true;
}

// Load all ROMs from a directory
bool mem_load_roms(C64Memory *mem, const char *rom_path)
{
    char filename[512];

    // Load BASIC ROM
    snprintf(filename, sizeof(filename), "%s/basic.rom", rom_path);
    mem->has_basic = mem_load_rom(mem, filename, mem->basic_rom, C64_BASIC_SIZE);

    // Load KERNAL ROM
    snprintf(filename, sizeof(filename), "%s/kernal.rom", rom_path);
    mem->has_kernal = mem_load_rom(mem, filename, mem->kernal_rom, C64_KERNAL_SIZE);

    // Load Character ROM
    snprintf(filename, sizeof(filename), "%s/char.rom", rom_path);
    mem->has_charom = mem_load_rom(mem, filename, mem->char_rom, C64_CHAR_SIZE);

    if (mem->has_basic)
        printf("Loaded BASIC ROM\n");
    if (mem->has_kernal)
        printf("Loaded KERNAL ROM\n");
    if (mem->has_charom)
        printf("Loaded Character ROM\n");

    return mem->has_basic && mem->has_kernal && mem->has_charom;
}

void mem_dump(C64Memory *mem, u16 addr)
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

        printf("%02X ", mem_read_raw(mem, addr + i));
    }
    printf("\n");
}
