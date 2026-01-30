#ifndef C64_H
#define C64_H

#include <stdint.h>
#include <stddef.h>

#include "mos6510.h"
#include "cia6526.h"
#include "sid6581.h"
#include "vic.h"
#include "clock.h"

#define PAL_CPU_FREQUENCY  985248
#define NTSC_CPU_FREQUENCY 1022727

#define BASIC_ROM_START 0xA000
#define BASIC_ROM_END BASIC_ROM_START + (BASIC_ROM_SIZE - 1)
#define BASIC_ROM_SIZE 0x2000
#define CHAR_ROM_START 0xD000
#define CHAR_ROM_END CHAR_ROM_START + (CHAR_ROM_SIZE - 1)
#define CHAR_ROM_SIZE 0x1000
#define KERNAL_ROM_START 0xE000
#define KERNAL_ROM_END KERNAL_ROM_START + (KERNAL_ROM_SIZE - 1)
#define KERNAL_ROM_SIZE 0x2000

typedef union
{
    uint8_t v;
    struct
    {
        uint8_t loram : 1;  // Bit 0: Direction of Bit 0 I O on port at next address. Default = 1(output)
        uint8_t hiram : 1;  // Bit 1: Direction of Bit 1 I/O on port at next address. Default = 1 (output)
        uint8_t charen : 1; // Bit 2: Direction of Bit 2 I/O on port at next address. Default = 1 (output)

        uint8_t cass_wrt : 1;   // Bit 3: Direction of Bit 3 I/O on port at next address. Default = 1 (output)
        uint8_t cass_sense : 1; // Bit 4: Direction of Bit 4 I/O on port at next address. Default = 0 (input)
        uint8_t cass_motor : 1; // Bit 5: Direction of Bit 5 I/O on port at next address. Default = 1 (output)

        uint8_t undefined : 2; // Bit 6-7: Direction of Bit 6-7 I/O on port at next address. Not used.
    };
} data_direction_register_t;

typedef union
{
    uint8_t v;
    struct
    {
        uint8_t loram : 1;  // Bit 0 (LORAM): Controls if BASIC ROM is visible (1 = Visible, 0 = RAM).
        uint8_t hiram : 1;  // Bit 1 (HIRAM): Controls if KERNAL ROM is visible (1 = Visible, 0 = RAM).
        uint8_t charen : 1; // Bit 2 (CHAREN): Controls if Character ROM or I/O is visible (1 = Character ROM/IO, 0 = RAM).

        uint8_t cass_wrt : 1;   // Bit 3: Cassette Data Output Line.
        uint8_t cass_sense : 1; // Bit 4: Cassette Switch Sense: 1 = Switch Closed
        uint8_t cass_motor : 1; // Bit 5: Cassette Motor Control 0 = ON, 1 = OFF

        uint8_t undefined : 2; // Bits 6-7: Undefined.
    };
} data_register_t;

// $01 bits 6 and 7 fall-off cycles (1->0), average is about 350 msec
#define CPU_DATA_PORT_FALL_OFF_CYCLES 350000

typedef struct
{
    // value read from processor port
    uint8_t dataRead;

	// State of processor port pins.
	uint8_t dataOut;

    // cycle that should invalidate the unused bits of the data port.
    int64_t dataSetClkBit6;
    int64_t dataSetClkBit7;

    // indicates if the unused bits of the data port are still valid or should be
    // read as 0, 1 = unused bits valid, 0 = unused bits should be 0
    bool dataSetBit6;
    bool dataSetBit7;

    // indicates if the unused bits are in the process of falling off. 
    bool dataFalloffBit6;
    bool dataFalloffBit7;
} zero_ram_t;

typedef struct
{
    CPU cpu;
    CIA cia1, cia2;
    SID sid;
    VIC vic;

    uint8_t basic[BASIC_ROM_SIZE];
    uint8_t characters[CHAR_ROM_SIZE];
    uint8_t kernal[KERNAL_ROM_SIZE];

    zero_ram_t zero_ram;

    clock_t clock;
} C64;

