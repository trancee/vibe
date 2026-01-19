#include "../include/vicii.h"
#include <string.h>
#include <assert.h>

void vicii_init(VICII *vicii)
{
    if (vicii == NULL) return;
    
    memset(vicii, 0, sizeof(VICII));
    
    // Initialize to power-up state
    vicii->control1 = VICII_CTRL1_DEN | VICII_CTRL1_RSEL;  // Display enabled, 25 rows
    vicii->control2 = 0xC8;  // 40 columns, no multicolor
    vicii->border_color = 0x0E;  // Light blue border
    vicii->background_color[0] = 0x06;  // Blue background
    vicii->background_color[1] = 0x00;
    vicii->background_color[2] = 0x00;
    vicii->background_color[3] = 0x00;
    
    // Initialize raster
    vicii->raster = 0;
    vicii->current_raster = 0;
    vicii->raster_compare = 0;
    
    // Clear color RAM
    memset(vicii->color_ram, 0, sizeof(vicii->color_ram));
    
    // Clear framebuffer
    vicii_clear_framebuffer(vicii);
    
    // Display state
    vicii->display_state = false;
    vicii->idle_state = true;
    vicii->bad_line = false;
}

void vicii_reset(VICII *vicii)
{
    if (vicii == NULL) return;
    vicii_init(vicii);
}

uint8_t vicii_read(VICII *vicii, uint16_t addr)
{
    if (vicii == NULL || addr < 0xD000 || addr > 0xD02E) return 0;
    
    uint16_t offset = addr - 0xD000;
    uint8_t result = 0;
    
    switch (offset) {
        case 0x00: case 0x02: case 0x04: case 0x06:
        case 0x08: case 0x0A: case 0x0C: case 0x0E:
            result = vicii->sprite_x[offset >> 1];
            break;
            
        case 0x01: case 0x03: case 0x05: case 0x07:
        case 0x09: case 0x0B: case 0x0D: case 0x0F:
            result = vicii->sprite_y[offset >> 1];
            break;
            
        case 0x10:  // Sprite X MSB
            result = vicii->sprite_x_msb;
            break;
            
        case 0x11:  // Control 1
            result = vicii->control1 | 0x80;  // Bit 7 always reads as 1
            break;
            
        case 0x12:  // Current raster line
            result = vicii->current_raster & 0xFF;
            break;
            
        case 0x13:  // Lightpen X
            result = vicii->lightpen_x;
            break;
            
        case 0x14:  // Lightpen Y
            result = vicii->lightpen_y;
            break;
            
        case 0x15:  // Sprite enable
            result = vicii->sprite_enable;
            break;
            
        case 0x16:  // Control 2
            result = vicii->control2;
            break;
            
        case 0x17:  // Sprite Y expand
            result = vicii->sprite_y_expand;
            break;
            
        case 0x18:  // Memory pointers
            result = vicii->mem_pointers;
            break;
            
        case 0x19:  // Interrupt register
            result = vicii->interrupt;
            break;
            
        case 0x1A:  // IRQ enable
            result = vicii->irq_enable;
            break;
            
        case 0x1B:  // Sprite priority
            result = vicii->sprite_priority;
            break;
            
        case 0x1C:  // Sprite multicolor
            result = vicii->sprite_mcol;
            break;
            
        case 0x1D:  // Sprite X expand
            result = vicii->sprite_x_expand;
            break;
            
        case 0x1E:  // Sprite-sprite collision
            result = vicii->sprite_sprite_coll;
            // Reading clears the register
            vicii->sprite_sprite_coll = 0;
            break;
            
        case 0x1F:  // Sprite-data collision
            result = vicii->sprite_data_coll;
            // Reading clears the register
            vicii->sprite_data_coll = 0;
            break;
            
        case 0x20:  // Border color
            result = vicii->border_color;
            break;
            
        case 0x21: case 0x22: case 0x23: case 0x24:
            result = vicii->background_color[offset - 0x21];
            break;
            
        case 0x25: case 0x26:
            result = vicii->sprite_mcolor[offset - 0x25];
            break;
            
        case 0x27: case 0x28: case 0x29: case 0x2A:
        case 0x2B: case 0x2C: case 0x2D: case 0x2E:
            result = vicii->sprite_color[offset - 0x27];
            break;
            
        default:
            result = 0;
            break;
    }
    
    return result;
}

