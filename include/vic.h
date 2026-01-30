#ifndef VIC_H
#define VIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define VIC_MEM_START 0xD000
#define VIC_MEM_END (VIC_MEM_START + VIC_MEM_SIZE - 1)
#define VIC_MEM_SIZE 0x0400

// VIC-II Register Addresses (at $D000-$D03E)
#define VIC_SPRITE0_X 0xD000
#define VIC_SPRITE0_Y 0xD001
#define VIC_SPRITE1_X 0xD002
#define VIC_SPRITE1_Y 0xD003
#define VIC_SPRITE2_X 0xD004
#define VIC_SPRITE2_Y 0xD005
#define VIC_SPRITE3_X 0xD006
#define VIC_SPRITE3_Y 0xD007
#define VIC_SPRITE4_X 0xD008
#define VIC_SPRITE4_Y 0xD009
#define VIC_SPRITE5_X 0xD00A
#define VIC_SPRITE5_Y 0xD00B
#define VIC_SPRITE6_X 0xD00C
#define VIC_SPRITE6_Y 0xD00D
#define VIC_SPRITE7_X 0xD00E
#define VIC_SPRITE7_Y 0xD00F
#define VIC_SPRITE_X_MSB 0xD010
#define VIC_CONTROL1 0xD011
#define VIC_RASTER 0xD012
#define VIC_LIGHTPEN_X 0xD013
#define VIC_LIGHTPEN_Y 0xD014
#define VIC_SPRITE_ENABLE 0xD015
#define VIC_CONTROL2 0xD016
#define VIC_SPRITE_Y_EXPAND 0xD017
#define VIC_MEM_POINTERS 0xD018
#define VIC_INTERRUPT 0xD019
#define VIC_IRQ_ENABLE 0xD01A
#define VIC_SPRITE_PRIORITY 0xD01B
#define VIC_SPRITE_MCOL 0xD01C
#define VIC_SPRITE_X_EXPAND 0xD01D
#define VIC_SPRITE_SPRITE_COLL 0xD01E
#define VIC_SPRITE_DATA_COLL 0xD01F
#define VIC_BORDER_COLOR 0xD020
#define VIC_BACKGROUND0 0xD021
#define VIC_BACKGROUND1 0xD022
#define VIC_BACKGROUND2 0xD023
#define VIC_BACKGROUND3 0xD024
#define VIC_SPRITE_MCOLOR0 0xD025
#define VIC_SPRITE_MCOLOR1 0xD026
#define VIC_SPRITE0_COLOR 0xD027
#define VIC_SPRITE1_COLOR 0xD028
#define VIC_SPRITE2_COLOR 0xD029
#define VIC_SPRITE3_COLOR 0xD02A
#define VIC_SPRITE4_COLOR 0xD02B
#define VIC_SPRITE5_COLOR 0xD02C
#define VIC_SPRITE6_COLOR 0xD02D
#define VIC_SPRITE7_COLOR 0xD02E
#define VIC_KERNAL_REV 0xD02F  // Not actually a VIC register
#define VIC_KERNAL_LOW 0xD030  // Not actually a VIC register
#define VIC_KERNAL_HIGH 0xD031 // Not actually a VIC register

// VIC-II Control 1 Register Bits ($D011)
#define VIC_CTRL1_BMM 0x20     // Bit 5: Bitmap Mode
#define VIC_CTRL1_DEN 0x10     // Bit 4: Display Enable
#define VIC_CTRL1_RSEL 0x08    // Bit 3: Row Select (24/25 row select)
#define VIC_CTRL1_YSCROLL 0x07 // Bits 0-2: Y Scroll

// VIC-II Control 2 Register Bits ($D016)
#define VIC_CTRL2_MCM 0x10     // Bit 4: Multicolor Mode
#define VIC_CTRL2_RES 0x08     // Bit 3: Select 40/25 column
#define VIC_CTRL2_XSCROLL 0x07 // Bits 0-2: X Scroll

// VIC-II Memory Pointers Register Bits ($D018)
#define VIC_VM13 0xF0 // Bits 4-7: Video matrix base address (bits 10-13 of address)
#define VIC_CB13 0x0E // Bits 1-3: Character/bitmap base address (bits 11-13 of address)
// Note: Bit 0 is unused

// VIC-II Interrupt Register Bits ($D019)
#define VIC_IRQ_RST 0x80      // Bit 7: IRQ reset (set to 1 to clear)
#define VIC_IRQ_LIGHTPEN 0x08 // Bit 3: Lightpen interrupt
#define VIC_IRQ_SPRITE 0x04   // Bit 2: Sprite-sprite collision interrupt
#define VIC_IRQ_DATA 0x02     // Bit 1: Sprite-background collision interrupt
#define VIC_IRQ_RASTER 0x01   // Bit 0: Raster interrupt

