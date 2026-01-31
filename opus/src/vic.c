/**
 * vic.c - VIC-II (6569 PAL) video chip emulation
 * 
 * Implements raster timing, bad lines, sprite DMA,
 * and ANSI console rendering.
 */

#include "vic.h"
#include "c64.h"
#include <stdio.h>
#include <string.h>

void vic_init(C64Vic *vic, C64System *sys) {
    memset(vic, 0, sizeof(C64Vic));
    vic->sys = sys;
}

void vic_reset(C64Vic *vic) {
    memset(vic->regs, 0, sizeof(vic->regs));
    
    // Default register values
    vic->regs[VIC_CR1] = 0x1B;      // DEN=1, RSEL=1, YSCROLL=3
    vic->regs[VIC_CR2] = 0xC8;      // MCM=0, CSEL=1, XSCROLL=0
    vic->regs[VIC_MEMPTR] = 0x14;   // Screen at $0400, Char at $1000
    vic->regs[VIC_BORDER] = 0x0E;   // Light blue border
    vic->regs[VIC_BG0] = 0x06;      // Blue background
    
    vic->raster_line = 0;
    vic->raster_cycle = 1;
    vic->raster_irq_line = 0;
    
    vic->bad_line_condition = false;
    vic->bad_line_active = false;
    vic->ba_low = false;
    
    vic->den = true;
    vic->yscroll = 3;
    vic->xscroll = 0;
    vic->rc = 0;
    vic->vc = 0;
    vic->vcbase = 0;
    vic->vmli = 0;
    
    vic->video_matrix_base = 0x0400;
    vic->char_base = 0x1000;
    
    vic->irq_pending = false;
    vic->frame_count = 0;
    
    memset(vic->screen_chars, 0x20, sizeof(vic->screen_chars)); // Spaces
    memset(vic->screen_colors, 0x0E, sizeof(vic->screen_colors)); // Light blue
}

// Update internal state from registers
static void update_from_regs(C64Vic *vic) {
    u8 cr1 = vic->regs[VIC_CR1];
    u8 cr2 = vic->regs[VIC_CR2];
    u8 memptr = vic->regs[VIC_MEMPTR];
    
    vic->den = (cr1 & 0x10) != 0;
    vic->yscroll = cr1 & 0x07;
    vic->xscroll = cr2 & 0x07;
    
    // Screen memory base (bits 4-7 of $D018)
    vic->video_matrix_base = ((memptr >> 4) & 0x0F) * 0x400;
    
    // Character base (bits 1-3 of $D018)
    vic->char_base = ((memptr >> 1) & 0x07) * 0x800;
}

// Check for raster IRQ
static void check_raster_irq(C64Vic *vic) {
    // Compare current raster with target
    if (vic->raster_line == vic->raster_irq_line) {
        // Set raster IRQ flag
        vic->regs[VIC_IRR] |= 0x01;
        
        // Check if IRQ is enabled
        if (vic->regs[VIC_IMR] & 0x01) {
            vic->regs[VIC_IRR] |= 0x80;  // Set IRQ occurred bit
            vic->irq_pending = true;
        }
    }
}

// Fetch screen data for current line
static void fetch_screen_line(C64Vic *vic, int screen_row) {
    if (screen_row < 0 || screen_row >= SCREEN_ROWS) return;
    
    C64Memory *mem = &vic->sys->mem;
    
    for (int col = 0; col < SCREEN_COLS; col++) {
        u16 screen_addr = vic->video_matrix_base + (screen_row * SCREEN_COLS) + col;
        u16 color_addr = (screen_row * SCREEN_COLS) + col;
        
        vic->screen_chars[screen_row * SCREEN_COLS + col] = 
            mem_vic_read(mem, screen_addr);
        vic->screen_colors[screen_row * SCREEN_COLS + col] = 
            mem->color_ram[color_addr] & 0x0F;
    }
}

