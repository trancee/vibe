/**
 * cpu.c - 6510 CPU emulation
 * 
 * Full 6502-compatible CPU with 6510 I/O port.
 * Implements all legal opcodes with cycle-accurate timing.
 */

#include "cpu.h"
#include "c64.h"
#include <stdio.h>
#include <string.h>

// Forward declarations
static u8 cpu_read(C64Cpu *cpu, u16 addr);
static void cpu_write(C64Cpu *cpu, u16 addr, u8 value);
static void cpu_push(C64Cpu *cpu, u8 value);
static u8 cpu_pop(C64Cpu *cpu);
static void cpu_push16(C64Cpu *cpu, u16 value);
static u16 cpu_pop16(C64Cpu *cpu);

// Addressing mode helpers
static u16 addr_immediate(C64Cpu *cpu);
static u16 addr_zeropage(C64Cpu *cpu);
static u16 addr_zeropage_x(C64Cpu *cpu);
static u16 addr_zeropage_y(C64Cpu *cpu);
static u16 addr_absolute(C64Cpu *cpu);
static u16 addr_absolute_x(C64Cpu *cpu, bool check_page);
static u16 addr_absolute_y(C64Cpu *cpu, bool check_page);
static u16 addr_indirect(C64Cpu *cpu);
static u16 addr_indirect_x(C64Cpu *cpu);
static u16 addr_indirect_y(C64Cpu *cpu, bool check_page);

void cpu_init(C64Cpu *cpu, C64System *sys) {
    memset(cpu, 0, sizeof(C64Cpu));
    cpu->sys = sys;
    cpu->P = FLAG_U | FLAG_I;  // Unused always set, I set on reset
    cpu->SP = 0xFD;
    cpu->port_dir = 0x2F;   // Default direction
    cpu->port_data = 0x37;  // Default: BASIC, KERNAL, I/O visible
}

void cpu_reset(C64Cpu *cpu) {
    cpu->A = 0;
    cpu->X = 0;
    cpu->Y = 0;
    cpu->SP = 0xFD;
    cpu->P = FLAG_U | FLAG_I;
    
    cpu->port_dir = 0x2F;
    cpu->port_data = 0x37;
    cpu->cpu_port_floating = 0xC0;  // Bits 6,7 always floating (no external pins)
    
    cpu->nmi_pending = false;
    cpu->irq_pending = false;
    cpu->nmi_edge = false;
    cpu->extra_cycles = 0;
    cpu->page_crossed = false;
    
    // Read reset vector
    u16 lo = mem_read_raw(&cpu->sys->mem, 0xFFFC);
    u16 hi = mem_read_raw(&cpu->sys->mem, 0xFFFD);
    cpu->PC = lo | (hi << 8);
}

// Memory access (goes through memory subsystem which ticks)
static u8 cpu_read(C64Cpu *cpu, u16 addr) {
    // Handle CPU port at $00/$01
    if (addr == 0x0000) {
        return cpu->port_dir;
    }
    if (addr == 0x0001) {
        // Complex read behavior for 6510 port
        u8 output_bits = cpu->port_dir;
        u8 input_bits = ~cpu->port_dir;
        
        u8 result = 0;
        
        // Output bits: read what we wrote
        result |= (cpu->port_data & output_bits);
        
        // Input bits: complex behavior
        // Bits 0-2: LORAM, HIRAM, CHAREN - external pull-ups when input
        // Bits 3-4: Cassette sense/motor - typically pulled low
        // Bit 5: Cassette motor output sense - reads low when configured as input
        // Bits 6-7: Not connected, floating
        
        // External pull-ups on bits 0-4 (accent grave key, cassette)
        u8 external = 0x1F;  // Bits 0-4 pulled high
        
        // Bit 5 specifically reads low when input (6510 quirk)
        if (!(cpu->port_dir & 0x20)) {
            external &= ~0x20;
        }
        
        // Floating bits retain last written value
        external |= (cpu->cpu_port_floating & 0xC0);
        
        result |= (external & input_bits);
        
        return result;
    }
    
    return mem_read(&cpu->sys->mem, addr);
}

static void cpu_write(C64Cpu *cpu, u16 addr, u8 value) {
    // Handle CPU port at $00/$01
    if (addr == 0x0000) {
        cpu->port_dir = value;
        return;
    }
    if (addr == 0x0001) {
        cpu->port_data = value;
        // Track written values for floating bits
        cpu->cpu_port_floating = value;
        return;
    }
    
    mem_write(&cpu->sys->mem, addr, value);
}

// Stack operations
static void cpu_push(C64Cpu *cpu, u8 value) {
    cpu_write(cpu, 0x0100 | cpu->SP, value);
    cpu->SP--;
}

static u8 cpu_pop(C64Cpu *cpu) {
    cpu->SP++;
    return cpu_read(cpu, 0x0100 | cpu->SP);
}

static void cpu_push16(C64Cpu *cpu, u16 value) {
    cpu_push(cpu, (value >> 8) & 0xFF);
    cpu_push(cpu, value & 0xFF);
}

static u16 cpu_pop16(C64Cpu *cpu) {
    u16 lo = cpu_pop(cpu);
    u16 hi = cpu_pop(cpu);
    return lo | (hi << 8);
}

// Addressing modes
static u16 addr_immediate(C64Cpu *cpu) {
    return cpu->PC++;
}

