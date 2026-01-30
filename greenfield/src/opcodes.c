/*
 * C64 Emulator - Legal Opcode Implementations
 */

#include "cpu.h"
#include "mos6510.h"

/* Helper macro to advance PC based on addressing mode */
#define ADVANCE_PC(cpu, inst) do { \
    cpu_set_pc(cpu, cpu_get_pc(cpu) + (inst)->size); \
} while(0)

/* ============================================================
 * Load/Store Operations
 * ============================================================ */

void LDA(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    cpu->A = fetch_operand(cpu, inst->mode);
    set_nz_flags(cpu, cpu->A);
    ADVANCE_PC(cpu, inst);
}

void LDX(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    cpu->X = fetch_operand(cpu, inst->mode);
    set_nz_flags(cpu, cpu->X);
    ADVANCE_PC(cpu, inst);
}

void LDY(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    cpu->Y = fetch_operand(cpu, inst->mode);
    set_nz_flags(cpu, cpu->Y);
    ADVANCE_PC(cpu, inst);
}

void STA(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, inst->mode);
    cpu_write(cpu, addr, cpu->A);
    ADVANCE_PC(cpu, inst);
}

void STX(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, inst->mode);
    cpu_write(cpu, addr, cpu->X);
    ADVANCE_PC(cpu, inst);
}

void STY(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, inst->mode);
    cpu_write(cpu, addr, cpu->Y);
    ADVANCE_PC(cpu, inst);
}

/* ============================================================
 * Transfer Operations
 * ============================================================ */

void TAX(CPU *cpu)
{
    cpu->X = cpu->A;
    set_nz_flags(cpu, cpu->X);
    cpu->PC++;
}

void TAY(CPU *cpu)
{
    cpu->Y = cpu->A;
    set_nz_flags(cpu, cpu->Y);
    cpu->PC++;
}

void TXA(CPU *cpu)
{
    cpu->A = cpu->X;
    set_nz_flags(cpu, cpu->A);
    cpu->PC++;
}

void TYA(CPU *cpu)
{
    cpu->A = cpu->Y;
    set_nz_flags(cpu, cpu->A);
    cpu->PC++;
}

void TSX(CPU *cpu)
{
    cpu->X = cpu->SP;
    set_nz_flags(cpu, cpu->X);
    cpu->PC++;
}

void TXS(CPU *cpu)
{
    cpu->SP = cpu->X;
    cpu->PC++;
}

/* ============================================================
 * Stack Operations
 * ============================================================ */

void PHA(CPU *cpu)
{
    cpu_push(cpu, cpu->A);
    cpu->PC++;
}

void PHP(CPU *cpu)
{
    /* Push with B and reserved flags set */
    cpu_push(cpu, cpu->P | FLAG_BREAK | FLAG_RESERVED);
    cpu->PC++;
}

void PLA(CPU *cpu)
{
    cpu->A = cpu_pull(cpu);
    set_nz_flags(cpu, cpu->A);
    cpu->PC++;
}

void PLP(CPU *cpu)
{
    /* Pull, ignoring B flag, preserving reserved */
    cpu->P = (cpu_pull(cpu) & ~FLAG_BREAK) | FLAG_RESERVED;
    cpu->PC++;
}

/* ============================================================
 * Logical Operations
 * ============================================================ */

void AND(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    cpu->A &= fetch_operand(cpu, inst->mode);
    set_nz_flags(cpu, cpu->A);
    ADVANCE_PC(cpu, inst);
}

void ORA(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    cpu->A |= fetch_operand(cpu, inst->mode);
    set_nz_flags(cpu, cpu->A);
    ADVANCE_PC(cpu, inst);
}

void EOR(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    cpu->A ^= fetch_operand(cpu, inst->mode);
    set_nz_flags(cpu, cpu->A);
    ADVANCE_PC(cpu, inst);
}

void BIT(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint8_t value = fetch_operand(cpu, inst->mode);
    set_flag_zero(cpu, (cpu->A & value) == 0);
    set_flag_negative(cpu, value & 0x80);
    set_flag_overflow(cpu, value & 0x40);
    ADVANCE_PC(cpu, inst);
}

/* ============================================================
 * Arithmetic Operations
 * ============================================================ */

void ADC(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    cpu->A = add_with_carry(cpu, cpu->A, fetch_operand(cpu, inst->mode));
    ADVANCE_PC(cpu, inst);
}

