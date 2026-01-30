/**
 * vic.h - VIC-II (6569 PAL) video chip emulation header
 */

#ifndef C64_VIC_H
#define C64_VIC_H

#include "types.h"

// Forward declaration
typedef struct C64System C64System;

// VIC-II register offsets (from $D000)
#define VIC_M0X     0x00  // Sprite 0 X position
#define VIC_M0Y     0x01  // Sprite 0 Y position
// ... sprites 1-7 similar
#define VIC_MSBX    0x10  // Sprite X MSB
#define VIC_CR1     0x11  // Control Register 1
#define VIC_RASTER  0x12  // Raster counter
#define VIC_LPX     0x13  // Light pen X
#define VIC_LPY     0x14  // Light pen Y
#define VIC_ME      0x15  // Sprite enable
#define VIC_CR2     0x16  // Control Register 2
#define VIC_MYE     0x17  // Sprite Y expansion
#define VIC_MEMPTR  0x18  // Memory pointers
#define VIC_IRR     0x19  // Interrupt request register
#define VIC_IMR     0x1A  // Interrupt mask register
#define VIC_MDP     0x1B  // Sprite-data priority
#define VIC_MMC     0x1C  // Sprite multicolor
#define VIC_MXE     0x1D  // Sprite X expansion
#define VIC_MM      0x1E  // Sprite-sprite collision
#define VIC_MD      0x1F  // Sprite-data collision
#define VIC_BORDER  0x20  // Border color
#define VIC_BG0     0x21  // Background color 0
#define VIC_BG1     0x22  // Background color 1
#define VIC_BG2     0x23  // Background color 2
#define VIC_BG3     0x24  // Background color 3
#define VIC_SMC0    0x25  // Sprite multicolor 0
#define VIC_SMC1    0x26  // Sprite multicolor 1
// $D027-$D02E: Sprite colors 0-7

// VIC-II state structure
typedef struct {
    // Registers (directly accessible)
    u8 regs[0x40];

    // Internal state
    u16 raster_line;        // Current raster line (0-311 PAL)
    u8  raster_cycle;       // Current cycle within line (1-63 PAL)
    u16 raster_irq_line;    // Latched raster interrupt line

    // Bad line state
    bool bad_line_condition;
    bool bad_line_active;
    bool ba_low;            // Bus available signal (active low)

    // Display state
    bool den;               // Display Enable (CR1 bit 4)
    u8   yscroll;           // Y scroll value
    u8   xscroll;           // X scroll value
    u8   rc;                // Row counter (0-7)
    u16  vc;                // Video counter (0-999)
    u16  vcbase;            // Video counter base
    u8   vmli;              // Video matrix line index

    // Memory pointers
    u16 video_matrix_base;  // Screen memory base ($0400 default)
    u16 char_base;          // Character memory base

    // Screen buffer for ANSI rendering
    u8 screen_chars[SCREEN_ROWS * SCREEN_COLS];
    u8 screen_colors[SCREEN_ROWS * SCREEN_COLS];

    // Interrupt status
    bool irq_pending;

    // Frame counter
    u32 frame_count;

    // Reference to system
    C64System *sys;
} C64Vic;

// VIC functions
void vic_init(C64Vic *vic, C64System *sys);
void vic_reset(C64Vic *vic);
void vic_step(C64Vic *vic);  // Advance one cycle

// Register access
u8   vic_read(C64Vic *vic, u8 reg);
void vic_write(C64Vic *vic, u8 reg, u8 value);

// Rendering
void vic_render_frame_ansi(C64Vic *vic);

#endif // C64_VIC_H
