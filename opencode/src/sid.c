#include "c64.h"
#include <string.h>
#include <math.h>

void sid_init(C64SID* sid) {
    memset(sid, 0, sizeof(C64SID));
    sid->filter_cutoff = 0.0f;
    sid->filter_resonance = 0.0f;
    sid->filter_freq = 0.0f;
}

static float sid_filter_freq_calc(u16 cutoff_reg) {
    return cutoff_reg * 0.060959467;
}

static void sid_update_filter(C64SID* sid) {
    u16 cutoff = (sid->filter_cutoff_hi << 3) | (sid->filter_cutoff_lo & 0x07);
    sid->filter_freq = sid_filter_freq_calc(cutoff);
    sid->filter_resonance = (sid->filter_res_ctrl >> 4) & 0x0F;
    sid->filter_cutoff = sid->filter_freq;
}

u8 sid_read_reg(C64SID* sid, u8 reg) {
    if (reg >= 0x19 && reg <= 0x1C) {
        return sid->voice3_output;
    }
    if (reg == 0x1D || reg == 0x1E) {
        return 0xFF;
    }
    if (reg == 0x1F) {
        return sid->pot_x;
    }
    if (reg == 0x20) {
        return sid->pot_y;
    }
    return 0;
}

void sid_write_reg(C64SID* sid, u8 reg, u8 data) {
    if (reg >= 0x00 && reg <= 0x06) {
        u8 voice = (reg >> 4) & 0x03;
        u8 offset = reg & 0x0F;
        
        switch (offset) {
            case 0x00:
                sid->freq_lo[voice] = data;
                break;
            case 0x01:
                sid->freq_hi[voice] = data;
                break;
            case 0x02:
                sid->pw_lo[voice] = data;
                break;
            case 0x03:
                sid->pw_hi[voice] = data & 0x0F;
                break;
            case 0x04:
                sid->control[voice] = data;
                break;
            case 0x05:
                sid->att_dec[voice] = data;
                break;
            case 0x06:
                sid->sus_rel[voice] = data;
                break;
        }
    } else {
        switch (reg) {
            case 0x15:
                sid->filter_cutoff_lo = data;
                sid_update_filter(sid);
                break;
            case 0x16:
                sid->filter_cutoff_hi = data & 0x07;
                sid_update_filter(sid);
                break;
            case 0x17:
                sid->filter_res_ctrl = data;
                sid_update_filter(sid);
                break;
            case 0x18:
                sid->filter_mode_vol = data;
                break;
            case 0x1D:
                sid->voice3_output = data;
                break;
            case 0x1E:
                sid->voice3_unused = data;
                break;
            case 0x1F:
                sid->pot_x = data;
                break;
            case 0x20:
                sid->pot_y = data;
                break;
        }
    }
}