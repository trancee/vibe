/**
 * cpu.c - 6510 CPU emulation
 *
 * Full 6502-compatible CPU with 6510 I/O port.
 * Implements all legal opcodes with cycle-accurate timing.
 * Uses lookup table (LUT) for opcode dispatch.
 */

#include "cpu.h"
#include "c64.h"
#include <stdio.h>
#include <string.h>

// Opcode handler function type
typedef int (*OpcodeHandler)(C64Cpu *cpu);

// Forward declarations for internal functions
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

// ALU operations
static void op_adc(C64Cpu *cpu, u8 value);
static void op_sbc(C64Cpu *cpu, u8 value);
static void op_cmp(C64Cpu *cpu, u8 reg, u8 value);
static u8 op_asl(C64Cpu *cpu, u8 value);
static u8 op_lsr(C64Cpu *cpu, u8 value);
static u8 op_rol(C64Cpu *cpu, u8 value);
static u8 op_ror(C64Cpu *cpu, u8 value);

// Helpers
static void do_branch(C64Cpu *cpu, bool condition);
static void do_interrupt(C64Cpu *cpu, u16 vector, bool brk);

// ============================================================================
// CPU Initialization and Reset
// ============================================================================

void cpu_init(C64Cpu *cpu, C64System *sys)
{
    memset(cpu, 0, sizeof(C64Cpu));
    cpu->sys = sys;
    cpu->P = FLAG_U | FLAG_I;
    cpu->SP = 0xFD;
    cpu->port_dir = 0x2F;
    cpu->port_data = 0x37;
}

void cpu_reset(C64Cpu *cpu)
{
    cpu->A = 0;
    cpu->X = 0;
    cpu->Y = 0;
    cpu->SP = 0xFD;
    cpu->P = FLAG_U | FLAG_I;
    cpu->port_dir = 0x2F;
    cpu->port_data = 0x37;
    cpu->cpu_port_floating = 0xC0;
    cpu->nmi_pending = false;
    cpu->irq_pending = false;
    cpu->nmi_edge = false;
    cpu->extra_cycles = 0;
    cpu->page_crossed = false;

    u16 lo = mem_read_raw(&cpu->sys->mem, 0xFFFC);
    u16 hi = mem_read_raw(&cpu->sys->mem, 0xFFFD);
    cpu->PC = lo | (hi << 8);
}

// ============================================================================
// Memory Access
// ============================================================================

static u8 cpu_read(C64Cpu *cpu, u16 addr)
{
    if (addr == 0x0000)
    {
        if (cpu->sys->debug)
            printf("CPU #$%04X -> $%02X\n", addr, cpu->port_dir);
        return cpu->port_dir;
    }
    if (addr == 0x0001)
    {
        u8 output_bits = cpu->port_dir;
        u8 input_bits = ~cpu->port_dir;
        u8 result = 0;
        result |= (cpu->port_data & output_bits);
        u8 external = 0x1F;
        if (!(cpu->port_dir & 0x20))
            external &= ~0x20;
        external |= (cpu->cpu_port_floating & 0xC0);
        result |= (external & input_bits);
        if (cpu->sys->debug)
            printf("CPU #$%04X -> $%02X\n", addr, result);
        return result;
    }
    return mem_read(&cpu->sys->mem, addr);
}

static void cpu_write(C64Cpu *cpu, u16 addr, u8 value)
{
    if (addr == 0x0000)
    {
        if (cpu->sys->debug)
            printf("CPU #$%04X <- $%02X\n", addr, value);
        cpu->port_dir = value;
        return;
    }
    if (addr == 0x0001)
    {
        if (cpu->sys->debug)
            printf("CPU #$%04X <- $%02X\n", addr, value);
        cpu->port_data = value;
        cpu->cpu_port_floating = value;
        return;
    }
    mem_write(&cpu->sys->mem, addr, value);
}

// ============================================================================
// Stack Operations
// ============================================================================

static void cpu_push(C64Cpu *cpu, u8 value)
{
    cpu_write(cpu, 0x0100 | cpu->SP, value);
    cpu->SP--;
}

static u8 cpu_pop(C64Cpu *cpu)
{
    cpu->SP++;
    return cpu_read(cpu, 0x0100 | cpu->SP);
}

static void cpu_push16(C64Cpu *cpu, u16 value)
{
    cpu_push(cpu, (value >> 8) & 0xFF);
    cpu_push(cpu, value & 0xFF);
}

static u16 cpu_pop16(C64Cpu *cpu)
{
    u16 lo = cpu_pop(cpu);
    u16 hi = cpu_pop(cpu);
    return lo | (hi << 8);
}

// ============================================================================
// Addressing Modes
// ============================================================================

static u16 addr_immediate(C64Cpu *cpu) { return cpu->PC++; }

static u16 addr_zeropage(C64Cpu *cpu) { return cpu_read(cpu, cpu->PC++); }

static u16 addr_zeropage_x(C64Cpu *cpu)
{
    u8 base = cpu_read(cpu, cpu->PC++);
    cpu_read(cpu, base);
    return (base + cpu->X) & 0xFF;
}

static u16 addr_zeropage_y(C64Cpu *cpu)
{
    u8 base = cpu_read(cpu, cpu->PC++);
    cpu_read(cpu, base);
    return (base + cpu->Y) & 0xFF;
}

