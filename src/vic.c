#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "vic.h"

void vic_init(VIC *vic, uint8_t *memory)
{
    if (vic == NULL)
        return;

    vic->memory = memory;

    vic_reset(vic);
}

void vic_reset(VIC *vic)
{
    if (vic == NULL)
        return;

    memset(vic, 0, sizeof(VIC));

    // Initialize to power-up state
    vic->control1 = VIC_CTRL1_DEN | VIC_CTRL1_RSEL; // Display enabled, 25 rows
    vic->control2 = 0xC8;                           // 40 columns, no multicolor
    vic->border_color = 0x0E;                       // Light blue border
    vic->background_color[0] = 0x06;                // Blue background
    vic->background_color[1] = 0x00;
    vic->background_color[2] = 0x00;
    vic->background_color[3] = 0x00;

    // Initialize raster
    vic->raster = 0;
    vic->current_raster = 0;
    vic->raster_compare = 0;

    // Clear color RAM
    memset(vic->color_ram, 0, sizeof(vic->color_ram));

    // Clear framebuffer
    vic_clear_framebuffer(vic);

    // Display state
    vic->display_state = false;
    vic->idle_state = true;
    vic->bad_line = false;
}

uint8_t vic_read_byte(VIC *vic, uint16_t addr)
{
    printf("VIC #$%04X → $%02X\n", addr, vic->memory[addr]);

    return vic->memory[addr];
}

void vic_write_byte(VIC *vic, uint16_t addr, uint8_t data)
{
    printf("VIC #$%04X ← $%02X\n", addr, data);

    vic->memory[addr] = data;
}

uint8_t vic_read(VIC *vic, uint16_t addr)
{
    // printf("VIC READ #$%04X\n", addr);
    if (vic == NULL || addr < VIC_MEM_START || addr > VIC_MEM_END)
        return 0;

    uint16_t offset = addr & 0x3F; // VIC-II register images (repeated every $40, 64 bytes).

    uint8_t result = 0;
    switch (offset)
    {
    case 0x00:
    case 0x02:
    case 0x04:
    case 0x06:
    case 0x08:
    case 0x0A:
    case 0x0C:
    case 0x0E:
        result = vic->sprite_x[offset >> 1];
        break;

    case 0x01:
    case 0x03:
    case 0x05:
    case 0x07:
    case 0x09:
    case 0x0B:
    case 0x0D:
    case 0x0F:
        result = vic->sprite_y[offset >> 1];
        break;

    case 0x10: // Sprite X MSB
        result = vic->sprite_x_msb;
        break;

    case 0x11: // Control 1
        // Bit 7 contains RC8 (MSB of current raster line)
        result = (vic->control1 & 0x7F) | ((vic->current_raster >> 1) & 0x80);
        break;

    case 0x12: // Current raster line
        result = vic->current_raster & 0xFF;
        break;

    case 0x13: // Lightpen X
        result = vic->lightpen_x;
        break;

    case 0x14: // Lightpen Y
        result = vic->lightpen_y;
        break;

    case 0x15: // Sprite enable
        result = vic->sprite_enable;
        break;

    case 0x16: // Control 2
        result = vic->control2;
        break;

    case 0x17: // Sprite Y expand
        result = vic->sprite_y_expand;
        break;

    case 0x18: // Memory pointers
        result = vic->mem_pointers;
        break;

    case 0x19: // Interrupt register
        result = vic->interrupt;
        break;

    case 0x1A: // IRQ enable
        result = vic->irq_enable;
        break;

    case 0x1B: // Sprite priority
        result = vic->sprite_priority;
        break;

    case 0x1C: // Sprite multicolor
        result = vic->sprite_mcol;
        break;

    case 0x1D: // Sprite X expand
        result = vic->sprite_x_expand;
        break;

    case 0x1E: // Sprite-sprite collision
        result = vic->sprite_sprite_coll;
        // Reading clears the register
        vic->sprite_sprite_coll = 0;
        break;

    case 0x1F: // Sprite-data collision
        result = vic->sprite_data_coll;
        // Reading clears the register
        vic->sprite_data_coll = 0;
        break;

    case 0x20: // Border color
        result = vic->border_color;
        break;

    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
        result = vic->background_color[offset - 0x21];
        break;

    case 0x25:
    case 0x26:
        result = vic->sprite_mcolor[offset - 0x25];
        break;

    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    case 0x2E:
        result = vic->sprite_color[offset - 0x27];
        break;

    case 0x2F:
    case 0x30:
    case 0x31:
    case 0x33:
    case 0x35:
    case 0x37:
    case 0x39:
    case 0x3B:
    case 0x3D:
    case 0x3F:
        result = 0xFF;
        break;

    default:
        result = 0;
        break;
    }

    return result;
}

