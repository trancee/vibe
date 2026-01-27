#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "c64.h"

uint8_t c64_read(void *c64, uint16_t addr)
{
    return c64_read_byte((C64 *)c64, addr);
}
void c64_write(void *c64, uint16_t addr, uint8_t data)
{
    c64_write_byte((C64 *)c64, addr, data);
}

void c64_init(C64 *c64, bool debug)
{
    cpu_init(&c64->cpu);

    cpu_set_read_write(&c64->cpu, c64_read, c64_write);
    cpu_set_debug(&c64->cpu, debug, NULL);

    /* Clear ROMs */
    memset(c64->basic, 0, BASIC_ROM_SIZE);
    memset(c64->characters, 0, CHAR_ROM_SIZE);
    memset(c64->kernal, 0, KERNAL_ROM_SIZE);

    /* Load ROMs */
    load_rom("roms/basic.901226-01.bin", c64->basic, BASIC_ROM_SIZE);
    load_rom("roms/characters.901225-01.bin", c64->characters, CHAR_ROM_SIZE);
    load_rom("roms/kernal.901227-03.bin", c64->kernal, KERNAL_ROM_SIZE);

    /* Reset chips */
    c64_reset(c64);
}

void c64_reset(C64 *c64)
{
    cpu_reset(&c64->cpu);

    cia_reset(&c64->cia1);
    cia_reset(&c64->cia2);

    vic_reset(&c64->vic, c64->cpu.memory);

    // sid_reset(SAMPLE_RATE);

    // https://sta.c64.org/cbm64mem.html

    /// Processor port data direction register.
    c64_write_byte(c64, D6510, 0x2F); // %00101111

    /// Processor port.
    c64_write_byte(c64, R6510, 0x37); // %00110111

    /// Execution address of routine converting floating point to integer.
    c64_write_word(c64, ADRAY1, 0xB1AA);
    /// Execution address of routine converting integer to floating point.
    c64_write_word(c64, ADRAY2, 0xB391);

    /// Current I/O device number.
    c64_write_byte(c64, CHANNL, 0x00); // Keyboard for input and screen for output.
    /// Pointer to next expression in string stack.
    /// Values: $19; $1C; $1F; $22.
    c64_write_byte(c64, TEMPPT, 0x19);

    /// Pointer to beginning of BASIC area.
    c64_write_word(c64, TXTTAB, 0x0801);
    /// Pointer to end of BASIC area.
    c64_write_word(c64, MEMSIZ, 0xA000);

    /// Current input device number.
    c64_write_byte(c64, DFLTN, 0x00); // Keyboard.
    /// Current output device number.
    c64_write_byte(c64, DFLTO, 0x03); // Screen.

    /// Pointer to datasette buffer.
    c64_write_word(c64, TAPE1, 0x033C);

    /// Pointer to beginning of BASIC area after memory test.
    c64_write_word(c64, MEMSTR, 0x0800);
    /// Pointer to end of BASIC area after memory test.
    c64_write_word(c64, MEMEND, 0xA000);

    /// High byte of pointer to screen memory for screen input/output.
    c64_write_byte(c64, HIBASE, 0x04);

    /// Execution address of routine that, based on the status of shift keys, sets the pointer at memory address $00F5-$00F6 to the appropriate conversion table for converting keyboard matrix codes to PETSCII codes.
    c64_write_word(c64, KEYLOG, 0xEB48);

    /// Execution address of warm reset, displaying optional BASIC error message and entering BASIC idle loop.
    c64_write_word(c64, IERROR, 0xE38B);
    /// Execution address of BASIC idle loop.
    c64_write_word(c64, IMAIN, 0xA483);
    /// Execution address of BASIC line tokenizater routine.
    c64_write_word(c64, ICRNCH, 0xA57C);
    /// Execution address of BASIC token decoder routine.
    c64_write_word(c64, IQPLOP, 0xA71A);
    /// Execution address of BASIC instruction executor routine.
    c64_write_word(c64, IGONE, 0xA7E4);
    /// Execution address of routine reading next item of BASIC expression.
    c64_write_word(c64, IEVAL, 0xAE86);

    /// Execution address of interrupt service routine.
    c64_write_word(c64, CINV, 0xEA31);
    /// Execution address of BRK service routine.
    c64_write_word(c64, CNBINV, 0xFE66);
    /// Execution address of non-maskable interrupt service routine.
    c64_write_word(c64, NMINV, 0xFE47);
    /// Execution address of OPEN, routine opening files.
    c64_write_word(c64, IOPEN, 0xF34A);
    /// Execution address of CLOSE, routine closing files.
    c64_write_word(c64, ICLOSE, 0xF291);
    /// Execution address of CHKIN, routine defining file as default input.
    c64_write_word(c64, ICHKIN, 0xF20E);
    /// Execution address of CHKOUT, routine defining file as default output.
    c64_write_word(c64, ICKOUT, 0xF250);
    /// Execution address of CLRCHN, routine initializating input/output.
    c64_write_word(c64, ICLRCH, 0xF333);
    /// Execution address of CHRIN, data input routine, except for keyboard and RS232 input.
    c64_write_word(c64, IBASIN, 0xF157);
    /// Execution address of CHROUT, general purpose data output routine.
    c64_write_word(c64, IBSOUT, 0xF1CA);
    /// Execution address of STOP, routine checking the status of Stop key indicator, at memory address $0091.
    c64_write_word(c64, ISTOP, 0xF6ED);
    /// Execution address of GETIN, general purpose data input routine.
    c64_write_word(c64, IGETIN, 0xF13E);
    /// Execution address of CLALL, routine initializing input/output and clearing all file assignment tables.
    c64_write_word(c64, ICLALL, 0xF32F);
    /// Unused.
    c64_write_word(c64, USRCMD, 0xFE66);
    /// Execution address of LOAD, routine loading files.
    c64_write_word(c64, ILOAD, 0xF4A5);
    /// Execution address of SAVE, routine saving files.
    c64_write_word(c64, ISAVE, 0xF5ED);

    /// Hardware vectors
    /// Execution address of non-maskable interrupt service routine.
    c64_write_word(c64, NMI, 0xFE43);
    /// Execution address of cold reset.
    c64_write_word(c64, RESET, 0xFCE2);
    /// Execution address of interrupt service routine.
    c64_write_word(c64, IRQ, 0xFF48);
}
void c64_reset_pc(C64 *c64, uint16_t addr)
{
    cpu_reset_pc(&c64->cpu, addr);
}