static u16 addr_absolute(C64Cpu *cpu)
{
    u16 lo = cpu_read(cpu, cpu->PC++);
    u16 hi = cpu_read(cpu, cpu->PC++);
    return lo | (hi << 8);
}

static u16 addr_absolute_x(C64Cpu *cpu, bool check_page)
{
    u16 lo = cpu_read(cpu, cpu->PC++);
    u16 hi = cpu_read(cpu, cpu->PC++);
    u16 base = lo | (hi << 8);
    u16 result = base + cpu->X;
    if (check_page && ((base & 0xFF00) != (result & 0xFF00)))
    {
        cpu->page_crossed = true;
        cpu->extra_cycles++;
    }
    return result;
}

static u16 addr_absolute_y(C64Cpu *cpu, bool check_page)
{
    u16 lo = cpu_read(cpu, cpu->PC++);
    u16 hi = cpu_read(cpu, cpu->PC++);
    u16 base = lo | (hi << 8);
    u16 result = base + cpu->Y;
    if (check_page && ((base & 0xFF00) != (result & 0xFF00)))
    {
        cpu->page_crossed = true;
        cpu->extra_cycles++;
    }
    return result;
}

static u16 addr_indirect(C64Cpu *cpu)
{
    u16 ptr_lo = cpu_read(cpu, cpu->PC++);
    u16 ptr_hi = cpu_read(cpu, cpu->PC++);
    u16 ptr = ptr_lo | (ptr_hi << 8);
    u16 lo = cpu_read(cpu, ptr);
    u16 hi = cpu_read(cpu, (ptr & 0xFF00) | ((ptr + 1) & 0xFF));
    return lo | (hi << 8);
}

static u16 addr_indirect_x(C64Cpu *cpu)
{
    u8 base = cpu_read(cpu, cpu->PC++);
    cpu_read(cpu, base);
    u8 ptr = (base + cpu->X) & 0xFF;
    u16 lo = cpu_read(cpu, ptr);
    u16 hi = cpu_read(cpu, (ptr + 1) & 0xFF);
    return lo | (hi << 8);
}

static u16 addr_indirect_y(C64Cpu *cpu, bool check_page)
{
    u8 ptr = cpu_read(cpu, cpu->PC++);
    u16 lo = cpu_read(cpu, ptr);
    u16 hi = cpu_read(cpu, (ptr + 1) & 0xFF);
    u16 base = lo | (hi << 8);
    u16 result = base + cpu->Y;
    if (check_page && ((base & 0xFF00) != (result & 0xFF00)))
    {
        cpu->page_crossed = true;
        cpu->extra_cycles++;
    }
    return result;
}

// ============================================================================
// ALU Operations
// ============================================================================

static void op_adc(C64Cpu *cpu, u8 value)
{
    if (cpu->P & FLAG_D)
    {
        u16 al = (cpu->A & 0x0F) + (value & 0x0F) + (cpu->P & FLAG_C ? 1 : 0);
        if (al > 9) al += 6;
        u16 ah = (cpu->A >> 4) + (value >> 4) + (al > 15 ? 1 : 0);
        u16 bin_result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu_set_flag(cpu, FLAG_Z, (bin_result & 0xFF) == 0);
        cpu_set_flag(cpu, FLAG_N, ah & 0x08);
        cpu_set_flag(cpu, FLAG_V, ((cpu->A ^ bin_result) & (value ^ bin_result) & 0x80) != 0);
        if (ah > 9) ah += 6;
        cpu_set_flag(cpu, FLAG_C, ah > 15);
        cpu->A = ((ah & 0x0F) << 4) | (al & 0x0F);
    }
    else
    {
        u16 result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu_set_flag(cpu, FLAG_C, result > 0xFF);
        cpu_set_flag(cpu, FLAG_V, ((cpu->A ^ result) & (value ^ result) & 0x80) != 0);
        cpu->A = result & 0xFF;
        cpu_update_nz(cpu, cpu->A);
    }
}

static void op_sbc(C64Cpu *cpu, u8 value)
{
    if (cpu->P & FLAG_D)
    {
        u16 al = (cpu->A & 0x0F) - (value & 0x0F) - (cpu->P & FLAG_C ? 0 : 1);
        u16 ah = (cpu->A >> 4) - (value >> 4);
        if (al & 0x10) { al -= 6; ah--; }
        if (ah & 0x10) { ah -= 6; }
        u16 bin_result = cpu->A - value - (cpu->P & FLAG_C ? 0 : 1);
        cpu_set_flag(cpu, FLAG_C, bin_result < 0x100);
        cpu_set_flag(cpu, FLAG_V, ((cpu->A ^ value) & (cpu->A ^ bin_result) & 0x80) != 0);
        cpu_update_nz(cpu, bin_result & 0xFF);
        cpu->A = ((ah & 0x0F) << 4) | (al & 0x0F);
    }
    else
    {
        u16 result = cpu->A - value - (cpu->P & FLAG_C ? 0 : 1);
        cpu_set_flag(cpu, FLAG_C, result < 0x100);
        cpu_set_flag(cpu, FLAG_V, ((cpu->A ^ value) & (cpu->A ^ result) & 0x80) != 0);
        cpu->A = result & 0xFF;
        cpu_update_nz(cpu, cpu->A);
    }
}

