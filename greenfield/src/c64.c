/*
 * C64 Emulator - Main System Implementation
 *
 * System integration including:
 * - Memory bus with PLA bank switching
 * - ROM loading
 * - c64_tick heartbeat orchestration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "c64.h"

/* CPU memory read callback - routes through PLA */
static uint8_t cpu_mem_read(void *context, uint16_t addr)
{
    C64 *c64 = (C64 *)context;
    return c64_read(c64, addr);
}

/* CPU memory write callback - routes through PLA */
static void cpu_mem_write(void *context, uint16_t addr, uint8_t data)
{
    C64 *c64 = (C64 *)context;
    c64_write(c64, addr, data);
}

void c64_init_internal(C64 *c64)
{
    memset(c64, 0, sizeof(C64));

    /* Initialize components */
    clock_init(&c64->clock, C64_PAL_CLOCK);
    cpu_init(&c64->cpu);
    vic_init(&c64->vic, c64->ram);
    cia_init(&c64->cia1, C64_CIA1_START);
    cia_init(&c64->cia2, C64_CIA2_START);
    sid_init(&c64->sid, C64_PAL_CLOCK, 44100);

    /* Set CPU memory callbacks */
    c64->cpu.context = c64;
    c64->cpu.read = cpu_mem_read;
    c64->cpu.write = cpu_mem_write;

    /* Default CPU port values */
    c64->cpu_port_ddr = 0x2F;
    c64->cpu_port = 0x37;
    c64->cpu_port_floating = 0xFF; /* Initial floating state */

    /* Clear keyboard matrix (all keys released) */
    memset(c64->keyboard_matrix, 0xFF, sizeof(c64->keyboard_matrix));

    c64->running = true;
}

void c64_reset(C64 *c64)
{
    clock_reset(&c64->clock);
    cpu_reset(&c64->cpu);
    vic_reset(&c64->vic);
    cia_reset(&c64->cia1);
    cia_reset(&c64->cia2);
    sid_reset(&c64->sid);

    /* Reset CPU port */
    c64->cpu_port_ddr = 0x2F;
    c64->cpu_port = 0x37;
    c64->cpu_port_floating = 0xFF;

    /* Clear interrupts */
    c64->nmi_line = false;
    c64->irq_line = false;
    c64->ba_low = false;

    /* Clear keyboard */
    memset(c64->keyboard_matrix, 0xFF, sizeof(c64->keyboard_matrix));

    c64->total_cycles = 0;
    c64->frame_count = 0;
    c64->running = true;

    /* Read reset vector from KERNAL */
    uint16_t reset_lo = c64_read(c64, 0xFFFC);
    uint16_t reset_hi = c64_read(c64, 0xFFFD);
    c64->cpu.PC = (reset_hi << 8) | reset_lo;
}

bool c64_load_basic(C64 *c64, const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        fprintf(stderr, "Failed to open BASIC ROM: %s\n", filename);
        return false;
    }

    size_t bytes = fread(c64->basic_rom, 1, C64_BASIC_SIZE, f);
    fclose(f);

    if (bytes != C64_BASIC_SIZE)
    {
        fprintf(stderr, "Invalid BASIC ROM size: %zu (expected %d)\n", bytes, C64_BASIC_SIZE);
        return false;
    }

    c64->basic_loaded = true;
    return true;
}

bool c64_load_kernal(C64 *c64, const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        fprintf(stderr, "Failed to open KERNAL ROM: %s\n", filename);
        return false;
    }

    size_t bytes = fread(c64->kernal_rom, 1, C64_KERNAL_SIZE, f);
    fclose(f);

    if (bytes != C64_KERNAL_SIZE)
    {
        fprintf(stderr, "Invalid KERNAL ROM size: %zu (expected %d)\n", bytes, C64_KERNAL_SIZE);
        return false;
    }

    c64->kernal_loaded = true;
    return true;
}

bool c64_load_char(C64 *c64, const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        fprintf(stderr, "Failed to open CHAR ROM: %s\n", filename);
        return false;
    }

    size_t bytes = fread(c64->char_rom, 1, C64_CHAR_SIZE, f);
    fclose(f);

    if (bytes != C64_CHAR_SIZE)
    {
        fprintf(stderr, "Invalid CHAR ROM size: %zu (expected %d)\n", bytes, C64_CHAR_SIZE);
        return false;
    }

    c64->char_loaded = true;
    return true;
}