uint16_t c64_get_pc(C64 *c64)
{
    return cpu_get_pc(&c64->cpu);
}
void c64_set_pc(C64 *c64, uint16_t addr)
{
    cpu_set_pc(&c64->cpu, addr);
}

/*
 Memory Configuration

  The signals /CharEn, /HiRam and /LoRam are used to select the memory
  configuration. They can be set using the processor port $01. The following
  logic applies (if expression on the right side is true, area mentioned on
  the left side will be activated. Otherwise, there is RAM):

    Kernal ROM = (/HiRam)

    Basic ROM  = (/LoRam AND /HiRam)

      That means, you cannot switch in Basic when the Kernal is off. Please
      note also, that an EPROM starting at $8000 will be disabled when you
      switch off the Basic ROM.

    Char. ROM  = ((NOT (/CharEn)) AND (/LoRam OR /HiRam))

    I/O-Area   = (/CharEn AND (/LoRam OR /HiRam))

      These two areas can just be activated, if at least one other signal is
      high. Then /CharEn selects either Char. ROM or I/O-Area.

  As a result, we get the following possibilities for the corresponding
  bits of $01:

       Bit+-------------+-----------+------------+
       210| $8000-$BFFF |$D000-$DFFF|$E000-$FFFF |
  +---+---+-------------+-----------+------------+
  | 7 |111| Cart.+Basic |    I/O    | Kernal ROM |
  +---+---+-------------+-----------+------------+
  | 6 |110|     RAM     |    I/O    | Kernal ROM |
  +---+---+-------------+-----------+------------+
  | 5 |101|     RAM     |    I/O    |    RAM     |
  +---+---+-------------+-----------+------------+
  | 4 |100|     RAM     |    RAM    |    RAM     |
  +---+---+-------------+-----------+------------+
  | 3 |011| Cart.+Basic | Char. ROM | Kernal ROM |
  +---+---+-------------+-----------+------------+
  | 2 |010|     RAM     | Char. ROM | Kernal ROM |
  +---+---+-------------+-----------+------------+
  | 1 |001|     RAM     | Char. ROM |    RAM     |
  +---+---+-------------+-----------+------------+
  | 0 |000|     RAM     |    RAM    |    RAM     |
  +---+---+-------------+-----------+------------+
       |||
 /CharEn|/LoRam
        |
      /HiRam
*/

#define LORAM(ddr, dr) ((ddr.loram && dr.loram) || !ddr.loram)
#define HIRAM(ddr, dr) ((ddr.hiram && dr.hiram) || !ddr.hiram)
#define CHAREN(ddr, dr) ((ddr.charen && dr.charen) || !ddr.charen)