void c64_init(C64 *c64);
void c64_reset(C64 *c64);
void c64_reset_pc(C64 *c64, uint16_t addr);
uint16_t c64_get_pc(C64 *c64);
void c64_set_pc(C64 *c64, uint16_t addr);
uint8_t c64_read_byte(C64 *c64, uint16_t addr);
uint16_t c64_read_word(C64 *c64, uint16_t addr);
void c64_write_byte(C64 *c64, uint16_t addr, uint8_t data);
void c64_write_word(C64 *c64, uint16_t addr, uint16_t data);
void c64_write_data(C64 *c64, uint16_t addr, uint8_t data[], size_t size);
bool c64_trap(C64 *c64, uint16_t addr, handler_t handler);
uint8_t c64_step(C64 *c64);

void c64_set_debug(C64 *c64, bool debug, FILE *debug_file);

void load_rom(const char *path, uint8_t *memory, size_t size);

/*
         MOS 6510 Micro-Processor
           On-Chip I/O Port
  0      /LORAM Signal (0=Switch BASIC ROM Out)
  1      /HIRAM Signal (0=Switch Kernal ROM Out)
  2      /CHAREN Signal (0=Switch Char. ROM In)
  3      Cassette Data Output Line
  4      Cassette Switch Sense: 1 = Switch Closed
  5      Cassette Motor Control 0 = ON, 1 = OFF
  6-7    Undefined
*/
#define IO_PORT_LORAM 0x01
#define IO_PORT_HIRAM 0x02
#define IO_PORT_CHAREN 0x04

// typedef struct R6510
// {
//     uint8_t loram : 1;  // BASIC ROM enable
//     uint8_t hiram : 1;  // KERNAL ROM enable
//     uint8_t charen : 1; // Character ROM enable
//     uint8_t cas_out : 1; // Cassette data output
//     uint8_t cas_sense : 1; // Cassette switch sense
//     uint8_t cas_motor : 1; // Cassette motor control
//     uint8_t unused : 2; // Undefined bits
// } io_port_t;

/// Page 0

