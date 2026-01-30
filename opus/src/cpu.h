/**
 * cpu.h - 6510 CPU emulation header
 */

#ifndef C64_CPU_H
#define C64_CPU_H

#include "types.h"

// Forward declaration
typedef struct C64System C64System;

// Status flag bits
#define FLAG_C  0x01  // Carry
#define FLAG_Z  0x02  // Zero
#define FLAG_I  0x04  // Interrupt Disable
#define FLAG_D  0x08  // Decimal Mode
#define FLAG_B  0x10  // Break (only pushed to stack)
#define FLAG_U  0x20  // Unused (always 1)
#define FLAG_V  0x40  // Overflow
#define FLAG_N  0x80  // Negative

// CPU state structure
typedef struct {
    // Registers
    u8  A;          // Accumulator
    u8  X;          // X Index
    u8  Y;          // Y Index
    u8  SP;         // Stack Pointer
    u8  P;          // Status Register
    u16 PC;         // Program Counter

    // 6510 I/O port
    u8  port_dir;   // $00 - Data direction register
    u8  port_data;  // $01 - Data register
    u8  cpu_port_floating;  // Tracks floating bits for proper read behavior

    // Interrupt state
    bool nmi_pending;
    bool irq_pending;
    bool nmi_edge;      // NMI is edge-triggered

    // Cycle tracking
    int extra_cycles;   // Extra cycles from page crossing/branch taken
    bool page_crossed;  // Flag for page crossing detection

    // Reference to system
    C64System *sys;
} C64Cpu;

// CPU functions
void cpu_init(C64Cpu *cpu, C64System *sys);
void cpu_reset(C64Cpu *cpu);
int  cpu_step(C64Cpu *cpu);  // Execute one instruction, returns cycles used

// Interrupt handling
void cpu_trigger_nmi(C64Cpu *cpu);
void cpu_trigger_irq(C64Cpu *cpu);

// Status flag helpers
static inline void cpu_set_flag(C64Cpu *cpu, u8 flag, bool value) {
    if (value) cpu->P |= flag;
    else cpu->P &= ~flag;
}

static inline bool cpu_get_flag(C64Cpu *cpu, u8 flag) {
    return (cpu->P & flag) != 0;
}

static inline void cpu_update_nz(C64Cpu *cpu, u8 value) {
    cpu_set_flag(cpu, FLAG_Z, value == 0);
    cpu_set_flag(cpu, FLAG_N, value & 0x80);
}

#endif // C64_CPU_H