static void op_cmp(C64Cpu *cpu, u8 reg, u8 value)
{
    u16 result = reg - value;
    cpu_set_flag(cpu, FLAG_C, reg >= value);
    cpu_update_nz(cpu, result & 0xFF);
}

static u8 op_asl(C64Cpu *cpu, u8 value)
{
    cpu_set_flag(cpu, FLAG_C, value & 0x80);
    value <<= 1;
    cpu_update_nz(cpu, value);
    return value;
}

static u8 op_lsr(C64Cpu *cpu, u8 value)
{
    cpu_set_flag(cpu, FLAG_C, value & 0x01);
    value >>= 1;
    cpu_update_nz(cpu, value);
    return value;
}

static u8 op_rol(C64Cpu *cpu, u8 value)
{
    bool old_carry = cpu->P & FLAG_C;
    cpu_set_flag(cpu, FLAG_C, value & 0x80);
    value = (value << 1) | (old_carry ? 1 : 0);
    cpu_update_nz(cpu, value);
    return value;
}

static u8 op_ror(C64Cpu *cpu, u8 value)
{
    bool old_carry = cpu->P & FLAG_C;
    cpu_set_flag(cpu, FLAG_C, value & 0x01);
    value = (value >> 1) | (old_carry ? 0x80 : 0);
    cpu_update_nz(cpu, value);
    return value;
}

// ============================================================================
// Helpers
// ============================================================================

static void do_branch(C64Cpu *cpu, bool condition)
{
    i8 offset = (i8)cpu_read(cpu, cpu->PC++);
    if (condition)
    {
        u16 old_pc = cpu->PC;
        cpu->PC += offset;
        cpu->extra_cycles++;
        if ((old_pc & 0xFF00) != (cpu->PC & 0xFF00))
            cpu->extra_cycles++;
        cpu_read(cpu, old_pc);
    }
}

static void do_interrupt(C64Cpu *cpu, u16 vector, bool brk)
{
    cpu_read(cpu, cpu->PC);
    if (brk) cpu->PC++;
    cpu_push16(cpu, cpu->PC);
    cpu_push(cpu, cpu->P | FLAG_U | (brk ? FLAG_B : 0));
    cpu->P |= FLAG_I;
    u16 lo = cpu_read(cpu, vector);
    u16 hi = cpu_read(cpu, vector + 1);
    cpu->PC = lo | (hi << 8);
}

void cpu_trigger_nmi(C64Cpu *cpu)
{
    if (!cpu->nmi_edge)
    {
        cpu->nmi_pending = true;
        cpu->nmi_edge = true;
    }
}

void cpu_trigger_irq(C64Cpu *cpu)
{
    cpu->irq_pending = true;
}

// ============================================================================
// Opcode Handlers
// ============================================================================

// BRK
static int op_00(C64Cpu *cpu) { do_interrupt(cpu, 0xFFFE, true); return 7; }

// ORA
static int op_01(C64Cpu *cpu) { cpu->A |= cpu_read(cpu, addr_indirect_x(cpu)); cpu_update_nz(cpu, cpu->A); return 6; }
static int op_05(C64Cpu *cpu) { cpu->A |= cpu_read(cpu, addr_zeropage(cpu)); cpu_update_nz(cpu, cpu->A); return 3; }
static int op_09(C64Cpu *cpu) { cpu->A |= cpu_read(cpu, addr_immediate(cpu)); cpu_update_nz(cpu, cpu->A); return 2; }
static int op_0D(C64Cpu *cpu) { cpu->A |= cpu_read(cpu, addr_absolute(cpu)); cpu_update_nz(cpu, cpu->A); return 4; }
static int op_11(C64Cpu *cpu) { cpu->A |= cpu_read(cpu, addr_indirect_y(cpu, true)); cpu_update_nz(cpu, cpu->A); return 5; }
static int op_15(C64Cpu *cpu) { cpu->A |= cpu_read(cpu, addr_zeropage_x(cpu)); cpu_update_nz(cpu, cpu->A); return 4; }
static int op_19(C64Cpu *cpu) { cpu->A |= cpu_read(cpu, addr_absolute_y(cpu, true)); cpu_update_nz(cpu, cpu->A); return 4; }
static int op_1D(C64Cpu *cpu) { cpu->A |= cpu_read(cpu, addr_absolute_x(cpu, true)); cpu_update_nz(cpu, cpu->A); return 4; }

// ASL
static int op_06(C64Cpu *cpu) { u16 a = addr_zeropage(cpu); u8 v = cpu_read(cpu, a); cpu_write(cpu, a, op_asl(cpu, v)); return 5; }
static int op_0A(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->A = op_asl(cpu, cpu->A); return 2; }
static int op_0E(C64Cpu *cpu) { u16 a = addr_absolute(cpu); u8 v = cpu_read(cpu, a); cpu_write(cpu, a, op_asl(cpu, v)); return 6; }
static int op_16(C64Cpu *cpu) { u16 a = addr_zeropage_x(cpu); u8 v = cpu_read(cpu, a); cpu_write(cpu, a, op_asl(cpu, v)); return 6; }
static int op_1E(C64Cpu *cpu) { u16 a = addr_absolute_x(cpu, false); u8 v = cpu_read(cpu, a); cpu_write(cpu, a, op_asl(cpu, v)); return 7; }