#define D6510 0x0000
#define R6510 0x0001
#define UNUSED 0x0002
#define ADRAY1 0x0003 // WORD
#define ADRAY2 0x0005 // WORD
#define CHARAC 0x0007
// #define INTEGR 0x0007
#define ENDCHR 0x0008
#define TRMPOS 0x0009
#define VERCK 0x000A
#define COUNT 0x000B
#define DIMFLG 0x000C
#define VALTYP 0x000D
#define INTFLG 0x000E
#define GARBFL 0x000F
#define SUBFLG 0x0010
#define INPFLG 0x0011
#define TANSGN 0x0012
#define CHANNL 0x0013
#define LINNUM 0x0014 // WORD
#define TEMPPT 0x0016
#define LASTPT 0x0017 // WORD
#define TEMPST 0x0019 // ARRAY [3*3]
#define INDEX 0x0022  // ARRAY [4]
#define RESHO 0x0026  // ARRAY [5]
#define TXTTAB 0x002B // WORD
#define VARTAB 0x002D // WORD
#define ARYTAB 0x002F // WORD
#define STREND 0x0031 // WORD
#define FRETOP 0x0033 // WORD
#define FRESPC 0x0035 // WORD
#define MEMSIZ 0x0037 // WORD
#define CURLIN 0x0039 // WORD
#define OLDLIN 0x003B // WORD
#define OLDTXT 0x003D // WORD
#define DATLIN 0x003F // WORD
#define DATPTR 0x0041 // WORD
#define INPPTR 0x0043 // WORD
#define VARNAM 0x0045 // WORD
#define VARPNT 0x0047 // WORD
#define FORPNT 0x0049 // WORD
#define OPPTR 0x004B  // WORD
#define OPMASK 0x004D
#define DEFPNT 0x004E // WORD
#define DSCPNT 0x0050 // PTR + LEN
#define FOUR6 0x0053
#define JMPER 0x0054 // OP + PTR
// #define TEMPF1 0x0057
// #define TEMPF2 0x005C
#define FAC1 0x0061   // FP
#define FACEXP 0x0061 // Exponent
#define FACHO 0x0062  // Mantissa
#define FACSGN 0x0066 // Sign
#define SGNFLG 0x0067
#define BITS 0x0068
#define AFAC 0x0069   // FP
#define ARGEXP 0x0069 // Exponent
#define ARGHO 0x006A  // Mantissa
#define ARGSGN 0x006E // Sign
#define ARISGN 0x006F
#define FACOV 0x0070
#define FBUFPT 0x0071 // WORD
#define CHRGET 0x0073 // FN
#define CHRGOT 0x0079 // FN
#define TXTPTR 0x007A // FN
#define RNDX 0x008B   // ARRAY[5]
#define STATUS 0x0090
#define STKEY 0x0091
#define SVXT 0x0092
#define VERCKK 0x0093
#define C3PO 0x0094
#define BSOUR 0x0095
#define SYNO 0x0096
#define XSAV 0x0097
#define LDTND 0x0098
#define DFLTN 0x0099
#define DFLTO 0x009A
#define PRTY 0x009B
#define DPSW 0x009C
#define MSGFLG 0x009D
#define FNMIDX 0x009E
#define PTR1 0x009E
#define PTR2 0x009F
#define TIME 0x00A0 // ARRAY[3]
#define TSFCNT 0x00A3
#define TBTCNT 0x00A4
#define CNTDN 0x00A5
#define BUFPNT 0x00A6
#define INBIT 0x00A7
#define BITC1 0x00A8
#define RINONE 0x00A9
#define RIDATA 0x00AA
#define RIPRTY 0x00AB
#define SAL 0x00AC   // WORD
#define EAL 0x00AE   // WORD
#define CMPO 0x00B0  // WORD
#define TAPE1 0x00B2 // WORD
#define BITTS 0x00B4
#define NXTBIT 0x00B5
#define RODATA 0x00B6
#define FNLEN 0x00B7
#define LA 0x00B8
#define SA 0x00B9
#define FA 0x00BA
#define FNADR 0x00BB // WORD
#define ROPRTY 0x00BD
#define FSBLK 0x00BE
#define MYCH 0x00BF
#define CAS1 0x00C0
#define STAL 0x00C1   // WORD
#define MEMUSS 0x00C3 // WORD
#define LSTX 0x00C5
#define NDX 0x00C6
#define RVS 0x00C7
#define INDX 0x00C8
#define LXSP 0x00C9 // WORD
#define SFDX 0x00CB
#define BLNSW 0x00CC
#define BLNCT 0x00CD
#define GDBLN 0x00CE
#define BLNON 0x00CF
#define CRSW 0x00D0
#define PNT 0x00D1 // WORD
#define PNTR 0x00D3
#define QTSW 0x00D4
#define LNMX 0x00D5
#define TBLX 0x00D6
#define SCHAR 0x00D7
#define INSRT 0x00D8
#define LDTB1 0x00D9  // ARRAY[25]
#define USER 0x00F3   // WORD
#define KEYTAB 0x00F5 // WORD
#define RIBUF 0x00F7  // WORD
#define ROBUF 0x00F9  // WORD
#define FREKZP 0x00FB // ARRAY[4]
#define BASZPT 0x00FF
#define ASCWRK 0x00FF // FIXME

/// Page 1

// _ => 0x100 // ARRAY[10]
#define BAD 0x0100    // ARRAY[62]
#define STACK 0x0100  // ARRAY[256]
#define BSTACK 0x013F // ARRAY[192]
//
#define PC 0x01FE
#define PCL 0x01FE
#define PCH 0x01FF

/// Page 2