bool c64_load_roms(C64 *c64, const char *rom_path)
{
    char path[512];
    bool success = true;

    snprintf(path, sizeof(path), "%s/basic.rom", rom_path);
    if (!c64_load_basic(c64, path))
        success = false;

    snprintf(path, sizeof(path), "%s/kernal.rom", rom_path);
    if (!c64_load_kernal(c64, path))
        success = false;

    snprintf(path, sizeof(path), "%s/char.rom", rom_path);
    if (!c64_load_char(c64, path))
        success = false;

    return success;
}

uint8_t c64_read(C64 *c64, uint16_t addr)
{
    /* CPU port at $0000/$0001 */
    if (addr == 0x0000)
    {
        return c64->cpu_port_ddr;
    }
    if (addr == 0x0001)
    {
        /* 6510 CPU port read behavior:
         * - Output bits (DDR=1): read from port latch
         * - Input bits (DDR=0): different behavior per bit group
         *
         * C64 hardware specifics:
         * - Bits 0,1,2,4,5 (0x37): Have external pull-ups/hardware
         *   When input: read high (pulled up), OR'd with port latch
         *   Formula: (~DDR | PORT) for these bits
         * - Bits 3,6,7 (0xC8): Truly floating
         *   When input: retain last driven value (capacitance)
         * - Bit 5 exception: Pulled LOW when configured as input
         */
        uint8_t ddr = c64->cpu_port_ddr;
        uint8_t port = c64->cpu_port;

        /* Output bits: read from port latch */
        uint8_t output_bits = port & ddr;

        /* Input bits with external pull-ups (0,1,2,4,5): read high */
        uint8_t pulled_up = (~ddr | port) & 0x37;

        /* Floating bits (3,6,7): read from floating state */
        uint8_t floating_bits = c64->cpu_port_floating & ~ddr & 0xC8;

        uint8_t result = output_bits | pulled_up | floating_bits;

        /* Bit 5 is drawn low when configured as input */
        if (!(ddr & 0x20))
        {
            result &= 0xDF;
        }
        return result;
    }

    /* Basic ROM at $A000-$BFFF */
    if (addr >= C64_BASIC_START && addr <= C64_BASIC_END)
    {
        if ((c64->cpu_port & CPU_PORT_LORAM) && c64->basic_loaded)
        {
            return c64->basic_rom[addr - C64_BASIC_START];
        }
        return c64->ram[addr];
    }

    /* I/O or CHAR ROM at $D000-$DFFF */
    if (addr >= C64_IO_START && addr <= C64_IO_END)
    {
        if (c64->cpu_port & CPU_PORT_CHAREN)
        {
            /* I/O space */
            if (addr >= C64_VIC_START && addr <= C64_VIC_END)
            {
                return vic_read(&c64->vic, addr);
            }
            if (addr >= C64_SID_START && addr <= C64_SID_END)
            {
                return sid_read(&c64->sid, addr);
            }
            if (addr >= C64_COLOR_START && addr <= C64_COLOR_END)
            {
                return c64->color_ram[addr - C64_COLOR_START] | 0xF0;
            }
            if (addr >= C64_CIA1_START && addr <= C64_CIA1_END)
            {
                /* CIA1 with keyboard matrix */
                if ((addr & 0x0F) == CIA_PRA || (addr & 0x0F) == CIA_PRB)
                {
                    /* Scan keyboard matrix */
                    uint8_t cols = c64->cia1.pra;
                    uint8_t rows = 0xFF;
                    for (int i = 0; i < 8; i++)
                    {
                        if (!(cols & (1 << i)))
                        {
                            rows &= c64->keyboard_matrix[i];
                        }
                    }
                    if ((addr & 0x0F) == CIA_PRB)
                    {
                        return rows;
                    }
                }
                return cia_read(&c64->cia1, addr);
            }
            if (addr >= C64_CIA2_START && addr <= C64_CIA2_END)
            {
                return cia_read(&c64->cia2, addr);
            }
            /* Unmapped I/O returns floating bus value */
            return (addr >> 8) & 0xFF;
        }
        else
        {
            /* Character ROM */
            if (c64->char_loaded)
            {
                return c64->char_rom[addr - C64_CHAR_START];
            }
            return c64->ram[addr];
        }
    }

    /* KERNAL ROM at $E000-$FFFF */
    if (addr >= C64_KERNAL_START)
    {
        if ((c64->cpu_port & CPU_PORT_HIRAM) && c64->kernal_loaded)
        {
            return c64->kernal_rom[addr - C64_KERNAL_START];
        }
        return c64->ram[addr];
    }

    /* Default: RAM */
    return c64->ram[addr];
}