// PHP, PLP, PHA, PLA
static int op_08(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu_push(cpu, cpu->P | FLAG_B | FLAG_U); return 3; }
static int op_28(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu_read(cpu, 0x0100 | cpu->SP); cpu->P = (cpu_pop(cpu) & ~FLAG_B) | FLAG_U; return 4; }
static int op_48(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu_push(cpu, cpu->A); return 3; }
static int op_68(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu_read(cpu, 0x0100 | cpu->SP); cpu->A = cpu_pop(cpu); cpu_update_nz(cpu, cpu->A); return 4; }

// Branches
static int op_10(C64Cpu *cpu) { do_branch(cpu, !(cpu->P & FLAG_N)); return 2; }
static int op_30(C64Cpu *cpu) { do_branch(cpu, cpu->P & FLAG_N); return 2; }
static int op_50(C64Cpu *cpu) { do_branch(cpu, !(cpu->P & FLAG_V)); return 2; }
static int op_70(C64Cpu *cpu) { do_branch(cpu, cpu->P & FLAG_V); return 2; }
static int op_90(C64Cpu *cpu) { do_branch(cpu, !(cpu->P & FLAG_C)); return 2; }
static int op_B0(C64Cpu *cpu) { do_branch(cpu, cpu->P & FLAG_C); return 2; }
static int op_D0(C64Cpu *cpu) { do_branch(cpu, !(cpu->P & FLAG_Z)); return 2; }
static int op_F0(C64Cpu *cpu) { do_branch(cpu, cpu->P & FLAG_Z); return 2; }

// Flags
static int op_18(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->P &= ~FLAG_C; return 2; }
static int op_38(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->P |= FLAG_C; return 2; }
static int op_58(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->P &= ~FLAG_I; return 2; }
static int op_78(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->P |= FLAG_I; return 2; }
static int op_B8(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->P &= ~FLAG_V; return 2; }
static int op_D8(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->P &= ~FLAG_D; return 2; }
static int op_F8(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->P |= FLAG_D; return 2; }

// AND
static int op_21(C64Cpu *cpu) { cpu->A &= cpu_read(cpu, addr_indirect_x(cpu)); cpu_update_nz(cpu, cpu->A); return 6; }
static int op_25(C64Cpu *cpu) { cpu->A &= cpu_read(cpu, addr_zeropage(cpu)); cpu_update_nz(cpu, cpu->A); return 3; }
static int op_29(C64Cpu *cpu) { cpu->A &= cpu_read(cpu, addr_immediate(cpu)); cpu_update_nz(cpu, cpu->A); return 2; }
static int op_2D(C64Cpu *cpu) { cpu->A &= cpu_read(cpu, addr_absolute(cpu)); cpu_update_nz(cpu, cpu->A); return 4; }
static int op_31(C64Cpu *cpu) { cpu->A &= cpu_read(cpu, addr_indirect_y(cpu, true)); cpu_update_nz(cpu, cpu->A); return 5; }
static int op_35(C64Cpu *cpu) { cpu->A &= cpu_read(cpu, addr_zeropage_x(cpu)); cpu_update_nz(cpu, cpu->A); return 4; }
static int op_39(C64Cpu *cpu) { cpu->A &= cpu_read(cpu, addr_absolute_y(cpu, true)); cpu_update_nz(cpu, cpu->A); return 4; }
static int op_3D(C64Cpu *cpu) { cpu->A &= cpu_read(cpu, addr_absolute_x(cpu, true)); cpu_update_nz(cpu, cpu->A); return 4; }

// BIT
static int op_24(C64Cpu *cpu) { u8 v = cpu_read(cpu, addr_zeropage(cpu)); cpu_set_flag(cpu, FLAG_Z, (cpu->A & v) == 0); cpu_set_flag(cpu, FLAG_N, v & 0x80); cpu_set_flag(cpu, FLAG_V, v & 0x40); return 3; }
static int op_2C(C64Cpu *cpu) { u8 v = cpu_read(cpu, addr_absolute(cpu)); cpu_set_flag(cpu, FLAG_Z, (cpu->A & v) == 0); cpu_set_flag(cpu, FLAG_N, v & 0x80); cpu_set_flag(cpu, FLAG_V, v & 0x40); return 4; }

// ROL
static int op_26(C64Cpu *cpu) { u16 a = addr_zeropage(cpu); u8 v = cpu_read(cpu, a); cpu_write(cpu, a, op_rol(cpu, v)); return 5; }
static int op_2A(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->A = op_rol(cpu, cpu->A); return 2; }
static int op_2E(C64Cpu *cpu) { u16 a = addr_absolute(cpu); u8 v = cpu_read(cpu, a); cpu_write(cpu, a, op_rol(cpu, v)); return 6; }
static int op_36(C64Cpu *cpu) { u16 a = addr_zeropage_x(cpu); u8 v = cpu_read(cpu, a); cpu_write(cpu, a, op_rol(cpu, v)); return 6; }
static int op_3E(C64Cpu *cpu) { u16 a = addr_absolute_x(cpu, false); u8 v = cpu_read(cpu, a); cpu_write(cpu, a, op_rol(cpu, v)); return 7; }

// RTI
static int op_40(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu_read(cpu, 0x0100 | cpu->SP); cpu->P = (cpu_pop(cpu) & ~FLAG_B) | FLAG_U; cpu->PC = cpu_pop16(cpu); return 6; }