#define BUF 0x0200    // ARRAY[88]
#define LAT 0x0259    // ARRAY[9]
#define FAT 0x0263    // ARRAY[9]
#define SAT 0x026D    // ARRAY[9]
#define KEYD 0x0277   // ARRAY[9]
#define MEMSTR 0x0281 // WORD
#define MEMEND 0x0283 // WORD
#define TIMOUT 0x0285
#define COLOR 0x0286
#define GDCOL 0x0287
#define HIBASE 0x0288
#define XMAX 0x0289
#define RPTFLG 0x028A
#define KOUNT 0x028B
#define DELAY 0x028C
#define SHFLAG 0x028D
#define LSTSHF 0x028E
#define KEYLOG 0x028F // WORD
#define MODE 0x0291
#define AUTODN 0x0292
#define M51CTR 0x0293
#define M51CDR 0x0294
#define M51AJB 0x0295 // WORD
#define RSSTAT 0x0297
#define BITNUM 0x0298
#define BAUDOF 0x0299 // WORD
#define RIDBE 0x029B
#define RIDBS 0x029C
#define RODBS 0x029D
#define RODBE 0x029E
#define IRQTMP 0x029F // WORD
#define ENABL 0x02A1
#define TODSNS 0x02A2
#define TRDTMP 0x02A3
#define TD1IRQ 0x02A4
#define TLNIDX 0x02A5
#define TVSFLG 0x02A6
// #define UNUSED 0x02A7 // ARRAY[88]
// #define SPR11 0x02C0 // FIXME

/// Page 3

#define IERROR 0x0300 // WORD
#define IMAIN 0x0302  // WORD
#define ICRNCH 0x0304 // WORD
#define IQPLOP 0x0306 // WORD
#define IGONE 0x0308  // WORD
#define IEVAL 0x030A  // WORD
#define SAREG 0x030C
#define SXREG 0x030D
#define SYREG 0x030E
#define SPREG 0x030F
#define USRPOK 0x0310
#define USRADD 0x0311 // WORD
// #define UNUSED 0x0313
#define CINV 0x0314   // WORD
#define CNBINV 0x0316 // WORD
#define NMINV 0x0318  // WORD
#define IOPEN 0x031A  // WORD
#define ICLOSE 0x031C // WORD
#define ICHKIN 0x031E // WORD
#define ICKOUT 0x0320 // WORD
#define ICLRCH 0x0322 // WORD
#define IBASIN 0x0324 // WORD
#define IBSOUT 0x0326 // WORD
#define ISTOP 0x0328  // WORD
#define IGETIN 0x032A // WORD
#define ICLALL 0x032C // WORD
#define USRCMD 0x032E // WORD
#define ILOAD 0x0330  // WORD
#define ISAVE 0x0332  // WORD
// #define UNUSED 0x0334 // ARRAY[7]
#define TBUFFR 0x033C // ARRAY[192]
                      // #define SPR13 0x0340 // FIXME
                      // #define SPR14 0x0380 // FIXME
                      // #define SPR15 0x03C0 // FIXME
                      // #define UNUSED 0x03FC // ARRAY[4]

/// Page 4

#define VICSCN 0x0400 // ARRAY[1024]
// #define UNUSED 0x07E8
// #define SPNTRS 0x07F8 // FIXME

///
#define COLD 0xA000 // WORD
#define WARM 0xA002 // WORD

///
#define READY 0xA474 // ARRAY[12]

///
/*
CHECKS FOR REAL IRQ'S OR BREAKS

This routine is pointed to by the hardware IRQ vector at
$fffe. This routine is able to distinguish between a
hardware IRQ, and a software BRK. The two types of
interrupts are processed by its own routine.
*/
#define PULS 0xFF48

#define CHROUT 0xFFD2 // JMP
#define GETIN 0xFFE4  // JMP

///
#define NMI 0xFFFA   // WORD
#define RESET 0xFFFC // WORD
#define IRQ 0xFFFE   // WORD

///
#define CARTROM 0x8000
#define BASICROM 0xA000
#define KERNALROM 0xE000

#endif // C64_H