### Key Features Implemented

- **CPU (6510)**: All 151 legal opcodes, all 105 illegal/undocumented opcodes, addressing modes, decimal mode ADC/SBC, interrupt handling, CPU port ($00/$01) with proper floating state
- **Memory**: PLA bank switching logic, ROM visibility control, Color RAM, I/O mapping
- **VIC-II**: Raster timing, bad line detection, BA stall, raster IRQ, ANSI console rendering
- **CIA**: Timer A/B with pipeline delays, ICR bit 7 timing, TOD clock, keyboard matrix scanning
- **SID**: Register access, envelope generator, oscillators, filter (simplified), open bus behavior
- **System**: Central tick mechanism ensuring all components stay synchronized

### Illegal/Undocumented Opcodes

All 105 illegal 6502 opcodes are implemented per the `65xx_ill.txt` documentation:

#### Combined Read-Modify-Write + ALU Operations (28 opcodes)
| Name | Description | Opcodes |
|------|-------------|---------|
| **SLO** | ASL memory, then ORA with A | $03, $07, $0F, $13, $17, $1B, $1F |
| **RLA** | ROL memory, then AND with A | $23, $27, $2F, $33, $37, $3B, $3F |
| **SRE** | LSR memory, then EOR with A | $43, $47, $4F, $53, $57, $5B, $5F |
| **RRA** | ROR memory, then ADC with A | $63, $67, $6F, $73, $77, $7B, $7F |

#### Combined Load/Store Operations (10 opcodes)
| Name | Description | Opcodes |
|------|-------------|---------|
| **SAX** | Store A & X to memory | $83, $87, $8F, $97 |
| **LAX** | Load A and X from memory | $A3, $A7, $AF, $B3, $B7, $BF |

#### Combined Memory + Arithmetic (14 opcodes)
| Name | Description | Opcodes |
|------|-------------|---------|
| **DCP** | DEC memory, then CMP with A | $C3, $C7, $CF, $D3, $D7, $DB, $DF |
| **ISC** | INC memory, then SBC from A | $E3, $E7, $EF, $F3, $F7, $FB, $FF |

#### Immediate Mode Operations (8 opcodes)
| Name | Description | Opcode |
|------|-------------|--------|
| **ANC** | AND #imm, copy N flag to C | $0B, $2B |
| **ALR** | AND #imm, then LSR A | $4B |
| **ARR** | AND #imm, then ROR A (special V/C flags) | $6B |
| **XAA** | (A\|$EE) & X & #imm → A (unstable) | $8B |
| **LAX#** | (A\|$EE) & #imm → A, X (unstable) | $AB |
| **SBX** | (A & X) - #imm → X | $CB |
| **SBC#** | Same as $E9 SBC #imm | $EB |

#### NOP Variants (27 opcodes)
| Addressing Mode | Opcodes |
|-----------------|---------|
| Implied (1 byte, 2 cycles) | $1A, $3A, $5A, $7A, $DA, $FA |
| Immediate (2 bytes, 2 cycles) | $80, $82, $89, $C2, $E2 |
| Zeropage (2 bytes, 3 cycles) | $04, $44, $64 |
| Zeropage,X (2 bytes, 4 cycles) | $14, $34, $54, $74, $D4, $F4 |
| Absolute (3 bytes, 4 cycles) | $0C |
| Absolute,X (3 bytes, 4+ cycles) | $1C, $3C, $5C, $7C, $DC, $FC |

#### JAM/HLT - Halt CPU (12 opcodes)
Halts the processor until reset: $02, $12, $22, $32, $42, $52, $62, $72, $92, $B2, $D2, $F2

#### Unstable Store Operations (6 opcodes)
| Name | Description | Opcodes |
|------|-------------|---------|
| **SHA** | Store A & X & (addr_hi+1) | $93, $9F |
| **TAS** | SP = A & X; store SP & (addr_hi+1) | $9B |
| **SHY** | Store Y & (addr_hi+1) | $9C |
| **SHX** | Store X & (addr_hi+1) | $9E |
| **LAS** | mem & SP → A, X, SP | $BB |

> **Note**: The "unstable" opcodes (XAA, LAX#, SHA, SHX, SHY, TAS) have behavior that can vary between physical chips. This implementation uses the most common documented behavior with the magic constant $EE for XAA/LAX#.

#### Build & Run

```sh
make           # Build the emulator
make run       # Run the emulator
make debug     # Run with debug output
make test      # Run for 100 frames (testing)
```

### Test Files Created

1. test_framework.h  
Test framework with macros for assertions  

1. test_cpu.c - **139 CPU tests** covering:  
Load/Store operations (LDA, LDX, LDY, STA, STX, STY with all addressing modes)  
Register transfers (TAX, TAY, TXA, TYA, TSX, TXS)  
Stack operations (PHA, PLA, PHP, PLP)  
ADC/SBC in binary and decimal modes  
Logical operations (AND, ORA, EOR, BIT)  
Compare operations (CMP, CPX, CPY)  
Increment/Decrement (INC, DEC, INX, INY, DEX, DEY)  
Shift/Rotate (ASL, LSR, ROL, ROR)  
Jumps/Branches (JMP, JSR, RTS, all branch instructions)  
BRK/RTI interrupt handling  
CPU port ($00/$01) with floating bits behavior  
IRQ/NMI interrupt tests  
**All illegal opcodes** (SLO, RLA, SRE, RRA, SAX, LAX, DCP, ISC, ANC, ALR, ARR, SBX, XAA, LAS, SHA, SHX, SHY, TAS, NOP variants, JAM)  

1. test_memory.c - **26 memory tests** covering:  
Basic RAM read/write  
PLA bank switching (BASIC, KERNAL, I/O, Char ROM visibility)  
Color RAM 4-bit behavior  
VIC bank selection  
I/O area mirroring (VIC, SID, CIA1, CIA2)  
Edge cases (zero page wrap, stack wrap, write to ROM)  

1. test_vic.c - **30 VIC-II tests** covering:  
Raster counter timing  
Raster IRQ triggering and acknowledgment  
Bad line detection and BA signal timing  
Register read/write behavior  
Sprite position registers  
Frame counter  

1. test_cia.c - **28 CIA tests** covering:  
Timer A/B countdown and underflow  
Timer pipeline delay (2 cycles per documentation)  
ICR bit 7 delay (1 cycle per documentation)  
Timer B counting Timer A underflows  
One-shot vs continuous mode  
TOD clock with latch behavior  
Port data direction  
Keyboard matrix scanning  

1. test_sid.c - **28 SID tests** covering:  
Voice frequency/pulse width/control registers  
ADSR envelope parameters  
Filter cutoff, resonance, and modes  
Read-only registers (POTX, POTY, OSC3, ENV3)  
Open bus behavior for write-only registers  
Waveform selection (triangle, saw, pulse, noise)  
Ring modulation and sync  

#### Running Tests

```sh
make test          # Build and run all tests
make test-verbose  # Run with verbose output
make quicktest     # Run emulator for 100 frames (quick sanity check)
```