static u16 addr_zeropage(C64Cpu *cpu) {
    return cpu_read(cpu, cpu->PC++);
}

static u16 addr_zeropage_x(C64Cpu *cpu) {
    u8 base = cpu_read(cpu, cpu->PC++);
    cpu_read(cpu, base);  // Dummy read (cycle timing)
    return (base + cpu->X) & 0xFF;
}

static u16 addr_zeropage_y(C64Cpu *cpu) {
    u8 base = cpu_read(cpu, cpu->PC++);
    cpu_read(cpu, base);  // Dummy read
    return (base + cpu->Y) & 0xFF;
}

static u16 addr_absolute(C64Cpu *cpu) {
    u16 lo = cpu_read(cpu, cpu->PC++);
    u16 hi = cpu_read(cpu, cpu->PC++);
    return lo | (hi << 8);
}

static u16 addr_absolute_x(C64Cpu *cpu, bool check_page) {
    u16 lo = cpu_read(cpu, cpu->PC++);
    u16 hi = cpu_read(cpu, cpu->PC++);
    u16 base = lo | (hi << 8);
    u16 result = base + cpu->X;
    
    if (check_page && ((base & 0xFF00) != (result & 0xFF00))) {
        cpu->page_crossed = true;
        cpu->extra_cycles++;
    }
    
    return result;
}

static u16 addr_absolute_y(C64Cpu *cpu, bool check_page) {
    u16 lo = cpu_read(cpu, cpu->PC++);
    u16 hi = cpu_read(cpu, cpu->PC++);
    u16 base = lo | (hi << 8);
    u16 result = base + cpu->Y;
    
    if (check_page && ((base & 0xFF00) != (result & 0xFF00))) {
        cpu->page_crossed = true;
        cpu->extra_cycles++;
    }
    
    return result;
}

static u16 addr_indirect(C64Cpu *cpu) {
    u16 ptr_lo = cpu_read(cpu, cpu->PC++);
    u16 ptr_hi = cpu_read(cpu, cpu->PC++);
    u16 ptr = ptr_lo | (ptr_hi << 8);
    
    // 6502 bug: wraps within page for JMP indirect
    u16 lo = cpu_read(cpu, ptr);
    u16 hi = cpu_read(cpu, (ptr & 0xFF00) | ((ptr + 1) & 0xFF));
    return lo | (hi << 8);
}

static u16 addr_indirect_x(C64Cpu *cpu) {
    u8 base = cpu_read(cpu, cpu->PC++);
    cpu_read(cpu, base);  // Dummy read
    u8 ptr = (base + cpu->X) & 0xFF;
    u16 lo = cpu_read(cpu, ptr);
    u16 hi = cpu_read(cpu, (ptr + 1) & 0xFF);
    return lo | (hi << 8);
}

static u16 addr_indirect_y(C64Cpu *cpu, bool check_page) {
    u8 ptr = cpu_read(cpu, cpu->PC++);
    u16 lo = cpu_read(cpu, ptr);
    u16 hi = cpu_read(cpu, (ptr + 1) & 0xFF);
    u16 base = lo | (hi << 8);
    u16 result = base + cpu->Y;
    
    if (check_page && ((base & 0xFF00) != (result & 0xFF00))) {
        cpu->page_crossed = true;
        cpu->extra_cycles++;
    }
    
    return result;
}

// ALU operations
static void op_adc(C64Cpu *cpu, u8 value) {
    if (cpu->P & FLAG_D) {
        // Decimal mode
        u16 al = (cpu->A & 0x0F) + (value & 0x0F) + (cpu->P & FLAG_C ? 1 : 0);
        if (al > 9) al += 6;
        
        u16 ah = (cpu->A >> 4) + (value >> 4) + (al > 15 ? 1 : 0);
        
        // Set Z flag based on binary result
        u16 bin_result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu_set_flag(cpu, FLAG_Z, (bin_result & 0xFF) == 0);
        
        // N and V flags based on high nibble result before correction
        cpu_set_flag(cpu, FLAG_N, ah & 0x08);
        cpu_set_flag(cpu, FLAG_V, ((cpu->A ^ bin_result) & (value ^ bin_result) & 0x80) != 0);
        
        if (ah > 9) ah += 6;
        cpu_set_flag(cpu, FLAG_C, ah > 15);
        
        cpu->A = ((ah & 0x0F) << 4) | (al & 0x0F);
    } else {
        // Binary mode
        u16 result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu_set_flag(cpu, FLAG_C, result > 0xFF);
        cpu_set_flag(cpu, FLAG_V, ((cpu->A ^ result) & (value ^ result) & 0x80) != 0);
        cpu->A = result & 0xFF;
        cpu_update_nz(cpu, cpu->A);
    }
}

static void op_sbc(C64Cpu *cpu, u8 value) {
    if (cpu->P & FLAG_D) {
        // Decimal mode
        u16 al = (cpu->A & 0x0F) - (value & 0x0F) - (cpu->P & FLAG_C ? 0 : 1);
        u16 ah = (cpu->A >> 4) - (value >> 4);
        
        if (al & 0x10) {
            al -= 6;
            ah--;
        }
        if (ah & 0x10) {
            ah -= 6;
        }
        
        // Flags based on binary result
        u16 bin_result = cpu->A - value - (cpu->P & FLAG_C ? 0 : 1);
        cpu_set_flag(cpu, FLAG_C, bin_result < 0x100);
        cpu_set_flag(cpu, FLAG_V, ((cpu->A ^ value) & (cpu->A ^ bin_result) & 0x80) != 0);
        cpu_update_nz(cpu, bin_result & 0xFF);
        
        cpu->A = ((ah & 0x0F) << 4) | (al & 0x0F);
    } else {
        // Binary mode
        u16 result = cpu->A - value - (cpu->P & FLAG_C ? 0 : 1);
        cpu_set_flag(cpu, FLAG_C, result < 0x100);
        cpu_set_flag(cpu, FLAG_V, ((cpu->A ^ value) & (cpu->A ^ result) & 0x80) != 0);
        cpu->A = result & 0xFF;
        cpu_update_nz(cpu, cpu->A);
    }
}