void SBC(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    cpu->A = subtract_with_borrow(cpu, cpu->A, fetch_operand(cpu, inst->mode));
    ADVANCE_PC(cpu, inst);
}

void CMP(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint8_t value = fetch_operand(cpu, inst->mode);
    uint8_t result = cpu->A - value;
    set_flag_carry(cpu, cpu->A >= value);
    set_nz_flags(cpu, result);
    ADVANCE_PC(cpu, inst);
}

void CPX(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint8_t value = fetch_operand(cpu, inst->mode);
    uint8_t result = cpu->X - value;
    set_flag_carry(cpu, cpu->X >= value);
    set_nz_flags(cpu, result);
    ADVANCE_PC(cpu, inst);
}

void CPY(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint8_t value = fetch_operand(cpu, inst->mode);
    uint8_t result = cpu->Y - value;
    set_flag_carry(cpu, cpu->Y >= value);
    set_nz_flags(cpu, result);
    ADVANCE_PC(cpu, inst);
}

/* ============================================================
 * Increment/Decrement Operations
 * ============================================================ */

void INC(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, inst->mode);
    uint8_t value = cpu_read(cpu, addr) + 1;
    cpu_write(cpu, addr, value);
    set_nz_flags(cpu, value);
    ADVANCE_PC(cpu, inst);
}

void INX(CPU *cpu)
{
    cpu->X++;
    set_nz_flags(cpu, cpu->X);
    cpu->PC++;
}

void INY(CPU *cpu)
{
    cpu->Y++;
    set_nz_flags(cpu, cpu->Y);
    cpu->PC++;
}

void DEC(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, inst->mode);
    uint8_t value = cpu_read(cpu, addr) - 1;
    cpu_write(cpu, addr, value);
    set_nz_flags(cpu, value);
    ADVANCE_PC(cpu, inst);
}

void DEX(CPU *cpu)
{
    cpu->X--;
    set_nz_flags(cpu, cpu->X);
    cpu->PC++;
}

void DEY(CPU *cpu)
{
    cpu->Y--;
    set_nz_flags(cpu, cpu->Y);
    cpu->PC++;
}

/* ============================================================
 * Shift/Rotate Operations
 * ============================================================ */

void ASL(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    
    if (inst->mode == Accumulator) {
        set_flag_carry(cpu, cpu->A & 0x80);
        cpu->A <<= 1;
        set_nz_flags(cpu, cpu->A);
    } else {
        uint16_t addr = fetch_address(cpu, inst->mode);
        uint8_t value = cpu_read(cpu, addr);
        set_flag_carry(cpu, value & 0x80);
        value <<= 1;
        cpu_write(cpu, addr, value);
        set_nz_flags(cpu, value);
    }
    ADVANCE_PC(cpu, inst);
}

void LSR(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    
    if (inst->mode == Accumulator) {
        set_flag_carry(cpu, cpu->A & 0x01);
        cpu->A >>= 1;
        set_nz_flags(cpu, cpu->A);
    } else {
        uint16_t addr = fetch_address(cpu, inst->mode);
        uint8_t value = cpu_read(cpu, addr);
        set_flag_carry(cpu, value & 0x01);
        value >>= 1;
        cpu_write(cpu, addr, value);
        set_nz_flags(cpu, value);
    }
    ADVANCE_PC(cpu, inst);
}

void ROL(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint8_t old_carry = get_flag_carry(cpu) ? 1 : 0;
    
    if (inst->mode == Accumulator) {
        set_flag_carry(cpu, cpu->A & 0x80);
        cpu->A = (cpu->A << 1) | old_carry;
        set_nz_flags(cpu, cpu->A);
    } else {
        uint16_t addr = fetch_address(cpu, inst->mode);
        uint8_t value = cpu_read(cpu, addr);
        set_flag_carry(cpu, value & 0x80);
        value = (value << 1) | old_carry;
        cpu_write(cpu, addr, value);
        set_nz_flags(cpu, value);
    }
    ADVANCE_PC(cpu, inst);
}

