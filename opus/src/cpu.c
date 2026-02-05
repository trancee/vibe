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
typedef int (*OpcodeHandler)(CPU *cpu);

// Forward declarations for internal functions
static u8 cpu_read(CPU *cpu, u16 addr);
static void cpu_write(CPU *cpu, u16 addr, u8 value);
static void cpu_push(CPU *cpu, u8 value);
static u8 cpu_pop(CPU *cpu);
static void cpu_push16(CPU *cpu, u16 value);
static u16 cpu_pop16(CPU *cpu);

// Addressing mode helpers
static u16 addr_immediate(CPU *cpu);
static u16 addr_zeropage(CPU *cpu);
static u16 addr_zeropage_x(CPU *cpu);
static u16 addr_zeropage_y(CPU *cpu);
static u16 addr_absolute(CPU *cpu);
static u16 addr_absolute_x(CPU *cpu, bool check_page);
static u16 addr_absolute_y(CPU *cpu, bool check_page);
static u16 addr_indirect(CPU *cpu);
static u16 addr_indirect_x(CPU *cpu);
static u16 addr_indirect_y(CPU *cpu, bool check_page);

// ALU operations
static void op_adc(CPU *cpu, u8 value);
static void op_sbc(CPU *cpu, u8 value);
static void op_cmp(CPU *cpu, u8 reg, u8 value);
static u8 op_asl(CPU *cpu, u8 value);
static u8 op_lsr(CPU *cpu, u8 value);
static u8 op_rol(CPU *cpu, u8 value);
static u8 op_ror(CPU *cpu, u8 value);

// Helpers
static void do_branch(CPU *cpu, bool condition);
static void do_interrupt(CPU *cpu, u16 vector, bool brk);

// ============================================================================
// CPU Initialization and Reset
// ============================================================================

void cpu_init(CPU *cpu, C64 *sys)
{
    memset(cpu, 0, sizeof(CPU));
    cpu->sys = sys;
    cpu->P = FLAG_U | FLAG_I;
    cpu->SP = 0xFD;
    cpu->port_dir = 0x2F;
    cpu->port_data = 0x37;
    cpu->port_latch = 0x37;
    cpu->mode = CPU_MODE_6510; // Default: enable 6510 I/O port
}

void cpu_reset(CPU *cpu)
{
    cpu->A = 0;
    cpu->X = 0;
    cpu->Y = 0;
    cpu->SP = 0xFD;
    cpu->P = FLAG_U | FLAG_I;
    cpu->port_dir = 0x2F;
    cpu->port_data = 0x37;
    cpu->port_latch = 0x37;
    cpu->cpu_port_floating = 0xC0;
    cpu->nmi_pending = false;
    cpu->nmi_sampled = false;
    cpu->irq_pending = false;
    cpu->irq_pending_new = false;
    cpu->nmi_edge = false;
    cpu->nmi_triggered_this_insn = false;
    cpu->extra_cycles = 0;
    cpu->page_crossed = false;

    u16 lo = mem_read_raw(&cpu->sys->mem, 0xFFFC);
    u16 hi = mem_read_raw(&cpu->sys->mem, 0xFFFD);
    cpu->PC = lo | (hi << 8);
}

// ============================================================================
// Memory Access
// ============================================================================

static u8 cpu_read(CPU *cpu, u16 addr)
{
    // 6510 I/O port handling (only when enabled)
    if (cpu->mode == CPU_MODE_6510)
    {
        if (addr == 0x0000)
        {
            // Must tick the system even for port reads (every memory access = 1 cycle)
            c64_tick(cpu->sys);
            if (cpu->sys->debug)
                printf("CPU #$%04X -> $%02X\n", addr, cpu->port_dir);
            return cpu->port_dir;
        }
        if (addr == 0x0001)
        {
            // Must tick the system even for port reads (every memory access = 1 cycle)
            c64_tick(cpu->sys);

            u8 output_bits = cpu->port_dir;
            u8 input_bits = ~cpu->port_dir;
            u8 result = 0;

            // Output bits return the value written to port_data
            result |= (cpu->port_data & output_bits);

            // Input bits return external hardware state:
            // - Bits 0-2 (LORAM, HIRAM, CHAREN): External pullup resistors pull high
            // - Bit 3: No pullup, retains last OUTPUT value (from latch)
            // - Bit 4: Directly connected, reads high when input (similar to bits 0-2)
            // - Bit 5: Cassette motor, no pullup, drawn LOW when input
            // - Bits 6-7: Datasette lines, retain last OUTPUT value (from latch)
            u8 external = 0x17;                   // Bits 0-2,4 read high when input
            external |= (cpu->port_latch & 0x08); // Bit 3 retains latch value
            // Bit 5 is 0 when input (no pullup, drawn low)
            external |= (cpu->port_latch & 0xC0); // Bits 6-7 retain latch value
            result |= (external & input_bits);

            if (cpu->sys->debug)
                printf("CPU #$%04X -> $%02X\n", addr, result);
            return result;
        }
    }
    return mem_read(&cpu->sys->mem, addr);
}

static void cpu_write(CPU *cpu, u16 addr, u8 value)
{
    // 6510 I/O port handling (only when enabled)
    if (cpu->mode == CPU_MODE_6510)
    {
        if (addr == 0x0000)
        {
            if (cpu->sys->debug)
                printf("CPU #$%04X <- $%02X\n", addr, value);
            cpu->port_dir = value;
            // Update latch: output bits take on DATA value, input bits retain latch
            cpu->port_latch = (cpu->port_data & value) | (cpu->port_latch & ~value);
            // Also write to underlying RAM (RAM exists at $0000 on C64)
            mem_write(&cpu->sys->mem, addr, value);
            return;
        }
        if (addr == 0x0001)
        {
            if (cpu->sys->debug)
                printf("CPU #$%04X <- $%02X\n", addr, value);
            cpu->port_data = value;
            // Only update latch for bits configured as outputs
            // Input bits in the latch retain their previous value
            cpu->port_latch = (value & cpu->port_dir) | (cpu->port_latch & ~cpu->port_dir);
            cpu->cpu_port_floating = value;
            // Also write to underlying RAM (RAM exists at $0001 on C64)
            mem_write(&cpu->sys->mem, addr, value);
            return;
        }
    }
    mem_write(&cpu->sys->mem, addr, value);
}

// ============================================================================
// Stack Operations
// ============================================================================

static void cpu_push(CPU *cpu, u8 value)
{
    cpu_write(cpu, 0x0100 | cpu->SP, value);
    cpu->SP--;
}

static u8 cpu_pop(CPU *cpu)
{
    cpu->SP++;
    return cpu_read(cpu, 0x0100 | cpu->SP);
}

static void cpu_push16(CPU *cpu, u16 value)
{
    cpu_push(cpu, (value >> 8) & 0xFF);
    cpu_push(cpu, value & 0xFF);
}

static u16 cpu_pop16(CPU *cpu)
{
    u16 lo = cpu_pop(cpu);
    u16 hi = cpu_pop(cpu);
    return lo | (hi << 8);
}

// ============================================================================
// Addressing Modes
// ============================================================================

static u16 addr_immediate(CPU *cpu) { return cpu->PC++; }

static u16 addr_zeropage(CPU *cpu) { return cpu_read(cpu, cpu->PC++); }

static u16 addr_zeropage_x(CPU *cpu)
{
    u8 base = cpu_read(cpu, cpu->PC++);
    cpu_read(cpu, base);
    return (base + cpu->X) & 0xFF;
}

static u16 addr_zeropage_y(CPU *cpu)
{
    u8 base = cpu_read(cpu, cpu->PC++);
    cpu_read(cpu, base);
    return (base + cpu->Y) & 0xFF;
}

static u16 addr_absolute(CPU *cpu)
{
    u16 lo = cpu_read(cpu, cpu->PC++);
    u16 hi = cpu_read(cpu, cpu->PC++);
    return lo | (hi << 8);
}

static u16 addr_absolute_x(CPU *cpu, bool check_page)
{
    u16 lo = cpu_read(cpu, cpu->PC++);
    u16 hi = cpu_read(cpu, cpu->PC++);
    u16 base = lo | (hi << 8);
    u16 result = base + cpu->X;
    if (check_page && ((base & 0xFF00) != (result & 0xFF00)))
    {
        cpu->page_crossed = true;
        // Dummy read at "wrong" address (base + X with original high byte)
        cpu_read(cpu, (base & 0xFF00) | (result & 0xFF));
    }
    return result;
}

