#ifndef VIC_H
#define VIC_H

#include "c64.h"

void vic_init(C64VIC* vic);
void vic_clock(C64VIC* vic);
u8 vic_read_reg(C64VIC* vic, u8 reg, C64* sys);
void vic_write_reg(C64VIC* vic, u8 reg, u8 data);
void vic_render_frame(C64VIC* vic, C64* sys);

#endif