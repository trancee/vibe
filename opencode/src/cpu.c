#include "c64.h"
#include <string.h>
#include <stdio.h>

static u8 mem_read(C64* sys, u16 addr);
static void mem_write(C64* sys, u16 addr, u8 data);

#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_R 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80

static void cpu_set_flag(C64CPU* cpu, u8 flag, bool set) {
    if (set) {
        cpu->status |= flag;
    } else {
        cpu->status &= ~flag;
    }
}

bool cpu_get_flag(C64CPU* cpu, u8 flag) {
    return (cpu->status & flag) != 0;
}

static u8 cpu_get_zn(C64CPU* cpu, u8 value) {
    cpu_set_flag(cpu, FLAG_Z, value == 0);
    cpu_set_flag(cpu, FLAG_N, (value & 0x80) != 0);
    return value;
}

static void cpu_push(C64* sys, u8 value) {
    sys->mem.ram[0x0100 + sys->cpu.sp] = value;
    sys->cpu.sp--;
}

static u8 cpu_pull(C64* sys) {
    sys->cpu.sp++;
    return sys->mem.ram[0x0100 + sys->cpu.sp];
}

static void cpu_push16(C64* sys, u16 value) {
    cpu_push(sys, (value >> 8) & 0xFF);
    cpu_push(sys, value & 0xFF);
}

static u16 cpu_pull16(C64* sys) {
    u8 lo = cpu_pull(sys);
    u8 hi = cpu_pull(sys);
    return lo | (hi << 8);
}

static u8 cpu_adc_bcd(C64CPU* cpu, u8 acc, u8 data) {
    u16 result = acc + data + (cpu_get_flag(cpu, FLAG_C) ? 1 : 0);
    
    if (cpu_get_flag(cpu, FLAG_D)) {
        u16 al = (acc & 0x0F) + (data & 0x0F) + (cpu_get_flag(cpu, FLAG_C) ? 1 : 0);
        u16 ah = (acc >> 4) + (data >> 4) + (al > 0x09 ? 1 : 0);
        
        if (al > 0x09) {
            al += 0x06;
        }
        if (ah > 0x09) {
            ah += 0x06;
        }
        
        result = (ah << 4) | (al & 0x0F);
        cpu_set_flag(cpu, FLAG_C, ah > 0x0F);
        
        bool v = !((acc ^ data) & 0x80) && ((acc ^ result) & 0x80);
        cpu_set_flag(cpu, FLAG_V, v);
    } else {
        bool v = !((acc ^ data) & 0x80) && ((acc ^ result) & 0x80);
        cpu_set_flag(cpu, FLAG_V, v);
        cpu_set_flag(cpu, FLAG_C, result > 0xFF);
    }
    
    return cpu_get_zn(cpu, (u8)result);
}

static u8 cpu_sbc_bcd(C64CPU* cpu, u8 acc, u8 data) {
    u16 result = acc - data - (cpu_get_flag(cpu, FLAG_C) ? 0 : 1);
    
    if (cpu_get_flag(cpu, FLAG_D)) {
        u16 al = (acc & 0x0F) - (data & 0x0F) - (cpu_get_flag(cpu, FLAG_C) ? 0 : 1);
        u16 ah = (acc >> 4) - (data >> 4) - 1;
        
        if (al & 0x10) {
            al -= 0x06;
            ah++;
        }
        if (ah & 0x10) {
            ah -= 0x06;
        }
        
        result = (ah << 4) | (al & 0x0F);
        cpu_set_flag(cpu, FLAG_C, (result & 0x100) == 0);
        
        bool v = ((acc ^ data) & 0x80) && ((acc ^ result) & 0x80);
        cpu_set_flag(cpu, FLAG_V, v);
    } else {
        bool v = ((acc ^ data) & 0x80) && ((acc ^ result) & 0x80);
        cpu_set_flag(cpu, FLAG_V, v);
        cpu_set_flag(cpu, FLAG_C, (result & 0x100) == 0);
    }
    
    return cpu_get_zn(cpu, (u8)result);
}

static u16 addr_mode_immediate(C64* sys) {
    return sys->cpu.pc++;
}