// VIC-II IRQ Enable Register Bits ($D01A)
#define VIC_IEN_LIGHTPEN 0x08 // Bit 3: Enable lightpen interrupt
#define VIC_IEN_SPRITE 0x04   // Bit 2: Enable sprite-sprite collision interrupt
#define VIC_IEN_DATA 0x02     // Bit 1: Enable sprite-background collision interrupt
#define VIC_IEN_RASTER 0x01   // Bit 0: Enable raster interrupt

// VIC-II Screen Dimensions
#define VIC_SCREEN_WIDTH 320
#define VIC_SCREEN_HEIGHT 200
#define VIC_VISIBLE_WIDTH 320
#define VIC_VISIBLE_HEIGHT 200
#define VIC_TOTAL_RASTERS 312
#define VIC_FIRST_VISIBLE_LINE 16
#define VIC_LAST_VISIBLE_LINE 215

// VIC-II Sprites
#define VIC_NUM_SPRITES 8
#define VIC_SPRITE_WIDTH 24
#define VIC_SPRITE_HEIGHT 21
#define VIC_SPRITE_DATA_SIZE 63

typedef uint8_t (*read_mem_t)(uint8_t *mem, uint16_t addr);
typedef void (*write_mem_t)(uint8_t *mem, uint16_t addr, uint8_t data);

typedef struct
{
    // VIC-II Registers
    uint8_t sprite_x[VIC_NUM_SPRITES];
    uint8_t sprite_y[VIC_NUM_SPRITES];
    uint8_t sprite_x_msb;
    uint8_t control1;
    uint8_t raster;
    uint8_t lightpen_x;
    uint8_t lightpen_y;
    uint8_t sprite_enable;
    uint8_t control2;
    uint8_t sprite_y_expand;
    uint8_t mem_pointers;
    uint8_t interrupt;
    uint8_t irq_enable;
    uint8_t sprite_priority;
    uint8_t sprite_mcol;
    uint8_t sprite_x_expand;
    uint8_t sprite_sprite_coll;
    uint8_t sprite_data_coll;
    uint8_t border_color;
    uint8_t background_color[4];
    uint8_t sprite_mcolor[2];
    uint8_t sprite_color[VIC_NUM_SPRITES];

    // Internal state
    uint16_t current_raster;
    uint8_t raster_compare;
    bool irq_pending;
    bool bad_line;
    uint16_t vc;      // Video counter
    uint16_t vc_base; // Video counter base
    uint16_t rc;      // Raster counter
    uint16_t vmli;    // Video matrix line index
    bool display_state;
    bool idle_state;

    uint8_t *memory;

    // Color memory (at $D800-$DBFF)
    uint8_t color_ram[1024];

    // Screen buffer for rendering
    uint32_t framebuffer[VIC_SCREEN_HEIGHT][VIC_SCREEN_WIDTH];
} VIC;

// VIC-II Functions
void vic_init(VIC *vic, uint8_t *memory);
void vic_reset(VIC *vic);

uint8_t vic_read(VIC *vic, uint16_t addr);
void vic_write(VIC *vic, uint16_t addr, uint8_t data);

void vic_clock(VIC *vic);

void vic_render_line(VIC *vic, uint16_t raster_line);
bool vic_get_irq(VIC *vic);

// Helper functions
bool vic_is_bad_line(VIC *vic);
bool vic_is_display_enabled(VIC *vic);
uint16_t vic_get_screen_base(VIC *vic);
uint16_t vic_get_charset_base(VIC *vic);
uint16_t vic_get_bitmap_base(VIC *vic);
bool vic_is_sprite_enabled(VIC *vic, uint8_t sprite);
bool vic_is_sprite_x_expand(VIC *vic, uint8_t sprite);
bool vic_is_sprite_y_expand(VIC *vic, uint8_t sprite);
bool vic_is_sprite_multicolor(VIC *vic, uint8_t sprite);
uint16_t vic_get_sprite_x(VIC *vic, uint8_t sprite);
uint16_t vic_get_sprite_y(VIC *vic, uint8_t sprite);
uint8_t vic_get_sprite_color(VIC *vic, uint8_t sprite);

// Internal rendering functions
void vic_render_text_line(VIC *vic, uint16_t line, bool multicolor);
void vic_render_bitmap_line(VIC *vic, uint16_t line, bool multicolor);
void vic_render_sprites_line(VIC *vic, uint16_t line);

// Utility function
uint32_t vic_color_to_rgb(uint8_t color);

// Utility functions
void vic_clear_framebuffer(VIC *vic);
void vic_get_framebuffer(VIC *vic, uint32_t *buffer, size_t size);

#endif // VIC_H