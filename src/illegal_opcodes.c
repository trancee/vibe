#include "cpu.h"
#include "mos6510.h"

// Illegal opcodes implementation
void ANC(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    cpu->A &= fetch_operand(cpu, instruction->mode);
    set_flag_carry(cpu, cpu->A & 0x80);
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void ANE(CPU *cpu)
{
    // ANE / XAA opcode $8B
    // Per 64doc.txt: A = (A | #$EE) & X & #byte
    // The #$EE constant is the most common value on C64 (6510)
    // This is an unstable opcode - behavior varies by chip revision
    const instruction_t *instruction = fetch_instruction(cpu);
    uint8_t imm = fetch_operand(cpu, instruction->mode);
    cpu->A = (cpu->A | 0xEE) & cpu->X & imm;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

/*
Binary mode (D flag clear):

1. A = A AND immediate
2. A = (A >> 1) | (C << 7) (ROR with carry)
3. N = bit 7 of result
4. Z = (result == 0)
5. C = bit 6 of result
6. V = bit 6 XOR bit 5 of result

Decimal mode (D flag set):

1. Perform AND and ROR as in binary mode
2. N = initial C flag
3. Z = (ROR result == 0)
4. V = (bit 6 of AND result) XOR (bit 6 of ROR result)
5. If (low nybble of AND + (low nybble & 1)) > 5, add 6 to low nybble of result
6. If (high nybble of AND + (high nybble & 1)) > 5, add 6 to high nybble and set C, else clear C
*/
void ARR(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint8_t value = fetch_operand(cpu, instruction->mode);
    uint8_t and_result = cpu->A & value;
    bool old_carry = get_flag_carry(cpu);

    // Perform ROR on the AND result
    uint8_t result = (and_result >> 1) | (old_carry << 7);
    cpu->A = result;

    if (get_flag_decimal(cpu))
    {
        // Decimal mode (per 64doc.txt)
        // N = initial C flag
        set_flag_negative(cpu, old_carry);
        // Z = (ROR result == 0)
        set_flag_zero(cpu, result == 0);
        // V = bit 6 changed between AND and ROR result
        set_flag_overflow(cpu, (and_result ^ result) & 0x40);

        uint8_t al = and_result & 0x0F; // low nybble of AND result
        uint8_t ah = and_result >> 4;   // high nybble of AND result

        // BCD fixup for low nybble: if (AL + (AL & 1)) > 5
        if (al + (al & 1) > 5)
        {
            cpu->A = (cpu->A & 0xF0) | ((cpu->A + 6) & 0x0F);
        }

        // BCD fixup for high nybble and set carry: if (AH + (AH & 1)) > 5
        if (ah + (ah & 1) > 5)
        {
            cpu->A = (cpu->A + 0x60) & 0xFF;
            set_flag_carry(cpu, true);
        }
        else
        {
            set_flag_carry(cpu, false);
        }
    }
    else
    {
        // Binary mode (per 64doc.txt)
        set_flag_negative(cpu, result & 0x80);
        set_flag_zero(cpu, result == 0);
        // C = bit 6 of result
        set_flag_carry(cpu, result & 0x40);
        // V = bit 6 XOR bit 5 of result
        set_flag_overflow(cpu, ((result >> 6) ^ (result >> 5)) & 0x01);
    }

    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void ASR(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    // A = (A & #{imm}) / 2
    cpu->A &= fetch_operand(cpu, instruction->mode);
    set_flag_carry(cpu, cpu->A & 0x01); // The carry flag has the value of accumulator's bit 0 if bit 0 of the mask is 1 or cleared otherwise.
    cpu->A = cpu->A >> 1;
    set_flag_negative(cpu, 0);       // The negative flag is always cleared.
    set_flag_zero(cpu, cpu->A == 0); // The zero flag is set if the result is zero, or cleared if it is non-zero.
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
}

void DCP(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);
    cpu_write_byte(cpu, addr, cpu_read_byte(cpu, addr) - 1);
    uint8_t result = cpu->A - cpu_read_byte(cpu, addr);
    set_flag_carry(cpu, cpu->A >= cpu_read_byte(cpu, addr));
    set_flag_negative(cpu, result & 0x80);
    set_flag_zero(cpu, result == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

// https://www.masswerk.at/6502/6502_instruction_set.html#ISC
void ISB(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);
    cpu_write_byte(cpu, addr, cpu_read_byte(cpu, addr) + 1);
    cpu->A = subtract_with_borrow(cpu, cpu->A, cpu_read_byte(cpu, addr));
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void LAS(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);
    uint8_t value = cpu_read_byte(cpu, addr);
    cpu->A = cpu->X = cpu->SP = value & cpu->SP;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 3);
}

void LAX(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint8_t value = fetch_operand(cpu, instruction->mode);
    cpu->A = cpu->X = value;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void LXA(CPU *cpu)
{
    // LXA opcode $AB (also known as LAX immediate, OAL, ATX)
    // Per 64doc.txt: A = X = (A | #$EE) & #byte
    // This is an unstable opcode similar to ANE but loads both A and X
    // The #$EE constant is the most common value on C64 (6510)
    const instruction_t *instruction = fetch_instruction(cpu);
    uint8_t imm = fetch_operand(cpu, instruction->mode);
    uint8_t result = (cpu->A | 0xEE) & imm;
    cpu->A = cpu->X = result;
    set_flag_negative(cpu, result & 0x80);
    set_flag_zero(cpu, result == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
}

void RLA(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);
    bool carry_in = get_flag_carry(cpu);
    uint8_t value = cpu_read_byte(cpu, addr);
    set_flag_carry(cpu, value & 0x80);
    value = (value << 1) | carry_in;
    cpu_write_byte(cpu, addr, value);
    cpu->A &= value;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void RRA(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);
    bool carry_in = get_flag_carry(cpu);
    uint8_t value = cpu_read_byte(cpu, addr);
    set_flag_carry(cpu, value & 0x01);
    value = (value >> 1) | (carry_in << 7);
    cpu_write_byte(cpu, addr, value);
    cpu->A = add_with_carry(cpu, cpu->A, value);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void SAX(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);
    cpu_write_byte(cpu, addr, cpu->A & cpu->X);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void SBX(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint8_t value = fetch_operand(cpu, instruction->mode);

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
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

/*
SHA (AHX, AXA)
Stores A AND X AND (high-byte of addr. + 1) at addr.

unstable: sometimes 'AND (H+1)' is dropped, page boundary crossings may not work (with the high-byte of the value used as the high-byte of the address)

A AND X AND (H+1) -> M

N	Z	C	I	D	V
-	-	-	-	-	-

addressing    assembler   	opc  bytes	cycles
----------------------------------------------------
absolute,Y    SHA oper,Y  	9F 	   3 	   5   †
(indirect),Y  SHA (oper),Y	93	   2       6   †
*/
void SHA(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);

    uint8_t value = cpu->A & cpu->X;

    if (page_boundary((addr - cpu->Y), addr))
    {
        value &= addr >> 8;
        addr = value << 8 | (addr & 0x00FF);
    }
    else
    {
        value &= (addr >> 8) + 1;
    }

    cpu_write_byte(cpu, addr, value);

    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (instruction->mode == AbsoluteY)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

/*
TAS (XAS, SHS)
Puts A AND X in SP and stores A AND X AND (high-byte of addr. + 1) at addr.

unstable: sometimes 'AND (H+1)' is dropped, page boundary crossings may not work (with the high-byte of the value used as the high-byte of the address)

A AND X -> SP, A AND X AND (H+1) -> M

N	Z	C	I	D	V
-	-	-	-	-	-

addressing	  assembler	    opc	 bytes	cycles
----------------------------------------------------
absolute,Y	  TAS oper,Y    9B	   3	   5   †
*/
void SHS(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);

    cpu->SP = cpu->A & cpu->X;

    uint8_t value = cpu->SP;

    if (page_boundary((addr - cpu->Y), addr))
    {
        // Page crossing: value = X & effective_addr_hi
        // And effective address high byte becomes value
        value &= addr >> 8;
        addr = value << 8 | (addr & 0x00FF);
    }
    else
    {
        // No page crossing: value = X & (base_addr_hi + 1)
        value &= (addr >> 8) + 1;
    }

    cpu_write_byte(cpu, addr, value);

    cpu_set_pc(cpu, cpu_get_pc(cpu) + 3);
}

/*
SHX (SXA, XAS)
Stores X AND (high-byte of addr. + 1) at addr.

unstable: sometimes 'AND (H+1)' is dropped, page boundary crossings may not work

X AND (H+1) -> M

N	Z	C	I	D	V
-	-	-	-	-	-

addressing    assembler   	opc  bytes	cycles
----------------------------------------------------
absolute,Y    SHX oper,Y  	9E 	   3 	   5   †
*/
void SHX(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);

    uint8_t value = cpu->X;

    if (page_boundary((addr - cpu->Y), addr))
    {
        // Page crossing: value = X & effective_addr_hi
        // And effective address high byte becomes value
        value &= addr >> 8;
        addr = value << 8 | (addr & 0x00FF);
    }
    else
    {
        // No page crossing: value = X & (base_addr_hi + 1)
        value &= (addr >> 8) + 1;
    }

    cpu_write_byte(cpu, addr, value);

    cpu_set_pc(cpu, cpu_get_pc(cpu) + 3);
}

/*
SHY (SYA, SAY)
Stores Y AND (high-byte of addr. + 1) at addr.

unstable: sometimes 'AND (H+1)' is dropped, page boundary crossings may not work

Y AND (H+1) -> M

N	Z	C	I	D	V
-	-	-	-	-	-

addressing    assembler   	opc  bytes	cycles
----------------------------------------------------
absolute,X    SHY oper,X  	9C 	   3 	   5   †
*/
void SHY(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);

    uint8_t value = cpu->Y;

    if (page_boundary((addr - cpu->X), addr))
    {
        // Page crossing: value = Y & effective_addr_hi
        // And effective address high byte becomes value
        value &= addr >> 8;
        addr = value << 8 | (addr & 0x00FF);
    }
    else
    {
        // No page crossing: value = Y & (base_addr_hi + 1)
        value &= (addr >> 8) + 1;
    }

    cpu_write_byte(cpu, addr, value);

    cpu_set_pc(cpu, cpu_get_pc(cpu) + 3);
}

void SLO(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);
    uint8_t value = cpu_read_byte(cpu, addr);
    set_flag_carry(cpu, value & 0x80);
    value <<= 1;
    cpu_write_byte(cpu, addr, value);
    cpu->A |= value;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}

void SRE(CPU *cpu)
{
    const instruction_t *instruction = fetch_instruction(cpu);
    uint16_t addr = fetch_address(cpu, instruction->mode);
    uint8_t value = cpu_read_byte(cpu, addr);
    set_flag_carry(cpu, value & 0x01);
    value >>= 1;
    cpu_write_byte(cpu, addr, value);
    cpu->A ^= value;
    set_flag_negative(cpu, cpu->A & 0x80);
    set_flag_zero(cpu, cpu->A == 0);
    cpu_set_pc(cpu, cpu_get_pc(cpu) + 2);
    if (instruction->mode == Absolute || instruction->mode == AbsoluteX || instruction->mode == AbsoluteY || instruction->mode == Indirect)
    {
        cpu_set_pc(cpu, cpu_get_pc(cpu) + 1);
    }
}