static u16 addr_mode_zero_page(C64* sys) {
    u8 addr = mem_read(sys, sys->cpu.pc++);
    return addr;
}

static u16 addr_mode_zero_page_x(C64* sys) {
    u8 addr = mem_read(sys, sys->cpu.pc++);
    sys->cpu.page_crossed = ((addr + sys->cpu.x) & 0xFF) < addr;
    return (addr + sys->cpu.x) & 0xFF;
}

static u16 addr_mode_zero_page_y(C64* sys) {
    u8 addr = mem_read(sys, sys->cpu.pc++);
    sys->cpu.page_crossed = ((addr + sys->cpu.y) & 0xFF) < addr;
    return (addr + sys->cpu.y) & 0xFF;
}

static u16 addr_mode_absolute(C64* sys) {
    u8 lo = mem_read(sys, sys->cpu.pc++);
    u8 hi = mem_read(sys, sys->cpu.pc++);
    return lo | (hi << 8);
}

static u16 addr_mode_absolute_x(C64* sys) {
    u8 lo = mem_read(sys, sys->cpu.pc++);
    u8 hi = mem_read(sys, sys->cpu.pc++);
    u16 addr = lo | (hi << 8);
    sys->cpu.page_crossed = ((addr + sys->cpu.x) & 0xFF00) != (addr & 0xFF00);
    return addr + sys->cpu.x;
}

static u16 addr_mode_absolute_y(C64* sys) {
    u8 lo = mem_read(sys, sys->cpu.pc++);
    u8 hi = mem_read(sys, sys->cpu.pc++);
    u16 addr = lo | (hi << 8);
    sys->cpu.page_crossed = ((addr + sys->cpu.y) & 0xFF00) != (addr & 0xFF00);
    return addr + sys->cpu.y;
}

static u16 addr_mode_indirect(C64* sys) {
    u8 lo = mem_read(sys, sys->cpu.pc++);
    u8 hi = mem_read(sys, sys->cpu.pc++);
    u16 addr = lo | (hi << 8);
    
    u8 new_lo = mem_read(sys, addr);
    u8 new_hi = mem_read(sys, (addr & 0xFF00) | ((addr + 1) & 0xFF));
    
    return new_lo | (new_hi << 8);
}

static u16 addr_mode_indexed_indirect(C64* sys) {
    u8 addr = mem_read(sys, sys->cpu.pc++);
    u8 lo = mem_read(sys, (addr + sys->cpu.x) & 0xFF);
    u8 hi = mem_read(sys, (addr + sys->cpu.x + 1) & 0xFF);
    return lo | (hi << 8);
}

static u16 addr_mode_indirect_indexed(C64* sys) {
    u8 addr = mem_read(sys, sys->cpu.pc++);
    u8 lo = mem_read(sys, addr);
    u8 hi = mem_read(sys, (addr + 1) & 0xFF);
    u16 base = lo | (hi << 8);
    sys->cpu.page_crossed = ((base + sys->cpu.y) & 0xFF00) != (base & 0xFF00);
    return base + sys->cpu.y;
}

static void cpu_illegal_opcode(C64* sys, u8 opcode) {
    printf("Illegal opcode: $%02X at PC: $%04X\n", opcode, sys->cpu.pc - 1);
}

void cpu_init(C64CPU* cpu) {
    memset(cpu, 0, sizeof(C64CPU));
    cpu->sp = 0xFF;
    cpu->status = FLAG_I | FLAG_R;
}

void cpu_interrupt(C64* sys, bool nmi) {
    if (nmi || !cpu_get_flag(&sys->cpu, FLAG_I)) {
        cpu_push16(sys, sys->cpu.pc);
        cpu_push(sys, sys->cpu.status & ~FLAG_B);
        
        if (nmi) {
            sys->cpu.pc = mem_read(sys, 0xFFFA) | (mem_read(sys, 0xFFFB) << 8);
        } else {
            sys->cpu.pc = mem_read(sys, 0xFFFE) | (mem_read(sys, 0xFFFF) << 8);
            cpu_set_flag(&sys->cpu, FLAG_I, true);
        }
    }
}