void vicii_write(VICII *vicii, uint16_t addr, uint8_t data)
{
    if (vicii == NULL || addr < 0xD000 || addr > 0xD02E) return;
    
    uint16_t offset = addr - 0xD000;
    
    switch (offset) {
        case 0x00: case 0x02: case 0x04: case 0x06:
        case 0x08: case 0x0A: case 0x0C: case 0x0E:
            vicii->sprite_x[offset >> 1] = data;
            break;
            
        case 0x01: case 0x03: case 0x05: case 0x07:
        case 0x09: case 0x0B: case 0x0D: case 0x0F:
            vicii->sprite_y[offset >> 1] = data;
            break;
            
        case 0x10:  // Sprite X MSB
            vicii->sprite_x_msb = data;
            break;
            
        case 0x11:  // Control 1
            vicii->control1 = data;
            break;
            
        case 0x12:  // Raster compare
            vicii->raster_compare = data;
            break;
            
        case 0x13:  // Lightpen X (write-only)
            vicii->lightpen_x = data;
            break;
            
        case 0x14:  // Lightpen Y (write-only)
            vicii->lightpen_y = data;
            break;
            
        case 0x15:  // Sprite enable
            vicii->sprite_enable = data;
            break;
            
        case 0x16:  // Control 2
            vicii->control2 = data;
            break;
            
        case 0x17:  // Sprite Y expand
            vicii->sprite_y_expand = data;
            break;
            
        case 0x18:  // Memory pointers
            vicii->mem_pointers = data;
            break;
            
        case 0x19:  // Interrupt register
            // Bits 0-3 are cleared by writing 1 to them
            vicii->interrupt &= ~(data & 0x0F);
            break;
            
        case 0x1A:  // IRQ enable
            vicii->irq_enable = data & 0x0F;
            break;
            
        case 0x1B:  // Sprite priority
            vicii->sprite_priority = data;
            break;
            
        case 0x1C:  // Sprite multicolor
            vicii->sprite_mcol = data;
            break;
            
        case 0x1D:  // Sprite X expand
            vicii->sprite_x_expand = data;
            break;
            
        case 0x1E:  // Sprite-sprite collision (read-only)
            // Cannot write, only clear by reading
            break;
            
        case 0x1F:  // Sprite-data collision (read-only)
            // Cannot write, only clear by reading
            break;
            
        case 0x20:  // Border color
            vicii->border_color = data & 0x0F;
            break;
            
        case 0x21: case 0x22: case 0x23: case 0x24:
            vicii->background_color[offset - 0x21] = data & 0x0F;
            break;
            
        case 0x25: case 0x26:
            vicii->sprite_mcolor[offset - 0x25] = data & 0x0F;
            break;
            
        case 0x27: case 0x28: case 0x29: case 0x2A:
        case 0x2B: case 0x2C: case 0x2D: case 0x2E:
            vicii->sprite_color[offset - 0x27] = data & 0x0F;
            break;
    }
}

void vicii_clock(VICII *vicii)
{
    if (vicii == NULL) return;
    
    // Update raster line
    vicii->current_raster++;
    if (vicii->current_raster >= VICII_TOTAL_RASTERS) {
        vicii->current_raster = 0;
    }
    
    // Update raster register
    vicii->raster = vicii->current_raster & 0xFF;
    
    // Check for bad line condition
    vicii->bad_line = vicii_is_bad_line(vicii);
    
    // Check for raster interrupt
    if (vicii->current_raster == vicii->raster_compare) {
        vicii->interrupt |= VICII_IRQ_RASTER;
    }
    
    // Render current line if in visible area
    if (vicii->current_raster >= VICII_FIRST_VISIBLE_LINE && 
        vicii->current_raster <= VICII_LAST_VISIBLE_LINE) {
        vicii_render_line(vicii, vicii->current_raster);
    }
}

