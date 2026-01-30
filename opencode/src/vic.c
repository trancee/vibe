#include "c64.h"
#include <stdio.h>
#include <string.h>

void vic_init(C64VIC* vic) {
    memset(vic, 0, sizeof(C64VIC));
    vic->raster_line = 0;
    vic->raster_irq_line = 0;
    vic->screen_mem = 0x0400;
    vic->char_mem = 0x1000;
    vic->vic_base = 0;
}

void vic_clock(C64VIC* vic) {
    vic->raster_line++;
    if (vic->raster_line >= 312) {
        vic->raster_line = 0;
        vic->vblank = 1;
    } else if (vic->raster_line == 1) {
        vic->vblank = 0;
    }
    
    if (vic->raster_line == vic->raster_irq_line) {
        vic->irq_status |= 0x01;
    }
    
    u8 cycle = vic->raster_line % 63;
    
    vic->bad_line_active = false;
    if (cycle >= 12 && cycle <= 54) {
        u8 line_low = vic->raster_line & 0xFF;
        u8 ctrl1_high = (vic->ctrl1 >> 7) & 0x01;
        u8 line_high = ctrl1_high << 1;
        u8 line_bits = line_high | (line_low & 0x01);
        
        if (line_bits == (vic->raster_line & 0x03)) {
            vic->bad_line_active = true;
        }
    }
    
    if (vic->bad_line_active || (vic->spr_enable && cycle >= 12 && cycle <= 54)) {
        vic->ba_low = true;
    } else {
        vic->ba_low = false;
    }
}

u8 vic_read_reg(C64VIC* vic, u8 reg, C64* sys) {
    switch (reg & 0x3F) {
        case 0x00:
            return vic->spr_pos[0] & 0xFF;
        case 0x01:
            return vic->spr_pos[1] & 0xFF;
        case 0x02:
            return vic->spr_pos[2] & 0xFF;
        case 0x03:
            return vic->spr_pos[3] & 0xFF;
        case 0x04:
            return vic->spr_pos[4] & 0xFF;
        case 0x05:
            return vic->spr_pos[5] & 0xFF;
        case 0x06:
            return vic->spr_pos[6] & 0xFF;
        case 0x07:
            return vic->spr_pos[7] & 0xFF;
        case 0x08:
            return (vic->spr_pos[0] >> 8) | (vic->spr_pos[1] >> 8) | 
                   (vic->spr_pos[2] >> 8) | (vic->spr_pos[3] >> 8) |
                   (vic->spr_pos[4] >> 8) | (vic->spr_pos[5] >> 8) |
                   (vic->spr_pos[6] >> 8) | (vic->spr_pos[7] >> 8);
        case 0x09:
            return vic->spr_enable;
        case 0x0A:
            return vic->ctrl1;
        case 0x0B:
            return vic->ctrl2;
        case 0x0C:
            return vic->spr_y_expand;
        case 0x0D:
            return vic->spr_pos[0] >> 8;
        case 0x0E:
            return vic->spr_pos[1] >> 8;
        case 0x0F:
            return vic->spr_pos[2] >> 8;
        case 0x10:
            return vic->spr_pos[3] >> 8;
        case 0x11:
            return vic->raster_line & 0xFF;
        case 0x12:
            return vic->vic_base | ((vic->raster_line & 0x100) >> 7);
        case 0x13:
            return vic->spr_priority;
        case 0x14:
            return vic->spr_multicolor;
        case 0x15:
            return vic->spr_expand_x;
        case 0x16:
            return vic->spr_color[0] | (vic->spr_color[1] << 4);
        case 0x17:
            return vic->spr_color[2] | (vic->spr_color[3] << 4);
        case 0x18:
            return vic->spr_color[4] | (vic->spr_color[5] << 4);
        case 0x19:
            return vic->spr_color[6] | (vic->spr_color[7] << 4);
        case 0x1A:
            return vic->spr_expand_y;
        case 0x1B:
            u8 value = vic->spr_pos[4] >> 8;
            return value;
        case 0x1C:
            return vic->spr_pos[5] >> 8;
        case 0x1D:
            return vic->spr_pos[6] >> 8;
        case 0x1E:
            return vic->spr_pos[7] >> 8;
        case 0x1F:
            return vic->irq_status | 0xF0;
        case 0x20:
            return vic->irq_enabled | 0xF0;
        case 0x21:
            return vic->bg_color[0];
        case 0x22:
            return vic->bg_color[1];
        case 0x23:
            return vic->bg_color[2];
        case 0x24:
            return vic->bg_color[3];
        case 0x25:
            return vic->spr_mc_color[0];
        case 0x26:
            return vic->spr_mc_color[1];
        case 0x27:
            return vic->border_color;
        default:
            if (reg >= 0x40 && reg < 0x48) {
                return sys->mem.color_ram[reg - 0x40] | 0xF0;
            }
            return 0xFF;
    }
}

