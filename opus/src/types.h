/**
 * types.h - Common type definitions for C64 emulator
 */

#ifndef C64_TYPES_H
#define C64_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Standard integer types
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

// Memory sizes
#define C64_RAM_SIZE      65536
#define C64_COLOR_RAM_SIZE 1024
#define C64_BASIC_SIZE    8192
#define C64_KERNAL_SIZE   8192
#define C64_CHAR_SIZE     4096

// Clock frequencies (PAL)
#define C64_CPU_FREQ      985248      // ~0.985 MHz
#define C64_VIC_FREQ      (C64_CPU_FREQ * 8)  // VIC runs at 8x CPU clock
#define C64_FRAME_CYCLES  19656       // PAL: 312 lines * 63 cycles

// PAL timing constants
#define PAL_RASTER_LINES     312
#define PAL_CYCLES_PER_LINE  63
#define PAL_VISIBLE_LINES    200
#define PAL_FIRST_VISIBLE    51
#define PAL_LAST_VISIBLE     250

// Screen dimensions
#define SCREEN_COLS 40
#define SCREEN_ROWS 25

// C64 color palette (indices 0-15)
typedef enum {
    C64_BLACK = 0,
    C64_WHITE,
    C64_RED,
    C64_CYAN,
    C64_PURPLE,
    C64_GREEN,
    C64_BLUE,
    C64_YELLOW,
    C64_ORANGE,
    C64_BROWN,
    C64_LIGHT_RED,
    C64_DARK_GREY,
    C64_GREY,
    C64_LIGHT_GREEN,
    C64_LIGHT_BLUE,
    C64_LIGHT_GREY
} C64Color;

#endif // C64_TYPES_H
