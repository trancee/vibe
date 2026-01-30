/*
 * C64 Emulator - MOS 6510 Specific Header
 * 
 * Defines 6510-specific features and all opcode declarations.
 * The 6510 is a 6502 with an integrated 8-bit I/O port at $00-$01.
 */

#ifndef MOS6510_H
#define MOS6510_H

#include "cpu.h"

/*
 * BCD Arithmetic Helpers
 * These implement the 6502's decimal mode behavior.
 */
static inline uint8_t add_with_carry(CPU *cpu, uint8_t a, uint8_t b)
{
    uint8_t c = get_flag_carry(cpu);
    uint16_t sum = a + b + c;

    if (cpu->decimal_mode && get_flag_decimal(cpu)) {
        /* BCD Addition */
        uint8_t lo = (a & 0x0F) + (b & 0x0F) + c;
        uint16_t hi = (a & 0xF0) + (b & 0xF0);

        if (lo > 0x09) lo += 0x06;
        if (lo > 0x0F) hi += 0x10;

        /* Z flag uses binary result */
        set_flag_zero(cpu, (sum & 0xFF) == 0);
        /* N and V flags set after low nibble fix, before high */
        set_flag_negative(cpu, hi & 0x80);
        set_flag_overflow(cpu, (~(a ^ b) & (a ^ hi) & 0x80));

        if (hi > 0x90) hi += 0x60;
        set_flag_carry(cpu, hi > 0xFF);

        return (hi & 0xF0) | (lo & 0x0F);
    } else {
        set_flag_carry(cpu, sum > 0xFF);
        set_flag_overflow(cpu, (~(a ^ b) & (a ^ sum) & 0x80));
        set_flag_negative(cpu, sum & 0x80);
        set_flag_zero(cpu, (sum & 0xFF) == 0);
        return sum & 0xFF;
    }
}

static inline uint8_t subtract_with_borrow(CPU *cpu, uint8_t a, uint8_t b)
{
    uint8_t c = get_flag_carry(cpu);
    uint16_t diff = a - b - (1 - c);

    /* Binary flags are set first (used even in decimal mode) */
    set_flag_carry(cpu, (int16_t)(a - b - (1 - c)) >= 0);
    set_flag_overflow(cpu, (a ^ b) & (a ^ diff) & 0x80);
    set_flag_negative(cpu, diff & 0x80);
    set_flag_zero(cpu, (diff & 0xFF) == 0);

    if (cpu->decimal_mode && get_flag_decimal(cpu)) {
        /* BCD Subtraction */
        uint8_t lo = (a & 0x0F) - (b & 0x0F) - (1 - c);
        uint16_t hi = (a & 0xF0) - (b & 0xF0);

        if (lo & 0x10) {
            lo -= 0x06;
            hi -= 0x10;
        }
        if (hi > 0xFF) hi -= 0x60;

        return (hi & 0xF0) | (lo & 0x0F);
    } else {
        return diff & 0xFF;
    }
}

/*
 * Legal Opcode Declarations
 */

/* Load/Store */
void LDA(CPU *cpu);
void LDX(CPU *cpu);
void LDY(CPU *cpu);
void STA(CPU *cpu);
void STX(CPU *cpu);
void STY(CPU *cpu);

/* Transfer */
void TAX(CPU *cpu);
void TAY(CPU *cpu);
void TXA(CPU *cpu);
void TYA(CPU *cpu);
void TSX(CPU *cpu);
void TXS(CPU *cpu);

/* Stack */
void PHA(CPU *cpu);
void PHP(CPU *cpu);
void PLA(CPU *cpu);
void PLP(CPU *cpu);

/* Logical */
void AND(CPU *cpu);
void ORA(CPU *cpu);
void EOR(CPU *cpu);
void BIT(CPU *cpu);

/* Arithmetic */
void ADC(CPU *cpu);
void SBC(CPU *cpu);
void CMP(CPU *cpu);
void CPX(CPU *cpu);
void CPY(CPU *cpu);

/* Increment/Decrement */
void INC(CPU *cpu);
void INX(CPU *cpu);
void INY(CPU *cpu);
void DEC(CPU *cpu);
void DEX(CPU *cpu);
void DEY(CPU *cpu);