void vic_write_reg(C64VIC* vic, u8 reg, u8 data) {
    switch (reg & 0x3F) {
        case 0x00:
            vic->spr_pos[0] = (vic->spr_pos[0] & 0xFF00) | data;
            break;
        case 0x01:
            vic->spr_pos[1] = (vic->spr_pos[1] & 0xFF00) | data;
            break;
        case 0x02:
            vic->spr_pos[2] = (vic->spr_pos[2] & 0xFF00) | data;
            break;
        case 0x03:
            vic->spr_pos[3] = (vic->spr_pos[3] & 0xFF00) | data;
            break;
        case 0x04:
            vic->spr_pos[4] = (vic->spr_pos[4] & 0xFF00) | data;
            break;
        case 0x05:
            vic->spr_pos[5] = (vic->spr_pos[5] & 0xFF00) | data;
            break;
        case 0x06:
            vic->spr_pos[6] = (vic->spr_pos[6] & 0xFF00) | data;
            break;
        case 0x07:
            vic->spr_pos[7] = (vic->spr_pos[7] & 0xFF00) | data;
            break;
        case 0x08:
            vic->spr_enable = data;
            break;
        case 0x09:
            vic->ctrl1 = data;
            vic->screen_mem = 0x0400 + ((data >> 4) & 0xF) * 0x0400;
            vic->vic_base = (data >> 1) & 0x7F;
            break;
        case 0x0A:
            vic->ctrl2 = data;
            vic->char_mem = 0x1000 + ((data >> 1) & 0x7) * 0x0800;
            break;
        case 0x0B:
            vic->spr_y_expand = data;
            break;
        case 0x0C:
            vic->raster_irq_line = data;
            break;
        case 0x0D:
            vic->spr_pos[0] = (vic->spr_pos[0] & 0x00FF) | ((data & 0x01) << 8);
            break;
        case 0x0E:
            vic->spr_pos[1] = (vic->spr_pos[1] & 0x00FF) | ((data & 0x01) << 8);
            break;
        case 0x0F:
            vic->spr_pos[2] = (vic->spr_pos[2] & 0x00FF) | ((data & 0x01) << 8);
            break;
        case 0x10:
            vic->spr_pos[3] = (vic->spr_pos[3] & 0x00FF) | ((data & 0x01) << 8);
            break;
        case 0x11:
            vic->raster_irq_line = (vic->raster_irq_line & 0xFF00) | data;
            break;
        case 0x12:
            vic->vic_base = data & 0x7F;
            break;
        case 0x13:
            vic->spr_priority = data;
            break;
        case 0x14:
            vic->spr_multicolor = data;
            break;
        case 0x15:
            vic->spr_expand_x = data;
            break;
        case 0x16:
            vic->spr_color[0] = data & 0x0F;
            vic->spr_color[1] = (data >> 4) & 0x0F;
            break;
        case 0x17:
            vic->spr_color[2] = data & 0x0F;
            vic->spr_color[3] = (data >> 4) & 0x0F;
            break;
        case 0x18:
            vic->spr_color[4] = data & 0x0F;
            vic->spr_color[5] = (data >> 4) & 0x0F;
            break;
        case 0x19:
            vic->spr_color[6] = data & 0x0F;
            vic->spr_color[7] = (data >> 4) & 0x0F;
            break;
        case 0x1A:
            vic->spr_expand_y = data;
            break;
        case 0x1B:
            vic->spr_pos[4] = (vic->spr_pos[4] & 0x00FF) | ((data & 0x01) << 8);
            break;
        case 0x1C:
            vic->spr_pos[5] = (vic->spr_pos[5] & 0x00FF) | ((data & 0x01) << 8);
            break;
        case 0x1D:
            vic->spr_pos[6] = (vic->spr_pos[6] & 0x00FF) | ((data & 0x01) << 8);
            break;
        case 0x1E:
            vic->spr_pos[7] = (vic->spr_pos[7] & 0x00FF) | ((data & 0x01) << 8);
            break;
        case 0x1F:
            vic->irq_status &= ~data;
            break;
        case 0x20:
            vic->irq_enabled = data;
            break;
        case 0x21:
            vic->bg_color[0] = data;
            break;
        case 0x22:
            vic->bg_color[1] = data;
            break;
        case 0x23:
            vic->bg_color[2] = data;
            break;
        case 0x24:
            vic->bg_color[3] = data;
            break;
        case 0x25:
            vic->spr_mc_color[0] = data;
            break;
        case 0x26:
            vic->spr_mc_color[1] = data;
            break;
        case 0x27:
            vic->border_color = data;
            break;
    }
}

void vic_render_frame(C64VIC* vic, C64* sys) {
    if (vic->vblank && vic->raster_line == 311) {
        printf("\e[2J\e[H");
        
        for (int y = 0; y < 25; y++) {
            for (int x = 0; x < 40; x++) {
                u16 screen_addr = vic->screen_mem + y * 40 + x;
                u16 color_addr = 0xD800 + y * 40 + x;
                
                u8 char_code = sys->mem.ram[screen_addr];
                u8 color_code = sys->mem.color_ram[color_addr] & 0x0F;
                
                printf("\e[38;5;%dm%c", 16 + color_code * 2, char_code);
            }
            printf("\n");
        }
        
        printf("\e[0mRaster: %d, Cycle: %llu\n", vic->raster_line, sys->mem.cycle_count);
    }
}