void vic_write(VIC *vic, uint16_t addr, uint8_t data)
{
    // printf("VIC WRITE #$%04X = $%02X\n", addr, data);
    if (vic == NULL || addr < VIC_MEM_START || addr > VIC_MEM_END)
        return;

    uint16_t offset = addr & 0x3F; // VIC-II register images (repeated every $40, 64 bytes).

    switch (offset)
    {
    case 0x00:
    case 0x02:
    case 0x04:
    case 0x06:
    case 0x08:
    case 0x0A:
    case 0x0C:
    case 0x0E:
        vic->sprite_x[offset >> 1] = data;
        break;

    case 0x01:
    case 0x03:
    case 0x05:
    case 0x07:
    case 0x09:
    case 0x0B:
    case 0x0D:
    case 0x0F:
        vic->sprite_y[offset >> 1] = data;
        break;

    case 0x10: // Sprite X MSB
        vic->sprite_x_msb = data;
        break;

    case 0x11: // Control 1
        vic->control1 = data;
        break;

    case 0x12: // Raster compare
        vic->raster_compare = data;
        break;

    case 0x13: // Lightpen X (write-only)
        vic->lightpen_x = data;
        break;

    case 0x14: // Lightpen Y (write-only)
        vic->lightpen_y = data;
        break;

    case 0x15: // Sprite enable
        vic->sprite_enable = data;
        break;

    case 0x16: // Control 2
        vic->control2 = data;
        break;

    case 0x17: // Sprite Y expand
        vic->sprite_y_expand = data;
        break;

    case 0x18: // Memory pointers
        vic->mem_pointers = data;
        break;

    case 0x19: // Interrupt register
        // Bits 0-3 are cleared by writing 1 to them
        vic->interrupt &= ~(data & 0x0F);
        break;

    case 0x1A: // IRQ enable
        vic->irq_enable = data & 0x0F;
        break;

    case 0x1B: // Sprite priority
        vic->sprite_priority = data;
        break;

    case 0x1C: // Sprite multicolor
        vic->sprite_mcol = data;
        break;

    case 0x1D: // Sprite X expand
        vic->sprite_x_expand = data;
        break;

    case 0x1E: // Sprite-sprite collision (read-only)
        // Cannot write, only clear by reading
        break;

    case 0x1F: // Sprite-data collision (read-only)
        // Cannot write, only clear by reading
        break;

    case 0x20: // Border color
        vic->border_color = data & 0x0F;
        break;

    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
        vic->background_color[offset - 0x21] = data & 0x0F;
        break;

    case 0x25:
    case 0x26:
        vic->sprite_mcolor[offset - 0x25] = data & 0x0F;
        break;

    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    case 0x2E:
        vic->sprite_color[offset - 0x27] = data & 0x0F;
        break;
    }
}

void vic_clock(VIC *vic)
{
    if (vic == NULL)
        return;

    // Update raster line
    vic->current_raster++;
    if (vic->current_raster >= VIC_TOTAL_RASTERS)
    {
        vic->current_raster = 0;
    }

    // Update raster register
    vic->raster = vic->current_raster & 0xFF;

    // Check for bad line condition
    vic->bad_line = vic_is_bad_line(vic);

    // Check for raster interrupt
    if (vic->current_raster == vic->raster_compare)
    {
        vic->interrupt |= VIC_IRQ_RASTER;
    }

    // Render current line if in visible area
    if (vic->current_raster >= VIC_FIRST_VISIBLE_LINE &&
        vic->current_raster <= VIC_LAST_VISIBLE_LINE)
    {
        vic_render_line(vic, vic->current_raster);
    }
}