void c64_write(C64 *c64, uint16_t addr, uint8_t data)
{
    /* CPU port at $0000/$0001 */
    if (addr == 0x0000)
    {
        /* When DDR changes, update floating state for bits becoming inputs:
         * Bits that were outputs (old DDR=1) keep their driven value in floating state
         */
        uint8_t becoming_input = c64->cpu_port_ddr & ~data; /* bits going 1->0 */
        c64->cpu_port_floating = (c64->cpu_port_floating & ~becoming_input) |
                                 (c64->cpu_port & becoming_input);
        c64->cpu_port_ddr = data;
        return;
    }
    if (addr == 0x0001)
    {
        /* Update floating state for output bits (they drive the bus) */
        c64->cpu_port_floating = (c64->cpu_port_floating & ~c64->cpu_port_ddr) |
                                 (data & c64->cpu_port_ddr);
        c64->cpu_port = data;
        return;
    }

    /* I/O space at $D000-$DFFF */
    if (addr >= C64_IO_START && addr <= C64_IO_END)
    {
        if (c64->cpu_port & CPU_PORT_CHAREN)
        {
            /* I/O writes */
            if (addr >= C64_VIC_START && addr <= C64_VIC_END)
            {
                vic_write(&c64->vic, addr, data);
                return;
            }
            if (addr >= C64_SID_START && addr <= C64_SID_END)
            {
                sid_write(&c64->sid, addr, data);
                return;
            }
            if (addr >= C64_COLOR_START && addr <= C64_COLOR_END)
            {
                c64->color_ram[addr - C64_COLOR_START] = data & 0x0F;
                return;
            }
            if (addr >= C64_CIA1_START && addr <= C64_CIA1_END)
            {
                cia_write(&c64->cia1, addr, data);
                return;
            }
            if (addr >= C64_CIA2_START && addr <= C64_CIA2_END)
            {
                cia_write(&c64->cia2, addr, data);
                return;
            }
            return;
        }
        /* CHAR ROM selected - writes go to RAM underneath */
    }

    /* All writes go to RAM (ROM is read-only) */
    c64->ram[addr] = data;
}

uint8_t c64_vic_read(C64 *c64, uint16_t addr)
{
    /* VIC sees different memory mapping based on CIA2 port A */
    uint16_t vic_bank = (~c64->cia2.pra & 0x03) << 14;
    uint16_t full_addr = vic_bank | (addr & 0x3FFF);

    /* Character ROM appears at $1000-$1FFF in banks 0 and 2 */
    if ((vic_bank == 0x0000 || vic_bank == 0x8000) &&
        (addr & 0x3000) == 0x1000)
    {
        if (c64->char_loaded)
        {
            return c64->char_rom[addr & 0x0FFF];
        }
    }

    return c64->ram[full_addr];
}

void c64_tick(C64 *c64)
{
    /* Clock the system clock */
    clock_step(&c64->clock, 1);

    /* Clock VIC (every cycle) */
    vic_clock(&c64->vic);

    /* Update BA line from VIC */
    c64->ba_low = c64->vic.ba_low;

    /* Clock CIAs (every cycle) - this updates irq_pending states */
    cia_clock(&c64->cia1);
    cia_clock(&c64->cia2);

    /* Clock SID (every cycle for accurate envelope/oscillator) */
    sid_clock(&c64->sid, 1);

    /* Update CPU interrupt lines from chips (must be AFTER cia_clock) */
    if (c64->cia1.irq_pending)
    {
        c64->cpu.irq_pending = true;
    }
    if (c64->vic.irq_pending)
    {
        c64->cpu.irq_pending = true;
    }
    if (c64->cia2.irq_pending)
    {
        c64->cpu.nmi_pending = true;
    }

    c64->total_cycles++;
}

void c64_run_cycles(C64 *c64, uint32_t cycles)
{
    for (uint32_t i = 0; i < cycles && c64->running; i++)
    {
        c64_tick(c64);
    }
}

void c64_run_frame(C64 *c64)
{
    c64_run_cycles(c64, C64_CYCLES_PER_FRAME);
    c64->frame_count++;
}

void c64_key_press(C64 *c64, int row, int col)
{
    if (row >= 0 && row < 8 && col >= 0 && col < 8)
    {
        c64->keyboard_matrix[col] &= ~(1 << row);
    }
}

void c64_key_release(C64 *c64, int row, int col)
{
    if (row >= 0 && row < 8 && col >= 0 && col < 8)
    {
        c64->keyboard_matrix[col] |= (1 << row);
    }
}

void c64_key_clear(C64 *c64)
{
    memset(c64->keyboard_matrix, 0xFF, sizeof(c64->keyboard_matrix));
}

