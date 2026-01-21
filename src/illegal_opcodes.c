#include "mos6510.h"

// Illegal opcodes implementation
void ANC(CPU *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[cpu_get_pc(cpu)]];
    cpu->A &= fetch_operand(cpu, op->mode);
    set_flag_carry(cpu, cpu->A & 0x80);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void ALR(CPU *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[cpu_get_pc(cpu)]];
    // A = (A & #{imm}) / 2
    cpu->A &= fetch_operand(cpu, op->mode);
    set_flag_carry(cpu, cpu->A & 0x01); // The carry flag has the value of accumulator's bit 0 if bit 0 of the mask is 1 or cleared otherwise.
    cpu->A = cpu->A >> 1;
    set_flag_negative(cpu, 0);       // The negative flag is always cleared.
    set_flag_zero(cpu, cpu->A == 0); // The zero flag is set if the result is zero, or cleared if it is non-zero.
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
}

void ARR(CPU *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[cpu_get_pc(cpu)]];
    uint8_t value = fetch_operand(cpu, op->mode);
    cpu->A = (cpu->A & value) >> 1 | (get_flag_carry(cpu) << 7);
    set_flag_carry(cpu, cpu->A & 0x40);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    if (get_flag_decimal(cpu))
    {
        uint16_t temp = cpu->A;
        temp = (temp & 0x0F) + (temp >> 4) + ((temp & 0x10) >> 4);
        set_flag_carry(cpu, temp > 0x0F);
        set_flag_zero(cpu, (temp & 0x0F) == 0);
        temp = (temp & 0x0F) | ((temp & 0xF0) << 4);
        cpu->A = temp & 0xFF;
        set_flag_negative(cpu, cpu->A & 0x80);
    }
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void DCP(CPU *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[cpu_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    cpu->memory[addr]--;
    uint8_t result = cpu->A - cpu->memory[addr];
    set_flag_carry(cpu, cpu->A >= cpu->memory[addr]);
    set_flag_negative(cpu, result & 0x80);
    set_flag_zero(cpu, result == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

// https://www.masswerk.at/6502/6502_instruction_set.html#ISC
void ISC(CPU *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[cpu_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    cpu->memory[addr]++;
    cpu->A = subtract_with_borrow(cpu, cpu->A, cpu->memory[addr]);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void LAS(CPU *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[cpu_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    uint8_t value = cpu->memory[addr];
    cpu->A = cpu->X = cpu->SP = value & cpu->SP;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 3);
}

void LAX(CPU *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[cpu_get_pc(cpu)]];
    uint8_t value = fetch_operand(cpu, op->mode);
    cpu->A = cpu->X = value;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void RLA(CPU *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[cpu_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    bool carry_in = get_flag_carry(cpu);
    uint8_t value = cpu->memory[addr];
    set_flag_carry(cpu, value & 0x80);
    value = (value << 1) | carry_in;
    cpu->memory[addr] = value;
    cpu->A &= value;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void RRA(CPU *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[cpu_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    bool carry_in = get_flag_carry(cpu);
    uint8_t value = cpu->memory[addr];
    set_flag_carry(cpu, value & 0x01);
    value = (value >> 1) | (carry_in << 7);
    cpu->memory[addr] = value;
    cpu->A = add_with_carry(cpu, cpu->A, value);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void SAX(CPU *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[cpu_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    cpu->memory[addr] = cpu->A & cpu->X;
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void SLO(CPU *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[cpu_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    uint8_t value = cpu->memory[addr];
    set_flag_carry(cpu, value & 0x80);
    value <<= 1;
    cpu->memory[addr] = value;
    cpu->A |= value;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void SRE(CPU *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[cpu_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    uint8_t value = cpu->memory[addr];
    set_flag_carry(cpu, value & 0x01);
    value >>= 1;
    cpu->memory[addr] = value;
    cpu->A ^= value;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void TAS(CPU *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[cpu_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    cpu->SP = cpu->A & cpu->X;
    cpu->memory[addr & 0xFF00] = cpu->SP & ((addr >> 8) + 1);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 3);
}

void XAA(CPU *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[cpu_get_pc(cpu)]];
    cpu->A &= cpu->X & fetch_operand(cpu, op->mode);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void SBX(CPU *cpu)
{
    const opcode_t *op = &opcode_table[cpu->memory[cpu_get_pc(cpu)]];
    uint8_t value = fetch_operand(cpu, op->mode);

    uint8_t ax = cpu->A & cpu->X;
    uint8_t result = ax - value;

    /* Carry is set if (A & X) >= value */
    set_flag_carry(cpu, ax >= value);
    /* V flag is typically unaffected for SBX */
    set_flag_zero(cpu, result == 0);
    set_flag_negative(cpu, result & 0x80);

    cpu->X = result;

    /* Advance PC: immediate/zero-page/absolute sizing handled elsewhere by opcode table patterns */
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}