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
    load_rom("roms/characters.906143-02.bin", c64->characters, CHAR_ROM_SIZE);
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

    /// 0             $0             D6510
    /// 6510 On-Chip I/O DATA Direction Register
    ///
    /// Bit 0: Direction of Bit 0 I/O on port at next address.  Default = 1 (output)
    /// Bit 1: Direction of Bit 1 I/O on port at next address.  Default = 1 (output)
    /// Bit 2: Direction of Bit 2 I/O on port at next address.  Default = 1 (output)
    /// Bit 3: Direction of Bit 3 I/O on port at next address.  Default = 1 (output)
    /// Bit 4: Direction of Bit 4 I/O on port at next address.  Default = 0 (input)
    /// Bit 5: Direction of Bit 5 I/O on port at next address.  Default = 1 (output)
    /// Bit 6: Direction of Bit 6 I/O on port at next address.  Not used.
    /// Bit 7: Direction of Bit 7 I/O on port at next address.  Not used.

    /// Processor port data direction register.
    c64_write_byte(c64, D6510, 0xEF);

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
                               The area at $d000-$dfff with
                                  CHAREN=1     CHAREN=0

 $ffff +--------------+  /$e000 +----------+  +----------+
       |  Kernal ROM  | /       |  I/O  2  |  |          |
 $e000 +--------------+/  $df00 +----------+  |          |
       |I/O, Char ROM |         |  I/O  1  |  |          |
 $d000 +--------------+\  $de00 +----------+  |          |
       |     RAM      | \       |  CIA  2  |  |          |
 $c000 +--------------+  \$dd00 +----------+  |          |
       |  Basic ROM   |         |  CIA  1  |  |          |
 $a000 +--------------+   $dc00 +----------+  | Char ROM |
       |              |         |Color RAM |  |          |
       .     RAM      .         |          |  |          |
       .              .   $d800 +----------+  |          |
       |              |         |   SID    |  |          |
 $0002 +--------------+         |registers |  |          |
       | I/O port DR  |   $d400 +----------+  |          |
 $0001 +--------------+         |   VIC    |  |          |
       | I/O port DDR |         |registers |  |          |
 $0000 +--------------+   $d000 +----------+  +----------+

  +---------+---+------------+--------------------------------------------+
  |  NAME   |BIT| DIRECTION  |                 DESCRIPTION                |
  +---------+---+------------+--------------------------------------------+
  |  LORAM  | 0 |   OUTPUT   | Control for RAM/ROM at $A000-$BFFF         |
  |  HIRAM  | 1 |   OUTPUT   | Control for RAM/ROM at $E000-$FFFF         |
  |  CHAREN | 2 |   OUTPUT   | Control for I/O/ROM at $D000-$DFFF         |
  |         | 3 |   OUTPUT   | Cassette write line                        |
  |         | 4 |   INPUT    | Cassette switch sense (0=play button down) |
  |         | 5 |   OUTPUT   | Cassette motor control (0=motor spins)     |
  +---------+---+------------+--------------------------------------------+
*/

uint8_t c64_read_byte(C64 *c64, uint16_t addr)
{
    data_register_t port = (data_register_t)cpu_read(&c64->cpu, R6510);

    if (addr >= BASIC_ROM_START && addr <= BASIC_ROM_END)
    {
        if (
            (port.charen && port.hiram && port.loram) // Mode 31 / 15
            ||
            (!(port.charen) && port.hiram && port.loram) // Mode 27 / 11
        )
        {
            // printf("BASIC #$%04X → $%02X\n", addr, c64->basic[addr - BASIC_ROM_START]);
            return c64->basic[addr - BASIC_ROM_START];
        }
    }
    else if (addr >= CHAR_ROM_START && addr <= CHAR_ROM_END)
    {
        if (
            (!(port.charen) && port.hiram && port.loram) // Mode 27 / 11 / 3
            ||
            (!(port.charen) && port.hiram && !(port.loram)) // Mode 26 / 10 / 2
            ||
            (!(port.charen) && !(port.hiram) && port.loram) // Mode 25 / 9
        )
        {
            // printf("CHARROM #$%04X → $%02X\n", addr, c64->characters[addr - CHAR_ROM_START]);
            return c64->characters[addr - CHAR_ROM_START];
        }
        else if (
            (port.charen && port.hiram && port.loram) // Mode 31 / 15 / 7
            ||
            (port.charen && port.hiram && !(port.loram)) // Mode 30 / 14 / 6
            ||
            (port.charen && !(port.hiram) && port.loram) // Mode 29 / 13 / 5
        )
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
        if (
            (port.charen && port.hiram && port.loram) // Mode 31 / 15 / 7
            ||
            (port.charen && port.hiram && !(port.loram)) // Mode 30 / 14 / 6
            ||
            (!(port.charen) && port.hiram && port.loram) // Mode 27 / 11 / 3
            ||
            (!(port.charen) && port.hiram && !(port.loram)) // Mode 26 / 10 / 2
        )
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
    data_register_t port = (data_register_t)cpu_read(&c64->cpu, R6510);

    // if (addr == R6510)
    //     printf("\n----  #$%02X [%d%d%d]\n", data, ((data_register_t)data).charen, ((data_register_t)data).hiram, ((data_register_t)data).loram);

    if (addr >= CHAR_ROM_START && addr < CHAR_ROM_END)
    {
        if (
            (port.charen && port.hiram && port.loram) // Mode 31 / 15 / 7
            ||
            (port.charen && port.hiram && !(port.loram)) // Mode 30 / 14 / 6
            ||
            (port.charen && !(port.hiram) && port.loram) // Mode 29 / 13 / 5
        )
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