static u16 addr_absolute_y(CPU *cpu, bool check_page)
{
    u16 lo = cpu_read(cpu, cpu->PC++);
    u16 hi = cpu_read(cpu, cpu->PC++);
    u16 base = lo | (hi << 8);
    u16 result = base + cpu->Y;
    if (check_page && ((base & 0xFF00) != (result & 0xFF00)))
    {
        cpu->page_crossed = true;
        // Dummy read at "wrong" address (base + Y with original high byte)
        cpu_read(cpu, (base & 0xFF00) | (result & 0xFF));
    }
    return result;
}

// RMW version of absolute,X - always does dummy read at possibly-wrong address
static u16 addr_absolute_x_rmw(CPU *cpu)
{
    u16 lo = cpu_read(cpu, cpu->PC++);
    u16 hi = cpu_read(cpu, cpu->PC++);
    u16 base = lo | (hi << 8);
    u16 result = base + cpu->X;
    // RMW always reads at the possibly-wrong address first
    cpu_read(cpu, (base & 0xFF00) | (result & 0xFF));
    return result;
}

// RMW version of absolute,Y - always does dummy read at possibly-wrong address
static u16 addr_absolute_y_rmw(CPU *cpu)
{
    u16 lo = cpu_read(cpu, cpu->PC++);
    u16 hi = cpu_read(cpu, cpu->PC++);
    u16 base = lo | (hi << 8);
    u16 result = base + cpu->Y;
    // RMW always reads at the possibly-wrong address first
    cpu_read(cpu, (base & 0xFF00) | (result & 0xFF));
    return result;
}

static u16 addr_indirect(CPU *cpu)
{
    u16 ptr_lo = cpu_read(cpu, cpu->PC++);
    u16 ptr_hi = cpu_read(cpu, cpu->PC++);
    u16 ptr = ptr_lo | (ptr_hi << 8);
    u16 lo = cpu_read(cpu, ptr);
    u16 hi = cpu_read(cpu, (ptr & 0xFF00) | ((ptr + 1) & 0xFF));
    return lo | (hi << 8);
}

static u16 addr_indirect_x(CPU *cpu)
{
    u8 base = cpu_read(cpu, cpu->PC++);
    cpu_read(cpu, base);
    u8 ptr = (base + cpu->X) & 0xFF;
    u16 lo = cpu_read(cpu, ptr);
    u16 hi = cpu_read(cpu, (ptr + 1) & 0xFF);
    return lo | (hi << 8);
}

static u16 addr_indirect_y(CPU *cpu, bool check_page)
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
        // Dummy read from address with wrong high byte
        cpu_read(cpu, (base & 0xFF00) | (result & 0x00FF));
    }
    return result;
}

// RMW version always does the dummy read cycle
static u16 addr_indirect_y_rmw(CPU *cpu)
{
    u8 ptr = cpu_read(cpu, cpu->PC++);
    u16 lo = cpu_read(cpu, ptr);
    u16 hi = cpu_read(cpu, (ptr + 1) & 0xFF);
    u16 base = lo | (hi << 8);
    u16 result = base + cpu->Y;
    // Always do dummy read for RMW
    cpu_read(cpu, (base & 0xFF00) | (result & 0x00FF));
    return result;
}

// ============================================================================
// ALU Operations
// ============================================================================