// EOR
static int op_41(C64Cpu *cpu) { cpu->A ^= cpu_read(cpu, addr_indirect_x(cpu)); cpu_update_nz(cpu, cpu->A); return 6; }
static int op_45(C64Cpu *cpu) { cpu->A ^= cpu_read(cpu, addr_zeropage(cpu)); cpu_update_nz(cpu, cpu->A); return 3; }
static int op_49(C64Cpu *cpu) { cpu->A ^= cpu_read(cpu, addr_immediate(cpu)); cpu_update_nz(cpu, cpu->A); return 2; }
static int op_4D(C64Cpu *cpu) { cpu->A ^= cpu_read(cpu, addr_absolute(cpu)); cpu_update_nz(cpu, cpu->A); return 4; }
static int op_51(C64Cpu *cpu) { cpu->A ^= cpu_read(cpu, addr_indirect_y(cpu, true)); cpu_update_nz(cpu, cpu->A); return 5; }
static int op_55(C64Cpu *cpu) { cpu->A ^= cpu_read(cpu, addr_zeropage_x(cpu)); cpu_update_nz(cpu, cpu->A); return 4; }
static int op_59(C64Cpu *cpu) { cpu->A ^= cpu_read(cpu, addr_absolute_y(cpu, true)); cpu_update_nz(cpu, cpu->A); return 4; }
static int op_5D(C64Cpu *cpu) { cpu->A ^= cpu_read(cpu, addr_absolute_x(cpu, true)); cpu_update_nz(cpu, cpu->A); return 4; }

// LSR
static int op_46(C64Cpu *cpu) { u16 a = addr_zeropage(cpu); u8 v = cpu_read(cpu, a); cpu_write(cpu, a, op_lsr(cpu, v)); return 5; }
static int op_4A(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->A = op_lsr(cpu, cpu->A); return 2; }
static int op_4E(C64Cpu *cpu) { u16 a = addr_absolute(cpu); u8 v = cpu_read(cpu, a); cpu_write(cpu, a, op_lsr(cpu, v)); return 6; }
static int op_56(C64Cpu *cpu) { u16 a = addr_zeropage_x(cpu); u8 v = cpu_read(cpu, a); cpu_write(cpu, a, op_lsr(cpu, v)); return 6; }
static int op_5E(C64Cpu *cpu) { u16 a = addr_absolute_x(cpu, false); u8 v = cpu_read(cpu, a); cpu_write(cpu, a, op_lsr(cpu, v)); return 7; }

// JMP
static int op_4C(C64Cpu *cpu) { cpu->PC = addr_absolute(cpu); return 3; }
static int op_6C(C64Cpu *cpu) { cpu->PC = addr_indirect(cpu); return 5; }

// RTS
static int op_60(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu_read(cpu, 0x0100 | cpu->SP); cpu->PC = cpu_pop16(cpu) + 1; cpu_read(cpu, cpu->PC); return 6; }

// ADC
static int op_61(C64Cpu *cpu) { op_adc(cpu, cpu_read(cpu, addr_indirect_x(cpu))); return 6; }
static int op_65(C64Cpu *cpu) { op_adc(cpu, cpu_read(cpu, addr_zeropage(cpu))); return 3; }
static int op_69(C64Cpu *cpu) { op_adc(cpu, cpu_read(cpu, addr_immediate(cpu))); return 2; }
static int op_6D(C64Cpu *cpu) { op_adc(cpu, cpu_read(cpu, addr_absolute(cpu))); return 4; }
static int op_71(C64Cpu *cpu) { op_adc(cpu, cpu_read(cpu, addr_indirect_y(cpu, true))); return 5; }
static int op_75(C64Cpu *cpu) { op_adc(cpu, cpu_read(cpu, addr_zeropage_x(cpu))); return 4; }
static int op_79(C64Cpu *cpu) { op_adc(cpu, cpu_read(cpu, addr_absolute_y(cpu, true))); return 4; }
static int op_7D(C64Cpu *cpu) { op_adc(cpu, cpu_read(cpu, addr_absolute_x(cpu, true))); return 4; }

// ROR
static int op_66(C64Cpu *cpu) { u16 a = addr_zeropage(cpu); u8 v = cpu_read(cpu, a); cpu_write(cpu, a, op_ror(cpu, v)); return 5; }
static int op_6A(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->A = op_ror(cpu, cpu->A); return 2; }
static int op_6E(C64Cpu *cpu) { u16 a = addr_absolute(cpu); u8 v = cpu_read(cpu, a); cpu_write(cpu, a, op_ror(cpu, v)); return 6; }
static int op_76(C64Cpu *cpu) { u16 a = addr_zeropage_x(cpu); u8 v = cpu_read(cpu, a); cpu_write(cpu, a, op_ror(cpu, v)); return 6; }
static int op_7E(C64Cpu *cpu) { u16 a = addr_absolute_x(cpu, false); u8 v = cpu_read(cpu, a); cpu_write(cpu, a, op_ror(cpu, v)); return 7; }

