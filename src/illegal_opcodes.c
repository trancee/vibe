#include "../include/mos6510.h"

// Illegal opcodes implementation
void ANC(MOS6510 *cpu) {
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    cpu->A &= fetch_operand(cpu, op->mode);
    set_flag_carry(cpu, cpu->A & 0x80);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void ARR(MOS6510 *cpu) {
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint8_t value = fetch_operand(cpu, op->mode);
    cpu->A = (cpu->A & value) >> 1 | (get_flag_carry(cpu) << 7);
    set_flag_carry(cpu, cpu->A & 0x40);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    if (get_flag_decimal(cpu)) {
        uint16_t temp = cpu->A;
        temp = (temp & 0x0F) + (temp >> 4) + ((temp & 0x10) >> 4);
        set_flag_carry(cpu, temp > 0x0F);
        set_flag_zero(cpu, (temp & 0x0F) == 0);
        temp = (temp & 0x0F) | ((temp & 0xF0) << 4);
        cpu->A = temp & 0xFF;
        set_flag_negative(cpu, cpu->A & 0x80);
    }
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void DCP(MOS6510 *cpu) {
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    cpu->memory[addr]--;
    uint8_t result = cpu->A - cpu->memory[addr];
    set_flag_carry(cpu, cpu->A >= cpu->memory[addr]);
    set_flag_negative(cpu, result & 0x80);
    set_flag_zero(cpu, result == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

// https://www.masswerk.at/6502/6502_instruction_set.html#ISC
void ISC(MOS6510 *cpu) {
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    cpu->memory[addr]++;
    cpu->A = subtract_with_borrow(cpu, cpu->A, cpu->memory[addr]);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void LAS(MOS6510 *cpu) {
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    uint8_t value = cpu->memory[addr];
    cpu->A = cpu->X = cpu->SP = value & cpu->SP;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 3);
}

void LAX(MOS6510 *cpu) {
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint8_t value = fetch_operand(cpu, op->mode);
    cpu->A = cpu->X = value;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void RLA(MOS6510 *cpu) {
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    bool carry_in = get_flag_carry(cpu);
    uint8_t value = cpu->memory[addr];
    set_flag_carry(cpu, value & 0x80);
    value = (value << 1) | carry_in;
    cpu->memory[addr] = value;
    cpu->A &= value;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void RRA(MOS6510 *cpu) {
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    bool carry_in = get_flag_carry(cpu);
    uint8_t value = cpu->memory[addr];
    set_flag_carry(cpu, value & 0x01);
    value = (value >> 1) | (carry_in << 7);
    cpu->memory[addr] = value;
    cpu->A = add_with_carry(cpu, cpu->A, value);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void SAX(MOS6510 *cpu) {
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    cpu->memory[addr] = cpu->A & cpu->X;
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void SLO(MOS6510 *cpu) {
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    uint8_t value = cpu->memory[addr];
    set_flag_carry(cpu, value & 0x80);
    value <<= 1;
    cpu->memory[addr] = value;
    cpu->A |= value;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void SRE(MOS6510 *cpu) {
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    uint8_t value = cpu->memory[addr];
    set_flag_carry(cpu, value & 0x01);
    value >>= 1;
    cpu->memory[addr] = value;
    cpu->A ^= value;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}

void TAS(MOS6510 *cpu) {
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    uint16_t addr = get_operand_address(cpu, op->mode);
    cpu->SP = cpu->A & cpu->X;
    cpu->memory[addr & 0xFF00] = cpu->SP & ((addr >> 8) + 1);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 3);
}

void XAA(MOS6510 *cpu) {
    const opcode_t *op = &opcode_table[cpu->memory[mos6510_get_pc(cpu)]];
    cpu->A &= cpu->X & fetch_operand(cpu, op->mode);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 2);
    if (op->mode == ADDR_ABS || op->mode == ADDR_ABSX || op->mode == ADDR_ABSY || op->mode == ADDR_IND)
    {
        mos6510_set_pc(cpu, mos6510_get_pc(cpu) + 1);
    }
}