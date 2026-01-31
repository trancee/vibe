/**
 * c64.c - C64 system integration
 * 
 * Central tick mechanism, interrupt routing, and system coordination.
 */

#include "c64.h"
#include <stdio.h>
#include <string.h>

void c64_init(C64System *sys) {
    memset(sys, 0, sizeof(C64System));
    
    // Initialize all components
    cpu_init(&sys->cpu, sys);
    mem_init(&sys->mem, sys);
    vic_init(&sys->vic, sys);
    cia_init(&sys->cia1, 1, sys);
    cia_init(&sys->cia2, 2, sys);
    sid_init(&sys->sid, sys);
    
    sys->running = false;
    sys->debug = false;
    sys->debug_interval = 1000000;  // Log every million cycles
}

void c64_reset(C64System *sys) {
    sys->cycle_count = 0;
    
    // Reset all components
    mem_reset(&sys->mem);
    vic_reset(&sys->vic);
    cia_reset(&sys->cia1);
    cia_reset(&sys->cia2);
    sid_reset(&sys->sid);
    cpu_reset(&sys->cpu);  // CPU last (reads reset vector)
    
    sys->running = true;
    
    printf("C64 System Reset\n");
    printf("CPU PC: $%04X\n", sys->cpu.PC);
}

void c64_destroy(C64System *sys) {
    // Nothing to free currently
    sys->running = false;
}

// Central tick - advances all peripherals by one cycle
void c64_tick(C64System *sys) {
    sys->cycle_count++;
    
    // Clock VIC-II
    vic_step(&sys->vic);
    
    // Clock CIAs
    cia_clock(&sys->cia1);
    cia_clock(&sys->cia2);
    
    // Clock SID
    sid_clock(&sys->sid);
    
    // Check interrupts
    c64_check_interrupts(sys);
    
    // Debug output
    // if (sys->debug && (sys->cycle_count % sys->debug_interval == 0)) {
    //     printf("[Cycle %lu] PC=$%04X A=$%02X X=$%02X Y=$%02X SP=$%02X P=$%02X "
    //            "Raster=%03d BA=%d\n",
    //            sys->cycle_count,
    //            sys->cpu.PC, sys->cpu.A, sys->cpu.X, sys->cpu.Y,
    //            sys->cpu.SP, sys->cpu.P,
    //            sys->vic.raster_line, sys->vic.ba_low);
    // }
}

// Run one complete frame
void c64_run_frame(C64System *sys) {
    u32 start_frame = sys->vic.frame_count;
    
    while (sys->running && sys->vic.frame_count == start_frame) {
        // Execute one CPU instruction
        cpu_step(&sys->cpu);
    }
}

// Load ROMs
bool c64_load_roms(C64System *sys, const char *rom_path) {
    return mem_load_roms(&sys->mem, rom_path);
}

// Check and route interrupts
void c64_check_interrupts(C64System *sys) {
    // CIA1 IRQ -> CPU IRQ
    if (sys->cia1.irq_pending) {
        cpu_trigger_irq(&sys->cpu);
    } else {
        sys->cpu.irq_pending = false;
    }
    
    // CIA2 -> CPU NMI
    if (sys->cia2.irq_pending) {
        cpu_trigger_nmi(&sys->cpu);
    }
    
    // VIC-II IRQ -> CPU IRQ
    if (sys->vic.irq_pending) {
        cpu_trigger_irq(&sys->cpu);
    }
}

// Get VIC bank from CIA2 port A
u16 c64_get_vic_bank(C64System *sys) {
    // Bits 0-1 of CIA2 port A select VIC bank (active low)
    u8 bank_sel = ~sys->cia2.pra & 0x03;
    
    // Bank 0: $0000-$3FFF
    // Bank 1: $4000-$7FFF
    // Bank 2: $8000-$BFFF
    // Bank 3: $C000-$FFFF
    return bank_sel * 0x4000;
}