/* Shift/Rotate */
void ASL(CPU *cpu);
void LSR(CPU *cpu);
void ROL(CPU *cpu);
void ROR(CPU *cpu);

/* Jump/Call */
void JMP(CPU *cpu);
void JSR(CPU *cpu);
void RTS(CPU *cpu);

/* Branch */
void BCC(CPU *cpu);
void BCS(CPU *cpu);
void BEQ(CPU *cpu);
void BMI(CPU *cpu);
void BNE(CPU *cpu);
void BPL(CPU *cpu);
void BVC(CPU *cpu);
void BVS(CPU *cpu);

/* Flag Operations */
void CLC(CPU *cpu);
void CLD(CPU *cpu);
void CLI(CPU *cpu);
void CLV(CPU *cpu);
void SEC(CPU *cpu);
void SED(CPU *cpu);
void SEI(CPU *cpu);

/* System */
void BRK(CPU *cpu);
void NOP(CPU *cpu);
void RTI(CPU *cpu);

/*
 * Illegal Opcode Declarations
 * These are "undocumented" opcodes that exist on the real 6502.
 */

/* Stable Illegals */
void LAX(CPU *cpu);  /* LDA + LDX */
void SAX(CPU *cpu);  /* Store A & X */
void DCP(CPU *cpu);  /* DEC + CMP */
void ISB(CPU *cpu);  /* INC + SBC */
void SLO(CPU *cpu);  /* ASL + ORA */
void RLA(CPU *cpu);  /* ROL + AND */
void SRE(CPU *cpu);  /* LSR + EOR */
void RRA(CPU *cpu);  /* ROR + ADC */

/* Combined Illegals */
void ANC(CPU *cpu);  /* AND + set C from bit 7 */
void ASR(CPU *cpu);  /* AND + LSR (also called ALR) */
void ARR(CPU *cpu);  /* AND + ROR with special flags */
void SBX(CPU *cpu);  /* (A & X) - imm -> X (also called AXS) */
void LAS(CPU *cpu);  /* LDA/TSX with SP AND */

/* Unstable Illegals */
void ANE(CPU *cpu);  /* A = (A | magic) & X & imm (also called XAA) */
void LXA(CPU *cpu);  /* A = X = (A | magic) & imm */
void SHA(CPU *cpu);  /* Store A & X & (addr_hi + 1) (also called AHX) */
void SHX(CPU *cpu);  /* Store X & (addr_hi + 1) */
void SHY(CPU *cpu);  /* Store Y & (addr_hi + 1) */
void SHS(CPU *cpu);  /* SP = A & X, store SP & (addr_hi + 1) (also called TAS) */

/* JAM/KIL - Halt CPU */
void JAM(CPU *cpu);

/*
 * Compatibility definitions for test suite (opcodes tests)
 */

/* Vector addresses */
#define NMI   NMI_VECTOR
#ifndef IRQ
#define IRQ   IRQ_VECTOR
#endif
#define RESET RESET_VECTOR

/* Flag aliases */
#define FLAG_INTERRUPT_DISABLE FLAG_INTERRUPT

/* Memory location aliases for stack inspection (for tests that don't include c64.h) */
#ifndef PCL
#define PCL 0x01FE   /* Low byte position after stack push */
#endif
#ifndef PCH
#define PCH 0x01FF   /* High byte position after stack push */
#endif

/* Helper functions for tests that don't include c64.h */
#ifndef cpu_write_data
static inline void cpu_write_data(CPU *cpu, uint16_t addr, uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        cpu_write(cpu, addr + i, data[i]);
    }
}
#endif

#ifndef cpu_write_byte
static inline void cpu_write_byte(CPU *cpu, uint16_t addr, uint8_t data)
{
    cpu_write(cpu, addr, data);
}
#endif

#ifndef cpu_read_byte
static inline uint8_t cpu_read_byte(CPU *cpu, uint16_t addr)
{
    return cpu_read(cpu, addr);
}
#endif

#ifndef cpu_reset_pc
#define cpu_reset_pc cpu_reset_at
#endif

#endif /* MOS6510_H */
