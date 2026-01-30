/*
 * C64 Emulator - Illegal Opcode Implementations
 * 
 * These are "undocumented" opcodes that exist on the real 6502/6510.
 * Most games and demos don't use them, but some do for code size/speed.
 */

#include "cpu.h"
#include "mos6510.h"

/* Helper macro to advance PC based on addressing mode */
#define ADVANCE_PC(cpu, inst) do { \
    cpu_set_pc(cpu, cpu_get_pc(cpu) + (inst)->size); \
} while(0)

/* ============================================================
 * Stable Illegal Opcodes
 * These have consistent, well-documented behavior.
 * ============================================================ */

/* LAX: LDA + LDX combined */
void LAX(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    cpu->A = cpu->X = fetch_operand(cpu, inst->mode);
    set_nz_flags(cpu, cpu->A);
    ADVANCE_PC(cpu, inst);
}

/* SAX: Store A & X */
void SAX(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, inst->mode);
    cpu_write(cpu, addr, cpu->A & cpu->X);
    ADVANCE_PC(cpu, inst);
}

/* DCP: DEC + CMP combined */
void DCP(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, inst->mode);
    uint8_t value = cpu_read(cpu, addr) - 1;
    cpu_write(cpu, addr, value);
    
    /* CMP portion */
    uint8_t result = cpu->A - value;
    set_flag_carry(cpu, cpu->A >= value);
    set_nz_flags(cpu, result);
    ADVANCE_PC(cpu, inst);
}

/* ISB (ISC): INC + SBC combined */
void ISB(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, inst->mode);
    uint8_t value = cpu_read(cpu, addr) + 1;
    cpu_write(cpu, addr, value);
    
    /* SBC portion */
    cpu->A = subtract_with_borrow(cpu, cpu->A, value);
    ADVANCE_PC(cpu, inst);
}

/* SLO: ASL + ORA combined */
void SLO(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, inst->mode);
    uint8_t value = cpu_read(cpu, addr);
    
    /* ASL portion */
    set_flag_carry(cpu, value & 0x80);
    value <<= 1;
    cpu_write(cpu, addr, value);
    
    /* ORA portion */
    cpu->A |= value;
    set_nz_flags(cpu, cpu->A);
    ADVANCE_PC(cpu, inst);
}

/* RLA: ROL + AND combined */
void RLA(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, inst->mode);
    uint8_t value = cpu_read(cpu, addr);
    uint8_t old_carry = get_flag_carry(cpu) ? 1 : 0;
    
    /* ROL portion */
    set_flag_carry(cpu, value & 0x80);
    value = (value << 1) | old_carry;
    cpu_write(cpu, addr, value);
    
    /* AND portion */
    cpu->A &= value;
    set_nz_flags(cpu, cpu->A);
    ADVANCE_PC(cpu, inst);
}

/* SRE: LSR + EOR combined */
void SRE(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, inst->mode);
    uint8_t value = cpu_read(cpu, addr);
    
    /* LSR portion */
    set_flag_carry(cpu, value & 0x01);
    value >>= 1;
    cpu_write(cpu, addr, value);
    
    /* EOR portion */
    cpu->A ^= value;
    set_nz_flags(cpu, cpu->A);
    ADVANCE_PC(cpu, inst);
}

/* RRA: ROR + ADC combined */
void RRA(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, inst->mode);
    uint8_t value = cpu_read(cpu, addr);
    uint8_t old_carry = get_flag_carry(cpu) ? 0x80 : 0;
    
    /* ROR portion */
    set_flag_carry(cpu, value & 0x01);
    value = (value >> 1) | old_carry;
    cpu_write(cpu, addr, value);
    
    /* ADC portion */
    cpu->A = add_with_carry(cpu, cpu->A, value);
    ADVANCE_PC(cpu, inst);
}

/* ============================================================
 * Combined Illegal Opcodes
 * ============================================================ */

/* ANC: AND + set C from bit 7 */
void ANC(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    cpu->A &= fetch_operand(cpu, inst->mode);
    set_nz_flags(cpu, cpu->A);
    set_flag_carry(cpu, cpu->A & 0x80);
    ADVANCE_PC(cpu, inst);
}

/* ASR (ALR): AND + LSR */
void ASR(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    cpu->A &= fetch_operand(cpu, inst->mode);
    set_flag_carry(cpu, cpu->A & 0x01);
    cpu->A >>= 1;
    set_nz_flags(cpu, cpu->A);
    ADVANCE_PC(cpu, inst);
}

/* ARR: AND + ROR with special V/C flag behavior
 * In decimal mode, has complex BCD fixup behavior */