static void op_cmp(C64Cpu *cpu, u8 reg, u8 value) {
    u16 result = reg - value;
    cpu_set_flag(cpu, FLAG_C, reg >= value);
    cpu_update_nz(cpu, result & 0xFF);
}

static u8 op_asl(C64Cpu *cpu, u8 value) {
    cpu_set_flag(cpu, FLAG_C, value & 0x80);
    value <<= 1;
    cpu_update_nz(cpu, value);
    return value;
}

static u8 op_lsr(C64Cpu *cpu, u8 value) {
    cpu_set_flag(cpu, FLAG_C, value & 0x01);
    value >>= 1;
    cpu_update_nz(cpu, value);
    return value;
}

static u8 op_rol(C64Cpu *cpu, u8 value) {
    bool old_carry = cpu->P & FLAG_C;
    cpu_set_flag(cpu, FLAG_C, value & 0x80);
    value = (value << 1) | (old_carry ? 1 : 0);
    cpu_update_nz(cpu, value);
    return value;
}

static u8 op_ror(C64Cpu *cpu, u8 value) {
    bool old_carry = cpu->P & FLAG_C;
    cpu_set_flag(cpu, FLAG_C, value & 0x01);
    value = (value >> 1) | (old_carry ? 0x80 : 0);
    cpu_update_nz(cpu, value);
    return value;
}

// Branch helper
static void do_branch(C64Cpu *cpu, bool condition) {
    i8 offset = (i8)cpu_read(cpu, cpu->PC++);
    if (condition) {
        u16 old_pc = cpu->PC;
        cpu->PC += offset;
        cpu->extra_cycles++;  // Branch taken
        
        if ((old_pc & 0xFF00) != (cpu->PC & 0xFF00)) {
            cpu->extra_cycles++;  // Page crossing
        }
        
        // Dummy read
        cpu_read(cpu, old_pc);
    }
}

// Interrupt handling
static void do_interrupt(C64Cpu *cpu, u16 vector, bool brk) {
    // Read next instruction byte (discarded)
    cpu_read(cpu, cpu->PC);
    if (brk) cpu->PC++;
    
    // Push PC and status
    cpu_push16(cpu, cpu->PC);
    cpu_push(cpu, cpu->P | FLAG_U | (brk ? FLAG_B : 0));
    
    // Set I flag
    cpu->P |= FLAG_I;
    
    // Clear D flag (not on 6502, but 65C02 and later do this)
    // The 6510 does NOT clear D on interrupt
    
    // Read vector
    u16 lo = cpu_read(cpu, vector);
    u16 hi = cpu_read(cpu, vector + 1);
    cpu->PC = lo | (hi << 8);
}

void cpu_trigger_nmi(C64Cpu *cpu) {
    if (!cpu->nmi_edge) {
        cpu->nmi_pending = true;
        cpu->nmi_edge = true;
    }
}

void cpu_trigger_irq(C64Cpu *cpu) {
    cpu->irq_pending = true;
}