void vicii_render_line(VICII *vicii, uint16_t raster_line)
{
    if (vicii == NULL) return;
    
    uint16_t display_line = raster_line - VICII_FIRST_VISIBLE_LINE;
    
    if (display_line >= VICII_SCREEN_HEIGHT) return;
    
    // Clear line to border color initially
    uint32_t border_rgb = vicii_color_to_rgb(vicii->border_color);
    
    for (int x = 0; x < VICII_SCREEN_WIDTH; x++) {
        vicii->framebuffer[display_line][x] = border_rgb;
    }
    
    if (!vicii_is_display_enabled(vicii)) {
        return;
    }
    
    // Determine text/bitmap mode
    bool bitmap_mode = (vicii->control1 & VICII_CTRL1_BMM) != 0;
    bool multicolor_mode = (vicii->control2 & VICII_CTRL2_MCM) != 0;
    
    if (bitmap_mode) {
        vicii_render_bitmap_line(vicii, display_line, multicolor_mode);
    } else {
        vicii_render_text_line(vicii, display_line, multicolor_mode);
    }
    
    // Render sprites
    vicii_render_sprites_line(vicii, display_line);
}

void vicii_render_text_line(VICII *vicii, uint16_t line, bool multicolor)
{
    uint16_t screen_base = vicii_get_screen_base(vicii);
    uint16_t charset_base = vicii_get_charset_base(vicii);
    uint8_t x_scroll = vicii->control2 & VICII_CTRL2_XSCROLL;
    uint8_t y_scroll = vicii->control1 & VICII_CTRL1_YSCROLL;
    
    // Adjust for scrolling
    uint16_t text_line = (line + y_scroll) / 8;
    uint8_t char_line = (line + y_scroll) % 8;
    
    // Render characters
    for (int col = 0; col < 40; col++) {
        uint16_t screen_addr = screen_base + text_line * 40 + col;
        uint8_t char_code = vicii->read_mem(vicii->memory_context, screen_addr);
        uint8_t color = vicii->color_ram[text_line * 40 + col] & 0x0F;
        
        // Get character data from charset
        uint16_t char_data_addr = charset_base + char_code * 8 + char_line;
        uint8_t char_data = vicii->read_mem(vicii->memory_context, char_data_addr);
        
        // Render character pixels
        for (int pixel = 0; pixel < 8; pixel++) {
            int x = col * 8 + pixel + x_scroll;
            if (x >= VICII_SCREEN_WIDTH) break;
            
            if (multicolor && (color & 0x08)) {
                // Multicolor text mode
                uint8_t color_bits = (char_data >> (7 - pixel)) & 0x03;
                uint32_t pixel_color;
                
                switch (color_bits) {
                    case 0: pixel_color = vicii_color_to_rgb(vicii->background_color[0]); break;
                    case 1: pixel_color = vicii_color_to_rgb(vicii->background_color[1]); break;
                    case 2: pixel_color = vicii_color_to_rgb(vicii->background_color[2]); break;
                    case 3: pixel_color = vicii_color_to_rgb(color & 0x07); break;
                    default: pixel_color = vicii_color_to_rgb(0); break;
                }
                
                vicii->framebuffer[line][x] = pixel_color;
            } else {
                // Standard text mode
                bool pixel_set = (char_data >> (7 - pixel)) & 0x01;
                uint32_t pixel_color = pixel_set ? 
                    vicii_color_to_rgb(color) : 
                    vicii_color_to_rgb(vicii->background_color[0]);
                
                vicii->framebuffer[line][x] = pixel_color;
            }
        }
    }
}

void vicii_render_bitmap_line(VICII *vicii, uint16_t line, bool multicolor)
{
    uint16_t bitmap_base = vicii_get_bitmap_base(vicii);
    uint16_t screen_base = vicii_get_screen_base(vicii);
    uint8_t y_scroll = vicii->control1 & VICII_CTRL1_YSCROLL;
    
    // Adjust for scrolling
    uint16_t bitmap_line = line + y_scroll;
    
    // Render bitmap
    for (int col = 0; col < 40; col++) {
        uint16_t bitmap_addr = bitmap_base + (bitmap_line / 8) * 320 + col * 8 + (bitmap_line % 8);
        uint16_t color_ram_addr = screen_base + (bitmap_line / 8) * 40 + col;
        
        uint8_t bitmap_data = vicii->read_mem(vicii->memory_context, bitmap_addr);
        uint8_t foreground_color = vicii->read_mem(vicii->memory_context, color_ram_addr) & 0x0F;
        uint8_t background_color = (vicii->read_mem(vicii->memory_context, color_ram_addr) >> 4) & 0x0F;
        
        for (int pixel = 0; pixel < 8; pixel++) {
            int x = col * 8 + pixel;
            if (x >= VICII_SCREEN_WIDTH) break;
            
            if (multicolor) {
                // Multicolor bitmap mode
                uint8_t color_bits = (bitmap_data >> (6 - pixel * 2)) & 0x03;
                uint32_t pixel_color;
                
                switch (color_bits) {
                    case 0: pixel_color = vicii_color_to_rgb(vicii->background_color[0]); break;
                    case 1: pixel_color = vicii_color_to_rgb(background_color); break;
                    case 2: pixel_color = vicii_color_to_rgb(foreground_color); break;
                    case 3: pixel_color = vicii_color_to_rgb(vicii->background_color[1]); break;
                    default: pixel_color = vicii_color_to_rgb(0); break;
                }
                
                vicii->framebuffer[line][x] = pixel_color;
            } else {
                // Standard bitmap mode
                bool pixel_set = (bitmap_data >> (7 - pixel)) & 0x01;
                uint32_t pixel_color = pixel_set ? 
                    vicii_color_to_rgb(foreground_color) : 
                    vicii_color_to_rgb(background_color);
                
                vicii->framebuffer[line][x] = pixel_color;
            }
        }
    }
}

