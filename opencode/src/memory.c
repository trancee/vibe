#include "c64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



void mem_init(C64* sys) {
    memset(sys->mem.ram, 0, sizeof(sys->mem.ram));
    memset(sys->mem.color_ram, 0, sizeof(sys->mem.color_ram));
    
    sys->mem.cpu_port_dir = 0;
    sys->mem.cpu_port_data = 0;
    sys->mem.cpu_port_floating = true;
    sys->mem.cycle_count = 0;
}

int mem_load_rom(C64* sys, const char* filename, u8* buffer, size_t size) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        printf("Failed to open ROM file: %s\n", filename);
        return -1;
    }
    
    size_t read = fread(buffer, 1, size, f);
    fclose(f);
    
    if (read != size) {
        printf("Warning: ROM file size mismatch (expected %zu, got %zu)\n", size, read);
    }
    
    return 0;
}

static u8 get_cpu_port_value(C64* sys) {
    if (sys->mem.cpu_port_floating) {
        u8 value = sys->mem.cpu_port_data;
        
        u8 floating_bits = ~sys->mem.cpu_port_dir & 0xFF;
        if (floating_bits & 0x3F) {
            value |= 0x3F;
        }
        
        if ((floating_bits & 0x20) && ((sys->mem.cpu_port_data & 0x20) == 0)) {
            value &= ~0x20;
        }
        
        return value;
    }
    
    return sys->mem.cpu_port_data;
}

u8 mem_read(C64* sys, u16 addr) {
    c64_tick(sys);
    
    if (addr < 0x0001) {
        if (addr == 0x0000) {
            return sys->mem.cpu_port_dir;
        } else {
            return get_cpu_port_value(sys);
        }
    }
    
    if (addr < 0x8000) {
        return sys->mem.ram[addr];
    }
    
    if (addr < 0xA000) {
        return sys->mem.ram[addr];
    }
    
    if (addr < 0xC000) {
        if (sys->mem.cpu_port_data & 0x20) {
            return sys->mem.basic_rom[addr - 0xA000];
        }
        return sys->mem.ram[addr];
    }
    
    if (addr < 0xD000) {
        return sys->mem.ram[addr];
    }
    
    if (addr < 0xE000) {
        return sys->mem.ram[addr];
    }
    
    if (addr < 0x10000) {
        if (sys->mem.cpu_port_data & 0x10) {
            return sys->mem.kernal_rom[addr - 0xE000];
        }
        return sys->mem.ram[addr];
    }
    
    return 0;
}

void mem_write(C64* sys, u16 addr, u8 data) {
    c64_tick(sys);
    
    if (addr < 0x0001) {
        if (addr == 0x0000) {
            sys->mem.cpu_port_dir = data;
            sys->mem.cpu_port_floating = (data == 0x00 || data == 0xFF);
        } else {
            sys->mem.cpu_port_data = data;
            sys->mem.cpu_port_floating = (sys->mem.cpu_port_dir == 0x00 || sys->mem.cpu_port_dir == 0xFF);
        }
        return;
    }
    
    if (addr < 0x8000) {
        sys->mem.ram[addr] = data;
        return;
    }
    
    if (addr < 0xD000) {
        sys->mem.ram[addr] = data;
        return;
    }
    
    if (addr >= 0xE000) {
        sys->mem.ram[addr] = data;
        return;
    }
}