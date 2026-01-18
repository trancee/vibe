#include "../include/mos6510.h"

// Load/Store Operations
void LDA(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    cpu->A = fetch_operand(cpu, op->mode);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void LDX(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    cpu->X = fetch_operand(cpu, op->mode);
    set_flag_negative(cpu, cpu->X & 0x80);
    set_flag_zero(cpu, cpu->X == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void LDY(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    cpu->Y = fetch_operand(cpu, op->mode);
    set_flag_negative(cpu, cpu->Y & 0x80);
    set_flag_zero(cpu, cpu->Y == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void STA(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    cpu->memory[addr] = cpu->A;
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void STX(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    cpu->memory[addr] = cpu->X;
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void STY(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    cpu->memory[addr] = cpu->Y;
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

// Transfer Operations
void TAX(MOS6510 *cpu)
{
    cpu->X = cpu->A;
    set_flag_negative(cpu, cpu->X & 0x80);
    set_flag_zero(cpu, cpu->X == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void TAY(MOS6510 *cpu)
{
    cpu->Y = cpu->A;
    set_flag_negative(cpu, cpu->Y & 0x80);
    set_flag_zero(cpu, cpu->Y == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void TXA(MOS6510 *cpu)
{
    cpu->A = cpu->X;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void TYA(MOS6510 *cpu)
{
    cpu->A = cpu->Y;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void TSX(MOS6510 *cpu)
{
    cpu->X = cpu->SP;
    set_flag_negative(cpu, cpu->X & 0x80);
    set_flag_zero(cpu, cpu->X == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void TXS(MOS6510 *cpu)
{
    cpu->SP = cpu->X;
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

// Stack Operations
void PHA(MOS6510 *cpu)
{
    mos6510_push(cpu, cpu->A);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void PHP(MOS6510 *cpu)
{
    mos6510_push(cpu, cpu->P | FLAG_BREAK);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void PLA(MOS6510 *cpu)
{
    cpu->A = mos6510_pull(cpu);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void PLP(MOS6510 *cpu)
{
    // nestest requires that BREAK flag is cleared when pulling P
    cpu->P = (mos6510_pull(cpu) & ~FLAG_BREAK) | FLAG_RESERVED;
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

// Logical Operations
void AND(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    cpu->A &= fetch_operand(cpu, op->mode);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void ORA(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    cpu->A |= fetch_operand(cpu, op->mode);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void EOR(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    cpu->A ^= fetch_operand(cpu, op->mode);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void BIT(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint8_t value = fetch_operand(cpu, op->mode);
    uint8_t result = cpu->A & value;
    set_flag_zero(cpu, result == 0);
    set_flag_negative(cpu, value & 0x80);
    set_flag_overflow(cpu, value & 0x40);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

// Arithmetic Operations
void ADC(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    cpu->A = add_with_carry(cpu, cpu->A, fetch_operand(cpu, op->mode));
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void SBC(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    cpu->A = subtract_with_borrow(cpu, cpu->A, fetch_operand(cpu, op->mode));
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void CMP(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint8_t value = fetch_operand(cpu, op->mode);
    uint8_t result = cpu->A - value;
    set_flag_carry(cpu, cpu->A >= value);
    set_flag_negative(cpu, result & 0x80);
    set_flag_zero(cpu, result == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void CPX(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint8_t value = fetch_operand(cpu, op->mode);
    uint8_t result = cpu->X - value;
    set_flag_carry(cpu, cpu->X >= value);
    set_flag_negative(cpu, result & 0x80);
    set_flag_zero(cpu, result == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void CPY(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint8_t value = fetch_operand(cpu, op->mode);
    uint8_t result = cpu->Y - value;
    set_flag_carry(cpu, cpu->Y >= value);
    set_flag_negative(cpu, result & 0x80);
    set_flag_zero(cpu, result == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

// Increment/Decrement Operations
void INC(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    cpu->memory[addr]++;
    set_flag_negative(cpu, cpu->memory[addr] & 0x80);
    set_flag_zero(cpu, cpu->memory[addr] == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void INX(MOS6510 *cpu)
{
    cpu->X++;
    set_flag_negative(cpu, cpu->X & 0x80);
    set_flag_zero(cpu, cpu->X == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void INY(MOS6510 *cpu)
{
    cpu->Y++;
    set_flag_negative(cpu, cpu->Y & 0x80);
    set_flag_zero(cpu, cpu->Y == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void DEC(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    cpu->memory[addr]--;
    set_flag_negative(cpu, cpu->memory[addr] & 0x80);
    set_flag_zero(cpu, cpu->memory[addr] == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void DEX(MOS6510 *cpu)
{
    cpu->X--;
    set_flag_negative(cpu, cpu->X & 0x80);
    set_flag_zero(cpu, cpu->X == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void DEY(MOS6510 *cpu)
{
    cpu->Y--;
    set_flag_negative(cpu, cpu->Y & 0x80);
    set_flag_zero(cpu, cpu->Y == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

// Shift Operations
void ASL(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint8_t value;
    uint16_t addr;

    if (op->mode == ADDR_IMP)
    {
        value = cpu->A;
        set_flag_carry(cpu, value & 0x80);
        value <<= 1;
        cpu->A = value;
    }
    else
    {
        addr = get_operand_address(cpu, op->mode);
        value = cpu->memory[addr];
        set_flag_carry(cpu, value & 0x80);
        value <<= 1;
        cpu->memory[addr] = value;
    }

    set_flag_negative(cpu, value & 0x80);
    set_flag_zero(cpu, value == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void LSR(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint8_t value;
    uint16_t addr;

    if (op->mode == ADDR_IMP)
    {
        value = cpu->A;
        set_flag_carry(cpu, value & 0x01);
        value >>= 1;
        cpu->A = value;
    }
    else
    {
        addr = get_operand_address(cpu, op->mode);
        value = cpu->memory[addr];
        set_flag_carry(cpu, value & 0x01);
        value >>= 1;
        cpu->memory[addr] = value;
    }

    set_flag_negative(cpu, value & 0x80);
    set_flag_zero(cpu, value == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void ROL(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint8_t value;
    uint16_t addr;
    bool carry_in = get_flag_carry(cpu);

    if (op->mode == ADDR_IMP)
    {
        value = cpu->A;
        set_flag_carry(cpu, value & 0x80);
        value = (value << 1) | carry_in;
        cpu->A = value;
    }
    else
    {
        addr = get_operand_address(cpu, op->mode);
        value = cpu->memory[addr];
        set_flag_carry(cpu, value & 0x80);
        value = (value << 1) | carry_in;
        cpu->memory[addr] = value;
    }

    set_flag_negative(cpu, value & 0x80);
    set_flag_zero(cpu, value == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void ROR(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint8_t value;
    uint16_t addr;
    bool carry_in = get_flag_carry(cpu);

    if (op->mode == ADDR_IMP)
    {
        value = cpu->A;
        set_flag_carry(cpu, value & 0x01);
        value = (value >> 1) | (carry_in << 7);
        cpu->A = value;
    }
    else
    {
        addr = get_operand_address(cpu, op->mode);
        value = cpu->memory[addr];
        set_flag_carry(cpu, value & 0x01);
        value = (value >> 1) | (carry_in << 7);
        cpu->memory[addr] = value;
    }

    set_flag_negative(cpu, value & 0x80);
    set_flag_zero(cpu, value == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

// Jump Operations
void JMP(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    mos6510_set_pc(cpu, addr);
}

void JSR(MOS6510 *cpu)
{
    uint16_t return_addr = mos6510_get_pc(cpu) + 2;
    mos6510_push(cpu, (return_addr >> 8) & 0xFF);
    mos6510_push(cpu, return_addr & 0xFF);

    uint16_t addr = get_operand_address(cpu, ADDR_ABS);
    mos6510_set_pc(cpu, addr);
}

void RTS(MOS6510 *cpu)
{
    uint8_t low_byte = mos6510_pull(cpu);
    uint8_t high_byte = mos6510_pull(cpu);
    uint16_t addr = (high_byte << 8) | low_byte;
    mos6510_set_pc(cpu, addr + 1);
}

// Branch Operations
void BCC(MOS6510 *cpu)
{
    uint16_t pc = mos6510_get_pc(cpu);
    if (!get_flag_carry(cpu))
    {
        int8_t offset = (int8_t)cpu->memory[pc + 1]; // Get operand directly
        uint16_t new_pc = pc + 2 + offset;           // Offset relative to next instruction
        mos6510_set_pc(cpu, new_pc);
    }
    else
    {
        mos6510_set_pc(cpu, pc + 2); // Skip to next instruction
    }
}

void BCS(MOS6510 *cpu)
{
    uint16_t pc = mos6510_get_pc(cpu);
    if (get_flag_carry(cpu))
    {
        int8_t offset = (int8_t)cpu->memory[pc + 1]; // Get operand directly
        uint16_t new_pc = pc + 2 + offset;           // Offset relative to next instruction
        mos6510_set_pc(cpu, new_pc);
    }
    else
    {
        mos6510_set_pc(cpu, pc + 2); // Skip to next instruction
    }
}

void BEQ(MOS6510 *cpu)
{
    uint16_t pc = mos6510_get_pc(cpu);
    if (get_flag_zero(cpu))
    {
        int8_t offset = (int8_t)cpu->memory[pc + 1]; // Get operand directly
        uint16_t new_pc = pc + 2 + offset;           // Offset relative to next instruction
        mos6510_set_pc(cpu, new_pc);
    }
    else
    {
        mos6510_set_pc(cpu, pc + 2); // Skip to next instruction
    }
}

void BMI(MOS6510 *cpu)
{
    uint16_t pc = mos6510_get_pc(cpu);
    if (get_flag_negative(cpu))
    {
        int8_t offset = (int8_t)cpu->memory[pc + 1]; // Get operand directly
        uint16_t new_pc = pc + 2 + offset;           // Offset relative to next instruction
        mos6510_set_pc(cpu, new_pc);
    }
    else
    {
        mos6510_set_pc(cpu, pc + 2); // Skip to next instruction
    }
}

void BNE(MOS6510 *cpu)
{
    uint16_t pc = mos6510_get_pc(cpu);
    if (!get_flag_zero(cpu))
    {
        int8_t offset = (int8_t)cpu->memory[pc + 1]; // Get operand directly
        uint16_t new_pc = pc + 2 + offset;           // Offset relative to next instruction
        mos6510_set_pc(cpu, new_pc);
    }
    else
    {
        mos6510_set_pc(cpu, pc + 2); // Skip to next instruction
    }
}

void BPL(MOS6510 *cpu)
{
    uint16_t pc = mos6510_get_pc(cpu);
    if (!get_flag_negative(cpu))
    {
        int8_t offset = (int8_t)cpu->memory[pc + 1]; // Get operand directly
        uint16_t new_pc = pc + 2 + offset;           // Offset relative to next instruction
        mos6510_set_pc(cpu, new_pc);
    }
    else
    {
        mos6510_set_pc(cpu, pc + 2); // Skip to next instruction
    }
}

void BVC(MOS6510 *cpu)
{
    uint16_t pc = mos6510_get_pc(cpu);
    if (!get_flag_overflow(cpu))
    {
        int8_t offset = (int8_t)cpu->memory[pc + 1]; // Get operand directly
        uint16_t new_pc = pc + 2 + offset;           // Offset relative to next instruction
        mos6510_set_pc(cpu, new_pc);
    }
    else
    {
        mos6510_set_pc(cpu, pc + 2); // Skip to next instruction
    }
}

void BVS(MOS6510 *cpu)
{
    uint16_t pc = mos6510_get_pc(cpu);
    if (get_flag_overflow(cpu))
    {
        int8_t offset = (int8_t)cpu->memory[pc + 1]; // Get operand directly
        uint16_t new_pc = pc + 2 + offset;           // Offset relative to next instruction
        mos6510_set_pc(cpu, new_pc);
    }
    else
    {
        mos6510_set_pc(cpu, pc + 2); // Skip to next instruction
    }
}

// Status Flag Operations
void CLC(MOS6510 *cpu)
{
    set_flag_carry(cpu, false);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void CLD(MOS6510 *cpu)
{
    set_flag_decimal(cpu, false);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void CLI(MOS6510 *cpu)
{
    set_flag_interrupt(cpu, false);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void CLV(MOS6510 *cpu)
{
    set_flag_overflow(cpu, false);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void SEC(MOS6510 *cpu)
{
    set_flag_carry(cpu, true);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void SED(MOS6510 *cpu)
{
    set_flag_decimal(cpu, true);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

void SEI(MOS6510 *cpu)
{
    set_flag_interrupt(cpu, true);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
}

// System Operations
void BRK(MOS6510 *cpu)
{
    mos6510_push(cpu, (mos6510_get_pc(cpu) >> 8) & 0xFF);
    mos6510_push(cpu, mos6510_get_pc(cpu) & 0xFF);
    mos6510_push(cpu, cpu->P | FLAG_BREAK);

    set_flag_interrupt(cpu, true);

    uint16_t addr = cpu->memory[0xFFFE] | (cpu->memory[0xFFFF] << 8);
    mos6510_set_pc(cpu, addr);
}

void RTI(MOS6510 *cpu)
{
    cpu->P = mos6510_pull(cpu) | FLAG_RESERVED;
    uint8_t low_byte = mos6510_pull(cpu);
    uint8_t high_byte = mos6510_pull(cpu);
    uint16_t addr = (high_byte << 8) | low_byte;
    mos6510_set_pc(cpu, addr);
}

void NOP(MOS6510 *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    if (op->mode != ADDR_IMP /*&& op->mode != ADDR_IMM*/)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}