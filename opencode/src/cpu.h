#ifndef CPU_H
#define CPU_H

#include "c64.h"

void cpu_init(C64CPU* cpu);
void cpu_interrupt(C64* sys, bool nmi);
void cpu_step(C64* sys);
bool cpu_get_flag(C64CPU* cpu, u8 flag);

// Flag constants
#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_R 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80

#endif