void vic_render_line(VIC *vic, uint16_t raster_line)
{
    if (vic == NULL)
        return;

    uint16_t display_line = raster_line - VIC_FIRST_VISIBLE_LINE;

    if (display_line >= VIC_SCREEN_HEIGHT)
        return;

    // Clear line to border color initially
    uint32_t border_rgb = vic_color_to_rgb(vic->border_color);

    for (int x = 0; x < VIC_SCREEN_WIDTH; x++)
    {
        vic->framebuffer[display_line][x] = border_rgb;
    }

    if (!vic_is_display_enabled(vic))
    {
        return;
    }

    // Determine text/bitmap mode
    bool bitmap_mode = (vic->control1 & VIC_CTRL1_BMM) != 0;
    bool multicolor_mode = (vic->control2 & VIC_CTRL2_MCM) != 0;

    if (bitmap_mode)
    {
        vic_render_bitmap_line(vic, display_line, multicolor_mode);
    }
    else
    {
        // vic_render_text_line(vic, display_line, multicolor_mode);
    }

    // Render sprites
    vic_render_sprites_line(vic, display_line);
}

void vic_render_text_line(VIC *vic, uint16_t line, bool multicolor)
{
    uint16_t screen_base = vic_get_screen_base(vic);
    uint16_t charset_base = vic_get_charset_base(vic);
    uint8_t x_scroll = vic->control2 & VIC_CTRL2_XSCROLL;
    uint8_t y_scroll = vic->control1 & VIC_CTRL1_YSCROLL;

    // Adjust for scrolling
    uint16_t text_line = (line + y_scroll) / 8;
    uint8_t char_line = (line + y_scroll) % 8;

    // Render characters
    for (int col = 0; col < 40; col++)
    {
        uint16_t screen_addr = screen_base + text_line * 40 + col;
        uint8_t char_code = vic_read_byte(vic, screen_addr);
        uint8_t color = vic->color_ram[text_line * 40 + col] & 0x0F;

        // Get character data from charset
        uint16_t char_data_addr = charset_base + char_code * 8 + char_line;
        uint8_t char_data = vic_read_byte(vic, char_data_addr);

        // Render character pixels
        for (int pixel = 0; pixel < 8; pixel++)
        {
            int x = col * 8 + pixel + x_scroll;
            if (x >= VIC_SCREEN_WIDTH)
                break;

            if (multicolor && (color & 0x08))
            {
                // Multicolor text mode
                uint8_t color_bits = (char_data >> (7 - pixel)) & 0x03;
                uint32_t pixel_color;

                switch (color_bits)
                {
                case 0:
                    pixel_color = vic_color_to_rgb(vic->background_color[0]);
                    break;
                case 1:
                    pixel_color = vic_color_to_rgb(vic->background_color[1]);
                    break;
                case 2:
                    pixel_color = vic_color_to_rgb(vic->background_color[2]);
                    break;
                case 3:
                    pixel_color = vic_color_to_rgb(color & 0x07);
                    break;
                default:
                    pixel_color = vic_color_to_rgb(0);
                    break;
                }

                vic->framebuffer[line][x] = pixel_color;
            }
            else
            {
                // Standard text mode
                bool pixel_set = (char_data >> (7 - pixel)) & 0x01;
                uint32_t pixel_color = pixel_set ? vic_color_to_rgb(color) : vic_color_to_rgb(vic->background_color[0]);

                vic->framebuffer[line][x] = pixel_color;
            }
        }
    }
}

void vic_render_bitmap_line(VIC *vic, uint16_t line, bool multicolor)
{
    uint16_t bitmap_base = vic_get_bitmap_base(vic);
    uint16_t screen_base = vic_get_screen_base(vic);
    uint8_t y_scroll = vic->control1 & VIC_CTRL1_YSCROLL;

    // Adjust for scrolling
    uint16_t bitmap_line = line + y_scroll;

    // Render bitmap
    for (int col = 0; col < 40; col++)
    {
        uint16_t bitmap_addr = bitmap_base + (bitmap_line / 8) * 320 + col * 8 + (bitmap_line % 8);
        uint16_t color_ram_addr = screen_base + (bitmap_line / 8) * 40 + col;

        uint8_t bitmap_data = vic_read_byte(vic, bitmap_addr);
        uint8_t foreground_color = vic_read_byte(vic, color_ram_addr) & 0x0F;
        uint8_t background_color = (vic_read_byte(vic, color_ram_addr) >> 4) & 0x0F;

        for (int pixel = 0; pixel < 8; pixel++)
        {
            int x = col * 8 + pixel;
            if (x >= VIC_SCREEN_WIDTH)
                break;

            if (multicolor)
            {
                // Multicolor bitmap mode
                uint8_t color_bits = (bitmap_data >> (6 - pixel * 2)) & 0x03;
                uint32_t pixel_color;

                switch (color_bits)
                {
                case 0:
                    pixel_color = vic_color_to_rgb(vic->background_color[0]);
                    break;
                case 1:
                    pixel_color = vic_color_to_rgb(background_color);
                    break;
                case 2:
                    pixel_color = vic_color_to_rgb(foreground_color);
                    break;
                case 3:
                    pixel_color = vic_color_to_rgb(vic->background_color[1]);
                    break;
                default:
                    pixel_color = vic_color_to_rgb(0);
                    break;
                }

                vic->framebuffer[line][x] = pixel_color;
            }
            else
            {
                // Standard bitmap mode
                bool pixel_set = (bitmap_data >> (7 - pixel)) & 0x01;
                uint32_t pixel_color = pixel_set ? vic_color_to_rgb(foreground_color) : vic_color_to_rgb(background_color);

                vic->framebuffer[line][x] = pixel_color;
            }
        }
    }
}