// STA
static int op_81(C64Cpu *cpu) { cpu_write(cpu, addr_indirect_x(cpu), cpu->A); return 6; }
static int op_85(C64Cpu *cpu) { cpu_write(cpu, addr_zeropage(cpu), cpu->A); return 3; }
static int op_8D(C64Cpu *cpu) { cpu_write(cpu, addr_absolute(cpu), cpu->A); return 4; }
static int op_91(C64Cpu *cpu) { cpu_write(cpu, addr_indirect_y(cpu, false), cpu->A); return 6; }
static int op_95(C64Cpu *cpu) { cpu_write(cpu, addr_zeropage_x(cpu), cpu->A); return 4; }
static int op_99(C64Cpu *cpu) { cpu_write(cpu, addr_absolute_y(cpu, false), cpu->A); return 5; }
static int op_9D(C64Cpu *cpu) { cpu_write(cpu, addr_absolute_x(cpu, false), cpu->A); return 5; }

// STX
static int op_86(C64Cpu *cpu) { cpu_write(cpu, addr_zeropage(cpu), cpu->X); return 3; }
static int op_8E(C64Cpu *cpu) { cpu_write(cpu, addr_absolute(cpu), cpu->X); return 4; }
static int op_96(C64Cpu *cpu) { cpu_write(cpu, addr_zeropage_y(cpu), cpu->X); return 4; }

// STY
static int op_84(C64Cpu *cpu) { cpu_write(cpu, addr_zeropage(cpu), cpu->Y); return 3; }
static int op_8C(C64Cpu *cpu) { cpu_write(cpu, addr_absolute(cpu), cpu->Y); return 4; }
static int op_94(C64Cpu *cpu) { cpu_write(cpu, addr_zeropage_x(cpu), cpu->Y); return 4; }

// Transfers
static int op_8A(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->A = cpu->X; cpu_update_nz(cpu, cpu->A); return 2; }
static int op_98(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->A = cpu->Y; cpu_update_nz(cpu, cpu->A); return 2; }
static int op_9A(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->SP = cpu->X; return 2; }
static int op_AA(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->X = cpu->A; cpu_update_nz(cpu, cpu->X); return 2; }
static int op_BA(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->X = cpu->SP; cpu_update_nz(cpu, cpu->X); return 2; }
static int op_A8(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->Y = cpu->A; cpu_update_nz(cpu, cpu->Y); return 2; }

// DEY, DEX, INY, INX
static int op_88(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->Y--; cpu_update_nz(cpu, cpu->Y); return 2; }
static int op_CA(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->X--; cpu_update_nz(cpu, cpu->X); return 2; }
static int op_C8(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->Y++; cpu_update_nz(cpu, cpu->Y); return 2; }
static int op_E8(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); cpu->X++; cpu_update_nz(cpu, cpu->X); return 2; }

// LDA
static int op_A1(C64Cpu *cpu) { cpu->A = cpu_read(cpu, addr_indirect_x(cpu)); cpu_update_nz(cpu, cpu->A); return 6; }
static int op_A5(C64Cpu *cpu) { cpu->A = cpu_read(cpu, addr_zeropage(cpu)); cpu_update_nz(cpu, cpu->A); return 3; }
static int op_A9(C64Cpu *cpu) { cpu->A = cpu_read(cpu, addr_immediate(cpu)); cpu_update_nz(cpu, cpu->A); return 2; }
static int op_AD(C64Cpu *cpu) { cpu->A = cpu_read(cpu, addr_absolute(cpu)); cpu_update_nz(cpu, cpu->A); return 4; }
static int op_B1(C64Cpu *cpu) { cpu->A = cpu_read(cpu, addr_indirect_y(cpu, true)); cpu_update_nz(cpu, cpu->A); return 5; }
static int op_B5(C64Cpu *cpu) { cpu->A = cpu_read(cpu, addr_zeropage_x(cpu)); cpu_update_nz(cpu, cpu->A); return 4; }
static int op_B9(C64Cpu *cpu) { cpu->A = cpu_read(cpu, addr_absolute_y(cpu, true)); cpu_update_nz(cpu, cpu->A); return 4; }
static int op_BD(C64Cpu *cpu) { cpu->A = cpu_read(cpu, addr_absolute_x(cpu, true)); cpu_update_nz(cpu, cpu->A); return 4; }

// LDX
static int op_A2(C64Cpu *cpu) { cpu->X = cpu_read(cpu, addr_immediate(cpu)); cpu_update_nz(cpu, cpu->X); return 2; }
static int op_A6(C64Cpu *cpu) { cpu->X = cpu_read(cpu, addr_zeropage(cpu)); cpu_update_nz(cpu, cpu->X); return 3; }
static int op_AE(C64Cpu *cpu) { cpu->X = cpu_read(cpu, addr_absolute(cpu)); cpu_update_nz(cpu, cpu->X); return 4; }
static int op_B6(C64Cpu *cpu) { cpu->X = cpu_read(cpu, addr_zeropage_y(cpu)); cpu_update_nz(cpu, cpu->X); return 4; }
static int op_BE(C64Cpu *cpu) { cpu->X = cpu_read(cpu, addr_absolute_y(cpu, true)); cpu_update_nz(cpu, cpu->X); return 4; }

