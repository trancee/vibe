#ifndef VICII_H
#define VICII_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// VIC-II Register Addresses (at $D000-$D03E)
#define VICII_SPRITE0_X       0xD000
#define VICII_SPRITE0_Y       0xD001
#define VICII_SPRITE1_X       0xD002
#define VICII_SPRITE1_Y       0xD003
#define VICII_SPRITE2_X       0xD004
#define VICII_SPRITE2_Y       0xD005
#define VICII_SPRITE3_X       0xD006
#define VICII_SPRITE3_Y       0xD007
#define VICII_SPRITE4_X       0xD008
#define VICII_SPRITE4_Y       0xD009
#define VICII_SPRITE5_X       0xD00A
#define VICII_SPRITE5_Y       0xD00B
#define VICII_SPRITE6_X       0xD00C
#define VICII_SPRITE6_Y       0xD00D
#define VICII_SPRITE7_X       0xD00E
#define VICII_SPRITE7_Y       0xD00F
#define VICII_SPRITE_X_MSB    0xD010
#define VICII_CONTROL1        0xD011
#define VICII_RASTER          0xD012
#define VICII_LIGHTPEN_X      0xD013
#define VICII_LIGHTPEN_Y      0xD014
#define VICII_SPRITE_ENABLE   0xD015
#define VICII_CONTROL2        0xD016
#define VICII_SPRITE_Y_EXPAND 0xD017
#define VICII_MEM_POINTERS    0xD018
#define VICII_INTERRUPT       0xD019
#define VICII_IRQ_ENABLE      0xD01A
#define VICII_SPRITE_PRIORITY 0xD01B
#define VICII_SPRITE_MCOL     0xD01C
#define VICII_SPRITE_X_EXPAND 0xD01D
#define VICII_SPRITE_SPRITE_COLL  0xD01E
#define VICII_SPRITE_DATA_COLL    0xD01F
#define VICII_BORDER_COLOR    0xD020
#define VICII_BACKGROUND0     0xD021
#define VICII_BACKGROUND1     0xD022
#define VICII_BACKGROUND2     0xD023
#define VICII_BACKGROUND3     0xD024
#define VICII_SPRITE_MCOLOR0  0xD025
#define VICII_SPRITE_MCOLOR1  0xD026
#define VICII_SPRITE0_COLOR   0xD027
#define VICII_SPRITE1_COLOR   0xD028
#define VICII_SPRITE2_COLOR   0xD029
#define VICII_SPRITE3_COLOR   0xD02A
#define VICII_SPRITE4_COLOR   0xD02B
#define VICII_SPRITE5_COLOR   0xD02C
#define VICII_SPRITE6_COLOR   0xD02D
#define VICII_SPRITE7_COLOR   0xD02E
#define VICII_KERNAL_REV      0xD02F  // Not actually a VIC register
#define VICII_KERNAL_LOW      0xD030  // Not actually a VIC register
#define VICII_KERNAL_HIGH     0xD031  // Not actually a VIC register

// VIC-II Control 1 Register Bits ($D011)
#define VICII_CTRL1_BMM       0x20  // Bit 5: Bitmap Mode
#define VICII_CTRL1_DEN       0x10  // Bit 4: Display Enable
#define VICII_CTRL1_RSEL      0x08  // Bit 3: Row Select (24/25 row select)
#define VICII_CTRL1_YSCROLL   0x07  // Bits 0-2: Y Scroll

// VIC-II Control 2 Register Bits ($D016)
#define VICII_CTRL2_MCM       0x10  // Bit 4: Multicolor Mode
#define VICII_CTRL2_RES       0x08  // Bit 3: Select 40/25 column
#define VICII_CTRL2_XSCROLL   0x07  // Bits 0-2: X Scroll

// VIC-II Memory Pointers Register Bits ($D018)
#define VICII_VM13            0xE0  // Bits 5-7: Video matrix base (bits 13-15)
#define VICII_CB13            0x0E  // Bits 1-3: Character base (bits 13-15)
#define VICII_VM12            0x01  // Bit 0: Video matrix base (bit 10)

// VIC-II Interrupt Register Bits ($D019)
#define VICII_IRQ_RST         0x80  // Bit 7: IRQ reset (set to 1 to clear)
#define VICII_IRQ_LIGHTPEN    0x08  // Bit 3: Lightpen interrupt
#define VICII_IRQ_SPRITE      0x04  // Bit 2: Sprite-sprite collision interrupt
#define VICII_IRQ_DATA        0x02  // Bit 1: Sprite-background collision interrupt
#define VICII_IRQ_RASTER      0x01  // Bit 0: Raster interrupt