void vicii_render_sprites_line(VICII *vicii, uint16_t line)
{
    if (!vicii->sprite_enable) return;
    
    for (int sprite = 0; sprite < VICII_NUM_SPRITES; sprite++) {
        if (!vicii_is_sprite_enabled(vicii, sprite)) continue;
        
        uint16_t sprite_y = vicii_get_sprite_y(vicii, sprite);
        uint16_t sprite_x = vicii_get_sprite_x(vicii, sprite);
        bool y_expand = vicii_is_sprite_y_expand(vicii, sprite);
        bool x_expand = vicii_is_sprite_x_expand(vicii, sprite);
        bool multicolor = vicii_is_sprite_multicolor(vicii, sprite);
        
        uint8_t sprite_height = y_expand ? 42 : 21;
        
        // Check if sprite is on this line
        if (line >= sprite_y && line < sprite_y + sprite_height) {
            uint8_t sprite_line = y_expand ? ((line - sprite_y) / 2) : (line - sprite_y);
            
            // Render sprite data
            for (int byte = 0; byte < 3; byte++) {
                uint16_t sprite_data_addr = (sprite << 6) + sprite_line * 3 + byte;
                uint8_t sprite_data = vicii->read_mem(vicii->memory_context, sprite_data_addr);
                
                for (int bit = 7; bit >= 0; bit--) {
                    int x = sprite_x + byte * 8 + (7 - bit);
                    if (x >= VICII_SCREEN_WIDTH) break;
                    
                    if (x_expand) {
                        x = sprite_x + (sprite_x + byte * 16 + (7 - bit) * 2);
                        if (x >= VICII_SCREEN_WIDTH) break;
                    }
                    
                    bool pixel_set = (sprite_data >> bit) & 0x01;
                    
                    if (pixel_set) {
                        uint32_t pixel_color;
                        
                        if (multicolor) {
                            // Get 2-bit color from sprite data
                            uint8_t color_bits = ((sprite_data >> (bit - 1)) & 0x03);
                            if (bit == 0) color_bits = (sprite_data & 0x01);
                            
                            switch (color_bits) {
                                case 0: pixel_color = vicii_color_to_rgb(0); break;  // Transparent
                                case 1: pixel_color = vicii_color_to_rgb(vicii->sprite_mcolor[0]); break;
                                case 2: pixel_color = vicii_color_to_rgb(vicii->sprite_color[sprite]); break;
                                case 3: pixel_color = vicii_color_to_rgb(vicii->sprite_mcolor[1]); break;
                                default: pixel_color = vicii_color_to_rgb(0); break;
                            }
                        } else {
                            pixel_color = vicii_color_to_rgb(vicii->sprite_color[sprite]);
                        }
                        
                        vicii->framebuffer[line][x] = pixel_color;
                        
                        if (x_expand && x + 1 < VICII_SCREEN_WIDTH) {
                            vicii->framebuffer[line][x + 1] = pixel_color;
                        }
                    }
                }
            }
        }
    }
}

bool vicii_get_irq(VICII *vicii)
{
    if (vicii == NULL) return false;
    
    vicii->irq_pending = (vicii->interrupt & vicii->irq_enable) != 0;
    return vicii->irq_pending;
}

