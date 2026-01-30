#ifndef SID_H
#define SID_H

#include "c64.h"

void sid_init(C64SID* sid);
u8 sid_read_reg(C64SID* sid, u8 reg);
void sid_write_reg(C64SID* sid, u8 reg, u8 data);

#endif