uint8_t c64_read_byte(C64 *c64, uint16_t addr)
{
    data_direction_register_t ddr = (data_direction_register_t)cpu_read(&c64->cpu, D6510);
    data_register_t dr = (data_register_t)cpu_read(&c64->cpu, R6510);

    if (!HIRAM(ddr, dr) && !LORAM(ddr, dr)) // %x00
    {
        // RAM visible in all three areas.
    }

    else if (addr >= BASIC_ROM_START && addr <= BASIC_ROM_END)
    {
        // Basic ROM  = (/LoRam AND /HiRam)
        if (HIRAM(ddr, dr) && LORAM(ddr, dr)) // %x11
        {
            // printf("BASIC #$%04X → $%02X\n", addr, c64->basic[addr - BASIC_ROM_START]);
            return c64->basic[addr - BASIC_ROM_START];
        }
    }

    else if (addr >= CHAR_ROM_START && addr <= CHAR_ROM_END)
    {
        // Char. ROM  = ((NOT (/CharEn)) AND (/LoRam OR /HiRam))
        if (!CHAREN(ddr, dr) && (HIRAM(ddr, dr) || LORAM(ddr, dr))) // %0xx
        {
            // printf("CHARROM #$%04X → $%02X\n", addr, c64->characters[addr - CHAR_ROM_START]);
            return c64->characters[addr - CHAR_ROM_START];
        }

        // I/O-Area   = (/CharEn AND (/LoRam OR /HiRam))
        else if (CHAREN(ddr, dr) && (HIRAM(ddr, dr) || LORAM(ddr, dr))) // %1xx
        {
            if (addr >= VIC_MEM_START && addr <= VIC_MEM_END)
            {
                // printf("VIC #$%04X → $%02X\n", addr, vic_read(&c64->vic, addr));
                return vic_read(&c64->vic, addr);
            }

            else if (addr >= CIA1_MEM_START && addr <= CIA1_MEM_END)
            {
                // printf("CIA1 #$%04X → $%02X\n", addr, cia_read(&c64->cia1, addr));
                return cia_read(&c64->cia1, addr);
            }
            else if (addr >= CIA2_MEM_START && addr <= CIA2_MEM_END)
            {
                // printf("CIA2 #$%04X → $%02X\n", addr, cia_read(&c64->cia2, addr));
                return cia_read(&c64->cia2, addr);
            }
        }
    }

    else if (addr >= KERNAL_ROM_START && addr <= KERNAL_ROM_END)
    {
        // Kernal ROM = (/HiRam)
        if (HIRAM(ddr, dr)) // %x1x
        {
            // printf("KERNAL #$%04X → $%02X\n", addr, c64->kernal[addr - KERNAL_ROM_START]);
            return c64->kernal[addr - KERNAL_ROM_START];
        }
    }

    // printf("C64 #$%04X → $%02X\n", addr, cpu_read(&c64->cpu, addr));
    return cpu_read(&c64->cpu, addr);
}
uint16_t c64_read_word(C64 *c64, uint16_t addr)
{
    return cpu_read_word(&c64->cpu, addr);
}

void c64_write_byte(C64 *c64, uint16_t addr, uint8_t data)
{
    data_direction_register_t ddr = (data_direction_register_t)cpu_read(&c64->cpu, D6510);
    data_register_t dr = (data_register_t)cpu_read(&c64->cpu, R6510);

    // if (addr == D6510)
    //     printf("\n----  #$%02X\n", data);
    // if (addr == R6510)
    //     printf("\n----  #$%02X [%d%d%d]\n", data, ((data_register_t)data).charen, ((data_register_t)data).hiram, ((data_register_t)data).loram);

    if (!HIRAM(ddr, dr) && !LORAM(ddr, dr)) // %x00
    {
        // RAM visible in all three areas.
    }

    else if (addr >= CHAR_ROM_START && addr < CHAR_ROM_END)
    {
        // I/O-Area   = (/CharEn AND (/LoRam OR /HiRam))
        if (CHAREN(ddr, dr) && (HIRAM(ddr, dr) || LORAM(ddr, dr))) // %1xx
        {
            if (addr >= VIC_MEM_START && addr <= VIC_MEM_END)
            {
                // printf("VIC #$%04X ← $%02X\n", addr, data);
                return vic_write(&c64->vic, addr, data);
            }

            else if (addr >= CIA1_MEM_START && addr <= CIA1_MEM_END)
            {
                // printf("CIA1 #$%04X ← $%02X\n", addr, data);
                return cia_write(&c64->cia1, addr, data);
            }
            else if (addr >= CIA2_MEM_START && addr <= CIA2_MEM_END)
            {
                // printf("CIA2 #$%04X ← $%02X\n", addr, data);
                return cia_write(&c64->cia2, addr, data);
            }
        }
    }

    // printf("C64 #$%04X ← $%02X\n", addr, data);
    cpu_write(&c64->cpu, addr, data);
}
void c64_write_word(C64 *c64, uint16_t addr, uint16_t data)
{
    cpu_write_word(&c64->cpu, addr, data);
}

void c64_write_data(C64 *c64, uint16_t addr, uint8_t data[], size_t size)
{
    cpu_write_data(&c64->cpu, addr, data, size);
}

bool c64_trap(C64 *c64, uint16_t addr, handler_t handler)
{
    return cpu_trap(&c64->cpu, addr, handler);
}

uint8_t c64_step(C64 *c64)
{
    uint8_t steps = cpu_step(&c64->cpu);

    vic_clock(&c64->vic /*, CYCLES_PER_FRAME*/);
    vic_clock(&c64->vic /*, CYCLES_PER_FRAME*/);

    cia_clock(&c64->cia1 /*, CYCLES_PER_FRAME*/);
    cia_clock(&c64->cia2 /*, CYCLES_PER_FRAME*/);

    // sid_clock(&sid, CYCLES_PER_FRAME);

    return steps;
}

/* ============================================================
   ROM Loading
   ============================================================ */

void load_rom(const char *path, uint8_t *memory, size_t size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        printf("Missing ROM: %s\n", path);
        exit(1);
    }
    size_t read = fread(memory, 1, size, f);
    assert(read == size);
    fclose(f);
}
