/*
 * C64 Emulator - VIC-II (6569/6567) Video Chip Header
 * 
 * Implements the Video Interface Chip with:
 * - Bad line detection and BA signal
 * - Raster IRQ generation
 * - Sprite DMA
 * - ANSI text-mode rendering for console output
 */

#ifndef VIC_H
#define VIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* VIC-II Memory Map */
#define VIC_MEM_START   0xD000
#define VIC_MEM_END     0xD3FF
#define VIC_MEM_SIZE    0x0400

/* Color RAM */
#define COLOR_RAM_START 0xD800
#define COLOR_RAM_END   0xDBFF
#define COLOR_RAM_SIZE  0x0400

/* PAL Timing Constants */
#define VIC_PAL_CYCLES_PER_LINE     63
#define VIC_PAL_LINES_PER_FRAME     312
#define VIC_PAL_FIRST_VISIBLE_LINE  16
#define VIC_PAL_LAST_VISIBLE_LINE   300

/* NTSC Timing Constants */
#define VIC_NTSC_CYCLES_PER_LINE    65
#define VIC_NTSC_LINES_PER_FRAME    263

/* Screen Dimensions */
#define VIC_SCREEN_WIDTH    320
#define VIC_SCREEN_HEIGHT   200
#define VIC_TEXT_COLS       40
#define VIC_TEXT_ROWS       25

/* Sprite Constants */
#define VIC_NUM_SPRITES     8
#define VIC_SPRITE_WIDTH    24
#define VIC_SPRITE_HEIGHT   21

/* VIC-II Register Offsets (from base $D000) */
#define VIC_REG_SPRITE0_X       0x00
#define VIC_REG_SPRITE0_Y       0x01
#define VIC_REG_SPRITE1_X       0x02
#define VIC_REG_SPRITE1_Y       0x03
#define VIC_REG_SPRITE2_X       0x04
#define VIC_REG_SPRITE2_Y       0x05
#define VIC_REG_SPRITE3_X       0x06
#define VIC_REG_SPRITE3_Y       0x07
#define VIC_REG_SPRITE4_X       0x08
#define VIC_REG_SPRITE4_Y       0x09
#define VIC_REG_SPRITE5_X       0x0A
#define VIC_REG_SPRITE5_Y       0x0B
#define VIC_REG_SPRITE6_X       0x0C
#define VIC_REG_SPRITE6_Y       0x0D
#define VIC_REG_SPRITE7_X       0x0E
#define VIC_REG_SPRITE7_Y       0x0F
#define VIC_REG_SPRITE_X_MSB    0x10
#define VIC_REG_CONTROL1        0x11
#define VIC_REG_RASTER          0x12
#define VIC_REG_LIGHTPEN_X      0x13
#define VIC_REG_LIGHTPEN_Y      0x14
#define VIC_REG_SPRITE_ENABLE   0x15
#define VIC_REG_CONTROL2        0x16
#define VIC_REG_SPRITE_Y_EXP    0x17
#define VIC_REG_MEM_POINTERS    0x18
#define VIC_REG_IRQ_STATUS      0x19
#define VIC_REG_IRQ_ENABLE      0x1A
#define VIC_REG_SPRITE_PRIORITY 0x1B
#define VIC_REG_SPRITE_MCOL     0x1C
#define VIC_REG_SPRITE_X_EXP    0x1D
#define VIC_REG_SPRITE_COLL     0x1E
#define VIC_REG_SPRITE_DATA_COLL 0x1F
#define VIC_REG_BORDER_COLOR    0x20
#define VIC_REG_BG_COLOR0       0x21
#define VIC_REG_BG_COLOR1       0x22
#define VIC_REG_BG_COLOR2       0x23
#define VIC_REG_BG_COLOR3       0x24
#define VIC_REG_SPRITE_MCOLOR0  0x25
#define VIC_REG_SPRITE_MCOLOR1  0x26
#define VIC_REG_SPRITE0_COLOR   0x27
/* ... through 0x2E for sprite colors */

