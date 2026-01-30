#include "c64.h"

static void cpu_step_extended(C64* sys);
static void cpu_illegal_opcode(C64* sys, u8 opcode);
static u16 addr_mode_zero_page_indirect(C64* sys);
static u16 addr_mode_absolute_indirect(C64* sys);

static u8 mem_read(C64* sys, u16 addr);
static void mem_write(C64* sys, u16 addr, u8 data);