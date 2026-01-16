#ifndef MOS6510_H
#define MOS6510_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t A;      // Accumulator
    uint8_t X;      // X Index Register
    uint8_t Y;      // Y Index Register
    uint8_t SP;     // Stack Pointer
    uint8_t PC;     // Program Counter (low byte)
    uint8_t PCH;    // Program Counter (high byte)
    uint8_t P;      // Status Register
    uint8_t memory[65536];  // 64KB memory
} MOS6510;

// Status Register bit definitions
#define FLAG_CARRY    0x01
#define FLAG_ZERO     0x02
#define FLAG_INTERRUPT_DISABLE 0x04
#define FLAG_DECIMAL  0x08
#define FLAG_BREAK    0x10
#define FLAG_RESERVED 0x20
#define FLAG_OVERFLOW 0x40
#define FLAG_NEGATIVE 0x80

// Addressing modes
typedef enum {
    ADDR_IMP,  // Implied
    ADDR_IMM,  // Immediate
    ADDR_ZP,   // Zero Page
    ADDR_ZPX,  // Zero Page, X
    ADDR_ZPY,  // Zero Page, Y
    ADDR_ABS,  // Absolute
    ADDR_ABSX, // Absolute, X
    ADDR_ABSY, // Absolute, Y
    ADDR_IND,  // Indirect
    ADDR_INDX, // Indexed Indirect
    ADDR_INDY, // Indirect Indexed
    ADDR_REL   // Relative
} addressing_mode_t;

// Opcode structure
typedef struct {
    uint8_t opcode;
    char name[4];
    addressing_mode_t addr_mode;
    uint8_t cycles;
    void (*execute)(MOS6510 *cpu);
} opcode_t;

// CPU functions
void mos6510_init(MOS6510 *cpu);
void mos6510_reset(MOS6510 *cpu);
uint8_t mos6510_step(MOS6510 *cpu);
uint16_t mos6510_get_pc(MOS6510 *cpu);
void mos6510_set_pc(MOS6510 *cpu, uint16_t addr);
uint8_t mos6510_read_byte(MOS6510 *cpu, uint16_t addr);
void mos6510_write_byte(MOS6510 *cpu, uint16_t addr, uint8_t data);

// Flag operations
static inline bool get_flag_carry(MOS6510 *cpu)    { return cpu->P & FLAG_CARRY; }
static inline bool get_flag_zero(MOS6510 *cpu)     { return cpu->P & FLAG_ZERO; }
static inline bool get_flag_interrupt(MOS6510 *cpu) { return cpu->P & FLAG_INTERRUPT_DISABLE; }
static inline bool get_flag_decimal(MOS6510 *cpu)  { return cpu->P & FLAG_DECIMAL; }
static inline bool get_flag_break(MOS6510 *cpu)    { return cpu->P & FLAG_BREAK; }
static inline bool get_flag_overflow(MOS6510 *cpu) { return cpu->P & FLAG_OVERFLOW; }
static inline bool get_flag_negative(MOS6510 *cpu) { return cpu->P & FLAG_NEGATIVE; }

static inline void set_flag_carry(MOS6510 *cpu, bool set)    { if (set) cpu->P |= FLAG_CARRY; else cpu->P &= ~FLAG_CARRY; }
static inline void set_flag_zero(MOS6510 *cpu, bool set)     { if (set) cpu->P |= FLAG_ZERO; else cpu->P &= ~FLAG_ZERO; }
static inline void set_flag_interrupt(MOS6510 *cpu, bool set) { if (set) cpu->P |= FLAG_INTERRUPT_DISABLE; else cpu->P &= ~FLAG_INTERRUPT_DISABLE; }
static inline void set_flag_decimal(MOS6510 *cpu, bool set)  { if (set) cpu->P |= FLAG_DECIMAL; else cpu->P &= ~FLAG_DECIMAL; }
static inline void set_flag_break(MOS6510 *cpu, bool set)    { if (set) cpu->P |= FLAG_BREAK; else cpu->P &= ~FLAG_BREAK; }
static inline void set_flag_overflow(MOS6510 *cpu, bool set) { if (set) cpu->P |= FLAG_OVERFLOW; else cpu->P &= ~FLAG_OVERFLOW; }
static inline void set_flag_negative(MOS6510 *cpu, bool set) { if (set) cpu->P |= FLAG_NEGATIVE; else cpu->P &= ~FLAG_NEGATIVE; }