/* Compatibility aliases for test suite (without REG_ prefix) */
#define VIC_SPRITE0_X       VIC_REG_SPRITE0_X
#define VIC_SPRITE0_Y       VIC_REG_SPRITE0_Y
#define VIC_SPRITE_X_MSB    VIC_REG_SPRITE_X_MSB
#define VIC_CONTROL1        VIC_REG_CONTROL1
#define VIC_RASTER          VIC_REG_RASTER
#define VIC_LIGHTPEN_X      VIC_REG_LIGHTPEN_X
#define VIC_LIGHTPEN_Y      VIC_REG_LIGHTPEN_Y
#define VIC_SPRITE_ENABLE   VIC_REG_SPRITE_ENABLE
#define VIC_CONTROL2        VIC_REG_CONTROL2
#define VIC_SPRITE_Y_EXPAND VIC_REG_SPRITE_Y_EXP
#define VIC_MEM_POINTERS    VIC_REG_MEM_POINTERS
#define VIC_IRQ_STATUS      VIC_REG_IRQ_STATUS
#define VIC_IRQ_ENABLE      VIC_REG_IRQ_ENABLE
#define VIC_SPRITE_PRIORITY VIC_REG_SPRITE_PRIORITY
#define VIC_SPRITE_MCOL     VIC_REG_SPRITE_MCOL
#define VIC_SPRITE_X_EXPAND VIC_REG_SPRITE_X_EXP
#define VIC_BORDER_COLOR    VIC_REG_BORDER_COLOR
#define VIC_BG_COLOR0       VIC_REG_BG_COLOR0
#define VIC_BG_COLOR1       VIC_REG_BG_COLOR1
#define VIC_BG_COLOR2       VIC_REG_BG_COLOR2
#define VIC_BG_COLOR3       VIC_REG_BG_COLOR3
#define VIC_SPRITE_MCOLOR0  VIC_REG_SPRITE_MCOLOR0
#define VIC_SPRITE_MCOLOR1  VIC_REG_SPRITE_MCOLOR1
#define VIC_INTERRUPT       VIC_REG_IRQ_STATUS
#define VIC_SPRITE_SPRITE_COLL VIC_REG_SPRITE_COLL
#define VIC_SPRITE_DATA_COLL VIC_REG_SPRITE_DATA_COLL
#define VIC_BACKGROUND0     VIC_REG_BG_COLOR0
#define VIC_BACKGROUND1     VIC_REG_BG_COLOR1
#define VIC_BACKGROUND2     VIC_REG_BG_COLOR2
#define VIC_BACKGROUND3     VIC_REG_BG_COLOR3
#define VIC_SPRITE0_COLOR   VIC_REG_SPRITE0_COLOR
#define VIC_IEN_RASTER      VIC_IRQ_RASTER

/* Control Register 1 Bits ($D011) */
#define VIC_CTRL1_YSCROLL   0x07    /* Bits 0-2: Y Scroll */
#define VIC_CTRL1_RSEL      0x08    /* Bit 3: Row select (24/25) */
#define VIC_CTRL1_DEN       0x10    /* Bit 4: Display enable */
#define VIC_CTRL1_BMM       0x20    /* Bit 5: Bitmap mode */
#define VIC_CTRL1_ECM       0x40    /* Bit 6: Extended color mode */
#define VIC_CTRL1_RST8      0x80    /* Bit 7: Raster bit 8 */

/* Control Register 2 Bits ($D016) */
#define VIC_CTRL2_XSCROLL   0x07    /* Bits 0-2: X Scroll */
#define VIC_CTRL2_CSEL      0x08    /* Bit 3: Column select (38/40) */
#define VIC_CTRL2_MCM       0x10    /* Bit 4: Multicolor mode */
#define VIC_CTRL2_RES       0x20    /* Bit 5: Reset */

/* IRQ Status/Enable Bits ($D019/$D01A) */
#define VIC_IRQ_RASTER      0x01    /* Raster interrupt */
#define VIC_IRQ_SPRITE_BG   0x02    /* Sprite-background collision */
#define VIC_IRQ_SPRITE_SPR  0x04    /* Sprite-sprite collision */
#define VIC_IRQ_LIGHTPEN    0x08    /* Light pen */
#define VIC_IRQ_ANY         0x80    /* Any interrupt (read-only) */

/* VIC-II State Structure */
typedef struct {
    /* Registers */
    uint8_t sprite_x[VIC_NUM_SPRITES];
    uint8_t sprite_y[VIC_NUM_SPRITES];
    uint8_t sprite_x_msb;
    uint8_t control1;
    uint8_t raster_compare;     /* Latched raster line for IRQ */
    uint8_t lightpen_x;
    uint8_t lightpen_y;
    uint8_t sprite_enable;
    uint8_t control2;
    uint8_t sprite_y_expand;
    uint8_t mem_pointers;
    uint8_t irq_status;
    uint8_t irq_enable;
    uint8_t sprite_priority;
    uint8_t sprite_multicolor;
    uint8_t sprite_x_expand;
    uint8_t sprite_sprite_coll;
    uint8_t sprite_data_coll;
    uint8_t border_color;
    uint8_t background[4];
    uint8_t sprite_mcolor[2];
    uint8_t sprite_color[VIC_NUM_SPRITES];

    /* Compatibility alias for test suite */
    #define background_color background
    #define interrupt irq_status

    /* Color RAM (directly accessible) */
    uint8_t color_ram[COLOR_RAM_SIZE];

    /* Internal State */
    uint16_t current_raster;    /* Current raster line (0-311 PAL) */
    uint8_t  current_cycle;     /* Current cycle within line (0-62 PAL) */
    
    uint16_t vc;                /* Video counter (0-999) */
    uint16_t vc_base;           /* Video counter base */
    uint8_t  rc;                /* Row counter (0-7) */
    
    bool     bad_line;          /* Bad line condition active */
    bool     ba_low;            /* BA signal low (CPU stalled) */
    bool     display_state;     /* Display state (vs idle) */
    
    bool     irq_pending;       /* IRQ signal to CPU */
    bool     frame_complete;    /* Frame just finished rendering */

    /* Memory interface */
    uint8_t *memory;            /* Pointer to main memory */
    
    /* Bank selection (from CIA2) */
    uint8_t  bank;              /* VIC memory bank (0-3) */

    /* Framebuffer for pixel rendering (stub for test compatibility) */
    uint32_t framebuffer[VIC_SCREEN_HEIGHT][VIC_SCREEN_WIDTH];
} VIC;

