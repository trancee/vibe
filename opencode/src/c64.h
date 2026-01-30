#ifndef C64_H
#define C64_H

#include "c64_types.h"

typedef struct {
    u8 ram[0x10000];
    u8 color_ram[0x0400];
    u8 kernal_rom[0x2000];
    u8 basic_rom[0x2000];
    u8 char_rom[0x1000];
    
    u8 cpu_port_dir;
    u8 cpu_port_data;
    bool cpu_port_floating;
    
    u64 cycle_count;
} C64Memory;

typedef struct {
    u16 pc;
    u8 a, x, y, sp;
    u8 status;
    
    bool irq_pending;
    bool nmi_pending;
    bool ba_low;
    
    u8 extra_cycles;
    bool page_crossed;
    bool decimal_mode;
} C64CPU;

typedef struct {
    u16 timer_a;
    u16 timer_a_latch;
    u16 timer_b;
    u16 timer_b_latch;
    
    u8 cra;
    u8 crb;
    u8 icr;
    u8 imr;
    u8 icr_data;
    bool irq_pending;
    bool icr_irq_flag_set; // Track if interrupt flag was just set (for ICR bit 7 delay)
    
    u8 ta_delay;
    u8 tb_delay;
    
    u8 tod_hours;
    u8 tod_minutes;
    u8 tod_seconds;
    u8 tod_tenth;
    u8 tod_alarm_hours;
    u8 tod_alarm_minutes;
    u8 tod_alarm_seconds;
    u8 tod_alarm_tenth;
    bool tod_latch;
    bool tod_running;
    
    u8 sdr;
    u8 data_dir_a;
    u8 data_dir_b;
    u8 pra;
    u8 prb;
} C64CIA;

typedef struct {
    u8 spr_enable;
    u8 spr_priority;
    u8 spr_multicolor;
    u8 spr_expand_x;
    u8 spr_expand_y;
    u8 spr_y_expand;
    u16 spr_pos[8];
    u8 spr_color[8];
    u8 spr_mc_color[2];
    
    u16 raster_line;
    u8 raster_irq_line;
    u8 irq_enabled;
    u8 irq_status;
    
    u16 vic_base;
    u16 screen_mem;
    u16 char_mem;
    
    u8 ctrl1;
    u8 ctrl2;
    u8 border_color;
    u8 bg_color[4];
    
    bool bad_line_active;
    bool ba_low;
    bool display_state;
    u8 vblank;
} C64VIC;

typedef struct {
    u8 freq_lo[3];
    u8 freq_hi[3];
    u8 pw_lo[3];
    u8 pw_hi[3];
    u8 control[3];
    u8 att_dec[3];
    u8 sus_rel[3];
    
    u8 filter_cutoff_lo;
    u8 filter_cutoff_hi;
    u8 filter_res_ctrl;
    u8 filter_mode_vol;
    
    u8 pot_x;
    u8 pot_y;
    
    u8 voice3_output;
    u8 voice3_unused;
    
    float filter_cutoff;
    float filter_resonance;
    float filter_freq;
} C64SID;

typedef struct {
    C64Memory mem;
    C64CPU cpu;
    C64CIA cia1;
    C64CIA cia2;
    C64VIC vic;
    C64SID sid;
    
    bool running;
    u64 target_cycles;
} C64;

// System-level functions
void c64_init(C64* sys);
void c64_step(C64* sys);
void c64_cleanup(C64* sys);
void c64_tick(C64* sys);

#endif