void vic_render_sprites_line(VIC *vic, uint16_t line)
{
    if (!vic->sprite_enable)
        return;

    // Get video matrix base for sprite pointer lookup
    uint16_t screen_base = vic_get_screen_base(vic);

    for (int sprite = 0; sprite < VIC_NUM_SPRITES; sprite++)
    {
        if (!vic_is_sprite_enabled(vic, sprite))
            continue;

        uint16_t sprite_y = vic_get_sprite_y(vic, sprite);
        uint16_t sprite_x = vic_get_sprite_x(vic, sprite);
        bool y_expand = vic_is_sprite_y_expand(vic, sprite);
        bool x_expand = vic_is_sprite_x_expand(vic, sprite);
        bool multicolor = vic_is_sprite_multicolor(vic, sprite);

        uint8_t sprite_height = y_expand ? 42 : 21;

        // Check if sprite is on this line
        if (line >= sprite_y && line < sprite_y + sprite_height)
        {
            uint8_t sprite_line = y_expand ? ((line - sprite_y) / 2) : (line - sprite_y);

            // Fetch sprite pointer from video matrix end (VM base + $3F8 + sprite number)
            // Sprite pointers are located at the last 8 bytes of the video matrix
            uint16_t sprite_pointer_addr = screen_base + 0x3F8 + sprite;
            uint8_t sprite_pointer = vic_read_byte(vic, sprite_pointer_addr);

            // Calculate sprite data base address (pointer * 64)
            uint16_t sprite_data_base = sprite_pointer * 64;

            // Render sprite data (3 bytes per line, 21 lines)
            for (int byte = 0; byte < 3; byte++)
            {
                uint16_t sprite_data_addr = sprite_data_base + sprite_line * 3 + byte;
                uint8_t sprite_data = vic_read_byte(vic, sprite_data_addr);
                for (int bit = 7; bit >= 0; bit--)
                {
                    int x = sprite_x + byte * 8 + (7 - bit);
                    if (x >= VIC_SCREEN_WIDTH)
                        break;

                    if (x_expand)
                    {
                        x = sprite_x + (sprite_x + byte * 16 + (7 - bit) * 2);
                        if (x >= VIC_SCREEN_WIDTH)
                            break;
                    }

                    bool pixel_set = (sprite_data >> bit) & 0x01;

                    if (pixel_set)
                    {
                        uint32_t pixel_color;

                        if (multicolor)
                        {
                            // Get 2-bit color from sprite data
                            uint8_t color_bits = ((sprite_data >> (bit - 1)) & 0x03);
                            if (bit == 0)
                                color_bits = (sprite_data & 0x01);

                            switch (color_bits)
                            {
                            case 0:
                                pixel_color = vic_color_to_rgb(0);
                                break; // Transparent
                            case 1:
                                pixel_color = vic_color_to_rgb(vic->sprite_mcolor[0]);
                                break;
                            case 2:
                                pixel_color = vic_color_to_rgb(vic->sprite_color[sprite]);
                                break;
                            case 3:
                                pixel_color = vic_color_to_rgb(vic->sprite_mcolor[1]);
                                break;
                            default:
                                pixel_color = vic_color_to_rgb(0);
                                break;
                            }
                        }
                        else
                        {
                            pixel_color = vic_color_to_rgb(vic->sprite_color[sprite]);
                        }

                        vic->framebuffer[line][x] = pixel_color;

                        if (x_expand && x + 1 < VIC_SCREEN_WIDTH)
                        {
                            vic->framebuffer[line][x + 1] = pixel_color;
                        }
                    }
                }
            }
        }
    }
}

