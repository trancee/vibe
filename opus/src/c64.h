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

typedef void (*render_frame_t)(C64Vic *);

// C64 System structure - contains all components
struct C64System {
    C64Cpu    cpu;
    C64Memory mem;
    C64Vic    vic;
    C64Cia    cia1;   // CIA1: Keyboard, joysticks
    C64Cia    cia2;   // CIA2: Serial, VIC bank
    C64Sid    sid;

    // Global cycle counter
    u64 cycle_count;

    // System state
    bool running;

    // Debug/logging
    bool debug;
    u64  debug_interval;    // Cycles between debug output

    render_frame_t frame_renderer;
};

// System functions
void c64_init(C64System *sys);
void c64_reset(C64System *sys);
void c64_destroy(C64System *sys);

// Central tick - advances all peripherals by one cycle
void c64_tick(C64System *sys);

// Run one frame
void c64_run_frame(C64System *sys);

// Load ROMs
bool c64_load_roms(C64System *sys, const char *rom_path);

// Interrupt routing
void c64_check_interrupts(C64System *sys);

// VIC bank selection (from CIA2 port A)
u16 c64_get_vic_bank(C64System *sys);

#endif // C64_H