// Arithmetic operations
static inline uint8_t add_with_carry(MOS6510 *cpu, uint8_t a, uint8_t b) {
    uint16_t sum = a + b + get_flag_carry(cpu);
    set_flag_carry(cpu, sum > 0xFF);
    set_flag_overflow(cpu, ((a ^ b) & 0x80) && ((a ^ sum) & 0x80));
    set_flag_negative(cpu, sum & 0x80);
    set_flag_zero(cpu, (sum & 0xFF) == 0);
    return sum & 0xFF;
}

static inline uint8_t subtract_with_borrow(MOS6510 *cpu, uint8_t a, uint8_t b) {
    return add_with_carry(cpu, a, ~b);
}

// Stack operations
void mos6510_push(MOS6510 *cpu, uint8_t data);
uint8_t mos6510_pull(MOS6510 *cpu);

// Addressing mode helpers
void advance_pc(MOS6510 *cpu, addressing_mode_t mode);
uint16_t get_operand_address(MOS6510 *cpu, addressing_mode_t mode);
uint8_t fetch_operand(MOS6510 *cpu, addressing_mode_t mode);

// Opcode declarations
void ADC(MOS6510 *cpu);
void AND(MOS6510 *cpu);
void ASL(MOS6510 *cpu);
void BCC(MOS6510 *cpu);
void BCS(MOS6510 *cpu);
void BEQ(MOS6510 *cpu);
void BIT(MOS6510 *cpu);
void BMI(MOS6510 *cpu);
void BNE(MOS6510 *cpu);
void BPL(MOS6510 *cpu);
void BRK(MOS6510 *cpu);
void BVC(MOS6510 *cpu);
void BVS(MOS6510 *cpu);
void CLC(MOS6510 *cpu);
void CLD(MOS6510 *cpu);
void CLI(MOS6510 *cpu);
void CLV(MOS6510 *cpu);
void CMP(MOS6510 *cpu);
void CPX(MOS6510 *cpu);
void CPY(MOS6510 *cpu);
void DEC(MOS6510 *cpu);
void DEX(MOS6510 *cpu);
void DEY(MOS6510 *cpu);
void EOR(MOS6510 *cpu);
void INC(MOS6510 *cpu);
void INX(MOS6510 *cpu);
void INY(MOS6510 *cpu);
void JMP(MOS6510 *cpu);
void JSR(MOS6510 *cpu);
void LDA(MOS6510 *cpu);
void LDX(MOS6510 *cpu);
void LDY(MOS6510 *cpu);
void LSR(MOS6510 *cpu);
void NOP(MOS6510 *cpu);
void ORA(MOS6510 *cpu);
void PHA(MOS6510 *cpu);
void PHP(MOS6510 *cpu);
void PLA(MOS6510 *cpu);
void PLP(MOS6510 *cpu);
void ROL(MOS6510 *cpu);
void ROR(MOS6510 *cpu);
void RTI(MOS6510 *cpu);
void RTS(MOS6510 *cpu);
void SBC(MOS6510 *cpu);
void SEC(MOS6510 *cpu);
void SED(MOS6510 *cpu);
void SEI(MOS6510 *cpu);
void STA(MOS6510 *cpu);
void STX(MOS6510 *cpu);
void STY(MOS6510 *cpu);
void TAX(MOS6510 *cpu);
void TAY(MOS6510 *cpu);
void TSX(MOS6510 *cpu);
void TXA(MOS6510 *cpu);
void TXS(MOS6510 *cpu);
void TYA(MOS6510 *cpu);

// Illegal opcodes (supported for completeness)
void ANC(MOS6510 *cpu);
void ARR(MOS6510 *cpu);
void DCP(MOS6510 *cpu);
void ISC(MOS6510 *cpu);
void LAS(MOS6510 *cpu);
void LAX(MOS6510 *cpu);
void RLA(MOS6510 *cpu);
void RRA(MOS6510 *cpu);
void SAX(MOS6510 *cpu);
void SLO(MOS6510 *cpu);
void SRE(MOS6510 *cpu);
void TAS(MOS6510 *cpu);
void XAA(MOS6510 *cpu);

// Global opcode table declaration
extern const opcode_t opcode_table[256];

#endif // MOS6510_H