/*
 * C64 Emulator - MOS 6502/6510 CPU Header
 * 
 * Core CPU definitions, addressing modes, and instruction format.
 */

#ifndef CPU_H
#define CPU_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Page boundary check macro */
#define page_boundary(a, b) ((a & 0xFF00) != (b & 0xFF00))

/* Memory access callbacks */
typedef uint8_t (*read_fn)(void *context, uint16_t addr);
typedef void (*write_fn)(void *context, uint16_t addr, uint8_t data);

/* Trap handler callback */
typedef void (*trap_handler_fn)(void *cpu);

/* Maximum number of traps */
#define MAX_TRAPS 16

/* Trap entry */
typedef struct {
    uint16_t addr;
    void (*handler)(void *cpu);
    bool active;
} TrapEntry;

/* CPU State */
typedef struct CPU {
    uint8_t A;          /* Accumulator */
    uint8_t X;          /* X Index Register */
    uint8_t Y;          /* Y Index Register */
    uint8_t SP;         /* Stack Pointer */
    uint8_t P;          /* Status Register (Flags) */
    uint16_t PC;        /* Program Counter */
    
    uint8_t memory[65536];  /* 64KB address space */
    
    bool nmi_pending;   /* NMI interrupt pending */
    bool irq_pending;   /* IRQ interrupt pending */
    bool nmi_edge;      /* NMI edge detection */
    
    bool decimal_mode;  /* Enable BCD mode for ADC/SBC */
    
    bool debug;         /* Debug output enabled */
    FILE *debug_file;   /* Debug output file */
    
    read_fn read;       /* Memory read callback */
    write_fn write;     /* Memory write callback */
    void *context;      /* Context for read/write callbacks */
    
    uint64_t cycle_count;  /* Total cycles executed */
    uint8_t extra_cycles;  /* Extra cycles for branches/page crossing */
    bool page_crossed;     /* Page crossing detected in addressing */
    
    /* Trap handlers */
    TrapEntry traps[MAX_TRAPS];
    int trap_count;
} CPU;

/* Handler type for trap callbacks (defined after CPU typedef) */
typedef void (*handler_t)(CPU *cpu);

/* Compatibility aliases for test suite */
#define nmi nmi_pending
#define irq irq_pending

/* Addressing Modes */
typedef enum {
    Absolute,           /* $nnnn      - Full 16-bit address */
    AbsoluteX,          /* $nnnn,X    - Absolute + X */
    AbsoluteY,          /* $nnnn,Y    - Absolute + Y */
    Accumulator,        /* A          - Accumulator */
    Immediate,          /* #$nn       - Immediate value */
    Implied,            /* (none)     - Implied by instruction */
    IndexedIndirect,    /* ($nn,X)    - Indirect through zero page + X */
    Indirect,           /* ($nnnn)    - Indirect (JMP only) */
    IndirectIndexed,    /* ($nn),Y    - Zero page indirect + Y */
    Relative,           /* $nn        - Relative branch offset */
    ZeroPage,           /* $nn        - Zero page address */
    ZeroPageX,          /* $nn,X      - Zero page + X */
    ZeroPageY           /* $nn,Y      - Zero page + Y */
} AddressMode;

/* Instruction definition */
typedef struct {
    uint8_t opcode;
    char name[4];
    void (*execute)(CPU *cpu);
    AddressMode mode;
    uint8_t size;       /* Instruction size in bytes */
    uint8_t cycles;     /* Base cycle count */
    bool illegal;       /* Is this an illegal opcode? */
} Instruction;

/* Hardware vectors */
#define NMI_VECTOR   0xFFFA
#define RESET_VECTOR 0xFFFC
#define IRQ_VECTOR   0xFFFE

/* Status Register (P) bit definitions */
#define FLAG_CARRY     0x01  /* Bit 0: Carry */
#define FLAG_ZERO      0x02  /* Bit 1: Zero */
#define FLAG_INTERRUPT 0x04  /* Bit 2: Interrupt Disable */
#define FLAG_DECIMAL   0x08  /* Bit 3: Decimal Mode */
#define FLAG_BREAK     0x10  /* Bit 4: Break Command */
#define FLAG_RESERVED  0x20  /* Bit 5: Reserved (always 1) */
#define FLAG_OVERFLOW  0x40  /* Bit 6: Overflow */
#define FLAG_NEGATIVE  0x80  /* Bit 7: Negative */