// LDY
static int op_A0(C64Cpu *cpu) { cpu->Y = cpu_read(cpu, addr_immediate(cpu)); cpu_update_nz(cpu, cpu->Y); return 2; }
static int op_A4(C64Cpu *cpu) { cpu->Y = cpu_read(cpu, addr_zeropage(cpu)); cpu_update_nz(cpu, cpu->Y); return 3; }
static int op_AC(C64Cpu *cpu) { cpu->Y = cpu_read(cpu, addr_absolute(cpu)); cpu_update_nz(cpu, cpu->Y); return 4; }
static int op_B4(C64Cpu *cpu) { cpu->Y = cpu_read(cpu, addr_zeropage_x(cpu)); cpu_update_nz(cpu, cpu->Y); return 4; }
static int op_BC(C64Cpu *cpu) { cpu->Y = cpu_read(cpu, addr_absolute_x(cpu, true)); cpu_update_nz(cpu, cpu->Y); return 4; }

// CMP
static int op_C1(C64Cpu *cpu) { op_cmp(cpu, cpu->A, cpu_read(cpu, addr_indirect_x(cpu))); return 6; }
static int op_C5(C64Cpu *cpu) { op_cmp(cpu, cpu->A, cpu_read(cpu, addr_zeropage(cpu))); return 3; }
static int op_C9(C64Cpu *cpu) { op_cmp(cpu, cpu->A, cpu_read(cpu, addr_immediate(cpu))); return 2; }
static int op_CD(C64Cpu *cpu) { op_cmp(cpu, cpu->A, cpu_read(cpu, addr_absolute(cpu))); return 4; }
static int op_D1(C64Cpu *cpu) { op_cmp(cpu, cpu->A, cpu_read(cpu, addr_indirect_y(cpu, true))); return 5; }
static int op_D5(C64Cpu *cpu) { op_cmp(cpu, cpu->A, cpu_read(cpu, addr_zeropage_x(cpu))); return 4; }
static int op_D9(C64Cpu *cpu) { op_cmp(cpu, cpu->A, cpu_read(cpu, addr_absolute_y(cpu, true))); return 4; }
static int op_DD(C64Cpu *cpu) { op_cmp(cpu, cpu->A, cpu_read(cpu, addr_absolute_x(cpu, true))); return 4; }

// CPX
static int op_E0(C64Cpu *cpu) { op_cmp(cpu, cpu->X, cpu_read(cpu, addr_immediate(cpu))); return 2; }
static int op_E4(C64Cpu *cpu) { op_cmp(cpu, cpu->X, cpu_read(cpu, addr_zeropage(cpu))); return 3; }
static int op_EC(C64Cpu *cpu) { op_cmp(cpu, cpu->X, cpu_read(cpu, addr_absolute(cpu))); return 4; }

// CPY
static int op_C0(C64Cpu *cpu) { op_cmp(cpu, cpu->Y, cpu_read(cpu, addr_immediate(cpu))); return 2; }
static int op_C4(C64Cpu *cpu) { op_cmp(cpu, cpu->Y, cpu_read(cpu, addr_zeropage(cpu))); return 3; }
static int op_CC(C64Cpu *cpu) { op_cmp(cpu, cpu->Y, cpu_read(cpu, addr_absolute(cpu))); return 4; }

// DEC
static int op_C6(C64Cpu *cpu) { u16 a = addr_zeropage(cpu); u8 v = cpu_read(cpu, a) - 1; cpu_write(cpu, a, v); cpu_update_nz(cpu, v); return 5; }
static int op_CE(C64Cpu *cpu) { u16 a = addr_absolute(cpu); u8 v = cpu_read(cpu, a) - 1; cpu_write(cpu, a, v); cpu_update_nz(cpu, v); return 6; }
static int op_D6(C64Cpu *cpu) { u16 a = addr_zeropage_x(cpu); u8 v = cpu_read(cpu, a) - 1; cpu_write(cpu, a, v); cpu_update_nz(cpu, v); return 6; }
static int op_DE(C64Cpu *cpu) { u16 a = addr_absolute_x(cpu, false); u8 v = cpu_read(cpu, a) - 1; cpu_write(cpu, a, v); cpu_update_nz(cpu, v); return 7; }

// INC
static int op_E6(C64Cpu *cpu) { u16 a = addr_zeropage(cpu); u8 v = cpu_read(cpu, a) + 1; cpu_write(cpu, a, v); cpu_update_nz(cpu, v); return 5; }
static int op_EE(C64Cpu *cpu) { u16 a = addr_absolute(cpu); u8 v = cpu_read(cpu, a) + 1; cpu_write(cpu, a, v); cpu_update_nz(cpu, v); return 6; }
static int op_F6(C64Cpu *cpu) { u16 a = addr_zeropage_x(cpu); u8 v = cpu_read(cpu, a) + 1; cpu_write(cpu, a, v); cpu_update_nz(cpu, v); return 6; }
static int op_FE(C64Cpu *cpu) { u16 a = addr_absolute_x(cpu, false); u8 v = cpu_read(cpu, a) + 1; cpu_write(cpu, a, v); cpu_update_nz(cpu, v); return 7; }

