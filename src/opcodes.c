#include "cpu.h"
#include "mos6510.h"

// Load/Store Operations
void LDA(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    cpu->A = fetch_operand(cpu, instruction->mode);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void LDX(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    cpu->X = fetch_operand(cpu, instruction->mode);
    set_flag_negative(cpu, cpu->X & 0x80);
    set_flag_zero(cpu, cpu->X == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void LDY(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    cpu->Y = fetch_operand(cpu, instruction->mode);
    set_flag_negative(cpu, cpu->Y & 0x80);
    set_flag_zero(cpu, cpu->Y == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void STA(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);
    cpu->memory[addr] = cpu->A;
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void STX(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);
    cpu->memory[addr] = cpu->X;
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void STY(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);
    cpu->memory[addr] = cpu->Y;
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

// Transfer Operations
void TAX(CPU *cpu)
{
    cpu->X = cpu->A;
    set_flag_negative(cpu, cpu->X & 0x80);
    set_flag_zero(cpu, cpu->X == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void TAY(CPU *cpu)
{
    cpu->Y = cpu->A;
    set_flag_negative(cpu, cpu->Y & 0x80);
    set_flag_zero(cpu, cpu->Y == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void TXA(CPU *cpu)
{
    cpu->A = cpu->X;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void TYA(CPU *cpu)
{
    cpu->A = cpu->Y;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void TSX(CPU *cpu)
{
    cpu->X = cpu->SP;
    set_flag_negative(cpu, cpu->X & 0x80);
    set_flag_zero(cpu, cpu->X == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void TXS(CPU *cpu)
{
    cpu->SP = cpu->X;
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

// Stack Operations
void PHA(CPU *cpu)
{
    cpu_push(cpu, cpu->A);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void PHP(CPU *cpu)
{
    cpu_push(cpu, cpu->P | FLAG_BREAK);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void PLA(CPU *cpu)
{
    cpu->A = cpu_pull(cpu);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void PLP(CPU *cpu)
{
    // nestest requires that BREAK flag is cleared when pulling P
    cpu->P = (cpu_pull(cpu) & ~FLAG_BREAK) | FLAG_RESERVED;
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

// Logical Operations
void AND(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    cpu->A &= fetch_operand(cpu, instruction->mode);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void ORA(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    cpu->A |= fetch_operand(cpu, instruction->mode);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void EOR(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    cpu->A ^= fetch_operand(cpu, instruction->mode);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void BIT(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint8_t value = fetch_operand(cpu, instruction->mode);
    uint8_t result = cpu->A & value;
    set_flag_zero(cpu, result == 0);
    set_flag_negative(cpu, value & 0x80);
    set_flag_overflow(cpu, value & 0x40);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

// Arithmetic Operations
void ADC(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    cpu->A = add_with_carry(cpu, cpu->A, fetch_operand(cpu, instruction->mode));
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void SBC(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    cpu->A = subtract_with_borrow(cpu, cpu->A, fetch_operand(cpu, instruction->mode));
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void CMP(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint8_t value = fetch_operand(cpu, instruction->mode);
    uint8_t result = cpu->A - value;
    set_flag_carry(cpu, cpu->A >= value);
    set_flag_negative(cpu, result & 0x80);
    set_flag_zero(cpu, result == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void CPX(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint8_t value = fetch_operand(cpu, instruction->mode);
    uint8_t result = cpu->X - value;
    set_flag_carry(cpu, cpu->X >= value);
    set_flag_negative(cpu, result & 0x80);
    set_flag_zero(cpu, result == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void CPY(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint8_t value = fetch_operand(cpu, instruction->mode);
    uint8_t result = cpu->Y - value;
    set_flag_carry(cpu, cpu->Y >= value);
    set_flag_negative(cpu, result & 0x80);
    set_flag_zero(cpu, result == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

// Increment/Decrement Operations
void INC(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);
    cpu->memory[addr]++;
    set_flag_negative(cpu, cpu->memory[addr] & 0x80);
    set_flag_zero(cpu, cpu->memory[addr] == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void INX(CPU *cpu)
{
    cpu->X++;
    set_flag_negative(cpu, cpu->X & 0x80);
    set_flag_zero(cpu, cpu->X == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void INY(CPU *cpu)
{
    cpu->Y++;
    set_flag_negative(cpu, cpu->Y & 0x80);
    set_flag_zero(cpu, cpu->Y == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void DEC(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);
    cpu->memory[addr]--;
    set_flag_negative(cpu, cpu->memory[addr] & 0x80);
    set_flag_zero(cpu, cpu->memory[addr] == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void DEX(CPU *cpu)
{
    cpu->X--;
    set_flag_negative(cpu, cpu->X & 0x80);
    set_flag_zero(cpu, cpu->X == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void DEY(CPU *cpu)
{
    cpu->Y--;
    set_flag_negative(cpu, cpu->Y & 0x80);
    set_flag_zero(cpu, cpu->Y == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

// Shift Operations
void ASL(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint8_t value;
    uint16_t addr;

    if (instruction->mode == Accumulator)
    {
        value = cpu->A;
        set_flag_carry(cpu, value & 0x80);
        value <<= 1;
        cpu->A = value;
    }
    else
    {
        addr = fetch_address(cpu, instruction->mode);
        value = cpu->memory[addr];
        set_flag_carry(cpu, value & 0x80);
        value <<= 1;
        cpu->memory[addr] = value;
    }

    set_flag_negative(cpu, value & 0x80);
    set_flag_zero(cpu, value == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Accumulator /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void LSR(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint8_t value;
    uint16_t addr;

    if (instruction->mode == Accumulator)
    {
        value = cpu->A;
        set_flag_carry(cpu, value & 0x01);
        value >>= 1;
        cpu->A = value;
    }
    else
    {
        addr = fetch_address(cpu, instruction->mode);
        value = cpu->memory[addr];
        set_flag_carry(cpu, value & 0x01);
        value >>= 1;
        cpu->memory[addr] = value;
    }

    set_flag_negative(cpu, value & 0x80);
    set_flag_zero(cpu, value == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Accumulator /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void ROL(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint8_t value;
    uint16_t addr;
    bool carry_in = get_flag_carry(cpu);

    if (instruction->mode == Accumulator)
    {
        value = cpu->A;
        set_flag_carry(cpu, value & 0x80);
        value = (value << 1) | carry_in;
        cpu->A = value;
    }
    else
    {
        addr = fetch_address(cpu, instruction->mode);
        value = cpu->memory[addr];
        set_flag_carry(cpu, value & 0x80);
        value = (value << 1) | carry_in;
        cpu->memory[addr] = value;
    }

    set_flag_negative(cpu, value & 0x80);
    set_flag_zero(cpu, value == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Accumulator /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void ROR(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint8_t value;
    uint16_t addr;
    bool carry_in = get_flag_carry(cpu);

    if (instruction->mode == Accumulator)
    {
        value = cpu->A;
        set_flag_carry(cpu, value & 0x01);
        value = (value >> 1) | (carry_in << 7);
        cpu->A = value;
    }
    else
    {
        addr = fetch_address(cpu, instruction->mode);
        value = cpu->memory[addr];
        set_flag_carry(cpu, value & 0x01);
        value = (value >> 1) | (carry_in << 7);
        cpu->memory[addr] = value;
    }

    set_flag_negative(cpu, value & 0x80);
    set_flag_zero(cpu, value == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Accumulator /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

// Jump Operations
void JMP(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);
    cpu_set_pc(cpu, addr);
}

void JSR(CPU *cpu)
{
    uint16_t return_addr = cpu_get_pc(cpu) + 2;
    cpu_push(cpu, (return_addr >> 8) & 0xFF);
    cpu_push(cpu, return_addr & 0xFF);

    uint16_t addr = fetch_address(cpu, Absolute);
    cpu_set_pc(cpu, addr);
}

void RTS(CPU *cpu)
{
    uint8_t low_byte = cpu_pull(cpu);
    uint8_t high_byte = cpu_pull(cpu);
    uint16_t addr = (high_byte << 8) | low_byte;
    cpu_set_pc(cpu, addr + 1);
}

// Branch Operations
void BCC(CPU *cpu)
{
    uint16_t pc = cpu_get_pc(cpu);
    if (!get_flag_carry(cpu))
    {
        int8_t offset = (int8_t)cpu->memory[pc + 1]; // Get operand directly
        uint16_t new_pc = pc + 2 + offset;           // Offset relative to next instruction
        cpu_set_pc(cpu, new_pc);
    }
    else
    {
        cpu_set_pc(cpu, pc + 2); // Skip to next instruction
    }
}

void BCS(CPU *cpu)
{
    uint16_t pc = cpu_get_pc(cpu);
    if (get_flag_carry(cpu))
    {
        int8_t offset = (int8_t)cpu->memory[pc + 1]; // Get operand directly
        uint16_t new_pc = pc + 2 + offset;           // Offset relative to next instruction
        cpu_set_pc(cpu, new_pc);
    }
    else
    {
        cpu_set_pc(cpu, pc + 2); // Skip to next instruction
    }
}

void BEQ(CPU *cpu)
{
    uint16_t pc = cpu_get_pc(cpu);
    if (get_flag_zero(cpu))
    {
        int8_t offset = (int8_t)cpu->memory[pc + 1]; // Get operand directly
        uint16_t new_pc = pc + 2 + offset;           // Offset relative to next instruction
        cpu_set_pc(cpu, new_pc);
    }
    else
    {
        cpu_set_pc(cpu, pc + 2); // Skip to next instruction
    }
}

void BMI(CPU *cpu)
{
    uint16_t pc = cpu_get_pc(cpu);
    if (get_flag_negative(cpu))
    {
        int8_t offset = (int8_t)cpu->memory[pc + 1]; // Get operand directly
        uint16_t new_pc = pc + 2 + offset;           // Offset relative to next instruction
        cpu_set_pc(cpu, new_pc);
    }
    else
    {
        cpu_set_pc(cpu, pc + 2); // Skip to next instruction
    }
}

void BNE(CPU *cpu)
{
    uint16_t pc = cpu_get_pc(cpu);
    if (!get_flag_zero(cpu))
    {
        int8_t offset = (int8_t)cpu->memory[pc + 1]; // Get operand directly
        uint16_t new_pc = pc + 2 + offset;           // Offset relative to next instruction
        cpu_set_pc(cpu, new_pc);
    }
    else
    {
        cpu_set_pc(cpu, pc + 2); // Skip to next instruction
    }
}

void BPL(CPU *cpu)
{
    uint16_t pc = cpu_get_pc(cpu);
    if (!get_flag_negative(cpu))
    {
        int8_t offset = (int8_t)cpu->memory[pc + 1]; // Get operand directly
        uint16_t new_pc = pc + 2 + offset;           // Offset relative to next instruction
        cpu_set_pc(cpu, new_pc);
    }
    else
    {
        cpu_set_pc(cpu, pc + 2); // Skip to next instruction
    }
}

void BVC(CPU *cpu)
{
    uint16_t pc = cpu_get_pc(cpu);
    if (!get_flag_overflow(cpu))
    {
        int8_t offset = (int8_t)cpu->memory[pc + 1]; // Get operand directly
        uint16_t new_pc = pc + 2 + offset;           // Offset relative to next instruction
        cpu_set_pc(cpu, new_pc);
    }
    else
    {
        cpu_set_pc(cpu, pc + 2); // Skip to next instruction
    }
}

void BVS(CPU *cpu)
{
    uint16_t pc = cpu_get_pc(cpu);
    if (get_flag_overflow(cpu))
    {
        int8_t offset = (int8_t)cpu->memory[pc + 1]; // Get operand directly
        uint16_t new_pc = pc + 2 + offset;           // Offset relative to next instruction
        cpu_set_pc(cpu, new_pc);
    }
    else
    {
        cpu_set_pc(cpu, pc + 2); // Skip to next instruction
    }
}

// Status Flag Operations
void CLC(CPU *cpu)
{
    set_flag_carry(cpu, false);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void CLD(CPU *cpu)
{
    set_flag_decimal(cpu, false);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void CLI(CPU *cpu)
{
    set_flag_interrupt(cpu, false);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void CLV(CPU *cpu)
{
    set_flag_overflow(cpu, false);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void SEC(CPU *cpu)
{
    set_flag_carry(cpu, true);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void SED(CPU *cpu)
{
    set_flag_decimal(cpu, true);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

void SEI(CPU *cpu)
{
    set_flag_interrupt(cpu, true);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
}

// System Operations
void BRK(CPU *cpu)
{
    cpu_push16(cpu, cpu_get_pc(cpu) + 2);
    cpu_push(cpu, cpu->P | FLAG_RESERVED | FLAG_BREAK);

    set_flag_interrupt(cpu, true);

    uint16_t addr = cpu_read_word(cpu, IRQ);
    cpu_set_pc(cpu, addr);
}

void RTI(CPU *cpu)
{
    cpu->P = (cpu_pull(cpu) & ~FLAG_BREAK) | FLAG_RESERVED;

    uint16_t addr = cpu_pull16(cpu);
    cpu_set_pc(cpu, addr);
}

void NOP(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    if (instruction->mode != Implied /*&& instruction->mode != Immediate*/)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}