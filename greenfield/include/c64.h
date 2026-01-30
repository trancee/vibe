/*
 * C64 Emulator - Main System Header
 * 
 * Complete Commodore 64 system integration including:
 * - Memory bus with PLA bank switching
 * - ROM loading (kernal, basic, char)
 * - System tick (heartbeat) orchestration
 * - Component interconnection
 */

#ifndef C64_H
#define C64_H

#include <stdint.h>
#include <stdbool.h>
#include "cpu.h"
#include "vic.h"
#include "cia6526.h"
#include "sid6581.h"
#include "clock.h"

/* Memory sizes */
#define C64_RAM_SIZE      65536
#define C64_BASIC_SIZE    8192
#define C64_KERNAL_SIZE   8192
#define C64_CHAR_SIZE     4096
#define C64_COLOR_SIZE    1024

/* Memory map regions */
#define C64_BASIC_START   0xA000
#define C64_BASIC_END     0xBFFF
#define C64_CHAR_START    0xD000
#define C64_CHAR_END      0xDFFF
#define C64_KERNAL_START  0xE000
#define C64_KERNAL_END    0xFFFF

/* I/O space regions */
#define C64_IO_START      0xD000
#define C64_IO_END        0xDFFF

#define C64_VIC_START     0xD000
#define C64_VIC_END       0xD3FF
#define C64_SID_START     0xD400
#define C64_SID_END       0xD7FF
#define C64_COLOR_START   0xD800
#define C64_COLOR_END     0xDBFF
#define C64_CIA1_START    0xDC00
#define C64_CIA1_END      0xDCFF
#define C64_CIA2_START    0xDD00
#define C64_CIA2_END      0xDDFF

/* CPU Port ($01) bit definitions for PLA */
#define CPU_PORT_LORAM    0x01  /* BASIC ROM at $A000-$BFFF */
#define CPU_PORT_HIRAM    0x02  /* KERNAL ROM at $E000-$FFFF */
#define CPU_PORT_CHAREN   0x04  /* I/O or CHAR ROM at $D000-$DFFF */

/* PAL timing constants */
#define C64_PAL_CLOCK     985248
#define C64_PAL_FPS       50
#define C64_CYCLES_PER_FRAME (C64_PAL_CLOCK / C64_PAL_FPS)

/* Screen dimensions for ANSI output */
#define C64_SCREEN_COLS   40
#define C64_SCREEN_ROWS   25

/* C64 system structure */
typedef struct {
    /* Components */
    CPU cpu;
    VIC vic;
    CIA cia1;
    CIA cia2;
    SID sid;
    Clock clock;
    
    /* Memory */
    uint8_t ram[C64_RAM_SIZE];
    uint8_t basic_rom[C64_BASIC_SIZE];
    uint8_t kernal_rom[C64_KERNAL_SIZE];
    uint8_t char_rom[C64_CHAR_SIZE];
    uint8_t color_ram[C64_COLOR_SIZE];
    
    /* PLA banking state */
    uint8_t cpu_port;           /* CPU port $01 */
    uint8_t cpu_port_ddr;       /* CPU port DDR $00 */
    uint8_t cpu_port_floating;  /* Floating bus state for input pins */
    
    /* Interrupt lines */
    bool nmi_line;
    bool irq_line;
    bool ba_low;           /* VIC badline stalls CPU */
    
    /* Keyboard matrix (directly maps to CIA1) */
    uint8_t keyboard_matrix[8];
    
    /* System state */
    bool running;
    uint64_t total_cycles;
    uint32_t frame_count;
    
    /* ROM loaded flags */
    bool basic_loaded;
    bool kernal_loaded;
    bool char_loaded;
    
} C64;

/* Initialization - internal implementation */
void c64_init_internal(C64 *c64);
void c64_reset(C64 *c64);