// Execute one instruction
int cpu_step(C64Cpu *cpu) {
    int cycles = 0;
    cpu->extra_cycles = 0;
    cpu->page_crossed = false;
    
    // Check for NMI (higher priority than IRQ)
    if (cpu->nmi_pending) {
        cpu->nmi_pending = false;
        do_interrupt(cpu, 0xFFFA, false);
        return 7;
    }
    
    // Check for IRQ
    if (cpu->irq_pending && !(cpu->P & FLAG_I)) {
        cpu->irq_pending = false;
        do_interrupt(cpu, 0xFFFE, false);
        return 7;
    }
    
    // Fetch opcode
    u8 opcode = cpu_read(cpu, cpu->PC++);
    
    u16 addr;
    u8 value;
    
    switch (opcode) {
        // LDA - Load Accumulator
        case 0xA9: // LDA #imm
            cpu->A = cpu_read(cpu, addr_immediate(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 2;
            break;
        case 0xA5: // LDA zp
            cpu->A = cpu_read(cpu, addr_zeropage(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 3;
            break;
        case 0xB5: // LDA zp,X
            cpu->A = cpu_read(cpu, addr_zeropage_x(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0xAD: // LDA abs
            cpu->A = cpu_read(cpu, addr_absolute(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0xBD: // LDA abs,X
            cpu->A = cpu_read(cpu, addr_absolute_x(cpu, true));
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0xB9: // LDA abs,Y
            cpu->A = cpu_read(cpu, addr_absolute_y(cpu, true));
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0xA1: // LDA (zp,X)
            cpu->A = cpu_read(cpu, addr_indirect_x(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 6;
            break;
        case 0xB1: // LDA (zp),Y
            cpu->A = cpu_read(cpu, addr_indirect_y(cpu, true));
            cpu_update_nz(cpu, cpu->A);
            cycles = 5;
            break;

        // LDX - Load X Register
        case 0xA2: // LDX #imm
            cpu->X = cpu_read(cpu, addr_immediate(cpu));
            cpu_update_nz(cpu, cpu->X);
            cycles = 2;
            break;
        case 0xA6: // LDX zp
            cpu->X = cpu_read(cpu, addr_zeropage(cpu));
            cpu_update_nz(cpu, cpu->X);
            cycles = 3;
            break;
        case 0xB6: // LDX zp,Y
            cpu->X = cpu_read(cpu, addr_zeropage_y(cpu));
            cpu_update_nz(cpu, cpu->X);
            cycles = 4;
            break;
        case 0xAE: // LDX abs
            cpu->X = cpu_read(cpu, addr_absolute(cpu));
            cpu_update_nz(cpu, cpu->X);
            cycles = 4;
            break;
        case 0xBE: // LDX abs,Y
            cpu->X = cpu_read(cpu, addr_absolute_y(cpu, true));
            cpu_update_nz(cpu, cpu->X);
            cycles = 4;
            break;

        // LDY - Load Y Register
        case 0xA0: // LDY #imm
            cpu->Y = cpu_read(cpu, addr_immediate(cpu));
            cpu_update_nz(cpu, cpu->Y);
            cycles = 2;
            break;
        case 0xA4: // LDY zp
            cpu->Y = cpu_read(cpu, addr_zeropage(cpu));
            cpu_update_nz(cpu, cpu->Y);
            cycles = 3;
            break;
        case 0xB4: // LDY zp,X
            cpu->Y = cpu_read(cpu, addr_zeropage_x(cpu));
            cpu_update_nz(cpu, cpu->Y);
            cycles = 4;
            break;
        case 0xAC: // LDY abs
            cpu->Y = cpu_read(cpu, addr_absolute(cpu));
            cpu_update_nz(cpu, cpu->Y);
            cycles = 4;
            break;
        case 0xBC: // LDY abs,X
            cpu->Y = cpu_read(cpu, addr_absolute_x(cpu, true));
            cpu_update_nz(cpu, cpu->Y);
            cycles = 4;
            break;

        // STA - Store Accumulator
        case 0x85: // STA zp
            cpu_write(cpu, addr_zeropage(cpu), cpu->A);
            cycles = 3;
            break;
        case 0x95: // STA zp,X
            cpu_write(cpu, addr_zeropage_x(cpu), cpu->A);
            cycles = 4;
            break;
        case 0x8D: // STA abs
            cpu_write(cpu, addr_absolute(cpu), cpu->A);
            cycles = 4;
            break;
        case 0x9D: // STA abs,X
            cpu_write(cpu, addr_absolute_x(cpu, false), cpu->A);
            cycles = 5;
            break;
        case 0x99: // STA abs,Y
            cpu_write(cpu, addr_absolute_y(cpu, false), cpu->A);
            cycles = 5;
            break;
        case 0x81: // STA (zp,X)
            cpu_write(cpu, addr_indirect_x(cpu), cpu->A);
            cycles = 6;
            break;
        case 0x91: // STA (zp),Y
            cpu_write(cpu, addr_indirect_y(cpu, false), cpu->A);
            cycles = 6;
            break;

        // STX - Store X Register
        case 0x86: // STX zp
            cpu_write(cpu, addr_zeropage(cpu), cpu->X);
            cycles = 3;
            break;
        case 0x96: // STX zp,Y
            cpu_write(cpu, addr_zeropage_y(cpu), cpu->X);
            cycles = 4;
            break;
        case 0x8E: // STX abs
            cpu_write(cpu, addr_absolute(cpu), cpu->X);
            cycles = 4;
            break;

        // STY - Store Y Register
        case 0x84: // STY zp
            cpu_write(cpu, addr_zeropage(cpu), cpu->Y);
            cycles = 3;
            break;
        case 0x94: // STY zp,X
            cpu_write(cpu, addr_zeropage_x(cpu), cpu->Y);
            cycles = 4;
            break;
        case 0x8C: // STY abs
            cpu_write(cpu, addr_absolute(cpu), cpu->Y);
            cycles = 4;
            break;

        // TAX, TAY, TXA, TYA - Transfer
        case 0xAA: // TAX
            cpu_read(cpu, cpu->PC);  // Dummy read
            cpu->X = cpu->A;
            cpu_update_nz(cpu, cpu->X);
            cycles = 2;
            break;
        case 0xA8: // TAY
            cpu_read(cpu, cpu->PC);
            cpu->Y = cpu->A;
            cpu_update_nz(cpu, cpu->Y);
            cycles = 2;
            break;
        case 0x8A: // TXA
            cpu_read(cpu, cpu->PC);
            cpu->A = cpu->X;
            cpu_update_nz(cpu, cpu->A);
            cycles = 2;
            break;
        case 0x98: // TYA
            cpu_read(cpu, cpu->PC);
            cpu->A = cpu->Y;
            cpu_update_nz(cpu, cpu->A);
            cycles = 2;
            break;
        case 0xBA: // TSX
            cpu_read(cpu, cpu->PC);
            cpu->X = cpu->SP;
            cpu_update_nz(cpu, cpu->X);
            cycles = 2;
            break;
        case 0x9A: // TXS
            cpu_read(cpu, cpu->PC);
            cpu->SP = cpu->X;
            cycles = 2;
            break;

        // Stack operations
        case 0x48: // PHA
            cpu_read(cpu, cpu->PC);  // Dummy read
            cpu_push(cpu, cpu->A);
            cycles = 3;
            break;
        case 0x08: // PHP
            cpu_read(cpu, cpu->PC);
            cpu_push(cpu, cpu->P | FLAG_B | FLAG_U);
            cycles = 3;
            break;
        case 0x68: // PLA
            cpu_read(cpu, cpu->PC);  // Dummy read
            cpu_read(cpu, 0x0100 | cpu->SP);  // Dummy stack read
            cpu->A = cpu_pop(cpu);
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0x28: // PLP
            cpu_read(cpu, cpu->PC);
            cpu_read(cpu, 0x0100 | cpu->SP);
            cpu->P = (cpu_pop(cpu) & ~FLAG_B) | FLAG_U;
            cycles = 4;
            break;

        // ADC - Add with Carry
        case 0x69: // ADC #imm
            op_adc(cpu, cpu_read(cpu, addr_immediate(cpu)));
            cycles = 2;
            break;
        case 0x65: // ADC zp
            op_adc(cpu, cpu_read(cpu, addr_zeropage(cpu)));
            cycles = 3;
            break;
        case 0x75: // ADC zp,X
            op_adc(cpu, cpu_read(cpu, addr_zeropage_x(cpu)));
            cycles = 4;
            break;
        case 0x6D: // ADC abs
            op_adc(cpu, cpu_read(cpu, addr_absolute(cpu)));
            cycles = 4;
            break;
        case 0x7D: // ADC abs,X
            op_adc(cpu, cpu_read(cpu, addr_absolute_x(cpu, true)));
            cycles = 4;
            break;
        case 0x79: // ADC abs,Y
            op_adc(cpu, cpu_read(cpu, addr_absolute_y(cpu, true)));
            cycles = 4;
            break;
        case 0x61: // ADC (zp,X)
            op_adc(cpu, cpu_read(cpu, addr_indirect_x(cpu)));
            cycles = 6;
            break;
        case 0x71: // ADC (zp),Y
            op_adc(cpu, cpu_read(cpu, addr_indirect_y(cpu, true)));
            cycles = 5;
            break;

        // SBC - Subtract with Carry
        case 0xE9: // SBC #imm
            op_sbc(cpu, cpu_read(cpu, addr_immediate(cpu)));
            cycles = 2;
            break;
        case 0xE5: // SBC zp
            op_sbc(cpu, cpu_read(cpu, addr_zeropage(cpu)));
            cycles = 3;
            break;
        case 0xF5: // SBC zp,X
            op_sbc(cpu, cpu_read(cpu, addr_zeropage_x(cpu)));
            cycles = 4;
            break;
        case 0xED: // SBC abs
            op_sbc(cpu, cpu_read(cpu, addr_absolute(cpu)));
            cycles = 4;
            break;
        case 0xFD: // SBC abs,X
            op_sbc(cpu, cpu_read(cpu, addr_absolute_x(cpu, true)));
            cycles = 4;
            break;
        case 0xF9: // SBC abs,Y
            op_sbc(cpu, cpu_read(cpu, addr_absolute_y(cpu, true)));
            cycles = 4;
            break;
        case 0xE1: // SBC (zp,X)
            op_sbc(cpu, cpu_read(cpu, addr_indirect_x(cpu)));
            cycles = 6;
            break;
        case 0xF1: // SBC (zp),Y
            op_sbc(cpu, cpu_read(cpu, addr_indirect_y(cpu, true)));
            cycles = 5;
            break;

        // AND - Logical AND
        case 0x29: // AND #imm
            cpu->A &= cpu_read(cpu, addr_immediate(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 2;
            break;
        case 0x25: // AND zp
            cpu->A &= cpu_read(cpu, addr_zeropage(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 3;
            break;
        case 0x35: // AND zp,X
            cpu->A &= cpu_read(cpu, addr_zeropage_x(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0x2D: // AND abs
            cpu->A &= cpu_read(cpu, addr_absolute(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0x3D: // AND abs,X
            cpu->A &= cpu_read(cpu, addr_absolute_x(cpu, true));
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0x39: // AND abs,Y
            cpu->A &= cpu_read(cpu, addr_absolute_y(cpu, true));
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0x21: // AND (zp,X)
            cpu->A &= cpu_read(cpu, addr_indirect_x(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 6;
            break;
        case 0x31: // AND (zp),Y
            cpu->A &= cpu_read(cpu, addr_indirect_y(cpu, true));
            cpu_update_nz(cpu, cpu->A);
            cycles = 5;
            break;

        // ORA - Logical OR
        case 0x09: // ORA #imm
            cpu->A |= cpu_read(cpu, addr_immediate(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 2;
            break;
        case 0x05: // ORA zp
            cpu->A |= cpu_read(cpu, addr_zeropage(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 3;
            break;
        case 0x15: // ORA zp,X
            cpu->A |= cpu_read(cpu, addr_zeropage_x(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0x0D: // ORA abs
            cpu->A |= cpu_read(cpu, addr_absolute(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0x1D: // ORA abs,X
            cpu->A |= cpu_read(cpu, addr_absolute_x(cpu, true));
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0x19: // ORA abs,Y
            cpu->A |= cpu_read(cpu, addr_absolute_y(cpu, true));
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0x01: // ORA (zp,X)
            cpu->A |= cpu_read(cpu, addr_indirect_x(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 6;
            break;
        case 0x11: // ORA (zp),Y
            cpu->A |= cpu_read(cpu, addr_indirect_y(cpu, true));
            cpu_update_nz(cpu, cpu->A);
            cycles = 5;
            break;

        // EOR - Exclusive OR
        case 0x49: // EOR #imm
            cpu->A ^= cpu_read(cpu, addr_immediate(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 2;
            break;
        case 0x45: // EOR zp
            cpu->A ^= cpu_read(cpu, addr_zeropage(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 3;
            break;
        case 0x55: // EOR zp,X
            cpu->A ^= cpu_read(cpu, addr_zeropage_x(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0x4D: // EOR abs
            cpu->A ^= cpu_read(cpu, addr_absolute(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0x5D: // EOR abs,X
            cpu->A ^= cpu_read(cpu, addr_absolute_x(cpu, true));
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0x59: // EOR abs,Y
            cpu->A ^= cpu_read(cpu, addr_absolute_y(cpu, true));
            cpu_update_nz(cpu, cpu->A);
            cycles = 4;
            break;
        case 0x41: // EOR (zp,X)
            cpu->A ^= cpu_read(cpu, addr_indirect_x(cpu));
            cpu_update_nz(cpu, cpu->A);
            cycles = 6;
            break;
        case 0x51: // EOR (zp),Y
            cpu->A ^= cpu_read(cpu, addr_indirect_y(cpu, true));
            cpu_update_nz(cpu, cpu->A);
            cycles = 5;
            break;

        // BIT - Bit Test
        case 0x24: // BIT zp
            value = cpu_read(cpu, addr_zeropage(cpu));
            cpu_set_flag(cpu, FLAG_Z, (cpu->A & value) == 0);
            cpu_set_flag(cpu, FLAG_N, value & 0x80);
            cpu_set_flag(cpu, FLAG_V, value & 0x40);
            cycles = 3;
            break;
        case 0x2C: // BIT abs
            value = cpu_read(cpu, addr_absolute(cpu));
            cpu_set_flag(cpu, FLAG_Z, (cpu->A & value) == 0);
            cpu_set_flag(cpu, FLAG_N, value & 0x80);
            cpu_set_flag(cpu, FLAG_V, value & 0x40);
            cycles = 4;
            break;

        // CMP - Compare Accumulator
        case 0xC9: // CMP #imm
            op_cmp(cpu, cpu->A, cpu_read(cpu, addr_immediate(cpu)));
            cycles = 2;
            break;
        case 0xC5: // CMP zp
            op_cmp(cpu, cpu->A, cpu_read(cpu, addr_zeropage(cpu)));
            cycles = 3;
            break;
        case 0xD5: // CMP zp,X
            op_cmp(cpu, cpu->A, cpu_read(cpu, addr_zeropage_x(cpu)));
            cycles = 4;
            break;
        case 0xCD: // CMP abs
            op_cmp(cpu, cpu->A, cpu_read(cpu, addr_absolute(cpu)));
            cycles = 4;
            break;
        case 0xDD: // CMP abs,X
            op_cmp(cpu, cpu->A, cpu_read(cpu, addr_absolute_x(cpu, true)));
            cycles = 4;
            break;
        case 0xD9: // CMP abs,Y
            op_cmp(cpu, cpu->A, cpu_read(cpu, addr_absolute_y(cpu, true)));
            cycles = 4;
            break;
        case 0xC1: // CMP (zp,X)
            op_cmp(cpu, cpu->A, cpu_read(cpu, addr_indirect_x(cpu)));
            cycles = 6;
            break;
        case 0xD1: // CMP (zp),Y
            op_cmp(cpu, cpu->A, cpu_read(cpu, addr_indirect_y(cpu, true)));
            cycles = 5;
            break;

        // CPX - Compare X Register
        case 0xE0: // CPX #imm
            op_cmp(cpu, cpu->X, cpu_read(cpu, addr_immediate(cpu)));
            cycles = 2;
            break;
        case 0xE4: // CPX zp
            op_cmp(cpu, cpu->X, cpu_read(cpu, addr_zeropage(cpu)));
            cycles = 3;
            break;
        case 0xEC: // CPX abs
            op_cmp(cpu, cpu->X, cpu_read(cpu, addr_absolute(cpu)));
            cycles = 4;
            break;

        // CPY - Compare Y Register
        case 0xC0: // CPY #imm
            op_cmp(cpu, cpu->Y, cpu_read(cpu, addr_immediate(cpu)));
            cycles = 2;
            break;
        case 0xC4: // CPY zp
            op_cmp(cpu, cpu->Y, cpu_read(cpu, addr_zeropage(cpu)));
            cycles = 3;
            break;
        case 0xCC: // CPY abs
            op_cmp(cpu, cpu->Y, cpu_read(cpu, addr_absolute(cpu)));
            cycles = 4;
            break;

        // INC - Increment Memory
        case 0xE6: // INC zp
            addr = addr_zeropage(cpu);
            value = cpu_read(cpu, addr) + 1;
            cpu_write(cpu, addr, value);
            cpu_update_nz(cpu, value);
            cycles = 5;
            break;
        case 0xF6: // INC zp,X
            addr = addr_zeropage_x(cpu);
            value = cpu_read(cpu, addr) + 1;
            cpu_write(cpu, addr, value);
            cpu_update_nz(cpu, value);
            cycles = 6;
            break;
        case 0xEE: // INC abs
            addr = addr_absolute(cpu);
            value = cpu_read(cpu, addr) + 1;
            cpu_write(cpu, addr, value);
            cpu_update_nz(cpu, value);
            cycles = 6;
            break;
        case 0xFE: // INC abs,X
            addr = addr_absolute_x(cpu, false);
            value = cpu_read(cpu, addr) + 1;
            cpu_write(cpu, addr, value);
            cpu_update_nz(cpu, value);
            cycles = 7;
            break;

        // DEC - Decrement Memory
        case 0xC6: // DEC zp
            addr = addr_zeropage(cpu);
            value = cpu_read(cpu, addr) - 1;
            cpu_write(cpu, addr, value);
            cpu_update_nz(cpu, value);
            cycles = 5;
            break;
        case 0xD6: // DEC zp,X
            addr = addr_zeropage_x(cpu);
            value = cpu_read(cpu, addr) - 1;
            cpu_write(cpu, addr, value);
            cpu_update_nz(cpu, value);
            cycles = 6;
            break;
        case 0xCE: // DEC abs
            addr = addr_absolute(cpu);
            value = cpu_read(cpu, addr) - 1;
            cpu_write(cpu, addr, value);
            cpu_update_nz(cpu, value);
            cycles = 6;
            break;
        case 0xDE: // DEC abs,X
            addr = addr_absolute_x(cpu, false);
            value = cpu_read(cpu, addr) - 1;
            cpu_write(cpu, addr, value);
            cpu_update_nz(cpu, value);
            cycles = 7;
            break;

        // INX, INY, DEX, DEY
        case 0xE8: // INX
            cpu_read(cpu, cpu->PC);
            cpu->X++;
            cpu_update_nz(cpu, cpu->X);
            cycles = 2;
            break;
        case 0xC8: // INY
            cpu_read(cpu, cpu->PC);
            cpu->Y++;
            cpu_update_nz(cpu, cpu->Y);
            cycles = 2;
            break;
        case 0xCA: // DEX
            cpu_read(cpu, cpu->PC);
            cpu->X--;
            cpu_update_nz(cpu, cpu->X);
            cycles = 2;
            break;
        case 0x88: // DEY
            cpu_read(cpu, cpu->PC);
            cpu->Y--;
            cpu_update_nz(cpu, cpu->Y);
            cycles = 2;
            break;

        // ASL - Arithmetic Shift Left
        case 0x0A: // ASL A
            cpu_read(cpu, cpu->PC);
            cpu->A = op_asl(cpu, cpu->A);
            cycles = 2;
            break;
        case 0x06: // ASL zp
            addr = addr_zeropage(cpu);
            value = cpu_read(cpu, addr);
            cpu_write(cpu, addr, op_asl(cpu, value));
            cycles = 5;
            break;
        case 0x16: // ASL zp,X
            addr = addr_zeropage_x(cpu);
            value = cpu_read(cpu, addr);
            cpu_write(cpu, addr, op_asl(cpu, value));
            cycles = 6;
            break;
        case 0x0E: // ASL abs
            addr = addr_absolute(cpu);
            value = cpu_read(cpu, addr);
            cpu_write(cpu, addr, op_asl(cpu, value));
            cycles = 6;
            break;
        case 0x1E: // ASL abs,X
            addr = addr_absolute_x(cpu, false);
            value = cpu_read(cpu, addr);
            cpu_write(cpu, addr, op_asl(cpu, value));
            cycles = 7;
            break;

        // LSR - Logical Shift Right
        case 0x4A: // LSR A
            cpu_read(cpu, cpu->PC);
            cpu->A = op_lsr(cpu, cpu->A);
            cycles = 2;
            break;
        case 0x46: // LSR zp
            addr = addr_zeropage(cpu);
            value = cpu_read(cpu, addr);
            cpu_write(cpu, addr, op_lsr(cpu, value));
            cycles = 5;
            break;
        case 0x56: // LSR zp,X
            addr = addr_zeropage_x(cpu);
            value = cpu_read(cpu, addr);
            cpu_write(cpu, addr, op_lsr(cpu, value));
            cycles = 6;
            break;
        case 0x4E: // LSR abs
            addr = addr_absolute(cpu);
            value = cpu_read(cpu, addr);
            cpu_write(cpu, addr, op_lsr(cpu, value));
            cycles = 6;
            break;
        case 0x5E: // LSR abs,X
            addr = addr_absolute_x(cpu, false);
            value = cpu_read(cpu, addr);
            cpu_write(cpu, addr, op_lsr(cpu, value));
            cycles = 7;
            break;

        // ROL - Rotate Left
        case 0x2A: // ROL A
            cpu_read(cpu, cpu->PC);
            cpu->A = op_rol(cpu, cpu->A);
            cycles = 2;
            break;
        case 0x26: // ROL zp
            addr = addr_zeropage(cpu);
            value = cpu_read(cpu, addr);
            cpu_write(cpu, addr, op_rol(cpu, value));
            cycles = 5;
            break;
        case 0x36: // ROL zp,X
            addr = addr_zeropage_x(cpu);
            value = cpu_read(cpu, addr);
            cpu_write(cpu, addr, op_rol(cpu, value));
            cycles = 6;
            break;
        case 0x2E: // ROL abs
            addr = addr_absolute(cpu);
            value = cpu_read(cpu, addr);
            cpu_write(cpu, addr, op_rol(cpu, value));
            cycles = 6;
            break;
        case 0x3E: // ROL abs,X
            addr = addr_absolute_x(cpu, false);
            value = cpu_read(cpu, addr);
            cpu_write(cpu, addr, op_rol(cpu, value));
            cycles = 7;
            break;

        // ROR - Rotate Right
        case 0x6A: // ROR A
            cpu_read(cpu, cpu->PC);
            cpu->A = op_ror(cpu, cpu->A);
            cycles = 2;
            break;
        case 0x66: // ROR zp
            addr = addr_zeropage(cpu);
            value = cpu_read(cpu, addr);
            cpu_write(cpu, addr, op_ror(cpu, value));
            cycles = 5;
            break;
        case 0x76: // ROR zp,X
            addr = addr_zeropage_x(cpu);
            value = cpu_read(cpu, addr);
            cpu_write(cpu, addr, op_ror(cpu, value));
            cycles = 6;
            break;
        case 0x6E: // ROR abs
            addr = addr_absolute(cpu);
            value = cpu_read(cpu, addr);
            cpu_write(cpu, addr, op_ror(cpu, value));
            cycles = 6;
            break;
        case 0x7E: // ROR abs,X
            addr = addr_absolute_x(cpu, false);
            value = cpu_read(cpu, addr);
            cpu_write(cpu, addr, op_ror(cpu, value));
            cycles = 7;
            break;

        // JMP - Jump
        case 0x4C: // JMP abs
            cpu->PC = addr_absolute(cpu);
            cycles = 3;
            break;
        case 0x6C: // JMP (ind)
            cpu->PC = addr_indirect(cpu);
            cycles = 5;
            break;

        // JSR - Jump to Subroutine
        case 0x20: // JSR abs
            {
                u16 target = cpu_read(cpu, cpu->PC++);
                cpu_read(cpu, 0x0100 | cpu->SP);  // Dummy stack read
                cpu_push16(cpu, cpu->PC);
                target |= cpu_read(cpu, cpu->PC) << 8;
                cpu->PC = target;
            }
            cycles = 6;
            break;

        // RTS - Return from Subroutine
        case 0x60: // RTS
            cpu_read(cpu, cpu->PC);
            cpu_read(cpu, 0x0100 | cpu->SP);
            cpu->PC = cpu_pop16(cpu) + 1;
            cpu_read(cpu, cpu->PC);
            cycles = 6;
            break;

        // RTI - Return from Interrupt
        case 0x40: // RTI
            cpu_read(cpu, cpu->PC);
            cpu_read(cpu, 0x0100 | cpu->SP);
            cpu->P = (cpu_pop(cpu) & ~FLAG_B) | FLAG_U;
            cpu->PC = cpu_pop16(cpu);
            cycles = 6;
            break;

        // Branch instructions
        case 0x10: do_branch(cpu, !(cpu->P & FLAG_N)); cycles = 2; break; // BPL
        case 0x30: do_branch(cpu, cpu->P & FLAG_N); cycles = 2; break;    // BMI
        case 0x50: do_branch(cpu, !(cpu->P & FLAG_V)); cycles = 2; break; // BVC
        case 0x70: do_branch(cpu, cpu->P & FLAG_V); cycles = 2; break;    // BVS
        case 0x90: do_branch(cpu, !(cpu->P & FLAG_C)); cycles = 2; break; // BCC
        case 0xB0: do_branch(cpu, cpu->P & FLAG_C); cycles = 2; break;    // BCS
        case 0xD0: do_branch(cpu, !(cpu->P & FLAG_Z)); cycles = 2; break; // BNE
        case 0xF0: do_branch(cpu, cpu->P & FLAG_Z); cycles = 2; break;    // BEQ

        // Flag instructions
        case 0x18: // CLC
            cpu_read(cpu, cpu->PC);
            cpu->P &= ~FLAG_C;
            cycles = 2;
            break;
        case 0x38: // SEC
            cpu_read(cpu, cpu->PC);
            cpu->P |= FLAG_C;
            cycles = 2;
            break;
        case 0x58: // CLI
            cpu_read(cpu, cpu->PC);
            cpu->P &= ~FLAG_I;
            cycles = 2;
            break;
        case 0x78: // SEI
            cpu_read(cpu, cpu->PC);
            cpu->P |= FLAG_I;
            cycles = 2;
            break;
        case 0xB8: // CLV
            cpu_read(cpu, cpu->PC);
            cpu->P &= ~FLAG_V;
            cycles = 2;
            break;
        case 0xD8: // CLD
            cpu_read(cpu, cpu->PC);
            cpu->P &= ~FLAG_D;
            cycles = 2;
            break;
        case 0xF8: // SED
            cpu_read(cpu, cpu->PC);
            cpu->P |= FLAG_D;
            cycles = 2;
            break;

        // BRK - Break
        case 0x00: // BRK
            do_interrupt(cpu, 0xFFFE, true);
            cycles = 7;
            break;

        // NOP
        case 0xEA: // NOP
            cpu_read(cpu, cpu->PC);
            cycles = 2;
            break;

        default:
            // Unknown opcode - treat as NOP
            fprintf(stderr, "Unknown opcode: $%02X at $%04X\n", opcode, cpu->PC - 1);
            cycles = 2;
            break;
    }
    
    return cycles + cpu->extra_cycles;
}