/* VIC-II Functions */
void vic_init(VIC *vic, uint8_t *memory);
void vic_reset(VIC *vic);

/* Register access */
uint8_t vic_read(VIC *vic, uint16_t addr);
void vic_write(VIC *vic, uint16_t addr, uint8_t data);

/* Color RAM access */
uint8_t vic_read_color(VIC *vic, uint16_t addr);
void vic_write_color(VIC *vic, uint16_t addr, uint8_t data);

/* Clock the VIC by one cycle */
void vic_clock(VIC *vic);

/* Get IRQ status */
bool vic_get_irq(VIC *vic);

/* Get BA signal (for CPU stalling) */
bool vic_get_ba(VIC *vic);

/* Check if frame is complete */
bool vic_frame_complete(VIC *vic);
void vic_clear_frame_flag(VIC *vic);

/* ANSI Console Rendering */
void vic_render_ansi(VIC *vic);

/* Memory address helpers */
uint16_t vic_get_screen_addr(VIC *vic);
uint16_t vic_get_char_addr(VIC *vic);
uint16_t vic_get_bitmap_addr(VIC *vic);

/* Compatibility aliases for address functions */
#define vic_get_screen_base  vic_get_screen_addr
#define vic_get_charset_base vic_get_char_addr
#define vic_get_bitmap_base  vic_get_bitmap_addr

/* Total raster lines constant */
#define VIC_TOTAL_RASTERS VIC_PAL_LINES_PER_FRAME

/* C64 Color Palette (ANSI 256-color approximations) */
extern const uint8_t c64_to_ansi256[16];

/* Additional compatibility aliases for IRQ bits */
#define VIC_IEN_SPRITE      VIC_IRQ_SPRITE_SPR
#define VIC_IEN_DATA        VIC_IRQ_SPRITE_BG
#define VIC_IEN_LIGHTPEN    VIC_IRQ_LIGHTPEN
#define VIC_IRQ_SPRITE      VIC_IRQ_SPRITE_SPR
#define VIC_IRQ_DATA        VIC_IRQ_SPRITE_BG

/* Inline helper functions for test compatibility */
static inline bool vic_is_sprite_enabled(VIC *vic, int n) {
    return (vic->sprite_enable >> n) & 1;
}

static inline uint16_t vic_get_sprite_x(VIC *vic, int n) {
    uint16_t x = vic->sprite_x[n];
    if (vic->sprite_x_msb & (1 << n)) x |= 0x100;
    return x;
}

static inline uint8_t vic_get_sprite_y(VIC *vic, int n) {
    return vic->sprite_y[n];
}

static inline bool vic_is_sprite_x_expand(VIC *vic, int n) {
    return (vic->sprite_x_expand >> n) & 1;
}

static inline bool vic_is_sprite_y_expand(VIC *vic, int n) {
    return (vic->sprite_y_expand >> n) & 1;
}

static inline bool vic_is_sprite_multicolor(VIC *vic, int n) {
    return (vic->sprite_multicolor >> n) & 1;
}

static inline uint8_t vic_get_sprite_color(VIC *vic, int n) {
    return vic->sprite_color[n] & 0x0F;
}

static inline bool vic_is_sprite_behind_bg(VIC *vic, int n) {
    return (vic->sprite_priority >> n) & 1;
}

static inline bool vic_is_bad_line(VIC *vic) {
    return vic->bad_line;
}

static inline bool vic_is_display_enabled(VIC *vic) {
    return (vic->control1 & VIC_CTRL1_DEN) != 0;
}

static inline bool vic_is_bitmap_mode(VIC *vic) {
    return (vic->control1 & VIC_CTRL1_BMM) != 0;
}

static inline bool vic_is_multicolor_mode(VIC *vic) {
    return (vic->control2 & VIC_CTRL2_MCM) != 0;
}

static inline bool vic_is_extended_color_mode(VIC *vic) {
    return (vic->control1 & VIC_CTRL1_ECM) != 0;
}

static inline uint8_t vic_get_x_scroll(VIC *vic) {
    return vic->control2 & VIC_CTRL2_XSCROLL;
}

static inline uint8_t vic_get_y_scroll(VIC *vic) {
    return vic->control1 & VIC_CTRL1_YSCROLL;
}

static inline uint16_t vic_get_raster_line(VIC *vic) {
    return vic->current_raster;
}

/* Framebuffer functions - implemented in vic.c for proper linkage */
uint32_t vic_color_to_rgb(uint8_t color);
void vic_clear_framebuffer(VIC *vic);
void vic_get_framebuffer(VIC *vic, uint32_t *buffer, size_t num_pixels);

static inline void vic_render_line(VIC *vic, int line) {
    (void)vic;
    (void)line;
    /* No-op stub for test compatibility */
}

#endif /* VIC_H */