void cpu_step(C64* sys) {
    u8 opcode = mem_read(sys, sys->cpu.pc++);
    
    sys->cpu.extra_cycles = 0;
    sys->cpu.page_crossed = false;
    
    // Don't count base cycle here - let each instruction handle its own timing
    
    switch (opcode) {
        case 0x00: {
            cpu_push16(sys, sys->cpu.pc);
            cpu_push(sys, sys->cpu.status | FLAG_B);
            cpu_set_flag(&sys->cpu, FLAG_I, true);
            sys->cpu.pc = mem_read(sys, 0xFFFE) | (mem_read(sys, 0xFFFF) << 8);
            break;
        }
        
        case 0x08: {
            cpu_push(sys, sys->cpu.status | FLAG_B);
            break;
        }
        
        case 0x10: { // BPL
            u8 rel = mem_read(sys, sys->cpu.pc++);
            if (!cpu_get_flag(&sys->cpu, FLAG_N)) {
                u16 old_pc = sys->cpu.pc;
                sys->cpu.pc += (i8)rel;
                sys->cpu.extra_cycles = 1;
                if ((sys->cpu.pc & 0xFF00) != (old_pc & 0xFF00)) {
                    sys->cpu.extra_cycles = 2;
                }
            }
            break;
        }
        
        case 0x18: {
            cpu_set_flag(&sys->cpu, FLAG_C, false);
            break;
        }
        
        case 0x20: {
            cpu_push16(sys, sys->cpu.pc);
            u8 lo = mem_read(sys, sys->cpu.pc++);
            u8 hi = mem_read(sys, sys->cpu.pc++);
            sys->cpu.pc = lo | (hi << 8);
            break;
        }
        
        case 0x28: {
            sys->cpu.status = cpu_pull(sys) | FLAG_R;
            break;
        }
        
        case 0x30: { // BMI
            u8 rel = mem_read(sys, sys->cpu.pc++);
            if (cpu_get_flag(&sys->cpu, FLAG_N)) {
                u16 old_pc = sys->cpu.pc;
                sys->cpu.pc += (i8)rel;
                sys->cpu.extra_cycles = 1;
                if ((sys->cpu.pc & 0xFF00) != (old_pc & 0xFF00)) {
                    sys->cpu.extra_cycles = 2;
                }
            }
            break;
        }
        
        case 0x38: {
            cpu_set_flag(&sys->cpu, FLAG_C, true);
            break;
        }
        
        case 0x40: {
            sys->cpu.status = cpu_pull(sys) | FLAG_R;
            sys->cpu.pc = cpu_pull16(sys);
            break;
        }
        
        case 0x48: {
            cpu_push(sys, sys->cpu.a);
            break;
        }
        
        case 0x50: { // BVC
            u8 rel = mem_read(sys, sys->cpu.pc++);
            if (!cpu_get_flag(&sys->cpu, FLAG_V)) {
                u16 old_pc = sys->cpu.pc;
                sys->cpu.pc += (i8)rel;
                sys->cpu.extra_cycles = 1;
                if ((sys->cpu.pc & 0xFF00) != (old_pc & 0xFF00)) {
                    sys->cpu.extra_cycles = 2;
                }
            }
            break;
        }
        
        case 0x58: {
            cpu_set_flag(&sys->cpu, FLAG_I, false);
            break;
        }
        
        case 0x60: { // RTS
            sys->cpu.pc = cpu_pull16(sys) + 1;
            sys->mem.cycle_count += 6; // RTS takes 6 cycles
            break;
        }
        
        case 0x68: {
            sys->cpu.a = cpu_get_zn(&sys->cpu, cpu_pull(sys));
            break;
        }
        
        case 0x70: { // BVS
            u8 rel = mem_read(sys, sys->cpu.pc++);
            if (cpu_get_flag(&sys->cpu, FLAG_V)) {
                u16 old_pc = sys->cpu.pc;
                sys->cpu.pc += (i8)rel;
                sys->cpu.extra_cycles = 1;
                if ((sys->cpu.pc & 0xFF00) != (old_pc & 0xFF00)) {
                    sys->cpu.extra_cycles = 2;
                }
            }
            break;
        }
        
        case 0x78: {
            cpu_set_flag(&sys->cpu, FLAG_I, true);
            break;
        }
        
        case 0x88: {
            sys->cpu.y = cpu_get_zn(&sys->cpu, sys->cpu.y - 1);
            break;
        }
        
        case 0x8A: {
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.x);
            break;
        }
        
        case 0x90: { // BCC
            u8 rel = mem_read(sys, sys->cpu.pc++);
            if (!cpu_get_flag(&sys->cpu, FLAG_C)) {
                u16 old_pc = sys->cpu.pc;
                sys->cpu.pc += (i8)rel;
                sys->cpu.extra_cycles = 1;
                if ((sys->cpu.pc & 0xFF00) != (old_pc & 0xFF00)) {
                    sys->cpu.extra_cycles = 2;
                }
            }
            break;
        }
        
        case 0x98: {
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.y);
            break;
        }
        
        case 0x9A: { // TXS
            sys->cpu.sp = sys->cpu.x;
            break;
        }
        
        case 0xBA: { // TSX
            sys->cpu.x = cpu_get_zn(&sys->cpu, sys->cpu.sp);
            break;
        }
        

        
        case 0xA0: {
            sys->cpu.y = cpu_get_zn(&sys->cpu, mem_read(sys, addr_mode_immediate(sys)));
            break;
        }
        
        case 0xA2: {
            sys->cpu.x = cpu_get_zn(&sys->cpu, mem_read(sys, addr_mode_immediate(sys)));
            break;
        }
        
        case 0xA8: {
            sys->cpu.y = cpu_get_zn(&sys->cpu, sys->cpu.a);
            break;
        }
        
        case 0xA9: {
            sys->cpu.a = cpu_get_zn(&sys->cpu, mem_read(sys, addr_mode_immediate(sys)));
            break;
        }
        
        case 0xAA: {
            sys->cpu.x = cpu_get_zn(&sys->cpu, sys->cpu.a);
            break;
        }
        
        case 0xB0: { // BCS
            u8 rel = mem_read(sys, sys->cpu.pc++);
            if (cpu_get_flag(&sys->cpu, FLAG_C)) {
                u16 old_pc = sys->cpu.pc;
                sys->cpu.pc += (i8)rel;
                sys->cpu.extra_cycles = 1;
                if ((sys->cpu.pc & 0xFF00) != (old_pc & 0xFF00)) {
                    sys->cpu.extra_cycles = 2;
                }
            }
            break;
        }
        
        case 0xB8: {
            cpu_set_flag(&sys->cpu, FLAG_V, false);
            break;
        }
        

        
        case 0xC8: {
            sys->cpu.y = cpu_get_zn(&sys->cpu, sys->cpu.y + 1);
            break;
        }
        
        case 0xC9: { // CMP immediate
            u8 data = mem_read(sys, addr_mode_immediate(sys));
            u8 result = sys->cpu.a - data;
            cpu_set_flag(&sys->cpu, FLAG_C, sys->cpu.a >= data);
            cpu_get_zn(&sys->cpu, result);
            break;
        }
        
        // CPX - Compare X Register
        case 0xE4: { // CPX zero page
            u8 data = mem_read(sys, addr_mode_zero_page(sys));
            u8 result = sys->cpu.x - data;
            cpu_set_flag(&sys->cpu, FLAG_C, sys->cpu.x >= data);
            cpu_get_zn(&sys->cpu, result);
            break;
        }
        case 0xEC: { // CPX absolute
            u8 data = mem_read(sys, addr_mode_absolute(sys));
            u8 result = sys->cpu.x - data;
            cpu_set_flag(&sys->cpu, FLAG_C, sys->cpu.x >= data);
            cpu_get_zn(&sys->cpu, result);
            break;
        }
        
        // CPY - Compare Y Register  
        case 0xC0: { // CPY immediate
            u8 data = mem_read(sys, addr_mode_immediate(sys));
            u8 result = sys->cpu.y - data;
            cpu_set_flag(&sys->cpu, FLAG_C, sys->cpu.y >= data);
            cpu_get_zn(&sys->cpu, result);
            break;
        }
        case 0xC4: { // CPY zero page
            u8 data = mem_read(sys, addr_mode_zero_page(sys));
            u8 result = sys->cpu.y - data;
            cpu_set_flag(&sys->cpu, FLAG_C, sys->cpu.y >= data);
            cpu_get_zn(&sys->cpu, result);
            break;
        }
        case 0xCC: { // CPY absolute
            u8 data = mem_read(sys, addr_mode_absolute(sys));
            u8 result = sys->cpu.y - data;
            cpu_set_flag(&sys->cpu, FLAG_C, sys->cpu.y >= data);
            cpu_get_zn(&sys->cpu, result);
            break;
        }
        
        case 0xD0: { // BNE
            u8 rel = mem_read(sys, sys->cpu.pc++);
            if (!cpu_get_flag(&sys->cpu, FLAG_Z)) {
                u16 old_pc = sys->cpu.pc;
                sys->cpu.pc += (i8)rel;
                sys->cpu.extra_cycles = 1;
                if ((sys->cpu.pc & 0xFF00) != (old_pc & 0xFF00)) {
                    sys->cpu.extra_cycles = 2;
                }
            }
            sys->mem.cycle_count += 2; // Base timing
            break;
        }
        
        case 0xD8: {
            cpu_set_flag(&sys->cpu, FLAG_D, false);
            break;
        }
        
        case 0xE0: {
            u8 data = mem_read(sys, addr_mode_immediate(sys));
            u8 result = sys->cpu.x - data;
            cpu_set_flag(&sys->cpu, FLAG_C, sys->cpu.x >= data);
            cpu_get_zn(&sys->cpu, result);
            break;
        }
        
        case 0xE8: {
            sys->cpu.x = cpu_get_zn(&sys->cpu, sys->cpu.x + 1);
            break;
        }
        
        case 0xF0: { // BEQ
            u8 rel = mem_read(sys, sys->cpu.pc++);
            if (cpu_get_flag(&sys->cpu, FLAG_Z)) {
                u16 old_pc = sys->cpu.pc;
                sys->cpu.pc += (i8)rel;
                sys->cpu.extra_cycles = 1;
                if ((sys->cpu.pc & 0xFF00) != (old_pc & 0xFF00)) {
                    sys->cpu.extra_cycles = 2;
                }
            }
            break;
        }
        
        case 0xF8: {
            cpu_set_flag(&sys->cpu, FLAG_D, true);
            break;
        }
        
        // ADC - Add with Carry
        case 0x69: { // Immediate
            u8 data = mem_read(sys, addr_mode_immediate(sys));
            sys->cpu.a = cpu_adc_bcd(&sys->cpu, sys->cpu.a, data);
            break;
        }
        case 0x65: { // Zero Page
            u8 data = mem_read(sys, addr_mode_zero_page(sys));
            sys->cpu.a = cpu_adc_bcd(&sys->cpu, sys->cpu.a, data);
            break;
        }
        case 0x75: { // Zero Page,X
            u8 data = mem_read(sys, addr_mode_zero_page_x(sys));
            sys->cpu.a = cpu_adc_bcd(&sys->cpu, sys->cpu.a, data);
            break;
        }
        case 0x6D: { // Absolute
            u8 data = mem_read(sys, addr_mode_absolute(sys));
            sys->cpu.a = cpu_adc_bcd(&sys->cpu, sys->cpu.a, data);
            break;
        }
        case 0x7D: { // Absolute,X
            u16 addr = addr_mode_absolute_x(sys);
            u8 data = mem_read(sys, addr);
            if (sys->cpu.page_crossed) mem_read(sys, addr & 0xFF00); // Dummy read on page cross
            sys->cpu.a = cpu_adc_bcd(&sys->cpu, sys->cpu.a, data);
            break;
        }
        case 0x79: { // Absolute,Y
            u16 addr = addr_mode_absolute_y(sys);
            u8 data = mem_read(sys, addr);
            if (sys->cpu.page_crossed) mem_read(sys, addr & 0xFF00); // Dummy read on page cross
            sys->cpu.a = cpu_adc_bcd(&sys->cpu, sys->cpu.a, data);
            break;
        }
        case 0x61: { // (Indirect,X)
            u16 addr = addr_mode_indexed_indirect(sys);
            u8 data = mem_read(sys, addr);
            sys->cpu.a = cpu_adc_bcd(&sys->cpu, sys->cpu.a, data);
            break;
        }
        case 0x71: { // (Indirect),Y
            u16 addr = addr_mode_indirect_indexed(sys);
            u8 data = mem_read(sys, addr);
            if (sys->cpu.page_crossed) mem_read(sys, addr & 0xFF00); // Dummy read on page cross
            sys->cpu.a = cpu_adc_bcd(&sys->cpu, sys->cpu.a, data);
            break;
        }
        
        // AND - Logical AND
        case 0x29: { // Immediate
            u8 data = mem_read(sys, addr_mode_immediate(sys));
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a & data);
            break;
        }
        case 0x25: { // Zero Page
            u8 data = mem_read(sys, addr_mode_zero_page(sys));
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a & data);
            break;
        }
        case 0x35: { // Zero Page,X
            u8 data = mem_read(sys, addr_mode_zero_page_x(sys));
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a & data);
            break;
        }
        case 0x2D: { // Absolute
            u8 data = mem_read(sys, addr_mode_absolute(sys));
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a & data);
            break;
        }
        case 0x3D: { // Absolute,X
            u16 addr = addr_mode_absolute_x(sys);
            u8 data = mem_read(sys, addr);
            if (sys->cpu.page_crossed) mem_read(sys, addr & 0xFF00); // Dummy read on page cross
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a & data);
            break;
        }
        case 0x39: { // Absolute,Y
            u16 addr = addr_mode_absolute_y(sys);
            u8 data = mem_read(sys, addr);
            if (sys->cpu.page_crossed) mem_read(sys, addr & 0xFF00); // Dummy read on page cross
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a & data);
            break;
        }
        case 0x21: { // (Indirect,X)
            u16 addr = addr_mode_indexed_indirect(sys);
            u8 data = mem_read(sys, addr);
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a & data);
            break;
        }
        case 0x31: { // (Indirect),Y
            u16 addr = addr_mode_indirect_indexed(sys);
            u8 data = mem_read(sys, addr);
            if (sys->cpu.page_crossed) mem_read(sys, addr & 0xFF00); // Dummy read on page cross
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a & data);
            break;
        }
        
        // EOR - Exclusive OR
        case 0x49: { // Immediate
            u8 data = mem_read(sys, addr_mode_immediate(sys));
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a ^ data);
            break;
        }
        case 0x45: { // Zero Page
            u8 data = mem_read(sys, addr_mode_zero_page(sys));
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a ^ data);
            break;
        }
        case 0x55: { // Zero Page,X
            u8 data = mem_read(sys, addr_mode_zero_page_x(sys));
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a ^ data);
            break;
        }
        case 0x4D: { // Absolute
            u8 data = mem_read(sys, addr_mode_absolute(sys));
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a ^ data);
            break;
        }
        case 0x5D: { // Absolute,X
            u16 addr = addr_mode_absolute_x(sys);
            u8 data = mem_read(sys, addr);
            if (sys->cpu.page_crossed) mem_read(sys, addr & 0xFF00); // Dummy read on page cross
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a ^ data);
            break;
        }
        case 0x59: { // Absolute,Y
            u16 addr = addr_mode_absolute_y(sys);
            u8 data = mem_read(sys, addr);
            if (sys->cpu.page_crossed) mem_read(sys, addr & 0xFF00); // Dummy read on page cross
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a ^ data);
            break;
        }
        case 0x41: { // (Indirect,X)
            u16 addr = addr_mode_indexed_indirect(sys);
            u8 data = mem_read(sys, addr);
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a ^ data);
            break;
        }
        case 0x51: { // (Indirect),Y
            u16 addr = addr_mode_indirect_indexed(sys);
            u8 data = mem_read(sys, addr);
            if (sys->cpu.page_crossed) mem_read(sys, addr & 0xFF00); // Dummy read on page cross
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a ^ data);
            break;
        }
        
        // ORA - Logical OR
        case 0x09: { // Immediate
            u8 data = mem_read(sys, addr_mode_immediate(sys));
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a | data);
            break;
        }
        case 0x05: { // Zero Page
            u8 data = mem_read(sys, addr_mode_zero_page(sys));
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a | data);
            break;
        }
        case 0x15: { // Zero Page,X
            u8 data = mem_read(sys, addr_mode_zero_page_x(sys));
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a | data);
            break;
        }
        case 0x0D: { // Absolute
            u8 data = mem_read(sys, addr_mode_absolute(sys));
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a | data);
            break;
        }
        case 0x1D: { // Absolute,X
            u16 addr = addr_mode_absolute_x(sys);
            u8 data = mem_read(sys, addr);
            if (sys->cpu.page_crossed) mem_read(sys, addr & 0xFF00); // Dummy read on page cross
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a | data);
            break;
        }
        case 0x19: { // Absolute,Y
            u16 addr = addr_mode_absolute_y(sys);
            u8 data = mem_read(sys, addr);
            if (sys->cpu.page_crossed) mem_read(sys, addr & 0xFF00); // Dummy read on page cross
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a | data);
            break;
        }
        case 0x01: { // (Indirect,X)
            u16 addr = addr_mode_indexed_indirect(sys);
            u8 data = mem_read(sys, addr);
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a | data);
            break;
        }
        case 0x11: { // (Indirect),Y
            u16 addr = addr_mode_indirect_indexed(sys);
            u8 data = mem_read(sys, addr);
            if (sys->cpu.page_crossed) mem_read(sys, addr & 0xFF00); // Dummy read on page cross
            sys->cpu.a = cpu_get_zn(&sys->cpu, sys->cpu.a | data);
            break;
        }
        
        // Additional LDA addressing modes
        case 0xA5: { // Zero Page
            sys->cpu.a = cpu_get_zn(&sys->cpu, mem_read(sys, addr_mode_zero_page(sys)));
            break;
        }
        case 0xB5: { // Zero Page,X
            sys->cpu.a = cpu_get_zn(&sys->cpu, mem_read(sys, addr_mode_zero_page_x(sys)));
            break;
        }
        case 0xAD: { // Absolute
            sys->cpu.a = cpu_get_zn(&sys->cpu, mem_read(sys, addr_mode_absolute(sys)));
            break;
        }
        case 0xBD: { // Absolute,X
            u16 addr = addr_mode_absolute_x(sys);
            u8 data = mem_read(sys, addr);
            if (sys->cpu.page_crossed) mem_read(sys, addr & 0xFF00); // Dummy read on page cross
            sys->cpu.a = cpu_get_zn(&sys->cpu, data);
            sys->mem.cycle_count += 4 + (sys->cpu.page_crossed ? 1 : 0); // Base + page cross
            break;
        }
        case 0xB9: { // Absolute,Y
            u16 addr = addr_mode_absolute_y(sys);
            u8 data = mem_read(sys, addr);
            if (sys->cpu.page_crossed) mem_read(sys, addr & 0xFF00); // Dummy read on page cross
            sys->cpu.a = cpu_get_zn(&sys->cpu, data);
            break;
        }
        case 0xA1: { // (Indirect,X)
            u16 addr = addr_mode_indexed_indirect(sys);
            sys->cpu.a = cpu_get_zn(&sys->cpu, mem_read(sys, addr));
            break;
        }
        case 0xB1: { // (Indirect),Y
            u16 addr = addr_mode_indirect_indexed(sys);
            u8 data = mem_read(sys, addr);
            if (sys->cpu.page_crossed) mem_read(sys, addr & 0xFF00); // Dummy read on page cross
            sys->cpu.a = cpu_get_zn(&sys->cpu, data);
            break;
        }
        
        default:
            cpu_illegal_opcode(sys, opcode);
            break;
    }
}