// SBC
static int op_E1(C64Cpu *cpu) { op_sbc(cpu, cpu_read(cpu, addr_indirect_x(cpu))); return 6; }
static int op_E5(C64Cpu *cpu) { op_sbc(cpu, cpu_read(cpu, addr_zeropage(cpu))); return 3; }
static int op_E9(C64Cpu *cpu) { op_sbc(cpu, cpu_read(cpu, addr_immediate(cpu))); return 2; }
static int op_ED(C64Cpu *cpu) { op_sbc(cpu, cpu_read(cpu, addr_absolute(cpu))); return 4; }
static int op_F1(C64Cpu *cpu) { op_sbc(cpu, cpu_read(cpu, addr_indirect_y(cpu, true))); return 5; }
static int op_F5(C64Cpu *cpu) { op_sbc(cpu, cpu_read(cpu, addr_zeropage_x(cpu))); return 4; }
static int op_F9(C64Cpu *cpu) { op_sbc(cpu, cpu_read(cpu, addr_absolute_y(cpu, true))); return 4; }
static int op_FD(C64Cpu *cpu) { op_sbc(cpu, cpu_read(cpu, addr_absolute_x(cpu, true))); return 4; }

// JSR
static int op_20(C64Cpu *cpu)
{
    u16 target = cpu_read(cpu, cpu->PC++);
    cpu_read(cpu, 0x0100 | cpu->SP);
    cpu_push16(cpu, cpu->PC);
    target |= cpu_read(cpu, cpu->PC) << 8;
    cpu->PC = target;
    return 6;
}

// NOP
static int op_EA(C64Cpu *cpu) { cpu_read(cpu, cpu->PC); return 2; }

// Unknown opcode
static int op_XX(C64Cpu *cpu) { fprintf(stderr, "Unknown opcode at $%04X\n", cpu->PC - 1); return 2; }

// ============================================================================
// Opcode Lookup Table
// ============================================================================

static const OpcodeHandler opcode_table[256] = {
    op_00, op_01, op_XX, op_XX, op_XX, op_05, op_06, op_XX, op_08, op_09, op_0A, op_XX, op_XX, op_0D, op_0E, op_XX,
    op_10, op_11, op_XX, op_XX, op_XX, op_15, op_16, op_XX, op_18, op_19, op_XX, op_XX, op_XX, op_1D, op_1E, op_XX,
    op_20, op_21, op_XX, op_XX, op_24, op_25, op_26, op_XX, op_28, op_29, op_2A, op_XX, op_2C, op_2D, op_2E, op_XX,
    op_30, op_31, op_XX, op_XX, op_XX, op_35, op_36, op_XX, op_38, op_39, op_XX, op_XX, op_XX, op_3D, op_3E, op_XX,
    op_40, op_41, op_XX, op_XX, op_XX, op_45, op_46, op_XX, op_48, op_49, op_4A, op_XX, op_4C, op_4D, op_4E, op_XX,
    op_50, op_51, op_XX, op_XX, op_XX, op_55, op_56, op_XX, op_58, op_59, op_XX, op_XX, op_XX, op_5D, op_5E, op_XX,
    op_60, op_61, op_XX, op_XX, op_XX, op_65, op_66, op_XX, op_68, op_69, op_6A, op_XX, op_6C, op_6D, op_6E, op_XX,
    op_70, op_71, op_XX, op_XX, op_XX, op_75, op_76, op_XX, op_78, op_79, op_XX, op_XX, op_XX, op_7D, op_7E, op_XX,
    op_XX, op_81, op_XX, op_XX, op_84, op_85, op_86, op_XX, op_88, op_XX, op_8A, op_XX, op_8C, op_8D, op_8E, op_XX,
    op_90, op_91, op_XX, op_XX, op_94, op_95, op_96, op_XX, op_98, op_99, op_9A, op_XX, op_XX, op_9D, op_XX, op_XX,
    op_A0, op_A1, op_A2, op_XX, op_A4, op_A5, op_A6, op_XX, op_A8, op_A9, op_AA, op_XX, op_AC, op_AD, op_AE, op_XX,
    op_B0, op_B1, op_XX, op_XX, op_B4, op_B5, op_B6, op_XX, op_B8, op_B9, op_BA, op_XX, op_BC, op_BD, op_BE, op_XX,
    op_C0, op_C1, op_XX, op_XX, op_C4, op_C5, op_C6, op_XX, op_C8, op_C9, op_CA, op_XX, op_CC, op_CD, op_CE, op_XX,
    op_D0, op_D1, op_XX, op_XX, op_XX, op_D5, op_D6, op_XX, op_D8, op_D9, op_XX, op_XX, op_XX, op_DD, op_DE, op_XX,
    op_E0, op_E1, op_XX, op_XX, op_E4, op_E5, op_E6, op_XX, op_E8, op_E9, op_EA, op_XX, op_EC, op_ED, op_EE, op_XX,
    op_F0, op_F1, op_XX, op_XX, op_XX, op_F5, op_F6, op_XX, op_F8, op_F9, op_XX, op_XX, op_XX, op_FD, op_FE, op_XX,
};

// ============================================================================
// CPU Step - Execute one instruction using LUT
// ============================================================================

int cpu_step(C64Cpu *cpu)
{
    cpu->extra_cycles = 0;
    cpu->page_crossed = false;

    if (cpu->nmi_pending)
    {
        cpu->nmi_pending = false;
        do_interrupt(cpu, 0xFFFA, false);
        return 7;
    }

    if (cpu->irq_pending && !(cpu->P & FLAG_I))
    {
        cpu->irq_pending = false;
        do_interrupt(cpu, 0xFFFE, false);
        return 7;
    }

    u8 opcode = cpu_read(cpu, cpu->PC++);
    int cycles = opcode_table[opcode](cpu);

    return cycles + cpu->extra_cycles;
}
