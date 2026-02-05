/**
 * c64.h - Main C64 system integration header
 */

#ifndef C64_H
#define C64_H

#include "types.h"
#include "cpu.h"
#include "memory.h"
#include "vic.h"
#include "cia.h"
#include "sid.h"

typedef void (*render_frame_t)(VIC *);

// C64 System structure - contains all components
struct C64
{
    CPU cpu;
    MEM mem;
    VIC vic;
    CIA cia1; // CIA1: Keyboard, joysticks
    CIA cia2; // CIA2: Serial, VIC bank
    SID sid;

    // Global cycle counter
    u64 cycles;

    // System state
    bool running;

    // Debug/logging
    bool debug;
    u64 debug_interval; // Cycles between debug output

    render_frame_t frame_renderer;
};

// System functions
void c64_init(C64 *sys);
void c64_reset(C64 *sys);
void c64_destroy(C64 *sys);

// Central tick - advances all peripherals by one cycle
void c64_tick(C64 *sys);

// Run one frame
void c64_run_frame(C64 *sys);

// Load ROMs
bool c64_load_roms(C64 *sys, const char *rom_path);

// Interrupt routing
void c64_check_interrupts(C64 *sys);

// VIC bank selection (from CIA2 port A)
u16 c64_get_vic_bank(C64 *sys);

#endif // C64_H