void ARR(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint8_t value = fetch_operand(cpu, inst->mode);
    uint8_t and_result = cpu->A & value;
    uint8_t old_carry = get_flag_carry(cpu) ? 0x80 : 0;
    
    /* Perform ROR on the AND result */
    uint8_t ror_result = (and_result >> 1) | old_carry;
    cpu->A = ror_result;
    
    if (cpu->decimal_mode && get_flag_decimal(cpu)) {
        /* Decimal mode behavior per 64doc.txt and extensive testing:
         * 
         * The ROR result is stored in A first, then BCD fixup is applied.
         * N = bit 7 of ROR result (from old carry)
         * Z = ROR result == 0
         * V = bit 6 changed between AND result and ROR result
         */
        set_flag_negative(cpu, ror_result & 0x80);
        set_flag_zero(cpu, ror_result == 0);
        set_flag_overflow(cpu, (and_result ^ ror_result) & 0x40);
        
        /* BCD fixup for low nibble: 
         * If (AND_result & 0x0F) + (AND_result & 0x01) > 5, add 6 to low nibble */
        uint8_t al = and_result & 0x0F;
        if ((al + (al & 1)) > 5) {
            cpu->A = (cpu->A & 0xF0) | ((cpu->A + 6) & 0x0F);
        }
        
        /* BCD fixup for high nibble and carry:
         * If (AND_result & 0xF0) + (AND_result & 0x10) > 0x50, add 0x60 and set carry */
        uint8_t ah = and_result & 0xF0;
        if ((ah + (ah & 0x10)) > 0x50) {
            cpu->A = (cpu->A + 0x60) & 0xFF;
            set_flag_carry(cpu, true);
        } else {
            set_flag_carry(cpu, false);
        }
    } else {
        /* Binary mode behavior:
         * N = bit 7 of result
         * Z = result == 0
         * C = bit 6 of result
         * V = bit 6 XOR bit 5 of result */
        set_nz_flags(cpu, cpu->A);
        set_flag_carry(cpu, cpu->A & 0x40);
        set_flag_overflow(cpu, ((cpu->A & 0x40) ^ ((cpu->A & 0x20) << 1)));
    }
    ADVANCE_PC(cpu, inst);
}

/* SBX (AXS): X = (A & X) - imm (without borrow) */
void SBX(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint8_t value = fetch_operand(cpu, inst->mode);
    uint8_t ax = cpu->A & cpu->X;
    cpu->X = ax - value;
    set_flag_carry(cpu, ax >= value);
    set_nz_flags(cpu, cpu->X);
    ADVANCE_PC(cpu, inst);
}

/* LAS: LDA/TSX anded with SP */
void LAS(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint8_t value = fetch_operand(cpu, inst->mode) & cpu->SP;
    cpu->A = cpu->X = cpu->SP = value;
    set_nz_flags(cpu, value);
    ADVANCE_PC(cpu, inst);
}

/* ============================================================
 * Unstable Illegal Opcodes
 * These have behavior that varies between chips or is data-dependent.
 * ============================================================ */

/* ANE (XAA): A = (A | magic) & X & imm
 * Magic constant varies between chips, commonly 0xEE or 0xFF
 * Per 64doc.txt and Lorenz tests, 0xEE is correct for C64 (6510) */
void ANE(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    const uint8_t magic = 0xEE;
    uint8_t imm = fetch_operand(cpu, inst->mode);
    cpu->A = (cpu->A | magic) & cpu->X & imm;
    set_nz_flags(cpu, cpu->A);
    ADVANCE_PC(cpu, inst);
}

/* LXA: A = X = (A | magic) & imm
 * Per 64doc.txt and Lorenz test source: A = (A | $EE) & imm, X = A */
void LXA(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    const uint8_t magic = 0xEE;
    uint8_t imm = fetch_operand(cpu, inst->mode);
    cpu->A = cpu->X = (cpu->A | magic) & imm;
    set_nz_flags(cpu, cpu->A);
    ADVANCE_PC(cpu, inst);
}

/* SHA (AHX): Store A & X & (addr_hi + 1) */
void SHA(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, inst->mode);
    uint8_t hi = (addr >> 8) + 1;
    cpu_write(cpu, addr, cpu->A & cpu->X & hi);
    ADVANCE_PC(cpu, inst);
}

/* SHX: Store X & (addr_hi + 1) */
void SHX(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, inst->mode);
    uint8_t hi = (addr >> 8) + 1;
    cpu_write(cpu, addr, cpu->X & hi);
    ADVANCE_PC(cpu, inst);
}

/* SHY: Store Y & (addr_hi + 1) */
void SHY(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, inst->mode);
    uint8_t hi = (addr >> 8) + 1;
    cpu_write(cpu, addr, cpu->Y & hi);
    ADVANCE_PC(cpu, inst);
}

/* SHS (TAS): SP = A & X, then store SP & (addr_hi + 1) */
void SHS(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    cpu->SP = cpu->A & cpu->X;
    uint16_t addr = fetch_address(cpu, inst->mode);
    uint8_t hi = (addr >> 8) + 1;
    cpu_write(cpu, addr, cpu->SP & hi);
    ADVANCE_PC(cpu, inst);
}

/* JAM (KIL): Halt the CPU */
void JAM(CPU *cpu)
{
    /* In a real CPU this locks up. We'll just not advance PC. */
    /* Could also set a halted flag here if needed */
}