bool vic_get_irq(VIC *vic)
{
    if (vic == NULL)
        return false;

    vic->irq_pending = (vic->interrupt & vic->irq_enable) != 0;
    return vic->irq_pending;
}

// Helper function implementations
bool vic_is_bad_line(VIC *vic)
{
    return (vic->control1 & VIC_CTRL1_DEN) &&
           (vic->current_raster >= 0x30 && vic->current_raster <= 0xF7) &&
           ((vic->current_raster - (vic->control1 & VIC_CTRL1_YSCROLL)) % 8) == 6;
}

bool vic_is_display_enabled(VIC *vic)
{
    return (vic->control1 & VIC_CTRL1_DEN) != 0;
}

uint16_t vic_get_screen_base(VIC *vic)
{
    // Bits 4-7 of $D018 form video matrix address bits 10-13
    // Video matrix base = (bits 4-7) << 10
    uint16_t base = ((vic->mem_pointers >> 4) & 0x0F) << 10;
    return base;
}

uint16_t vic_get_charset_base(VIC *vic)
{
    // Bits 1-3 of $D018 form character generator address bits 11-13
    // Character base = (bits 1-3) << 11
    return ((vic->mem_pointers >> 1) & 0x07) << 11;
}

uint16_t vic_get_bitmap_base(VIC *vic)
{
    // Bit 3 of $D018 (CB13) selects bitmap base: 0=$0000, 1=$2000
    return ((vic->mem_pointers >> 3) & 0x01) ? 0x2000 : 0x0000;
}

bool vic_is_sprite_enabled(VIC *vic, uint8_t sprite)
{
    return (vic->sprite_enable & (1 << sprite)) != 0;
}

bool vic_is_sprite_x_expand(VIC *vic, uint8_t sprite)
{
    return (vic->sprite_x_expand & (1 << sprite)) != 0;
}

bool vic_is_sprite_y_expand(VIC *vic, uint8_t sprite)
{
    return (vic->sprite_y_expand & (1 << sprite)) != 0;
}

bool vic_is_sprite_multicolor(VIC *vic, uint8_t sprite)
{
    return (vic->sprite_mcol & (1 << sprite)) != 0;
}

uint16_t vic_get_sprite_x(VIC *vic, uint8_t sprite)
{
    uint16_t x = vic->sprite_x[sprite];
    if (vic->sprite_x_msb & (1 << sprite))
    {
        x += 256;
    }
    return x;
}

uint16_t vic_get_sprite_y(VIC *vic, uint8_t sprite)
{
    return vic->sprite_y[sprite];
}

uint8_t vic_get_sprite_color(VIC *vic, uint8_t sprite)
{
    return vic->sprite_color[sprite] & 0x0F;
}

// Utility functions
void vic_clear_framebuffer(VIC *vic)
{
    if (vic == NULL)
        return;

    uint32_t border_color = vic_color_to_rgb(vic->border_color);
    for (int y = 0; y < VIC_SCREEN_HEIGHT; y++)
    {
        for (int x = 0; x < VIC_SCREEN_WIDTH; x++)
        {
            vic->framebuffer[y][x] = border_color;
        }
    }
}

void vic_get_framebuffer(VIC *vic, uint32_t *buffer, size_t size)
{
    if (vic == NULL || buffer == NULL)
        return;

    size_t copy_size = (size < VIC_SCREEN_HEIGHT * VIC_SCREEN_WIDTH) ? size : VIC_SCREEN_HEIGHT * VIC_SCREEN_WIDTH;

    memcpy(buffer, vic->framebuffer, copy_size * sizeof(uint32_t));
}

uint32_t vic_color_to_rgb(uint8_t color)
{
    // C64 color palette (RGB values)
    static const uint32_t c64_colors[16] = {
        0x000000, // 0: Black
        0xFFFFFF, // 1: White
        0x880000, // 2: Red
        0xAAFFEE, // 3: Cyan
        0x00CC55, // 4: Purple
        0x0000AA, // 5: Blue
        0xEE7777, // 6: Yellow
        0xDDDD88, // 7: Orange
        0x66CC00, // 8: Green
        0xFF6666, // 9: Brown
        0xFF8888, // A: Light Red
        0x888888, // B: Dark Gray
        0xBBBBBB, // C: Medium Gray
        0x8888FF, // D: Light Blue
        0x66CCCC, // E: Light Green
        0xFFFFFF  // F: Light Gray
    };

    return c64_colors[color & 0x0F];
}