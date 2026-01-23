#ifndef CPU_H
#define CPU_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define page_boundary(a, b) ((a & 0xFF00) != (b & 0xFF00))

typedef uint8_t (*read_t)(void *ptr, uint16_t addr);
typedef void (*write_t)(void *ptr, uint16_t addr, uint8_t data);

typedef struct
{
    uint8_t A;  // Accumulator
    uint8_t X;  // X Index Register
    uint8_t Y;  // Y Index Register
    uint8_t SP; // Stack Pointer
    uint8_t P;  // Status Register
    //
    uint16_t pc; // Program Counter
    //
    uint8_t memory[65536]; // 64KB memory
    //
    bool nmi;
    bool irq;
    bool has_interrupts;
    //
    bool debug;
    //
    read_t read;
    write_t write;
} CPU;

#define PCL 0x01FE
#define PCH 0x01FF

#define NMI 0xFFFA   // WORD
#define RESET 0xFFFC // WORD
#define IRQ 0xFFFE   // WORD

// Status Register bit definitions
#define FLAG_CARRY 0x01
#define FLAG_ZERO 0x02
#define FLAG_INTERRUPT_DISABLE 0x04
#define FLAG_DECIMAL 0x08
#define FLAG_BREAK 0x10
#define FLAG_RESERVED 0x20
#define FLAG_OVERFLOW 0x40
#define FLAG_NEGATIVE 0x80

typedef enum
{
    /// Absolute $nnnn
    /// In absolute addressing, the second byte of the instruction specifies the eight low order bits of the effective address while the third byte specifies the eight high order bits.
    /// Thus, the absolute addressing mode allows access to the entire 65K bytes of addressable memory.
    Absolute,        // 0
                     /// X-Indexed Absolute $nnnn,X
                     /// This form of addressing is used in conjunction with the `X` index register.
                     /// The effective address is formed by adding the contents of `X` to the address contained in the second and third bytes of the instruction.
                     /// This mode allows the index register to contain the index or count value and the instruction to contain the base address.
                     /// This type of indexing allows any location referencing and the index to modify multiple fields resulting in reduced coding and execution time.
    AbsoluteX,       // 1
                     /// Y-Indexed Absolute $nnnn,Y
                     /// This form of addressing is used in conjunction with the `Y` index register.
                     /// The effective address is formed by adding the contents of `Y` to the address contained in the second and third bytes of the instruction.
                     /// This mode allows the index register to contain the index or count value and the instruction to contain the base address.
                     /// This type of indexing allows any location referencing and the index to modify multiple fields resulting in reduced coding and execution time.
    AbsoluteY,       // 2
    Accumulator,     // 3
    Immediate,       // 4
    Implied,         // 5
                     /// X-Indexed Zero Page Indirect ($nn,X)
                     /// In indexed indirect addressing, the second byte of the instruction is added to the contents of the `X` index register, discarding the carry.
                     /// The result of this addition points to a memory location on page zero whose contents is the low order eight bits of the effective address.
                     /// The next memory location in page zero contains the high order eight bits of the effective address.
                     /// Both memory locations specifying the high and low order bytes of the effective address must be in page zero.
    IndexedIndirect, // 6
    Indirect,        // 7
                     /// Zero Page Indirect Y-Indexed($nn),Y
                     /// In indirect indexed addressing, the second byte of the instruction points to a memory location in page zero.
                     /// The contents of this memory location is added to the contents of the `Y` index register, the result being the low order eight bits of the effective address.
                     /// The carry from this addition is added to the contents of the next page zero memory location, the result being the high order eight bits of the effective address.
    IndirectIndexed, // 8
    Relative,        // 9
                     /// Zero Page $nn
                     /// The zero page instructions allow for shorter code and execution times by only fetching the second byte of the instruction and assuming a zero high address byte.
                     /// Careful use of the zero page can result in significant increase in code efficiency.
    ZeroPage,        // 10
                     /// X-Indexed Zero Page  $nn,X
                     /// This form of addressing is used in conjunction with the `X` index register.
                     /// The effective address is calculated by adding the second byte to the contents of the index register.
                     /// Since this is a form of "Zero Page" addressing, the content of the second byte references a location in page zero.
                     /// Additionally, due to the "Zero Page" addressing nature of this mode, no carry is added to the high order 8 bits of memory and crossing of page boundaries does not occur.
    ZeroPageX,       // 11
                     /// Y-Indexed Zero Page  $nn,Y
                     /// This form of addressing is used in conjunction with the `Y` index register.
                     /// The effective address is calculated by adding the second byte to the contents of the index register.
                     /// Since this is a form of "Zero Page" addressing, the content of the second byte references a location in page zero.
                     /// Additionally, due to the "Zero Page" addressing nature of this mode, no carry is added to the high order 8 bits of memory and crossing of page boundaries does not occur.
    ZeroPageY,       // 12
} addr_mode_t;