// VIC-II IRQ Enable Register Bits ($D01A)
#define VICII_IEN_LIGHTPEN    0x08  // Bit 3: Enable lightpen interrupt
#define VICII_IEN_SPRITE      0x04  // Bit 2: Enable sprite-sprite collision interrupt
#define VICII_IEN_DATA        0x02  // Bit 1: Enable sprite-background collision interrupt
#define VICII_IEN_RASTER      0x01  // Bit 0: Enable raster interrupt

// VIC-II Screen Dimensions
#define VICII_SCREEN_WIDTH    320
#define VICII_SCREEN_HEIGHT   200
#define VICII_VISIBLE_WIDTH   320
#define VICII_VISIBLE_HEIGHT  200
#define VICII_TOTAL_RASTERS   312
#define VICII_FIRST_VISIBLE_LINE  16
#define VICII_LAST_VISIBLE_LINE   215

// VIC-II Sprites
#define VICII_NUM_SPRITES     8
#define VICII_SPRITE_WIDTH    24
#define VICII_SPRITE_HEIGHT   21
#define VICII_SPRITE_DATA_SIZE 63

typedef struct {
    // VIC-II Registers
    uint8_t sprite_x[VICII_NUM_SPRITES];
    uint8_t sprite_y[VICII_NUM_SPRITES];
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
    uint8_t sprite_color[VICII_NUM_SPRITES];
    
    // Internal state
    uint16_t current_raster;
    uint8_t raster_compare;
    bool irq_pending;
    bool bad_line;
    uint16_t vc;          // Video counter
    uint16_t vc_base;     // Video counter base
    uint16_t rc;          // Raster counter
    uint16_t vmli;        // Video matrix line index
    bool display_state;
    bool idle_state;
    
    // Color memory (at $D800-$DBFF)
    uint8_t color_ram[1024];
    
    // Screen buffer for rendering
    uint32_t framebuffer[VICII_SCREEN_HEIGHT][VICII_SCREEN_WIDTH];
    
    // Memory access callback functions
    uint8_t (*read_mem)(void *context, uint16_t addr);
    void (*write_mem)(void *context, uint16_t addr, uint8_t data);
    void *memory_context;
} VICII;

// VIC-II Functions
void vicii_init(VICII *vicii);
void vicii_reset(VICII *vicii);
uint8_t vicii_read(VICII *vicii, uint16_t addr);
void vicii_write(VICII *vicii, uint16_t addr, uint8_t data);
void vicii_clock(VICII *vicii);
void vicii_render_line(VICII *vicii, uint16_t raster_line);
bool vicii_get_irq(VICII *vicii);
void vicii_set_memory_callbacks(VICII *vicii, void *context, 
                                uint8_t (*read_mem)(void *context, uint16_t addr),
                                void (*write_mem)(void *context, uint16_t addr, uint8_t data));

// Helper functions
bool vicii_is_bad_line(VICII *vicii);
bool vicii_is_display_enabled(VICII *vicii);
uint16_t vicii_get_screen_base(VICII *vicii);
uint16_t vicii_get_charset_base(VICII *vicii);
uint16_t vicii_get_bitmap_base(VICII *vicii);
bool vicii_is_sprite_enabled(VICII *vicii, uint8_t sprite);
bool vicii_is_sprite_x_expand(VICII *vicii, uint8_t sprite);
bool vicii_is_sprite_y_expand(VICII *vicii, uint8_t sprite);
bool vicii_is_sprite_multicolor(VICII *vicii, uint8_t sprite);
uint16_t vicii_get_sprite_x(VICII *vicii, uint8_t sprite);
uint16_t vicii_get_sprite_y(VICII *vicii, uint8_t sprite);
uint8_t vicii_get_sprite_color(VICII *vicii, uint8_t sprite);

// Internal rendering functions
void vicii_render_text_line(VICII *vicii, uint16_t line, bool multicolor);
void vicii_render_bitmap_line(VICII *vicii, uint16_t line, bool multicolor);
void vicii_render_sprites_line(VICII *vicii, uint16_t line);

// Utility function
uint32_t vicii_color_to_rgb(uint8_t color);

// Utility functions
void vicii_clear_framebuffer(VICII *vicii);
void vicii_get_framebuffer(VICII *vicii, uint32_t *buffer, size_t size);

#endif // VICII_H