/* c64_init accepts optional debug parameter for compatibility */
#define c64_init_1(c64) c64_init_internal(c64)
#define c64_init_2(c64, debug) c64_init_internal(c64)
#define c64_init_SELECTOR(_1, _2, NAME, ...) NAME
#define c64_init(...) c64_init_SELECTOR(__VA_ARGS__, c64_init_2, c64_init_1)(__VA_ARGS__)

/* ROM loading */
bool c64_load_basic(C64 *c64, const char *filename);
bool c64_load_kernal(C64 *c64, const char *filename);
bool c64_load_char(C64 *c64, const char *filename);
bool c64_load_roms(C64 *c64, const char *rom_path);

/* Memory bus (with PLA banking) */
uint8_t c64_read(C64 *c64, uint16_t addr);
void c64_write(C64 *c64, uint16_t addr, uint8_t data);

/* VIC memory access (separate bus with different banking) */
uint8_t c64_vic_read(C64 *c64, uint16_t addr);

/* System tick - the heartbeat */
void c64_tick(C64 *c64);

/* Run for a specific number of cycles */
void c64_run_cycles(C64 *c64, uint32_t cycles);

/* Run one frame */
void c64_run_frame(C64 *c64);

/* Keyboard input */
void c64_key_press(C64 *c64, int row, int col);
void c64_key_release(C64 *c64, int row, int col);
void c64_key_clear(C64 *c64);

/* Screen rendering (ANSI) */
void c64_render_screen(C64 *c64);

/* Debug helpers */
void c64_dump_state(C64 *c64);
uint16_t c64_get_pc(C64 *c64);

/* Trap handlers (wraps cpu_trap) */
bool c64_trap(C64 *c64, uint16_t addr, handler_t handler);

/* Step one CPU instruction */
uint8_t c64_step(C64 *c64);

#endif /* C64_H */

/*
 * Lorenz test suite compatibility definitions
 * These are intentionally placed AFTER #endif to avoid conflicting
 * with cpu.PC struct member when c64.c includes this header.
 * The test file will pick these up after including c64.h.
 */
#ifdef LORENZ_TEST_COMPAT

/* C64 memory locations */
#ifndef UNUSED
#define UNUSED   0x0002  /* Unused zero page location */
#endif
#ifndef SAREG
#define SAREG    0x030C  /* Storage for .A register */
#endif
#ifndef WARM
#define WARM     0xA002  /* Warm start vector */
#endif
#ifndef READY
#define READY    0xA474  /* BASIC READY prompt */
#endif
#ifndef PULS
#define PULS     0xFF48  /* IRQ handler pulse routine */
#endif
#ifndef CHROUT
#define CHROUT   0xFFD2  /* Character output routine */
#endif
#ifndef GETIN
#define GETIN    0xFFE4  /* Get character input routine */
#endif
#ifndef CARTROM
#define CARTROM  0x8000  /* Cartridge ROM start */
#endif

/* Stack memory locations */
#ifndef PC
#define PC       0x01FE  /* Stack return address position */
#endif
#ifndef PCL
#define PCL      0x01FE  /* Low byte position after stack push */
#endif
#ifndef PCH
#define PCH      0x01FF  /* High byte position after stack push */
#endif

/* Vector addresses */
#ifndef IRQ
#define IRQ      0xFFFE  /* IRQ vector */
#endif

/* Helper function for writing multiple bytes */
static inline void cpu_write_data(CPU *cpu, uint16_t addr, uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        cpu_write(cpu, addr + i, data[i]);
    }
}

/* Helper function for writing a single byte */
static inline void cpu_write_byte(CPU *cpu, uint16_t addr, uint8_t data)
{
    cpu_write(cpu, addr, data);
}

/* Helper function for reading a single byte */
static inline uint8_t cpu_read_byte(CPU *cpu, uint16_t addr)
{
    return cpu_read(cpu, addr);
}

/* Alias for resetting PC */
#ifndef cpu_reset_pc
#define cpu_reset_pc cpu_reset_at
#endif

#endif /* LORENZ_TEST_COMPAT */