void c64_render_screen(C64 *c64)
{
    /* Read screen memory and character ROM, render to console */
    uint16_t screen_base = ((c64->vic.mem_pointers >> 4) & 0x0F) << 10;
    uint16_t vic_bank = (~c64->cia2.pra & 0x03) << 14;

    /* Clear screen and home cursor */
    printf("\033[H\033[2J");

    /* Border color */
    uint8_t border = c64->vic.border_color & 0x0F;

    /* Render top border */
    printf("\033[48;5;%dm", c64_to_ansi256[border]);
    for (int i = 0; i < C64_SCREEN_COLS + 4; i++)
        printf("  ");
    printf("\033[0m\n");

    /* Render screen rows */
    for (int row = 0; row < C64_SCREEN_ROWS; row++)
    {
        /* Left border */
        printf("\033[48;5;%dm  \033[0m", c64_to_ansi256[border]);

        for (int col = 0; col < C64_SCREEN_COLS; col++)
        {
            uint16_t screen_addr = screen_base + row * 40 + col;
            uint8_t char_code = c64->ram[vic_bank | screen_addr];
            uint8_t color = c64->color_ram[row * 40 + col] & 0x0F;
            uint8_t bg = c64->vic.background[0] & 0x0F;

            /* Get character pattern from ROM (or RAM) */
            uint16_t char_base = ((c64->vic.mem_pointers >> 1) & 0x07) << 11;
            uint16_t char_addr = char_base + char_code * 8;
            uint8_t char_row = c64_vic_read(c64, char_addr + (row % 8));

            /* Render as block character based on upper nibble of char pattern */
            /* Simplified: just show foreground/background based on char */
            if (char_code == 32 || char_code == 0)
            {
                /* Space - background */
                printf("\033[48;5;%dm  \033[0m", c64_to_ansi256[bg]);
            }
            else
            {
                /* Character - foreground color */
                printf("\033[48;5;%d;38;5;%dm", c64_to_ansi256[bg], c64_to_ansi256[color]);
                /* Try to map to PETSCII */
                if (char_code >= 1 && char_code <= 26)
                {
                    printf("%c ", 'A' + char_code - 1);
                }
                else if (char_code >= 0x41 && char_code <= 0x5A)
                {
                    printf("%c ", char_code);
                }
                else
                {
                    printf("# ");
                }
                printf("\033[0m");
            }
        }

        /* Right border */
        printf("\033[48;5;%dm  \033[0m", c64_to_ansi256[border]);
        printf("\n");
    }

    /* Bottom border */
    printf("\033[48;5;%dm", c64_to_ansi256[border]);
    for (int i = 0; i < C64_SCREEN_COLS + 4; i++)
        printf("  ");
    printf("\033[0m\n");
}

void c64_dump_state(C64 *c64)
{
    printf("C64 State:\n");
    printf("  PC: $%04X  A: $%02X  X: $%02X  Y: $%02X  SP: $%02X\n",
           c64->cpu.PC, c64->cpu.A, c64->cpu.X, c64->cpu.Y, c64->cpu.SP);
    printf("  P: %c%c%c%c%c%c%c%c ($%02X)\n",
           c64->cpu.P & FLAG_NEGATIVE ? 'N' : '-',
           c64->cpu.P & FLAG_OVERFLOW ? 'V' : '-',
           c64->cpu.P & FLAG_RESERVED ? 'U' : '-',
           c64->cpu.P & FLAG_BREAK ? 'B' : '-',
           c64->cpu.P & FLAG_DECIMAL ? 'D' : '-',
           c64->cpu.P & FLAG_INTERRUPT ? 'I' : '-',
           c64->cpu.P & FLAG_ZERO ? 'Z' : '-',
           c64->cpu.P & FLAG_CARRY ? 'C' : '-',
           c64->cpu.P);
    printf("  Cycles: %llu  Frame: %u\n", c64->total_cycles, c64->frame_count);
    printf("  VIC Raster: %d  IRQ: %d\n", c64->vic.current_raster, c64->vic.irq_pending);
    printf("  CIA1 IRQ: %d  CIA2 NMI: %d\n", c64->cia1.irq_pending, c64->cia2.irq_pending);
}

uint16_t c64_get_pc(C64 *c64)
{
    return c64->cpu.PC;
}

bool c64_trap(C64 *c64, uint16_t addr, handler_t handler)
{
    return cpu_trap(&c64->cpu, addr, handler);
}

uint8_t c64_step(C64 *c64)
{
    /* Step CPU and return cycles used */
    uint8_t cycles = cpu_step(&c64->cpu);

    /* Tick components for each cycle */
    for (int i = 0; i < cycles; i++)
    {
        c64_tick(c64);
    }

    return cycles;
}
