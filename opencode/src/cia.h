#ifndef CIA_H
#define CIA_H

#include "c64.h"

void cia_init(C64CIA* cia);
void cia_clock(C64CIA* cia);
u8 cia_read_reg(C64CIA* cia, u8 reg);
void cia_write_reg(C64CIA* cia, u8 reg, u8 data);

#endif