/* CPU Core Functions */
void cpu_init(CPU *cpu);
void cpu_reset(CPU *cpu);
void cpu_reset_at(CPU *cpu, uint16_t addr);
uint8_t cpu_step(CPU *cpu);

/* Program Counter */
uint16_t cpu_get_pc(CPU *cpu);
void cpu_set_pc(CPU *cpu, uint16_t addr);

/* Memory Access */
uint8_t cpu_read(CPU *cpu, uint16_t addr);
uint16_t cpu_read_word(CPU *cpu, uint16_t addr);
uint16_t cpu_read_word_zp(CPU *cpu, uint8_t addr);
void cpu_write(CPU *cpu, uint16_t addr, uint8_t data);
void cpu_write_word(CPU *cpu, uint16_t addr, uint16_t data);

/* Interrupts */
void cpu_nmi(CPU *cpu);
void cpu_irq(CPU *cpu);
void cpu_trigger_nmi(CPU *cpu);
void cpu_trigger_irq(CPU *cpu);
void cpu_clear_irq(CPU *cpu);

/* Configuration */
void cpu_set_callbacks(CPU *cpu, read_fn read, write_fn write, void *context);
void cpu_set_debug(CPU *cpu, bool debug, FILE *debug_file);
void cpu_set_decimal_mode(CPU *cpu, bool enabled);

/* Trap handlers (for test compatibility - currently no-op) */
bool cpu_trap(CPU *cpu, uint16_t addr, handler_t handler);

/* Stack Operations */
void cpu_push(CPU *cpu, uint8_t value);
void cpu_push_word(CPU *cpu, uint16_t value);
#define cpu_push16 cpu_push_word  /* Alias for test compatibility */
uint8_t cpu_pull(CPU *cpu);
uint16_t cpu_pull_word(CPU *cpu);

/* Flag Operations - Inline for performance */
static inline bool get_flag_carry(CPU *cpu) { return cpu->P & FLAG_CARRY; }
static inline bool get_flag_zero(CPU *cpu) { return cpu->P & FLAG_ZERO; }
static inline bool get_flag_interrupt(CPU *cpu) { return cpu->P & FLAG_INTERRUPT; }
static inline bool get_flag_decimal(CPU *cpu) { return cpu->P & FLAG_DECIMAL; }
static inline bool get_flag_break(CPU *cpu) { return cpu->P & FLAG_BREAK; }
static inline bool get_flag_overflow(CPU *cpu) { return cpu->P & FLAG_OVERFLOW; }
static inline bool get_flag_negative(CPU *cpu) { return cpu->P & FLAG_NEGATIVE; }

static inline void set_flag_carry(CPU *cpu, bool set) {
    if (set) cpu->P |= FLAG_CARRY; else cpu->P &= ~FLAG_CARRY;
}
static inline void set_flag_zero(CPU *cpu, bool set) {
    if (set) cpu->P |= FLAG_ZERO; else cpu->P &= ~FLAG_ZERO;
}
static inline void set_flag_interrupt(CPU *cpu, bool set) {
    if (set) cpu->P |= FLAG_INTERRUPT; else cpu->P &= ~FLAG_INTERRUPT;
}
static inline void set_flag_decimal(CPU *cpu, bool set) {
    if (set) cpu->P |= FLAG_DECIMAL; else cpu->P &= ~FLAG_DECIMAL;
}
static inline void set_flag_break(CPU *cpu, bool set) {
    if (set) cpu->P |= FLAG_BREAK; else cpu->P &= ~FLAG_BREAK;
}
static inline void set_flag_overflow(CPU *cpu, bool set) {
    if (set) cpu->P |= FLAG_OVERFLOW; else cpu->P &= ~FLAG_OVERFLOW;
}
static inline void set_flag_negative(CPU *cpu, bool set) {
    if (set) cpu->P |= FLAG_NEGATIVE; else cpu->P &= ~FLAG_NEGATIVE;
}

/* Set N and Z flags based on a value */
static inline void set_nz_flags(CPU *cpu, uint8_t value) {
    set_flag_negative(cpu, value & 0x80);
    set_flag_zero(cpu, value == 0);
}

/* Helper function declarations for instruction implementations */
uint8_t fetch_operand(CPU *cpu, AddressMode mode);
uint16_t fetch_address(CPU *cpu, AddressMode mode);
const Instruction *fetch_instruction(CPU *cpu);

/* Instruction table (defined in instructions.c) */
extern const Instruction instructions[256];

#endif /* CPU_H */