void ROR(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    uint8_t old_carry = get_flag_carry(cpu) ? 0x80 : 0;
    
    if (inst->mode == Accumulator) {
        set_flag_carry(cpu, cpu->A & 0x01);
        cpu->A = (cpu->A >> 1) | old_carry;
        set_nz_flags(cpu, cpu->A);
    } else {
        uint16_t addr = fetch_address(cpu, inst->mode);
        uint8_t value = cpu_read(cpu, addr);
        set_flag_carry(cpu, value & 0x01);
        value = (value >> 1) | old_carry;
        cpu_write(cpu, addr, value);
        set_nz_flags(cpu, value);
    }
    ADVANCE_PC(cpu, inst);
}

/* ============================================================
 * Jump/Call Operations
 * ============================================================ */

void JMP(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    cpu->PC = fetch_address(cpu, inst->mode);
}

void JSR(CPU *cpu)
{
    uint16_t addr = cpu_read_word(cpu, cpu->PC + 1);
    /* Push return address - 1 (PC+2, pointing to last byte of JSR) */
    cpu_push_word(cpu, cpu->PC + 2);
    cpu->PC = addr;
}

void RTS(CPU *cpu)
{
    cpu->PC = cpu_pull_word(cpu) + 1;
}

/* ============================================================
 * Branch Operations
 * ============================================================ */

static void do_branch(CPU *cpu, bool condition)
{
    if (condition) {
        int8_t offset = (int8_t)cpu_read(cpu, cpu->PC + 1);
        uint16_t old_pc = cpu->PC + 2;
        uint16_t new_pc = old_pc + offset;
        
        /* Branch taken: +1 cycle */
        cpu->extra_cycles = 1;
        
        /* Page crossing: +1 additional cycle */
        if ((old_pc & 0xFF00) != (new_pc & 0xFF00)) {
            cpu->extra_cycles = 2;
        }
        
        cpu->PC = new_pc;
    } else {
        cpu->PC += 2;
    }
}

void BCC(CPU *cpu) { do_branch(cpu, !get_flag_carry(cpu)); }
void BCS(CPU *cpu) { do_branch(cpu, get_flag_carry(cpu)); }
void BEQ(CPU *cpu) { do_branch(cpu, get_flag_zero(cpu)); }
void BMI(CPU *cpu) { do_branch(cpu, get_flag_negative(cpu)); }
void BNE(CPU *cpu) { do_branch(cpu, !get_flag_zero(cpu)); }
void BPL(CPU *cpu) { do_branch(cpu, !get_flag_negative(cpu)); }
void BVC(CPU *cpu) { do_branch(cpu, !get_flag_overflow(cpu)); }
void BVS(CPU *cpu) { do_branch(cpu, get_flag_overflow(cpu)); }

/* ============================================================
 * Flag Operations
 * ============================================================ */

void CLC(CPU *cpu) { set_flag_carry(cpu, false); cpu->PC++; }
void CLD(CPU *cpu) { set_flag_decimal(cpu, false); cpu->PC++; }
void CLI(CPU *cpu) { set_flag_interrupt(cpu, false); cpu->PC++; }
void CLV(CPU *cpu) { set_flag_overflow(cpu, false); cpu->PC++; }
void SEC(CPU *cpu) { set_flag_carry(cpu, true); cpu->PC++; }
void SED(CPU *cpu) { set_flag_decimal(cpu, true); cpu->PC++; }
void SEI(CPU *cpu) { set_flag_interrupt(cpu, true); cpu->PC++; }

/* ============================================================
 * System Operations
 * ============================================================ */

void BRK(CPU *cpu)
{
    cpu->PC += 2;  /* BRK is 2 bytes (opcode + padding) */
    cpu_push_word(cpu, cpu->PC);
    cpu_push(cpu, cpu->P | FLAG_BREAK | FLAG_RESERVED);
    set_flag_interrupt(cpu, true);
    cpu->PC = cpu_read_word(cpu, IRQ_VECTOR);
}

void NOP(CPU *cpu)
{
    const Instruction *inst = fetch_instruction(cpu);
    /* Illegal NOPs with addressing modes still read from memory */
    if (inst->mode != Implied) {
        (void)fetch_operand(cpu, inst->mode);  /* Read and discard - triggers page crossing check */
    }
    ADVANCE_PC(cpu, inst);
}

void RTI(CPU *cpu)
{
    cpu->P = (cpu_pull(cpu) & ~FLAG_BREAK) | FLAG_RESERVED;
    cpu->PC = cpu_pull_word(cpu);
}
