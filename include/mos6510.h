#ifndef CPU_H
#define CPU_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint8_t A;             // Accumulator
    uint8_t X;             // X Index Register
    uint8_t Y;             // Y Index Register
    uint8_t SP;            // Stack Pointer
    uint8_t P;             // Status Register
    uint16_t pc;           // Program Counter
    uint8_t memory[65536]; // 64KB memory
    bool debug;
} CPU;

// Status Register bit definitions
#define FLAG_CARRY 0x01
#define FLAG_ZERO 0x02
#define FLAG_INTERRUPT_DISABLE 0x04
#define FLAG_DECIMAL 0x08
#define FLAG_BREAK 0x10
#define FLAG_RESERVED 0x20
#define FLAG_OVERFLOW 0x40
#define FLAG_NEGATIVE 0x80

// Addressing modes
typedef enum
{
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

// Opcode structure
typedef struct
{
    uint8_t opcode;
    char name[4];
    addressing_mode_t mode;
    uint8_t cycles;
    void (*execute)(CPU *cpu);
} opcode_t;

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
void cpu_init(CPU *cpu, bool debug);
void cpu_reset(CPU *cpu);
void cpu_reset_pc(CPU *cpu, uint16_t pc);
uint8_t cpu_step(CPU *cpu);
uint16_t cpu_get_pc(CPU *cpu);
void cpu_set_pc(CPU *cpu, uint16_t addr);
uint8_t cpu_read_byte(CPU *cpu, uint16_t addr);
uint16_t cpu_read_word(CPU *cpu, uint16_t addr);
uint16_t cpu_read_word_zp(CPU *cpu, uint16_t addr);
void cpu_write_byte(CPU *cpu, uint16_t addr, uint8_t data);
void cpu_write_word(CPU *cpu, uint16_t addr, uint16_t data);
void cpu_write_data(CPU *cpu, uint16_t addr, uint8_t data[], size_t size);

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
uint16_t get_operand_address(CPU *cpu, addressing_mode_t mode);
uint8_t fetch_operand(CPU *cpu, addressing_mode_t mode);

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
void ARR(CPU *cpu);
void DCP(CPU *cpu);
void ISC(CPU *cpu);
void LAS(CPU *cpu);
void LAX(CPU *cpu);
void RLA(CPU *cpu);
void RRA(CPU *cpu);
void SAX(CPU *cpu);
void SLO(CPU *cpu);
void SRE(CPU *cpu);
void TAS(CPU *cpu);
void XAA(CPU *cpu);

// Global opcode table declaration
extern const instruction_t instructions[256];
extern const opcode_t opcode_table[256];

// C64

/// Page 0

#define D6510 0x0000
#define R6510 0x0001
#define UNUSED 0x0002
#define ADRAY1 0x0003 // WORD
#define ADRAY2 0x0005 // WORD
#define CHARAC 0x0007
// #define INTEGR 0x0007
#define ENDCHR 0x0008
#define TRMPOS 0x0009
#define VERCK 0x000A
#define COUNT 0x000B
#define DIMFLG 0x000C
#define VALTYP 0x000D
#define INTFLG 0x000E
#define GARBFL 0x000F
#define SUBFLG 0x0010
#define INPFLG 0x0011
#define TANSGN 0x0012
#define CHANNL 0x0013
#define LINNUM 0x0014 // WORD
#define TEMPPT 0x0016
#define LASTPT 0x0017 // WORD
#define TEMPST 0x0019 // ARRAY [3*3]
#define INDEX 0x0022  // ARRAY [4]
#define RESHO 0x0026  // ARRAY [5]
#define TXTTAB 0x002B // WORD
#define VARTAB 0x002D // WORD
#define ARYTAB 0x002F // WORD
#define STREND 0x0031 // WORD
#define FRETOP 0x0033 // WORD
#define FRESPC 0x0035 // WORD
#define MEMSIZ 0x0037 // WORD
#define CURLIN 0x0039 // WORD
#define OLDLIN 0x003B // WORD
#define OLDTXT 0x003D // WORD
#define DATLIN 0x003F // WORD
#define DATPTR 0x0041 // WORD
#define INPPTR 0x0043 // WORD
#define VARNAM 0x0045 // WORD
#define VARPNT 0x0047 // WORD
#define FORPNT 0x0049 // WORD
#define OPPTR 0x004B  // WORD
#define OPMASK 0x004D
#define DEFPNT 0x004E // WORD
#define DSCPNT 0x0050 // PTR + LEN
#define FOUR6 0x0053
#define JMPER 0x0054 // OP + PTR
// #define TEMPF1 0x0057
// #define TEMPF2 0x005C
#define FAC1 0x0061   // FP
#define FACEXP 0x0061 // Exponent
#define FACHO 0x0062  // Mantissa
#define FACSGN 0x0066 // Sign
#define SGNFLG 0x0067
#define BITS 0x0068
#define AFAC 0x0069   // FP
#define ARGEXP 0x0069 // Exponent
#define ARGHO 0x006A  // Mantissa
#define ARGSGN 0x006E // Sign
#define ARISGN 0x006F
#define FACOV 0x0070
#define FBUFPT 0x0071 // WORD
#define CHRGET 0x0073 // FN
#define CHRGOT 0x0079 // FN
#define TXTPTR 0x007A // FN
#define RNDX 0x008B   // ARRAY[5]
#define STATUS 0x0090
#define STKEY 0x0091
#define SVXT 0x0092
#define VERCKK 0x0093
#define C3PO 0x0094
#define BSOUR 0x0095
#define SYNO 0x0096
#define XSAV 0x0097
#define LDTND 0x0098
#define DFLTN 0x0099
#define DFLTO 0x009A
#define PRTY 0x009B
#define DPSW 0x009C
#define MSGFLG 0x009D
#define FNMIDX 0x009E
#define PTR1 0x009E
#define PTR2 0x009F
#define TIME 0x00A0 // ARRAY[3]
#define TSFCNT 0x00A3
#define TBTCNT 0x00A4
#define CNTDN 0x00A5
#define BUFPNT 0x00A6
#define INBIT 0x00A7
#define BITC1 0x00A8
#define RINONE 0x00A9
#define RIDATA 0x00AA
#define RIPRTY 0x00AB
#define SAL 0x00AC   // WORD
#define EAL 0x00AE   // WORD
#define CMPO 0x00B0  // WORD
#define TAPE1 0x00B2 // WORD
#define BITTS 0x00B4
#define NXTBIT 0x00B5
#define RODATA 0x00B6
#define FNLEN 0x00B7
#define LA 0x00B8
#define SA 0x00B9
#define FA 0x00BA
#define FNADR 0x00BB // WORD
#define ROPRTY 0x00BD
#define FSBLK 0x00BE
#define MYCH 0x00BF
#define CAS1 0x00C0
#define STAL 0x00C1   // WORD
#define MEMUSS 0x00C3 // WORD
#define LSTX 0x00C5
#define NDX 0x00C6
#define RVS 0x00C7
#define INDX 0x00C8
#define LXSP 0x00C9 // WORD
#define SFDX 0x00CB
#define BLNSW 0x00CC
#define BLNCT 0x00CD
#define GDBLN 0x00CE
#define BLNON 0x00CF
#define CRSW 0x00D0
#define PNT 0x00D1 // WORD
#define PNTR 0x00D3
#define QTSW 0x00D4
#define LNMX 0x00D5
#define TBLX 0x00D6
#define SCHAR 0x00D7
#define INSRT 0x00D8
#define LDTB1 0x00D9  // ARRAY[25]
#define USER 0x00F3   // WORD
#define KEYTAB 0x00F5 // WORD
#define RIBUF 0x00F7  // WORD
#define ROBUF 0x00F9  // WORD
#define FREKZP 0x00FB // ARRAY[4]
#define BASZPT 0x00FF
#define ASCWRK 0x00FF // FIXME

/// Page 1

// _ => 0x100 // ARRAY[10]
#define BAD 0x0100    // ARRAY[62]
#define STACK 0x0100  // ARRAY[256]
#define BSTACK 0x013F // ARRAY[192]
//
#define PC 0x01FE
#define PCL 0x01FE
#define PCH 0x01FF

/// Page 2

#define BUF 0x0200    // ARRAY[88]
#define LAT 0x0259    // ARRAY[9]
#define FAT 0x0263    // ARRAY[9]
#define SAT 0x026D    // ARRAY[9]
#define KEYD 0x0277   // ARRAY[9]
#define MEMSTR 0x0281 // WORD
#define MEMEND 0x0283 // WORD
#define TIMOUT 0x0285
#define COLOR 0x0286
#define GDCOL 0x0287
#define HIBASE 0x0288
#define XMAX 0x0289
#define RPTFLG 0x028A
#define KOUNT 0x028B
#define DELAY 0x028C
#define SHFLAG 0x028D
#define LSTSHF 0x028E
#define KEYLOG 0x028F // WORD
#define MODE 0x0291
#define AUTODN 0x0292
#define M51CTR 0x0293
#define M51CDR 0x0294
#define M51AJB 0x0295 // WORD
#define RSSTAT 0x0297
#define BITNUM 0x0298
#define BAUDOF 0x0299 // WORD
#define RIDBE 0x029B
#define RIDBS 0x029C
#define RODBS 0x029D
#define RODBE 0x029E
#define IRQTMP 0x029F // WORD
#define ENABL 0x02A1
#define TODSNS 0x02A2
#define TRDTMP 0x02A3
#define TD1IRQ 0x02A4
#define TLNIDX 0x02A5
#define TVSFLG 0x02A6
// #define UNUSED 0x02A7 // ARRAY[88]
// #define SPR11 0x02C0 // FIXME

/// Page 3

#define IERROR 0x0300 // WORD
#define IMAIN 0x0302  // WORD
#define ICRNCH 0x0304 // WORD
#define IQPLOP 0x0306 // WORD
#define IGONE 0x0308  // WORD
#define IEVAL 0x030A  // WORD
#define SAREG 0x030C
#define SXREG 0x030D
#define SYREG 0x030E
#define SPREG 0x030F
#define USRPOK 0x0310
#define USRADD 0x0311 // WORD
// #define UNUSED 0x0313
#define CINV 0x0314   // WORD
#define CNBINV 0x0316 // WORD
#define NMINV 0x0318  // WORD
#define IOPEN 0x031A  // WORD
#define ICLOSE 0x031C // WORD
#define ICHKIN 0x031E // WORD
#define ICKOUT 0x0320 // WORD
#define ICLRCH 0x0322 // WORD
#define IBASIN 0x0324 // WORD
#define IBSOUT 0x0326 // WORD
#define ISTOP 0x0328  // WORD
#define IGETIN 0x032A // WORD
#define ICLALL 0x032C // WORD
#define USRCMD 0x032E // WORD
#define ILOAD 0x0330  // WORD
#define ISAVE 0x0332  // WORD
// #define UNUSED 0x0334 // ARRAY[7]
#define TBUFFR 0x033C // ARRAY[192]
                      // #define SPR13 0x0340 // FIXME
                      // #define SPR14 0x0380 // FIXME
                      // #define SPR15 0x03C0 // FIXME
                      // #define UNUSED 0x03FC // ARRAY[4]

/// Page 4

#define VICSCN 0x0400 // ARRAY[1024]
// #define UNUSED 0x07E8
// #define SPNTRS 0x07F8 // FIXME

///
#define COLD 0xA000 // WORD
#define WARM 0xA002 // WORD

///
#define READY 0xA474 // ARRAY[12]

///
#define CHROUT 0xFFD2 // JMP
#define GETIN 0xFFE4  // JMP

///
#define NMI 0xFFFA   // WORD
#define RESET 0xFFFC // WORD
#define IRQ 0xFFFE   // WORD

///
#define CARTROM 0x8000
#define BASICROM 0xA000
#define KERNALROM 0xE000

#endif // CPU_H