void vic_step(C64Vic *vic) {
    update_from_regs(vic);
    
    // Check for bad line condition
    // Bad line: raster 48-247, cycle 12, (raster & 7) == yscroll
    bool in_display = (vic->raster_line >= 48 && vic->raster_line <= 247);
    
    if (in_display && vic->den) {
        vic->bad_line_condition = ((vic->raster_line & 0x07) == vic->yscroll);
    } else {
        vic->bad_line_condition = false;
    }
    
    // Latch bad line state at cycle 12
    if (vic->raster_cycle == 12) {
        vic->bad_line_active = vic->bad_line_condition;
    }
    
    // BA (Bus Available) logic
    // BA goes low during bad lines (cycles 12-54) and sprite DMA
    if (vic->bad_line_active && vic->raster_cycle >= 12 && vic->raster_cycle <= 54) {
        vic->ba_low = true;
    } else {
        // TODO: Add sprite DMA BA logic
        vic->ba_low = false;
    }
    
    // Fetch screen data at the start of visible lines
    if (vic->raster_cycle == 14 && in_display && vic->bad_line_active) {
        int screen_row = (vic->raster_line - 48) / 8;
        if (screen_row >= 0 && screen_row < SCREEN_ROWS) {
            fetch_screen_line(vic, screen_row);
        }
    }
    
    // Check for raster IRQ at specific cycle
    if (vic->raster_cycle == 1) {
        check_raster_irq(vic);
    }
    
    // Advance cycle counter
    vic->raster_cycle++;
    
    // End of line
    if (vic->raster_cycle > PAL_CYCLES_PER_LINE) {
        vic->raster_cycle = 1;
        vic->raster_line++;
        
        // End of frame (PAL: 312 lines, 0-311)
        if (vic->raster_line >= PAL_RASTER_LINES) {
            vic->raster_line = 0;
            vic->frame_count++;
            
            // Render frame to console at end of vertical blank
            if (vic->sys->frame_renderer)
                vic->sys->frame_renderer(vic);
        }
        
        // Update $D011 bit 7 (raster line bit 8) and $D012
        vic->regs[VIC_RASTER] = vic->raster_line & 0xFF;
        if (vic->raster_line & 0x100) {
            vic->regs[VIC_CR1] |= 0x80;
        } else {
            vic->regs[VIC_CR1] &= ~0x80;
        }
    }
}

u8 vic_read(C64Vic *vic, u8 reg) {
    switch (reg) {
        case VIC_CR1:
            // Return register with current raster bit 8
            return (vic->regs[VIC_CR1] & 0x7F) | 
                   ((vic->raster_line & 0x100) ? 0x80 : 0);
        
        case VIC_RASTER:
            return vic->raster_line & 0xFF;
        
        case VIC_IRR:
            return vic->regs[VIC_IRR] | 0x70;  // Bits 4-6 always read as 1
        
        case VIC_IMR:
            return vic->regs[VIC_IMR] | 0xF0;  // Bits 4-7 always read as 1
        
        case VIC_MM:
        case VIC_MD:
            // Collision registers - clear on read
            {
                u8 val = vic->regs[reg];
                vic->regs[reg] = 0;
                return val;
            }
        
        default:
            if (reg < 0x2F) {
                return vic->regs[reg];
            }
            // Unused registers return 0xFF
            return 0xFF;
    }
}

void vic_write(C64Vic *vic, u8 reg, u8 value) {
    switch (reg) {
        case VIC_CR1:
            vic->regs[VIC_CR1] = value;
            // Update raster IRQ line bit 8
            vic->raster_irq_line = (vic->raster_irq_line & 0xFF) | 
                                   ((value & 0x80) ? 0x100 : 0);
            break;
        
        case VIC_RASTER:
            // Write sets raster IRQ compare value
            vic->raster_irq_line = (vic->raster_irq_line & 0x100) | value;
            break;
        
        case VIC_IRR:
            // Writing 1s clears interrupt flags
            vic->regs[VIC_IRR] &= ~(value & 0x0F);
            // Clear IRQ bit if no flags remain
            if (!(vic->regs[VIC_IRR] & vic->regs[VIC_IMR] & 0x0F)) {
                vic->regs[VIC_IRR] &= ~0x80;
                vic->irq_pending = false;
            }
            break;
        
        case VIC_IMR:
            vic->regs[VIC_IMR] = value & 0x0F;
            break;
        
        default:
            if (reg < 0x40) {
                vic->regs[reg] = value;
            }
            break;
    }
}