void vicii_set_memory_callbacks(VICII *vicii, void *context, 
                                uint8_t (*read_mem)(void *context, uint16_t addr),
                                void (*write_mem)(void *context, uint16_t addr, uint8_t data))
{
    if (vicii == NULL) return;
    
    vicii->memory_context = context;
    vicii->read_mem = read_mem;
    vicii->write_mem = write_mem;
}

// Helper function implementations
bool vicii_is_bad_line(VICII *vicii)
{
    return (vicii->control1 & VICII_CTRL1_DEN) && 
           (vicii->current_raster >= 0x30 && vicii->current_raster <= 0xF7) &&
           ((vicii->current_raster - (vicii->control1 & VICII_CTRL1_YSCROLL)) % 8) == 6;
}

bool vicii_is_display_enabled(VICII *vicii)
{
    return (vicii->control1 & VICII_CTRL1_DEN) != 0;
}

uint16_t vicii_get_screen_base(VICII *vicii)
{
    uint16_t base = ((vicii->mem_pointers & VICII_VM13) >> 5) << 10 | 
                    ((vicii->mem_pointers & VICII_VM12) << 10);
    return base + 0x400;
}

uint16_t vicii_get_charset_base(VICII *vicii)
{
    return ((vicii->mem_pointers & VICII_CB13) << 9) + 0x1000;
}

uint16_t vicii_get_bitmap_base(VICII *vicii)
{
    return (((vicii->mem_pointers & VICII_VM13) >> 5) << 8) + 0x2000;
}

bool vicii_is_sprite_enabled(VICII *vicii, uint8_t sprite)
{
    return (vicii->sprite_enable & (1 << sprite)) != 0;
}

bool vicii_is_sprite_x_expand(VICII *vicii, uint8_t sprite)
{
    return (vicii->sprite_x_expand & (1 << sprite)) != 0;
}

bool vicii_is_sprite_y_expand(VICII *vicii, uint8_t sprite)
{
    return (vicii->sprite_y_expand & (1 << sprite)) != 0;
}

bool vicii_is_sprite_multicolor(VICII *vicii, uint8_t sprite)
{
    return (vicii->sprite_mcol & (1 << sprite)) != 0;
}

uint16_t vicii_get_sprite_x(VICII *vicii, uint8_t sprite)
{
    uint16_t x = vicii->sprite_x[sprite];
    if (vicii->sprite_x_msb & (1 << sprite)) {
        x += 256;
    }
    return x;
}

uint16_t vicii_get_sprite_y(VICII *vicii, uint8_t sprite)
{
    return vicii->sprite_y[sprite];
}

uint8_t vicii_get_sprite_color(VICII *vicii, uint8_t sprite)
{
    return vicii->sprite_color[sprite] & 0x0F;
}

// Utility functions
void vicii_clear_framebuffer(VICII *vicii)
{
    if (vicii == NULL) return;
    
    uint32_t border_color = vicii_color_to_rgb(vicii->border_color);
    for (int y = 0; y < VICII_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < VICII_SCREEN_WIDTH; x++) {
            vicii->framebuffer[y][x] = border_color;
        }
    }
}

void vicii_get_framebuffer(VICII *vicii, uint32_t *buffer, size_t size)
{
    if (vicii == NULL || buffer == NULL) return;
    
    size_t copy_size = (size < VICII_SCREEN_HEIGHT * VICII_SCREEN_WIDTH) ? 
                       size : VICII_SCREEN_HEIGHT * VICII_SCREEN_WIDTH;
    
    memcpy(buffer, vicii->framebuffer, copy_size * sizeof(uint32_t));
}

uint32_t vicii_color_to_rgb(uint8_t color)
{
    // C64 color palette (RGB values)
    static const uint32_t c64_colors[16] = {
        0x000000,  // 0: Black
        0xFFFFFF,  // 1: White
        0x880000,  // 2: Red
        0xAAFFEE,  // 3: Cyan
        0x00CC55,  // 4: Purple
        0x0000AA,  // 5: Blue
        0xEE7777,  // 6: Yellow
        0xDDDD88,  // 7: Orange
        0x66CC00,  // 8: Green
        0xFF6666,  // 9: Brown
        0xFF8888,  // A: Light Red
        0x888888,  // B: Dark Gray
        0xBBBBBB,  // C: Medium Gray
        0x8888FF,  // D: Light Blue
        0x66CCCC,  // E: Light Green
        0xFFFFFF   // F: Light Gray
    };
    
    return c64_colors[color & 0x0F];
}