typedef struct
{
    uint8_t opcode;
    char name[4];
    void (*execute)(CPU *cpu);
    addr_mode_t mode;
    uint8_t size;
    uint8_t cycles;
    bool illegal;
} instruction_t;

typedef void (*handler_t)(CPU *);

typedef struct
{
    uint16_t address;
    handler_t handler;
} trap_t;

// CPU functions
void cpu_init(CPU *cpu, bool debug, read_t read, write_t write);
void cpu_reset(CPU *cpu);
void cpu_reset_pc(CPU *cpu, uint16_t pc);
uint8_t cpu_step(CPU *cpu);
uint16_t cpu_get_pc(CPU *cpu);
void cpu_set_pc(CPU *cpu, uint16_t addr);
uint8_t cpu_read(CPU *cpu, uint16_t addr);
uint8_t cpu_read_byte(CPU *cpu, uint16_t addr);
uint16_t cpu_read_word(CPU *cpu, uint16_t addr);
uint16_t cpu_read_word_zp(CPU *cpu, uint16_t addr);
void cpu_write(CPU *cpu, uint16_t addr, uint8_t data);
void cpu_write_byte(CPU *cpu, uint16_t addr, uint8_t data);
void cpu_write_word(CPU *cpu, uint16_t addr, uint16_t data);
void cpu_write_data(CPU *cpu, uint16_t addr, uint8_t data[], size_t size);

// void cpu_nmi(CPU *cpu);
// void cpu_irq(CPU *cpu);
// bool cpu_interrupts(CPU *cpu);

bool cpu_trap(CPU *cpu, uint16_t addr, handler_t handler);

// Flag operations
static inline bool get_flag_carry(CPU *cpu) { return cpu->P & FLAG_CARRY; }
static inline bool get_flag_zero(CPU *cpu) { return cpu->P & FLAG_ZERO; }
static inline bool get_flag_interrupt(CPU *cpu) { return cpu->P & FLAG_INTERRUPT_DISABLE; }
static inline bool get_flag_decimal(CPU *cpu) { return cpu->P & FLAG_DECIMAL; }
static inline bool get_flag_break(CPU *cpu) { return cpu->P & FLAG_BREAK; }
static inline bool get_flag_overflow(CPU *cpu) { return cpu->P & FLAG_OVERFLOW; }
static inline bool get_flag_negative(CPU *cpu) { return cpu->P & FLAG_NEGATIVE; }

static inline void set_flag_carry(CPU *cpu, bool set)
{
    if (set)
        cpu->P |= FLAG_CARRY;
    else
        cpu->P &= ~FLAG_CARRY;
}
static inline void set_flag_zero(CPU *cpu, bool set)
{
    if (set)
        cpu->P |= FLAG_ZERO;
    else
        cpu->P &= ~FLAG_ZERO;
}
static inline void set_flag_interrupt(CPU *cpu, bool set)
{
    if (set)
        cpu->P |= FLAG_INTERRUPT_DISABLE;
    else
        cpu->P &= ~FLAG_INTERRUPT_DISABLE;
}
static inline void set_flag_decimal(CPU *cpu, bool set)
{
    if (set)
        cpu->P |= FLAG_DECIMAL;
    else
        cpu->P &= ~FLAG_DECIMAL;
}
static inline void set_flag_break(CPU *cpu, bool set)
{
    if (set)
        cpu->P |= FLAG_BREAK;
    else
        cpu->P &= ~FLAG_BREAK;
}
static inline void set_flag_overflow(CPU *cpu, bool set)
{
    if (set)
        cpu->P |= FLAG_OVERFLOW;
    else
        cpu->P &= ~FLAG_OVERFLOW;
}
static inline void set_flag_negative(CPU *cpu, bool set)
{
    if (set)
        cpu->P |= FLAG_NEGATIVE;
    else
        cpu->P &= ~FLAG_NEGATIVE;
}

// Arithmetic operations
static inline uint8_t add_with_carry(CPU *cpu, uint8_t a, uint8_t b)
{
    uint8_t c = get_flag_carry(cpu);
    uint16_t sum = a + b + c;

    if (get_flag_decimal(cpu))
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

    if (get_flag_decimal(cpu))
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

// Stack operations
void cpu_push(CPU *cpu, uint8_t data);
void cpu_push16(CPU *cpu, uint16_t data);
uint8_t cpu_pull(CPU *cpu);
uint16_t cpu_pull16(CPU *cpu);

// Addressing mode helpers
uint16_t fetch_address(CPU *cpu, addr_mode_t mode);
uint8_t fetch_operand(CPU *cpu, addr_mode_t mode);

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

// Global opcode table declaration
extern const instruction_t instructions[256];
// extern const opcode_t instructions[256];

#endif // CPU_H