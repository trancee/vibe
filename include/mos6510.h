#ifndef MOS6510_H
#define MOS6510_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "cpu.h"

// Arithmetic operations
static inline uint8_t add_with_carry(CPU *cpu, uint8_t a, uint8_t b)
{
    uint8_t c = get_flag_carry(cpu);
    uint16_t sum = a + b + c;

    if (cpu->decimal_mode && get_flag_decimal(cpu))
    {
        uint8_t lo = (a & 0x0F) + (b & 0x0F) + c;
        uint16_t hi = (a & 0xF0) + (b & 0xF0);

        // BCD fixup for lower nibble.
        if (lo > 0x09)
            lo += 0x06;
        if (lo > 0x0F)
            hi += 0x10;

        // `Z` flag is not affected by decimal mode,
        // it will be set if the binary operation would become zero, regardless of the BCD result.
        set_flag_zero(cpu, (sum & 0xFF) == 0);

        // The `N` and `V` flags are set after fixing the lower nibble but before fixing the upper one.
        // They use the same logic as binary mode `ADC`.
        set_flag_negative(cpu, hi & 0x80);
        set_flag_overflow(cpu, (~(a ^ b) & (a ^ hi) & 0x80));

        // BCD fixup for higher nibble.
        if (hi > 0x90)
            hi += 0x60;

        // `C` flag works as a carry for multi-byte operations as expected.
        set_flag_carry(cpu, hi > 0xFF);

        return (hi & 0xF0) | (lo & 0x0F);
    }
    else
    {
        set_flag_carry(cpu, sum > 0xFF);
        set_flag_overflow(cpu, ~(a ^ b) & (a ^ sum) & 0x80);
        set_flag_negative(cpu, sum & 0x80);
        set_flag_zero(cpu, (sum & 0xFF) == 0);
        return sum & 0xFF;
    }
}

static inline uint8_t subtract_with_borrow(CPU *cpu, uint8_t a, uint8_t b)
{
    uint8_t c = get_flag_carry(cpu);
    uint16_t sum = a - b - (1 - c);

    set_flag_carry(cpu, a - b - (1 - c) >= 0);
    set_flag_overflow(cpu, (a ^ b) & (a ^ sum) & 0x80);
    set_flag_negative(cpu, sum & 0x80);
    set_flag_zero(cpu, (sum & 0xFF) == 0);

    if (cpu->decimal_mode && get_flag_decimal(cpu))
    {
        uint8_t lo = (a & 0x0F) - (b & 0x0F) - (1 - c);
        uint16_t hi = (a & 0xF0) - (b & 0xF0);

        if (lo & 0x10)
        {
            lo -= 0x06;
            hi -= 0x10;
        }

        if (hi > 0xFF)
            hi -= 0x60;

        return (hi & 0xF0) | (lo & 0x0F);
    }
    else
        return sum & 0xFF;
}

// Opcode declarations
void ADC(CPU *cpu);
void AND(CPU *cpu);
void ASL(CPU *cpu);
void BCC(CPU *cpu);
void BCS(CPU *cpu);
void BEQ(CPU *cpu);
void BIT(CPU *cpu);
void BMI(CPU *cpu);
void BNE(CPU *cpu);
void BPL(CPU *cpu);
void BRK(CPU *cpu);
void BVC(CPU *cpu);
void BVS(CPU *cpu);
void CLC(CPU *cpu);
void CLD(CPU *cpu);
void CLI(CPU *cpu);
void CLV(CPU *cpu);
void CMP(CPU *cpu);
void CPX(CPU *cpu);
void CPY(CPU *cpu);
void DEC(CPU *cpu);
void DEX(CPU *cpu);
void DEY(CPU *cpu);
void EOR(CPU *cpu);
void INC(CPU *cpu);
void INX(CPU *cpu);
void INY(CPU *cpu);
void JMP(CPU *cpu);
void JSR(CPU *cpu);
void LDA(CPU *cpu);
void LDX(CPU *cpu);
void LDY(CPU *cpu);
void LSR(CPU *cpu);
void NOP(CPU *cpu);
void ORA(CPU *cpu);
void PHA(CPU *cpu);
void PHP(CPU *cpu);
void PLA(CPU *cpu);
void PLP(CPU *cpu);
void ROL(CPU *cpu);
void ROR(CPU *cpu);
void RTI(CPU *cpu);
void RTS(CPU *cpu);
void SBC(CPU *cpu);
void SEC(CPU *cpu);
void SED(CPU *cpu);
void SEI(CPU *cpu);
void STA(CPU *cpu);
void STX(CPU *cpu);
void STY(CPU *cpu);
void TAX(CPU *cpu);
void TAY(CPU *cpu);
void TSX(CPU *cpu);
void TXA(CPU *cpu);
void TXS(CPU *cpu);
void TYA(CPU *cpu);

// Illegal opcodes (supported for completeness)
void ANC(CPU *cpu);
void ANE(CPU *cpu);
void ARR(CPU *cpu);
void ASR(CPU *cpu);
void DCP(CPU *cpu);
void ISB(CPU *cpu);
void LAS(CPU *cpu);
void LAX(CPU *cpu);
void LXA(CPU *cpu);
void RLA(CPU *cpu);
void RRA(CPU *cpu);
void SAX(CPU *cpu);
void SBX(CPU *cpu);
void SHA(CPU *cpu);
void SHS(CPU *cpu);
void SHX(CPU *cpu);
void SHY(CPU *cpu);
void SLO(CPU *cpu);
void SRE(CPU *cpu);

extern const instruction_t instructions[256];

#endif // MOS6510_H