static void op_adc(CPU *cpu, u8 value)
{
    if (cpu->P & FLAG_D)
    {
        u16 al = (cpu->A & 0x0F) + (value & 0x0F) + (cpu->P & FLAG_C ? 1 : 0);
        if (al > 9)
            al += 6;
        u16 ah = (cpu->A >> 4) + (value >> 4) + (al > 15 ? 1 : 0);
        u16 bin_result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu_set_flag(cpu, FLAG_Z, (bin_result & 0xFF) == 0);
        cpu_set_flag(cpu, FLAG_N, ah & 0x08);
        cpu_set_flag(cpu, FLAG_V, ((cpu->A ^ bin_result) & (value ^ bin_result) & 0x80) != 0);
        if (ah > 9)
            ah += 6;
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

static void op_sbc(CPU *cpu, u8 value)
{
    if (cpu->P & FLAG_D)
    {
        u16 al = (cpu->A & 0x0F) - (value & 0x0F) - (cpu->P & FLAG_C ? 0 : 1);
        u16 ah = (cpu->A >> 4) - (value >> 4);
        if (al & 0x10)
        {
            al -= 6;
            ah--;
        }
        if (ah & 0x10)
        {
            ah -= 6;
        }
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

static void op_cmp(CPU *cpu, u8 reg, u8 value)
{
    u16 result = reg - value;
    cpu_set_flag(cpu, FLAG_C, reg >= value);
    cpu_update_nz(cpu, result & 0xFF);
}

static u8 op_asl(CPU *cpu, u8 value)
{
    cpu_set_flag(cpu, FLAG_C, value & 0x80);
    value <<= 1;
    cpu_update_nz(cpu, value);
    return value;
}

static u8 op_lsr(CPU *cpu, u8 value)
{
    cpu_set_flag(cpu, FLAG_C, value & 0x01);
    value >>= 1;
    cpu_update_nz(cpu, value);
    return value;
}

static u8 op_rol(CPU *cpu, u8 value)
{
    bool old_carry = cpu->P & FLAG_C;
    cpu_set_flag(cpu, FLAG_C, value & 0x80);
    value = (value << 1) | (old_carry ? 1 : 0);
    cpu_update_nz(cpu, value);
    return value;
}

static u8 op_ror(CPU *cpu, u8 value)
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

static void do_branch(CPU *cpu, bool condition)
{
    i8 offset = (i8)cpu_read(cpu, cpu->PC++);
    if (condition)
    {
        u16 old_pc = cpu->PC;
        cpu->PC += offset;
        cpu->extra_cycles++;
        cpu_read(cpu, old_pc); // dummy read when branch taken
        if ((old_pc & 0xFF00) != (cpu->PC & 0xFF00))
        {
            cpu->extra_cycles++;
            cpu_read(cpu, (old_pc & 0xFF00) | (cpu->PC & 0x00FF)); // dummy read on page crossing
        }
    }
}

static void do_interrupt(CPU *cpu, u16 vector, bool brk)
{
    // Cycle 2: Read operand (discarded, but we read it)
    cpu_read(cpu, cpu->PC);
    if (brk)
        cpu->PC++;

    // Cycle 3: Push PCH
    cpu_push(cpu, (cpu->PC >> 8) & 0xFF);

    // Cycle 4: Push PCL
    cpu_push(cpu, cpu->PC & 0xFF);

    // Cycle 5: Push P (with B flag set for BRK)
    cpu_push(cpu, cpu->P | FLAG_U | (brk ? FLAG_B : 0));

    // NMI hijack check: if NMI is pending at end of cycle 5, use NMI vector
    // This is the only point where hijacking is checked according to 6502 behavior
    // Require age >= 1 to ensure NMI was pending BEFORE this cycle started
    // (NMI triggered during push P has age=0 and shouldn't hijack yet)
    if (cpu->nmi_pending && cpu->nmi_pending_age >= 1 && vector != NMI)
    {
        vector = NMI;
        cpu->nmi_pending = false;
        // Note: keep nmi_edge set - it's only cleared when ICR is read
        // This prevents spurious second NMI triggers
    }

    cpu->P |= FLAG_I;

    // Cycles 6-7: Fetch vector
    {
        u16 lo = cpu_read(cpu, vector);
        u16 hi = cpu_read(cpu, vector + 1);
        cpu->PC = lo | (hi << 8);
    }
}

void cpu_trigger_nmi(CPU *cpu)
{
    if (!cpu->nmi_edge)
    {
        cpu->nmi_pending = true;
        cpu->nmi_pending_age = 0;
        cpu->nmi_edge = true;
    }
}

void cpu_trigger_irq(CPU *cpu)
{
    cpu->irq_pending = true;
}

// ============================================================================
// Opcode Handlers
// ============================================================================

// BRK
static int op_00(CPU *cpu)
{
    do_interrupt(cpu, IRQ, true);
    return 7;
}

// ORA
static int op_01(CPU *cpu)
{
    cpu->A |= cpu_read(cpu, addr_indirect_x(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 6;
}
static int op_05(CPU *cpu)
{
    cpu->A |= cpu_read(cpu, addr_zeropage(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 3;
}
static int op_09(CPU *cpu)
{
    cpu->A |= cpu_read(cpu, addr_immediate(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 2;
}
static int op_0D(CPU *cpu)
{
    cpu->A |= cpu_read(cpu, addr_absolute(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}
static int op_11(CPU *cpu)
{
    cpu->A |= cpu_read(cpu, addr_indirect_y(cpu, true));
    cpu_update_nz(cpu, cpu->A);
    return 5;
}
static int op_15(CPU *cpu)
{
    cpu->A |= cpu_read(cpu, addr_zeropage_x(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}
static int op_19(CPU *cpu)
{
    cpu->A |= cpu_read(cpu, addr_absolute_y(cpu, true));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}
static int op_1D(CPU *cpu)
{
    cpu->A |= cpu_read(cpu, addr_absolute_x(cpu, true));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}

// ASL
static int op_06(CPU *cpu)
{
    u16 a = addr_zeropage(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    cpu_write(cpu, a, op_asl(cpu, v));
    return 5;
}
static int op_0A(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->A = op_asl(cpu, cpu->A);
    return 2;
}
static int op_0E(CPU *cpu)
{
    u16 a = addr_absolute(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    cpu_write(cpu, a, op_asl(cpu, v));
    return 6;
}
static int op_16(CPU *cpu)
{
    u16 a = addr_zeropage_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    cpu_write(cpu, a, op_asl(cpu, v));
    return 6;
}
static int op_1E(CPU *cpu)
{
    u16 a = addr_absolute_x_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    cpu_write(cpu, a, op_asl(cpu, v));
    return 7;
}

// PHP, PLP, PHA, PLA
static int op_08(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu_push(cpu, cpu->P | FLAG_B | FLAG_U);
    return 3;
}
static int op_28(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu_read(cpu, 0x0100 | cpu->SP);
    cpu->P = (cpu_pop(cpu) & ~FLAG_B) | FLAG_U;
    return 4;
}
static int op_48(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu_push(cpu, cpu->A);
    return 3;
}
static int op_68(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu_read(cpu, 0x0100 | cpu->SP);
    cpu->A = cpu_pop(cpu);
    cpu_update_nz(cpu, cpu->A);
    return 4;
}

// Branches
static int op_10(CPU *cpu)
{
    do_branch(cpu, !(cpu->P & FLAG_N));
    return 2;
}
static int op_30(CPU *cpu)
{
    do_branch(cpu, cpu->P & FLAG_N);
    return 2;
}
static int op_50(CPU *cpu)
{
    do_branch(cpu, !(cpu->P & FLAG_V));
    return 2;
}
static int op_70(CPU *cpu)
{
    do_branch(cpu, cpu->P & FLAG_V);
    return 2;
}
static int op_90(CPU *cpu)
{
    do_branch(cpu, !(cpu->P & FLAG_C));
    return 2;
}
static int op_B0(CPU *cpu)
{
    do_branch(cpu, cpu->P & FLAG_C);
    return 2;
}
static int op_D0(CPU *cpu)
{
    do_branch(cpu, !(cpu->P & FLAG_Z));
    return 2;
}
static int op_F0(CPU *cpu)
{
    do_branch(cpu, cpu->P & FLAG_Z);
    return 2;
}

// Flags
static int op_18(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->P &= ~FLAG_C;
    return 2;
}
static int op_38(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->P |= FLAG_C;
    return 2;
}
static int op_58(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->P &= ~FLAG_I;
    return 2;
}
static int op_78(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->P |= FLAG_I;
    return 2;
}
static int op_B8(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->P &= ~FLAG_V;
    return 2;
}
static int op_D8(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->P &= ~FLAG_D;
    return 2;
}
static int op_F8(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->P |= FLAG_D;
    return 2;
}

// AND
static int op_21(CPU *cpu)
{
    cpu->A &= cpu_read(cpu, addr_indirect_x(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 6;
}
static int op_25(CPU *cpu)
{
    cpu->A &= cpu_read(cpu, addr_zeropage(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 3;
}
static int op_29(CPU *cpu)
{
    cpu->A &= cpu_read(cpu, addr_immediate(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 2;
}
static int op_2D(CPU *cpu)
{
    cpu->A &= cpu_read(cpu, addr_absolute(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}
static int op_31(CPU *cpu)
{
    cpu->A &= cpu_read(cpu, addr_indirect_y(cpu, true));
    cpu_update_nz(cpu, cpu->A);
    return 5;
}
static int op_35(CPU *cpu)
{
    cpu->A &= cpu_read(cpu, addr_zeropage_x(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}
static int op_39(CPU *cpu)
{
    cpu->A &= cpu_read(cpu, addr_absolute_y(cpu, true));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}
static int op_3D(CPU *cpu)
{
    cpu->A &= cpu_read(cpu, addr_absolute_x(cpu, true));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}

// BIT
static int op_24(CPU *cpu)
{
    u8 v = cpu_read(cpu, addr_zeropage(cpu));
    cpu_set_flag(cpu, FLAG_Z, (cpu->A & v) == 0);
    cpu_set_flag(cpu, FLAG_N, v & 0x80);
    cpu_set_flag(cpu, FLAG_V, v & 0x40);
    return 3;
}
static int op_2C(CPU *cpu)
{
    u8 v = cpu_read(cpu, addr_absolute(cpu));
    cpu_set_flag(cpu, FLAG_Z, (cpu->A & v) == 0);
    cpu_set_flag(cpu, FLAG_N, v & 0x80);
    cpu_set_flag(cpu, FLAG_V, v & 0x40);
    return 4;
}

// ROL
static int op_26(CPU *cpu)
{
    u16 a = addr_zeropage(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    cpu_write(cpu, a, op_rol(cpu, v));
    return 5;
}
static int op_2A(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->A = op_rol(cpu, cpu->A);
    return 2;
}
static int op_2E(CPU *cpu)
{
    u16 a = addr_absolute(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    cpu_write(cpu, a, op_rol(cpu, v));
    return 6;
}
static int op_36(CPU *cpu)
{
    u16 a = addr_zeropage_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    cpu_write(cpu, a, op_rol(cpu, v));
    return 6;
}
static int op_3E(CPU *cpu)
{
    u16 a = addr_absolute_x_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    cpu_write(cpu, a, op_rol(cpu, v));
    return 7;
}

// RTI
static int op_40(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu_read(cpu, 0x0100 | cpu->SP);
    cpu->P = (cpu_pop(cpu) & ~FLAG_B) | FLAG_U;
    cpu->PC = cpu_pop16(cpu);
    return 6;
}

// EOR
static int op_41(CPU *cpu)
{
    cpu->A ^= cpu_read(cpu, addr_indirect_x(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 6;
}
static int op_45(CPU *cpu)
{
    cpu->A ^= cpu_read(cpu, addr_zeropage(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 3;
}
static int op_49(CPU *cpu)
{
    cpu->A ^= cpu_read(cpu, addr_immediate(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 2;
}
static int op_4D(CPU *cpu)
{
    cpu->A ^= cpu_read(cpu, addr_absolute(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}
static int op_51(CPU *cpu)
{
    cpu->A ^= cpu_read(cpu, addr_indirect_y(cpu, true));
    cpu_update_nz(cpu, cpu->A);
    return 5;
}
static int op_55(CPU *cpu)
{
    cpu->A ^= cpu_read(cpu, addr_zeropage_x(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}
static int op_59(CPU *cpu)
{
    cpu->A ^= cpu_read(cpu, addr_absolute_y(cpu, true));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}
static int op_5D(CPU *cpu)
{
    cpu->A ^= cpu_read(cpu, addr_absolute_x(cpu, true));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}

// LSR
static int op_46(CPU *cpu)
{
    u16 a = addr_zeropage(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    cpu_write(cpu, a, op_lsr(cpu, v));
    return 5;
}
static int op_4A(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->A = op_lsr(cpu, cpu->A);
    return 2;
}
static int op_4E(CPU *cpu)
{
    u16 a = addr_absolute(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    cpu_write(cpu, a, op_lsr(cpu, v));
    return 6;
}
static int op_56(CPU *cpu)
{
    u16 a = addr_zeropage_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    cpu_write(cpu, a, op_lsr(cpu, v));
    return 6;
}
static int op_5E(CPU *cpu)
{
    u16 a = addr_absolute_x_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    cpu_write(cpu, a, op_lsr(cpu, v));
    return 7;
}

// JMP
static int op_4C(CPU *cpu)
{
    cpu->PC = addr_absolute(cpu);
    return 3;
}
static int op_6C(CPU *cpu)
{
    cpu->PC = addr_indirect(cpu);
    return 5;
}

// RTS
static int op_60(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu_read(cpu, 0x0100 | cpu->SP);
    cpu->PC = cpu_pop16(cpu) + 1;
    cpu_read(cpu, cpu->PC);
    return 6;
}

// ADC
static int op_61(CPU *cpu)
{
    op_adc(cpu, cpu_read(cpu, addr_indirect_x(cpu)));
    return 6;
}
static int op_65(CPU *cpu)
{
    op_adc(cpu, cpu_read(cpu, addr_zeropage(cpu)));
    return 3;
}
static int op_69(CPU *cpu)
{
    op_adc(cpu, cpu_read(cpu, addr_immediate(cpu)));
    return 2;
}
static int op_6D(CPU *cpu)
{
    op_adc(cpu, cpu_read(cpu, addr_absolute(cpu)));
    return 4;
}
static int op_71(CPU *cpu)
{
    op_adc(cpu, cpu_read(cpu, addr_indirect_y(cpu, true)));
    return 5;
}
static int op_75(CPU *cpu)
{
    op_adc(cpu, cpu_read(cpu, addr_zeropage_x(cpu)));
    return 4;
}
static int op_79(CPU *cpu)
{
    op_adc(cpu, cpu_read(cpu, addr_absolute_y(cpu, true)));
    return 4;
}
static int op_7D(CPU *cpu)
{
    op_adc(cpu, cpu_read(cpu, addr_absolute_x(cpu, true)));
    return 4;
}

// ROR
static int op_66(CPU *cpu)
{
    u16 a = addr_zeropage(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    cpu_write(cpu, a, op_ror(cpu, v));
    return 5;
}
static int op_6A(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->A = op_ror(cpu, cpu->A);
    return 2;
}
static int op_6E(CPU *cpu)
{
    u16 a = addr_absolute(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    cpu_write(cpu, a, op_ror(cpu, v));
    return 6;
}
static int op_76(CPU *cpu)
{
    u16 a = addr_zeropage_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    cpu_write(cpu, a, op_ror(cpu, v));
    return 6;
}
static int op_7E(CPU *cpu)
{
    u16 a = addr_absolute_x_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    cpu_write(cpu, a, op_ror(cpu, v));
    return 7;
}

// STA
static int op_81(CPU *cpu)
{
    cpu_write(cpu, addr_indirect_x(cpu), cpu->A);
    return 6;
}
static int op_85(CPU *cpu)
{
    cpu_write(cpu, addr_zeropage(cpu), cpu->A);
    return 3;
}
static int op_8D(CPU *cpu)
{
    cpu_write(cpu, addr_absolute(cpu), cpu->A);
    return 4;
}
static int op_91(CPU *cpu)
{
    // STA (ind),Y always takes 6 cycles with dummy read
    cpu_write(cpu, addr_indirect_y_rmw(cpu), cpu->A);
    return 6;
}
static int op_95(CPU *cpu)
{
    cpu_write(cpu, addr_zeropage_x(cpu), cpu->A);
    return 4;
}
static int op_99(CPU *cpu)
{
    // STA abs,Y always takes 5 cycles, needs dummy read at possibly-wrong address
    cpu_write(cpu, addr_absolute_y_rmw(cpu), cpu->A);
    return 5;
}
static int op_9D(CPU *cpu)
{
    // STA abs,X always takes 5 cycles, needs dummy read at possibly-wrong address
    cpu_write(cpu, addr_absolute_x_rmw(cpu), cpu->A);
    return 5;
}

// STX
static int op_86(CPU *cpu)
{
    cpu_write(cpu, addr_zeropage(cpu), cpu->X);
    return 3;
}
static int op_8E(CPU *cpu)
{
    cpu_write(cpu, addr_absolute(cpu), cpu->X);
    return 4;
}
static int op_96(CPU *cpu)
{
    cpu_write(cpu, addr_zeropage_y(cpu), cpu->X);
    return 4;
}

// STY
static int op_84(CPU *cpu)
{
    cpu_write(cpu, addr_zeropage(cpu), cpu->Y);
    return 3;
}
static int op_8C(CPU *cpu)
{
    cpu_write(cpu, addr_absolute(cpu), cpu->Y);
    return 4;
}
static int op_94(CPU *cpu)
{
    cpu_write(cpu, addr_zeropage_x(cpu), cpu->Y);
    return 4;
}

// Transfers
static int op_8A(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->A = cpu->X;
    cpu_update_nz(cpu, cpu->A);
    return 2;
}
static int op_98(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->A = cpu->Y;
    cpu_update_nz(cpu, cpu->A);
    return 2;
}
static int op_9A(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->SP = cpu->X;
    return 2;
}
static int op_AA(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->X = cpu->A;
    cpu_update_nz(cpu, cpu->X);
    return 2;
}
static int op_BA(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->X = cpu->SP;
    cpu_update_nz(cpu, cpu->X);
    return 2;
}
static int op_A8(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->Y = cpu->A;
    cpu_update_nz(cpu, cpu->Y);
    return 2;
}

// DEY, DEX, INY, INX
static int op_88(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->Y--;
    cpu_update_nz(cpu, cpu->Y);
    return 2;
}
static int op_CA(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->X--;
    cpu_update_nz(cpu, cpu->X);
    return 2;
}
static int op_C8(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->Y++;
    cpu_update_nz(cpu, cpu->Y);
    return 2;
}
static int op_E8(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    cpu->X++;
    cpu_update_nz(cpu, cpu->X);
    return 2;
}

// LDA
static int op_A1(CPU *cpu)
{
    cpu->A = cpu_read(cpu, addr_indirect_x(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 6;
}
static int op_A5(CPU *cpu)
{
    cpu->A = cpu_read(cpu, addr_zeropage(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 3;
}
static int op_A9(CPU *cpu)
{
    cpu->A = cpu_read(cpu, addr_immediate(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 2;
}
static int op_AD(CPU *cpu)
{
    cpu->A = cpu_read(cpu, addr_absolute(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}
static int op_B1(CPU *cpu)
{
    cpu->A = cpu_read(cpu, addr_indirect_y(cpu, true));
    cpu_update_nz(cpu, cpu->A);
    return 5;
}
static int op_B5(CPU *cpu)
{
    cpu->A = cpu_read(cpu, addr_zeropage_x(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}
static int op_B9(CPU *cpu)
{
    cpu->A = cpu_read(cpu, addr_absolute_y(cpu, true));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}
static int op_BD(CPU *cpu)
{
    cpu->A = cpu_read(cpu, addr_absolute_x(cpu, true));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}

// LDX
static int op_A2(CPU *cpu)
{
    cpu->X = cpu_read(cpu, addr_immediate(cpu));
    cpu_update_nz(cpu, cpu->X);
    return 2;
}
static int op_A6(CPU *cpu)
{
    cpu->X = cpu_read(cpu, addr_zeropage(cpu));
    cpu_update_nz(cpu, cpu->X);
    return 3;
}
static int op_AE(CPU *cpu)
{
    cpu->X = cpu_read(cpu, addr_absolute(cpu));
    cpu_update_nz(cpu, cpu->X);
    return 4;
}
static int op_B6(CPU *cpu)
{
    cpu->X = cpu_read(cpu, addr_zeropage_y(cpu));
    cpu_update_nz(cpu, cpu->X);
    return 4;
}
static int op_BE(CPU *cpu)
{
    cpu->X = cpu_read(cpu, addr_absolute_y(cpu, true));
    cpu_update_nz(cpu, cpu->X);
    return 4;
}

// LDY
static int op_A0(CPU *cpu)
{
    cpu->Y = cpu_read(cpu, addr_immediate(cpu));
    cpu_update_nz(cpu, cpu->Y);
    return 2;
}
static int op_A4(CPU *cpu)
{
    cpu->Y = cpu_read(cpu, addr_zeropage(cpu));
    cpu_update_nz(cpu, cpu->Y);
    return 3;
}
static int op_AC(CPU *cpu)
{
    cpu->Y = cpu_read(cpu, addr_absolute(cpu));
    cpu_update_nz(cpu, cpu->Y);
    return 4;
}
static int op_B4(CPU *cpu)
{
    cpu->Y = cpu_read(cpu, addr_zeropage_x(cpu));
    cpu_update_nz(cpu, cpu->Y);
    return 4;
}
static int op_BC(CPU *cpu)
{
    cpu->Y = cpu_read(cpu, addr_absolute_x(cpu, true));
    cpu_update_nz(cpu, cpu->Y);
    return 4;
}

// CMP
static int op_C1(CPU *cpu)
{
    op_cmp(cpu, cpu->A, cpu_read(cpu, addr_indirect_x(cpu)));
    return 6;
}
static int op_C5(CPU *cpu)
{
    op_cmp(cpu, cpu->A, cpu_read(cpu, addr_zeropage(cpu)));
    return 3;
}
static int op_C9(CPU *cpu)
{
    op_cmp(cpu, cpu->A, cpu_read(cpu, addr_immediate(cpu)));
    return 2;
}
static int op_CD(CPU *cpu)
{
    op_cmp(cpu, cpu->A, cpu_read(cpu, addr_absolute(cpu)));
    return 4;
}
static int op_D1(CPU *cpu)
{
    op_cmp(cpu, cpu->A, cpu_read(cpu, addr_indirect_y(cpu, true)));
    return 5;
}
static int op_D5(CPU *cpu)
{
    op_cmp(cpu, cpu->A, cpu_read(cpu, addr_zeropage_x(cpu)));
    return 4;
}
static int op_D9(CPU *cpu)
{
    op_cmp(cpu, cpu->A, cpu_read(cpu, addr_absolute_y(cpu, true)));
    return 4;
}
static int op_DD(CPU *cpu)
{
    op_cmp(cpu, cpu->A, cpu_read(cpu, addr_absolute_x(cpu, true)));
    return 4;
}

// CPX
static int op_E0(CPU *cpu)
{
    op_cmp(cpu, cpu->X, cpu_read(cpu, addr_immediate(cpu)));
    return 2;
}
static int op_E4(CPU *cpu)
{
    op_cmp(cpu, cpu->X, cpu_read(cpu, addr_zeropage(cpu)));
    return 3;
}
static int op_EC(CPU *cpu)
{
    op_cmp(cpu, cpu->X, cpu_read(cpu, addr_absolute(cpu)));
    return 4;
}

// CPY
static int op_C0(CPU *cpu)
{
    op_cmp(cpu, cpu->Y, cpu_read(cpu, addr_immediate(cpu)));
    return 2;
}
static int op_C4(CPU *cpu)
{
    op_cmp(cpu, cpu->Y, cpu_read(cpu, addr_zeropage(cpu)));
    return 3;
}
static int op_CC(CPU *cpu)
{
    op_cmp(cpu, cpu->Y, cpu_read(cpu, addr_absolute(cpu)));
    return 4;
}

// DEC
static int op_C6(CPU *cpu)
{
    u16 a = addr_zeropage(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v--;
    cpu_write(cpu, a, v);
    cpu_update_nz(cpu, v);
    return 5;
}
static int op_CE(CPU *cpu)
{
    u16 a = addr_absolute(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v--;
    cpu_write(cpu, a, v);
    cpu_update_nz(cpu, v);
    return 6;
}
static int op_D6(CPU *cpu)
{
    u16 a = addr_zeropage_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v--;
    cpu_write(cpu, a, v);
    cpu_update_nz(cpu, v);
    return 6;
}
static int op_DE(CPU *cpu)
{
    u16 a = addr_absolute_x_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v--;
    cpu_write(cpu, a, v);
    cpu_update_nz(cpu, v);
    return 7;
}

// INC
static int op_E6(CPU *cpu)
{
    u16 a = addr_zeropage(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v++;
    cpu_write(cpu, a, v);
    cpu_update_nz(cpu, v);
    return 5;
}
static int op_EE(CPU *cpu)
{
    u16 a = addr_absolute(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v++;
    cpu_write(cpu, a, v);
    cpu_update_nz(cpu, v);
    return 6;
}
static int op_F6(CPU *cpu)
{
    u16 a = addr_zeropage_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v++;
    cpu_write(cpu, a, v);
    cpu_update_nz(cpu, v);
    return 6;
}
static int op_FE(CPU *cpu)
{
    u16 a = addr_absolute_x_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v++;
    cpu_write(cpu, a, v);
    cpu_update_nz(cpu, v);
    return 7;
}

// SBC
static int op_E1(CPU *cpu)
{
    op_sbc(cpu, cpu_read(cpu, addr_indirect_x(cpu)));
    return 6;
}
static int op_E5(CPU *cpu)
{
    op_sbc(cpu, cpu_read(cpu, addr_zeropage(cpu)));
    return 3;
}
static int op_E9(CPU *cpu)
{
    op_sbc(cpu, cpu_read(cpu, addr_immediate(cpu)));
    return 2;
}
static int op_ED(CPU *cpu)
{
    op_sbc(cpu, cpu_read(cpu, addr_absolute(cpu)));
    return 4;
}
static int op_F1(CPU *cpu)
{
    op_sbc(cpu, cpu_read(cpu, addr_indirect_y(cpu, true)));
    return 5;
}
static int op_F5(CPU *cpu)
{
    op_sbc(cpu, cpu_read(cpu, addr_zeropage_x(cpu)));
    return 4;
}
static int op_F9(CPU *cpu)
{
    op_sbc(cpu, cpu_read(cpu, addr_absolute_y(cpu, true)));
    return 4;
}
static int op_FD(CPU *cpu)
{
    op_sbc(cpu, cpu_read(cpu, addr_absolute_x(cpu, true)));
    return 4;
}

// JSR
static int op_20(CPU *cpu)
{
    u16 target = cpu_read(cpu, cpu->PC++);
    cpu_read(cpu, 0x0100 | cpu->SP);
    cpu_push16(cpu, cpu->PC);
    target |= cpu_read(cpu, cpu->PC) << 8;
    cpu->PC = target;
    return 6;
}

// NOP
static int op_EA(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    return 2;
}

// ============================================================================
// Illegal/Undocumented Opcodes
// ============================================================================

// JAM/HLT - Halt the CPU (requires reset to recover)
static int op_JAM(CPU *cpu)
{
    cpu->PC--; // Stay at this instruction forever
    return 2;
}

// NOP variants - implied (1 byte, 2 cycles)
static int op_NOP_impl(CPU *cpu)
{
    cpu_read(cpu, cpu->PC);
    return 2;
}

// NOP variants - immediate (2 bytes, 2 cycles) - also known as SKB/DOP
static int op_NOP_imm(CPU *cpu)
{
    cpu_read(cpu, addr_immediate(cpu)); // read and discard operand
    return 2;
}

// NOP variants - zeropage (2 bytes, 3 cycles)
static int op_NOP_zp(CPU *cpu)
{
    u16 a = addr_zeropage(cpu);
    cpu_read(cpu, a); // dummy read
    return 3;
}

// NOP variants - zeropage,x (2 bytes, 4 cycles)
static int op_NOP_zpx(CPU *cpu)
{
    u16 a = addr_zeropage_x(cpu);
    cpu_read(cpu, a); // dummy read
    return 4;
}

// NOP variants - absolute (3 bytes, 4 cycles) - also known as SKW/TOP
static int op_NOP_abs(CPU *cpu)
{
    u16 a = addr_absolute(cpu);
    cpu_read(cpu, a); // dummy read
    return 4;
}

// NOP variants - absolute,x (3 bytes, 4+ cycles)
static int op_NOP_abx(CPU *cpu)
{
    u16 a = addr_absolute_x(cpu, true);
    cpu_read(cpu, a); // dummy read
    return 4;
}

// SLO (ASO) - ASL memory then ORA with A
static int op_03(CPU *cpu)
{
    u16 a = addr_indirect_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_asl(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A |= v;
    cpu_update_nz(cpu, cpu->A);
    return 8;
}
static int op_07(CPU *cpu)
{
    u16 a = addr_zeropage(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_asl(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A |= v;
    cpu_update_nz(cpu, cpu->A);
    return 5;
}
static int op_0F(CPU *cpu)
{
    u16 a = addr_absolute(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_asl(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A |= v;
    cpu_update_nz(cpu, cpu->A);
    return 6;
}
static int op_13(CPU *cpu)
{
    u16 a = addr_indirect_y_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_asl(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A |= v;
    cpu_update_nz(cpu, cpu->A);
    return 8;
}
static int op_17(CPU *cpu)
{
    u16 a = addr_zeropage_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_asl(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A |= v;
    cpu_update_nz(cpu, cpu->A);
    return 6;
}
static int op_1B(CPU *cpu)
{
    u16 a = addr_absolute_y_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_asl(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A |= v;
    cpu_update_nz(cpu, cpu->A);
    return 7;
}
static int op_1F(CPU *cpu)
{
    u16 a = addr_absolute_x_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_asl(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A |= v;
    cpu_update_nz(cpu, cpu->A);
    return 7;
}

// RLA - ROL memory then AND with A
static int op_23(CPU *cpu)
{
    u16 a = addr_indirect_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_rol(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A &= v;
    cpu_update_nz(cpu, cpu->A);
    return 8;
}
static int op_27(CPU *cpu)
{
    u16 a = addr_zeropage(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_rol(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A &= v;
    cpu_update_nz(cpu, cpu->A);
    return 5;
}
static int op_2F(CPU *cpu)
{
    u16 a = addr_absolute(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_rol(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A &= v;
    cpu_update_nz(cpu, cpu->A);
    return 6;
}
static int op_33(CPU *cpu)
{
    u16 a = addr_indirect_y_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_rol(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A &= v;
    cpu_update_nz(cpu, cpu->A);
    return 8;
}
static int op_37(CPU *cpu)
{
    u16 a = addr_zeropage_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_rol(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A &= v;
    cpu_update_nz(cpu, cpu->A);
    return 6;
}
static int op_3B(CPU *cpu)
{
    u16 a = addr_absolute_y_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_rol(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A &= v;
    cpu_update_nz(cpu, cpu->A);
    return 7;
}
static int op_3F(CPU *cpu)
{
    u16 a = addr_absolute_x_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_rol(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A &= v;
    cpu_update_nz(cpu, cpu->A);
    return 7;
}

// SRE (LSE) - LSR memory then EOR with A
static int op_43(CPU *cpu)
{
    u16 a = addr_indirect_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_lsr(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A ^= v;
    cpu_update_nz(cpu, cpu->A);
    return 8;
}
static int op_47(CPU *cpu)
{
    u16 a = addr_zeropage(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_lsr(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A ^= v;
    cpu_update_nz(cpu, cpu->A);
    return 5;
}
static int op_4F(CPU *cpu)
{
    u16 a = addr_absolute(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_lsr(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A ^= v;
    cpu_update_nz(cpu, cpu->A);
    return 6;
}
static int op_53(CPU *cpu)
{
    u16 a = addr_indirect_y_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_lsr(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A ^= v;
    cpu_update_nz(cpu, cpu->A);
    return 8;
}
static int op_57(CPU *cpu)
{
    u16 a = addr_zeropage_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_lsr(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A ^= v;
    cpu_update_nz(cpu, cpu->A);
    return 6;
}
static int op_5B(CPU *cpu)
{
    u16 a = addr_absolute_y_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_lsr(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A ^= v;
    cpu_update_nz(cpu, cpu->A);
    return 7;
}
static int op_5F(CPU *cpu)
{
    u16 a = addr_absolute_x_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_lsr(cpu, v);
    cpu_write(cpu, a, v);
    cpu->A ^= v;
    cpu_update_nz(cpu, cpu->A);
    return 7;
}

// RRA - ROR memory then ADC with A
static int op_63(CPU *cpu)
{
    u16 a = addr_indirect_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_ror(cpu, v);
    cpu_write(cpu, a, v);
    op_adc(cpu, v);
    return 8;
}
static int op_67(CPU *cpu)
{
    u16 a = addr_zeropage(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_ror(cpu, v);
    cpu_write(cpu, a, v);
    op_adc(cpu, v);
    return 5;
}
static int op_6F(CPU *cpu)
{
    u16 a = addr_absolute(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_ror(cpu, v);
    cpu_write(cpu, a, v);
    op_adc(cpu, v);
    return 6;
}
static int op_73(CPU *cpu)
{
    u16 a = addr_indirect_y_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_ror(cpu, v);
    cpu_write(cpu, a, v);
    op_adc(cpu, v);
    return 8;
}
static int op_77(CPU *cpu)
{
    u16 a = addr_zeropage_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_ror(cpu, v);
    cpu_write(cpu, a, v);
    op_adc(cpu, v);
    return 6;
}
static int op_7B(CPU *cpu)
{
    u16 a = addr_absolute_y_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_ror(cpu, v);
    cpu_write(cpu, a, v);
    op_adc(cpu, v);
    return 7;
}
static int op_7F(CPU *cpu)
{
    u16 a = addr_absolute_x_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v = op_ror(cpu, v);
    cpu_write(cpu, a, v);
    op_adc(cpu, v);
    return 7;
}

// SAX (AXS) - Store A & X to memory (no flags affected)
static int op_83(CPU *cpu)
{
    cpu_write(cpu, addr_indirect_x(cpu), cpu->A & cpu->X);
    return 6;
}
static int op_87(CPU *cpu)
{
    cpu_write(cpu, addr_zeropage(cpu), cpu->A & cpu->X);
    return 3;
}
static int op_8F(CPU *cpu)
{
    cpu_write(cpu, addr_absolute(cpu), cpu->A & cpu->X);
    return 4;
}
static int op_97(CPU *cpu)
{
    cpu_write(cpu, addr_zeropage_y(cpu), cpu->A & cpu->X);
    return 4;
}

// LAX - Load A and X with memory
static int op_A3(CPU *cpu)
{
    cpu->A = cpu->X = cpu_read(cpu, addr_indirect_x(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 6;
}
static int op_A7(CPU *cpu)
{
    cpu->A = cpu->X = cpu_read(cpu, addr_zeropage(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 3;
}
static int op_AF(CPU *cpu)
{
    cpu->A = cpu->X = cpu_read(cpu, addr_absolute(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}
static int op_B3(CPU *cpu)
{
    cpu->A = cpu->X = cpu_read(cpu, addr_indirect_y(cpu, true));
    cpu_update_nz(cpu, cpu->A);
    return 5;
}
static int op_B7(CPU *cpu)
{
    cpu->A = cpu->X = cpu_read(cpu, addr_zeropage_y(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}
static int op_BF(CPU *cpu)
{
    cpu->A = cpu->X = cpu_read(cpu, addr_absolute_y(cpu, true));
    cpu_update_nz(cpu, cpu->A);
    return 4;
}

// DCP (DCM) - DEC memory then CMP with A
static int op_C3(CPU *cpu)
{
    u16 a = addr_indirect_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v--;
    cpu_write(cpu, a, v);
    op_cmp(cpu, cpu->A, v);
    return 8;
}
static int op_C7(CPU *cpu)
{
    u16 a = addr_zeropage(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v--;
    cpu_write(cpu, a, v);
    op_cmp(cpu, cpu->A, v);
    return 5;
}
static int op_CF(CPU *cpu)
{
    u16 a = addr_absolute(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v--;
    cpu_write(cpu, a, v);
    op_cmp(cpu, cpu->A, v);
    return 6;
}
static int op_D3(CPU *cpu)
{
    u16 a = addr_indirect_y_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v--;
    cpu_write(cpu, a, v);
    op_cmp(cpu, cpu->A, v);
    return 8;
}
static int op_D7(CPU *cpu)
{
    u16 a = addr_zeropage_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v--;
    cpu_write(cpu, a, v);
    op_cmp(cpu, cpu->A, v);
    return 6;
}
static int op_DB(CPU *cpu)
{
    u16 a = addr_absolute_y_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v--;
    cpu_write(cpu, a, v);
    op_cmp(cpu, cpu->A, v);
    return 7;
}
static int op_DF(CPU *cpu)
{
    u16 a = addr_absolute_x_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v--;
    cpu_write(cpu, a, v);
    op_cmp(cpu, cpu->A, v);
    return 7;
}

// ISC (ISB/INS) - INC memory then SBC with A
static int op_E3(CPU *cpu)
{
    u16 a = addr_indirect_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v++;
    cpu_write(cpu, a, v);
    op_sbc(cpu, v);
    return 8;
}
static int op_E7(CPU *cpu)
{
    u16 a = addr_zeropage(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v++;
    cpu_write(cpu, a, v);
    op_sbc(cpu, v);
    return 5;
}
static int op_EF(CPU *cpu)
{
    u16 a = addr_absolute(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v++;
    cpu_write(cpu, a, v);
    op_sbc(cpu, v);
    return 6;
}
static int op_F3(CPU *cpu)
{
    u16 a = addr_indirect_y_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v++;
    cpu_write(cpu, a, v);
    op_sbc(cpu, v);
    return 8;
}
static int op_F7(CPU *cpu)
{
    u16 a = addr_zeropage_x(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v++;
    cpu_write(cpu, a, v);
    op_sbc(cpu, v);
    return 6;
}
static int op_FB(CPU *cpu)
{
    u16 a = addr_absolute_y_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v++;
    cpu_write(cpu, a, v);
    op_sbc(cpu, v);
    return 7;
}
static int op_FF(CPU *cpu)
{
    u16 a = addr_absolute_x_rmw(cpu);
    u8 v = cpu_read(cpu, a);
    cpu_write(cpu, a, v); // dummy write (RMW)
    v++;
    cpu_write(cpu, a, v);
    op_sbc(cpu, v);
    return 7;
}

// ANC (AAC) - AND immediate, then copy N to C
static int op_0B(CPU *cpu)
{
    cpu->A &= cpu_read(cpu, addr_immediate(cpu));
    cpu_update_nz(cpu, cpu->A);
    cpu_set_flag(cpu, FLAG_C, cpu->P & FLAG_N);
    return 2;
}
static int op_2B(CPU *cpu)
{
    cpu->A &= cpu_read(cpu, addr_immediate(cpu));
    cpu_update_nz(cpu, cpu->A);
    cpu_set_flag(cpu, FLAG_C, cpu->P & FLAG_N);
    return 2;
}

// ALR (ASR) - AND immediate then LSR A
static int op_4B(CPU *cpu)
{
    cpu->A &= cpu_read(cpu, addr_immediate(cpu));
    cpu->A = op_lsr(cpu, cpu->A);
    return 2;
}

// ARR - AND immediate then ROR A with special flag handling
static int op_6B(CPU *cpu)
{
    u8 imm = cpu_read(cpu, addr_immediate(cpu));
    cpu->A &= imm;

    if (cpu->P & FLAG_D)
    {
        // Decimal mode - complex behavior
        u8 t = cpu->A;
        u8 ah = t >> 4;
        u8 al = t & 0x0F;

        // Do ROR
        bool old_carry = cpu->P & FLAG_C;
        cpu->A = (cpu->A >> 1) | (old_carry ? 0x80 : 0);

        // Set N and Z based on result
        cpu_set_flag(cpu, FLAG_N, old_carry);
        cpu_set_flag(cpu, FLAG_Z, cpu->A == 0);
        cpu_set_flag(cpu, FLAG_V, (t ^ cpu->A) & 0x40);

        // BCD fixup for low nibble
        if ((al + (al & 1)) > 5)
        {
            cpu->A = (cpu->A & 0xF0) | ((cpu->A + 6) & 0x0F);
        }

        // BCD fixup for high nibble and set carry
        if ((ah + (ah & 1)) > 5)
        {
            cpu_set_flag(cpu, FLAG_C, true);
            cpu->A = (cpu->A + 0x60) & 0xFF;
        }
        else
        {
            cpu_set_flag(cpu, FLAG_C, false);
        }
    }
    else
    {
        // Binary mode
        bool old_carry = cpu->P & FLAG_C;
        cpu->A = (cpu->A >> 1) | (old_carry ? 0x80 : 0);
        cpu_update_nz(cpu, cpu->A);
        // C flag is bit 6 of result, V flag is bit 6 XOR bit 5
        cpu_set_flag(cpu, FLAG_C, cpu->A & 0x40);
        cpu_set_flag(cpu, FLAG_V, ((cpu->A >> 6) ^ (cpu->A >> 5)) & 1);
    }
    return 2;
}

// XAA (ANE) - TXA then AND immediate (unstable - uses magic constant)
static int op_8B(CPU *cpu)
{
    // Most common behavior: A = (A | $EE) & X & imm
    // We use $EE as the magic constant for compatibility
    cpu->A = (cpu->A | 0xEE) & cpu->X & cpu_read(cpu, addr_immediate(cpu));
    cpu_update_nz(cpu, cpu->A);
    return 2;
}

// LAX immediate (LXA/OAL/ATX) - unstable
static int op_AB(CPU *cpu)
{
    // Most common: A = X = (A | $EE) & imm
    cpu->A = (cpu->A | 0xEE) & cpu_read(cpu, addr_immediate(cpu));
    cpu->X = cpu->A;
    cpu_update_nz(cpu, cpu->A);
    return 2;
}

// SBX (AXS/SAX) - (A & X) - immediate -> X, sets flags like CMP
static int op_CB(CPU *cpu)
{
    u8 imm = cpu_read(cpu, addr_immediate(cpu));
    u8 ax = cpu->A & cpu->X;
    u16 result = ax - imm;
    cpu->X = result & 0xFF;
    cpu_set_flag(cpu, FLAG_C, ax >= imm);
    cpu_update_nz(cpu, cpu->X);
    return 2;
}

// SBC immediate (USBC) - identical to regular SBC #imm
static int op_EB(CPU *cpu)
{
    op_sbc(cpu, cpu_read(cpu, addr_immediate(cpu)));
    return 2;
}

// SHA (AXA/AHX) - Store A & X & (addr_hi + 1)
static int op_93(CPU *cpu)
{
    u8 ptr = cpu_read(cpu, cpu->PC++);
    u16 lo = cpu_read(cpu, ptr);
    u16 hi = cpu_read(cpu, (ptr + 1) & 0xFF);
    u16 base = lo | (hi << 8);
    u16 addr = base + cpu->Y;
    // Dummy read (always happens for stores)
    cpu_read(cpu, (base & 0xFF00) | (addr & 0x00FF));
    u8 val = cpu->A & cpu->X & ((hi + 1) & 0xFF);
    // On page crossing, high byte of address gets corrupted
    if ((addr & 0xFF00) != (hi << 8))
    {
        addr = (addr & 0x00FF) | (val << 8);
    }
    cpu_write(cpu, addr, val);
    return 6;
}

static int op_9F(CPU *cpu)
{
    u16 lo = cpu_read(cpu, cpu->PC++);
    u16 hi = cpu_read(cpu, cpu->PC++);
    u16 base = lo | (hi << 8);
    u16 addr = base + cpu->Y;
    // Dummy read (always happens for stores)
    cpu_read(cpu, (base & 0xFF00) | (addr & 0x00FF));
    u8 val = cpu->A & cpu->X & ((hi + 1) & 0xFF);
    if ((addr & 0xFF00) != (hi << 8))
    {
        addr = (addr & 0x00FF) | (val << 8);
    }
    cpu_write(cpu, addr, val);
    return 5;
}

// TAS (SHS/XAS) - S = A & X, then store S & (addr_hi + 1)
static int op_9B(CPU *cpu)
{
    u16 lo = cpu_read(cpu, cpu->PC++);
    u16 hi = cpu_read(cpu, cpu->PC++);
    u16 base = lo | (hi << 8);
    u16 addr = base + cpu->Y;
    // Dummy read (always happens for stores)
    cpu_read(cpu, (base & 0xFF00) | (addr & 0x00FF));
    cpu->SP = cpu->A & cpu->X;
    u8 val = cpu->SP & ((hi + 1) & 0xFF);
    if ((addr & 0xFF00) != (hi << 8))
    {
        addr = (addr & 0x00FF) | (val << 8);
    }
    cpu_write(cpu, addr, val);
    return 5;
}

// SHY (SAY/SYA) - Store Y & (addr_hi + 1)
static int op_9C(CPU *cpu)
{
    u16 lo = cpu_read(cpu, cpu->PC++);
    u16 hi = cpu_read(cpu, cpu->PC++);
    u16 base = lo | (hi << 8);
    u16 addr = base + cpu->X;
    // Dummy read (always happens for stores)
    cpu_read(cpu, (base & 0xFF00) | (addr & 0x00FF));
    u8 val = cpu->Y & ((hi + 1) & 0xFF);
    if ((addr & 0xFF00) != (hi << 8))
    {
        addr = (addr & 0x00FF) | (val << 8);
    }
    cpu_write(cpu, addr, val);
    return 5;
}

// SHX (SXA/XAS) - Store X & (addr_hi + 1)
static int op_9E(CPU *cpu)
{
    u16 lo = cpu_read(cpu, cpu->PC++);
    u16 hi = cpu_read(cpu, cpu->PC++);
    u16 base = lo | (hi << 8);
    u16 addr = base + cpu->Y;
    // Dummy read (always happens for stores)
    cpu_read(cpu, (base & 0xFF00) | (addr & 0x00FF));
    u8 val = cpu->X & ((hi + 1) & 0xFF);
    if ((addr & 0xFF00) != (hi << 8))
    {
        addr = (addr & 0x00FF) | (val << 8);
    }
    cpu_write(cpu, addr, val);
    return 5;
}

// LAS (LAR) - AND memory with SP, store to A, X, and SP
static int op_BB(CPU *cpu)
{
    u8 v = cpu_read(cpu, addr_absolute_y(cpu, true));
    cpu->A = cpu->X = cpu->SP = v & cpu->SP;
    cpu_update_nz(cpu, cpu->A);
    return 4;
}

// ============================================================================
// Opcode Lookup Table
// ============================================================================

static const OpcodeHandler opcode_table[256] = {
    // 0x00-0x0F
    op_00,
    op_01,
    op_JAM,
    op_03,
    op_NOP_zp,
    op_05,
    op_06,
    op_07,
    op_08,
    op_09,
    op_0A,
    op_0B,
    op_NOP_abs,
    op_0D,
    op_0E,
    op_0F,
    // 0x10-0x1F
    op_10,
    op_11,
    op_JAM,
    op_13,
    op_NOP_zpx,
    op_15,
    op_16,
    op_17,
    op_18,
    op_19,
    op_NOP_impl,
    op_1B,
    op_NOP_abx,
    op_1D,
    op_1E,
    op_1F,
    // 0x20-0x2F
    op_20,
    op_21,
    op_JAM,
    op_23,
    op_24,
    op_25,
    op_26,
    op_27,
    op_28,
    op_29,
    op_2A,
    op_2B,
    op_2C,
    op_2D,
    op_2E,
    op_2F,
    // 0x30-0x3F
    op_30,
    op_31,
    op_JAM,
    op_33,
    op_NOP_zpx,
    op_35,
    op_36,
    op_37,
    op_38,
    op_39,
    op_NOP_impl,
    op_3B,
    op_NOP_abx,
    op_3D,
    op_3E,
    op_3F,
    // 0x40-0x4F
    op_40,
    op_41,
    op_JAM,
    op_43,
    op_NOP_zp,
    op_45,
    op_46,
    op_47,
    op_48,
    op_49,
    op_4A,
    op_4B,
    op_4C,
    op_4D,
    op_4E,
    op_4F,
    // 0x50-0x5F
    op_50,
    op_51,
    op_JAM,
    op_53,
    op_NOP_zpx,
    op_55,
    op_56,
    op_57,
    op_58,
    op_59,
    op_NOP_impl,
    op_5B,
    op_NOP_abx,
    op_5D,
    op_5E,
    op_5F,
    // 0x60-0x6F
    op_60,
    op_61,
    op_JAM,
    op_63,
    op_NOP_zp,
    op_65,
    op_66,
    op_67,
    op_68,
    op_69,
    op_6A,
    op_6B,
    op_6C,
    op_6D,
    op_6E,
    op_6F,
    // 0x70-0x7F
    op_70,
    op_71,
    op_JAM,
    op_73,
    op_NOP_zpx,
    op_75,
    op_76,
    op_77,
    op_78,
    op_79,
    op_NOP_impl,
    op_7B,
    op_NOP_abx,
    op_7D,
    op_7E,
    op_7F,
    // 0x80-0x8F
    op_NOP_imm,
    op_81,
    op_NOP_imm,
    op_83,
    op_84,
    op_85,
    op_86,
    op_87,
    op_88,
    op_NOP_imm,
    op_8A,
    op_8B,
    op_8C,
    op_8D,
    op_8E,
    op_8F,
    // 0x90-0x9F
    op_90,
    op_91,
    op_JAM,
    op_93,
    op_94,
    op_95,
    op_96,
    op_97,
    op_98,
    op_99,
    op_9A,
    op_9B,
    op_9C,
    op_9D,
    op_9E,
    op_9F,
    // 0xA0-0xAF
    op_A0,
    op_A1,
    op_A2,
    op_A3,
    op_A4,
    op_A5,
    op_A6,
    op_A7,
    op_A8,
    op_A9,
    op_AA,
    op_AB,
    op_AC,
    op_AD,
    op_AE,
    op_AF,
    // 0xB0-0xBF
    op_B0,
    op_B1,
    op_JAM,
    op_B3,
    op_B4,
    op_B5,
    op_B6,
    op_B7,
    op_B8,
    op_B9,
    op_BA,
    op_BB,
    op_BC,
    op_BD,
    op_BE,
    op_BF,
    // 0xC0-0xCF
    op_C0,
    op_C1,
    op_NOP_imm,
    op_C3,
    op_C4,
    op_C5,
    op_C6,
    op_C7,
    op_C8,
    op_C9,
    op_CA,
    op_CB,
    op_CC,
    op_CD,
    op_CE,
    op_CF,
    // 0xD0-0xDF
    op_D0,
    op_D1,
    op_JAM,
    op_D3,
    op_NOP_zpx,
    op_D5,
    op_D6,
    op_D7,
    op_D8,
    op_D9,
    op_NOP_impl,
    op_DB,
    op_NOP_abx,
    op_DD,
    op_DE,
    op_DF,
    // 0xE0-0xEF
    op_E0,
    op_E1,
    op_NOP_imm,
    op_E3,
    op_E4,
    op_E5,
    op_E6,
    op_E7,
    op_E8,
    op_E9,
    op_EA,
    op_EB,
    op_EC,
    op_ED,
    op_EE,
    op_EF,
    // 0xF0-0xFF
    op_F0,
    op_F1,
    op_JAM,
    op_F3,
    op_NOP_zpx,
    op_F5,
    op_F6,
    op_F7,
    op_F8,
    op_F9,
    op_NOP_impl,
    op_FB,
    op_NOP_abx,
    op_FD,
    op_FE,
    op_FF,
};

// ============================================================================
// CPU Step - Execute one instruction using LUT
// ============================================================================

int cpu_step(CPU *cpu)
{
    cpu->extra_cycles = 0;
    cpu->page_crossed = false;
    cpu->nmi_triggered_this_insn = false;

    // Sample NMI state at instruction start
    // NMI is sampled at the penultimate cycle of the previous instruction
    // We use age>=2 to ensure NMI was pending long enough to be sampled
    bool nmi_at_start = cpu->nmi_pending && cpu->nmi_pending_age >= 2;

    // Sample IRQ state and I flag BEFORE the first cycle tick
    // The CPU samples at phi2 of the previous cycle, so it sees IRQ state
    // before any changes happen in the current cycle
    // Also sample I flag - interrupts are committed based on I flag at instruction start,
    // not at end (so SEI doesn't block an already-decided interrupt)
    bool irq_at_start = cpu->irq_pending;
    bool i_flag_at_start = (cpu->P & FLAG_I) != 0;
    u8 opcode = cpu_read(cpu, cpu->PC++);
    int cycles = opcode_table[opcode](cpu);

    // Take NMI if it was pending at start, or if it became pending during
    // this instruction and was pending long enough (sampled at penultimate cycle)
    // NMI has higher priority than IRQ, so check it first
    //
    // Special case: for BRK (opcode $00), don't take NMI at instruction end
    // because BRK is itself an interrupt sequence. NMI triggered during BRK
    // should be taken at the end of the first instruction in the handler.
    int total_cycles = cycles + cpu->extra_cycles;
    bool is_3cycle_branch = ((opcode & 0x1F) == 0x10) && (total_cycles == 3);
    int nmi_threshold = is_3cycle_branch ? 2 : 1;
    // nmi_triggered_this_insn catches the case where NMI was triggered during
    // this instruction but nmi_pending was cleared by reading ICR
    bool take_nmi = nmi_at_start || cpu->nmi_triggered_this_insn || 
                    (cpu->nmi_pending && cpu->nmi_pending_age >= nmi_threshold);

    // Don't take NMI at the end of BRK - it should be taken at the next instruction
    if (opcode == 0x00)
    {
        take_nmi = false;
    }

    if (take_nmi)
    {
        cpu->nmi_pending = false;
        cpu->nmi_triggered_this_insn = false;  // Clear this too - NMI is being taken
        // Note: nmi_edge stays set - it's cleared when ICR is read
        do_interrupt(cpu, NMI, false);
        return cycles + cpu->extra_cycles + 7;
    }

    // Take IRQ if:
    // 1. IRQ was pending at start of instruction, OR
    // 2. IRQ became pending during the instruction and was pending long enough.
    //    The 6502 samples IRQ at the penultimate cycle of each instruction.
    //    For most instructions, threshold=1 works (IRQ set before last cycle).
    //    For 3-cycle taken branches only, threshold=2 due to internal timing.
    int threshold = is_3cycle_branch ? 2 : 1;
    bool take_irq = irq_at_start || (cpu->irq_pending && cpu->irq_pending_age >= threshold);

    // Use I flag sampled at start - SEI doesn't block an IRQ that was already going to be taken
    if (take_irq && !i_flag_at_start)
    {
        cpu->irq_pending = false;
        do_interrupt(cpu, IRQ, false);
        return cycles + cpu->extra_cycles + 7;
    }

    return cycles + cpu->extra_cycles;
}
