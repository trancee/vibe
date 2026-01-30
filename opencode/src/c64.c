#include "c64.h"
#include "memory.h"
#include "cpu.h"
#include "cia.h"
#include "vic.h"
#include "sid.h"
#include <stdlib.h>
#include <string.h>

void c64_init(C64* sys) {
    memset(sys, 0, sizeof(C64));
    
    // Initialize subsystems
    cpu_init(&sys->cpu);
    mem_init(sys);
    cia_init(&sys->cia1);
    cia_init(&sys->cia2);
    vic_init(&sys->vic);
    sid_init(&sys->sid);
    
    // Load ROMs
    mem_load_rom(sys, "roms/kernal.rom", sys->mem.kernal_rom, sizeof(sys->mem.kernal_rom));
    mem_load_rom(sys, "roms/basic.rom", sys->mem.basic_rom, sizeof(sys->mem.basic_rom));
    mem_load_rom(sys, "roms/char.rom", sys->mem.char_rom, sizeof(sys->mem.char_rom));
    
    sys->running = true;
}

void c64_tick(C64* sys) {
    // Advance all components by one cycle
    cia_clock(&sys->cia1);
    cia_clock(&sys->cia2);
    vic_clock(&sys->vic);
    
    sys->mem.cycle_count++;
    
    // Check for IRQs from CIA1
    if (sys->cia1.irq_pending && !cpu_get_flag(&sys->cpu, FLAG_I)) {
        sys->cpu.irq_pending = true;
    }
    
    // Check for NMIs from CIA2
    if (sys->cia2.irq_pending) {
        sys->cpu.nmi_pending = true;
    }
}

void c64_step(C64* sys) {
    if (!sys->running) return;
    
    // Handle pending interrupts
    if (sys->cpu.nmi_pending) {
        cpu_interrupt(sys, true);
        sys->cpu.nmi_pending = false;
    } else if (sys->cpu.irq_pending) {
        cpu_interrupt(sys, false);
        sys->cpu.irq_pending = false;
    }
    
    // Execute one CPU instruction
    cpu_step(sys);
    
    // Apply extra cycles from page crosses, branches, etc.
    for (int i = 0; i < sys->cpu.extra_cycles; i++) {
        c64_tick(sys);
    }
    
    // Render frame at end of VBlank
    if (sys->vic.raster_line == 0 && sys->vic.vblank == 0) {
        vic_render_frame(&sys->vic, sys);
    }
}

void c64_cleanup(C64* sys) {
    // Nothing to clean up for now
    (void)sys;
}