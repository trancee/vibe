# MOS 6510 CPU Emulator

A complete, feature-rich implementation of the MOS 6510 microprocessor (as used in the Commodore 64) in C with comprehensive test coverage for all 256 opcodes.

## Features

- **Complete Opcode Set**: Implements all 256 MOS 6510 opcodes, including legal and illegal instructions
- **All Addressing Modes**: Supports all 13 addressing modes (Implied, Immediate, Zero Page, etc.)
- **Accurate Timing**: Proper cycle counting for each opcode
- **Full 64KB Memory**: Complete memory map with read/write access
- **Stack Operations**: Full implementation of stack push/pull operations
- **Flag Management**: Complete status register with all flags
- **Comprehensive Tests**: Extensive test suite with 354+ test cases
- **Clean API**: Simple, well-documented interface
- **Static Library**: Build as both static library and executable examples

## Project Structure

```
mos6510/
├── include/
│   └── mos6510.h          # Main header file with API
├── src/
│   ├── mos6510.c         # Core CPU implementation
│   ├── opcodes.c         # Legal opcode implementations
│   ├── illegal_opcodes.c # Illegal opcode implementations
│   ├── instructions.c    # Complete opcode table
│   └── example.c         # Example usage
├── tests/
│   └── test_opcodes.c    # Comprehensive test suite
├── build/                # Build output directory
├── Makefile             # Build configuration
└── README.md            # This file
```

## Building

### Prerequisites

- GCC (or compatible C compiler)
- Make
- Standard C library

### Build Commands

```bash
# Build everything (library, tests, examples)
make all

# Build and run tests
make test

# Build and run example
make example

# Clean build artifacts
make clean

# Debug build
make debug

# Release build
make release

# Install to system (optional)
make install
```

## Usage

### Basic Usage

```c
#include "mos6510.h"

int main() {
    // Initialize CPU
    CPU cpu;
    mos6510_init(&cpu);
    
    // Load program into memory
    cpu.memory[0x1000] = 0xA9;  // LDA #$42
    cpu.memory[0x1001] = 0x42;
    
    // Set program counter
    mos6510_set_pc(&cpu, 0x1000);
    
    // Execute instruction
    uint8_t cycles = mos6510_step(&cpu);
    
    // Check result
    printf("A = $%02X\n", cpu.A);  // Should print 42
    
    return 0;
}
```

### CPU Structure

The CPU structure contains all CPU state:

```c
typedef struct {
    uint8_t A;      // Accumulator
    uint8_t X;      // X Index Register
    uint8_t Y;      // Y Index Register
    uint8_t SP;     // Stack Pointer
    uint8_t PC;     // Program Counter (low byte)
    uint8_t PCH;    // Program Counter (high byte)
    uint8_t P;      // Status Register
    uint8_t memory[65536];  // 64KB memory
} CPU;
```

### Status Flags

```c
#define FLAG_CARRY    0x01
#define FLAG_ZERO     0x02
#define FLAG_INTERRUPT_DISABLE 0x04
#define FLAG_DECIMAL  0x08
#define FLAG_BREAK    0x10
#define FLAG_RESERVED 0x20
#define FLAG_OVERFLOW 0x40
#define FLAG_NEGATIVE 0x80
```

### Addressing Modes

All 13 addressing modes are supported:

- `Implied` - Implied
- `Immediate` - Immediate
- `ZeroPage` - Zero Page
- `ZeroPageX` - Zero Page, X
- `ZeroPageY` - Zero Page, Y
- `Absolute` - Absolute
- `AbsoluteX` - Absolute, X
- `AbsoluteY` - Absolute, Y
- `Indirect` - Indirect
- `IndexedIndirect` - Indexed Indirect
- `IndirectIndexed` - Indirect Indexed
- `Relative` - Relative

## Opcode Coverage

### Legal Opcodes (151)
All standard MOS 6510 opcodes:
- **Load/Store**: LDA, LDX, LDY, STA, STX, STY
- **Transfer**: TAX, TAY, TXA, TYA, TSX, TXS
- **Stack**: PHA, PHP, PLA, PLP
- **Logical**: AND, ORA, EOR, BIT
- **Arithmetic**: ADC, SBC, CMP, CPX, CPY
- **Increment/Decrement**: INC, INX, INY, DEC, DEX, DEY
- **Shifts**: ASL, LSR, ROL, ROR
- **Jumps**: JMP, JSR, RTS
- **Branches**: BCC, BCS, BEQ, BNE, BPL, BMI, BVC, BVS
- **Status**: CLC, CLD, CLI, CLV, SEC, SED, SEI
- **System**: BRK, NOP, RTI

### Illegal Opcodes (105)
All undocumented/illegal opcodes for completeness:
- **Combined operations**: ANC, ARR, DCP, ISC, LAS, LAX, RLA, RRA
- **Store operations**: SAX, SLO, SRE, TAS, XAA
- **Various NOP variants**

## API Reference

### Core Functions

```c
// Initialize CPU state
void mos6510_init(CPU *cpu);

// Reset CPU to initial state
void mos6510_reset(CPU *cpu);

// Execute one instruction
uint8_t mos6510_step(CPU *cpu);

// Program counter management
uint16_t mos6510_get_pc(CPU *cpu);
void mos6510_set_pc(CPU *cpu, uint16_t addr);

// Memory access
uint8_t mos6510_read_byte(CPU *cpu, uint16_t addr);
void mos6510_write_byte(CPU *cpu, uint16_t addr, uint8_t data);

// Stack operations
void mos6510_push(CPU *cpu, uint8_t data);
uint8_t mos6510_pull(CPU *cpu);
```

### Flag Operations

```c
// Get flag states
bool get_flag_carry(CPU *cpu);
bool get_flag_zero(CPU *cpu);
// ... etc for all flags

// Set flag states
void set_flag_carry(CPU *cpu, bool set);
void set_flag_zero(CPU *cpu, bool set);
// ... etc for all flags
```

## Testing

The project includes a comprehensive test suite covering:

- **Flag Operations**: All flag set/clear operations
- **Addressing Modes**: All 13 addressing modes
- **Stack Operations**: Push/pull operations with boundary testing
- **Individual Opcodes**: Detailed tests for major opcodes
- **Opcode Coverage**: All 256 opcodes execute without crashing

### Running Tests

```bash
make test
```

Test results show detailed pass/fail information for each test case.

## Example Programs

### Basic Program Execution

```c
// Load: LDA #$42, STA $2000, BRK
cpu.memory[0x1000] = 0xA9;  // LDA #$42
cpu.memory[0x1001] = 0x42;
cpu.memory[0x1002] = 0x8D;  // STA $2000
cpu.memory[0x1003] = 0x00;
cpu.memory[0x1004] = 0x20;
cpu.memory[0x1005] = 0x00;  // BRK

mos6510_set_pc(&cpu, 0x1000);

while (cpu.memory[mos6510_get_pc(&cpu)] != 0x00) {
    mos6510_step(&cpu);
}
```

### Arithmetic Operations

```c
// Example: Add two numbers
cpu.A = 0x50;
set_flag_carry(&cpu, false);
cpu.A = add_with_carry(&cpu, cpu.A, 0x30);
// Result: A = 0x80, flags set appropriately
```

## Implementation Details

### Memory Layout
- Full 64KB address space (0x0000 - 0xFFFF)
- Zero page at 0x0000 - 0x00FF
- Stack at 0x0100 - 0x01FF
- Vector area at 0xFFFA - 0xFFFF

### Timing
- Each opcode has accurate cycle counting
- Branch instructions add cycles for page boundary crosses
- Addressing mode timing properly implemented

### Compatibility
- Written in standard C99
- No external dependencies beyond standard library
- Cross-platform compatible
- Endianness-agnostic for cross-platform use

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Ensure all tests pass
6. Submit a pull request

## License

This project is released under the MIT License. See LICENSE file for details.

## Acknowledgments

- MOS Technology for the original 6510 specification
- Commodore International for the C64 implementation
- The 6502.org community for detailed opcode documentation
- Various open-source 6502/6510 emulators for reference

## Resources

- [MOS 6510 Datasheet](https://www.6502.org/documents/datasheets/mos/6510_mpu.pdf)
- [6502 Opcode Reference](https://www.masswerk.at/6502/6502_instruction_set.html)
- [6502.org](https://www.6502.org/)
- [Commodore 64 Memory Map](https://www.c64-wiki.com/wiki/Memory_Map)

# vsid

Build vsid from VICE source code.

## Prerequisites

xa is a high-speed, two-pass portable cross-assembler.
https://www.floodgap.com/retrotech/xa/dists/

```bash
sudo dnf install glib-devel glib2-devel SDL2-devel SDL2_image-devel curl-devel pulseaudio-libs-devel alsa-lib-devel libpng-devel
```


# Lorenz Test Suite

### cpuport

The output `0=ff 1=ff 1=00 0=00 1=ff 1=ff` shows the sequence of writes in format `port=value`:

1. `0=ff` - DDR = 0xFF (all outputs)
1. `1=ff` - Port = 0xFF
1. `1=00` - Port = 0x00
1. `0=00` - DDR = 0x00 (all inputs)
1. `1=ff` - Port = 0xFF
1. `1=ff` - Port = 0xFF

After this sequence: DDR=0x00, Port read should be 0x17 but we're getting 0x00.

Wait, the expected value is 0x17, not 0xDF! Let me trace through:

1. DDR=0xFF, Port=0xFF → floating = 0xFF (all bits driven high)
1. Port=0x00 → floating = 0x00 (all bits driven low)
1. DDR=0x00 → bits becoming inputs, save driven value → floating = 0x00
1. Port=0xFF (DDR=0x00, so no output bits, floating unchanged) = 0x00
1. Port=0xFF again, same

So at read time: DDR=0x00, floating=0x00

- Result = floating & ~DDR = 0x00 & 0xFF = 0x00
- Then bit 5 is cleared (already 0)
- Result = 0x00

But expected is 0x17 = 0b00010111. Hmm, where does 0x17 come from?

Let me re-read the test logic more carefully. Looking at lines 63-87:

```
         LDX #8
         LDA CONFIG
PUSH
         ASL A         ; shift CONFIG left, putting bit 7 into carry
         PHP           ; push status (with carry)
         DEX
         BNE PUSH      ; do 8 times - pushes all 8 config bits
         
         LDX #4
PULL
         PLA           ; pop status
         AND #1        ; get carry bit (0 or 1)
         TAY           ; Y = port address (0 or 1)
         LDA #0
         PLP           ; restore status with carry
         SBC #0        ; A = 0 - 0 - C = -C = 0xFF if C=1, or 0x00 if C=0
         STA 0,Y       ; store to port 0 or 1
         STA ABACKUP,Y
```

So CONFIG bits control what happens:

- 8 bits in CONFIG get pushed as carry flags
- Then 4 pairs are pulled: each pair determines (port_number, value)
- CONFIG bits 7,6 → first write
- CONFIG bits 5,4 → second write
- CONFIG bits 3,2 → third write
- CONFIG bits 1,0 → fourth write

For each pair (bit N, bit N-1):

- bit N = which port (0 or 1)
- bit N-1 = value (0x00 or 0xFF based on carry)

The test output `0=ff 1=ff 1=00 0=00 1=ff 1=ff` means CONFIG has specific bits set.

Now look at the expected value calculation in lines 68-100:

```
;INPUTS: KEEP LAST STATE
         LDA ABACKUP+0      ; DDR value
         EOR #$FF           ; invert - 1s where inputs are
         AND LASTSTATE      ; keep only input bits from LASTSTATE
         STA OR1+1          ; save for later OR
         
;OUTPUTS: SET NEW STATE  
         LDA ABACKUP+0      ; DDR value
         AND ABACKUP+1      ; DDR & PORT = bits that are output AND driven high
OR1      ORA #$11           ; OR in the saved input bits
         STA LASTSTATE      ; new expected state
```

So LASTSTATE tracks the expected bus state:

- For input bits: keep whatever was in LASTSTATE before
- For output bits: use the actual driven value (DDR & PORT)

Then at line 89-105, the RIGHT (expected read) value is calculated:

```
         LDA ABACKUP+0
         EOR #$FF           ; invert DDR
         ORA ABACKUP+1      ; ~DDR | PORT 
         AND #$37           ; mask to bits 0,1,2,4,5
         STA OR2+1
         LDA LASTSTATE
         AND #$C8           ; bits 3,6,7 from LASTSTATE
OR2      ORA #$11
```

Wait, this is different from what I thought! The expected read value is:

- Bits 0,1,2,4,5: (~DDR | PORT) & 0x37
- Bits 3,6,7: LASTSTATE & 0xC8

Then bit 5 is cleared if DDR bit 5 = 0.

This is more complex. Let me trace through the specific failing case:

From the test, the sequence with those specific writes led to:

- DDR (ABACKUP+0) = 0x00
- PORT (ABACKUP+1) = 0xFF (the last write to port 1)
- LASTSTATE = ? (depends on history)

Let me think about this differently. The key insight is that different bits have different behaviors:

- Bits 0,1,2,4,5 (the "controlled" bits): read value depends on (~DDR | PORT)
- Bits 3,6,7: read value depends on LASTSTATE (floating bus)

This matches C64 hardware where:

- Bits 0-2 (LORAM, HIRAM, CHAREN): have external pull-ups
- Bit 4 (cassette sense): has external circuitry
- Bit 5 (motor): pulled low when input
- Bits 3,6,7: truly floating

Let me re-examine the test expected value calculation:

For `RIGHT`:

```
   LDA ABACKUP+0      ; DDR=0x00
   EOR #$FF           ; 0xFF (all inputs)
   ORA ABACKUP+1      ; 0xFF | 0xFF = 0xFF
   AND #$37           ; 0xFF & 0x37 = 0x37 (bits 0,1,2,4,5)
   STA OR2+1
   LDA LASTSTATE      ; need to figure out LASTSTATE
   AND #$C8           ; bits 3,6,7 of LASTSTATE
OR2 ORA #$37          ; OR them together
```

So RIGHT = (LASTSTATE & 0xC8) | 0x37 before the bit 5 adjustment.

If LASTSTATE = 0x00 (from when port was driven to 0x00):  
RIGHT = (0x00 & 0xC8) | 0x37 = 0x37

Then bit 5 check: if DDR bit 5 = 0, clear bit 5:  
RIGHT = 0x37 & 0xDF = 0x17 ✓

That matches the expected value! So the issue is that my implementation needs to:

1. Have different handling for different bit groups
1. Bits 0,1,2,4,5 should read as (~DDR | PORT) & 0x37 - i.e., they have external pull-ups and read high when configured as inputs
1. Bits 3,6,7 should read from the floating state
1. Bit 5 should be forced low when input

The issue was that the 6510 CPU port has complex read behavior:

1. **Output bits** (DDR=1):  
Read from the port latch
1. **Input bits with external pull-ups** (bits 0,1,2,4,5):  
Read high when inputs due to external pull-up resistors on the C64 motherboard
1. **Truly floating bits** (bits 3,6,7):  
Retain last driven value due to bus capacitance
1. **Bit 5 exception**:  
Always reads LOW when configured as input (cassette motor hardware)

The formula implemented:

```c
result = (port & ddr)                    // output bits from latch
       | ((~ddr | port) & 0x37)          // pulled-up input bits  
       | (floating & ~ddr & 0xC8);       // floating bits
if (!(ddr & 0x20)) result &= 0xDF;       // bit 5 low when input
```

### cputiming

1. Branch timing (+1 for taken, +2 for page cross) is correct
1. Page crossing penalties for indexed addressing modes are correct

#### Summary

1. Branch timing:  
Added +1 cycle when branch is taken  
Added +2 cycles when branch crosses page boundary

1. Page crossing detection:  
Added `page_crossed` flag to CPU struct  
AbsoluteX, AbsoluteY, and IndirectIndexed addressing modes now set `page_crossed` when the index causes crossing to a different page  

1. Page crossing penalty:  
Read instructions (LDA, LDX, LDY, ADC, SBC, CMP, CPX, CPY, AND, ORA, EOR, BIT, NOP, LAX, LAS) add +1 cycle when page crossing occurs

1. NOP with addressing modes:  
Illegal NOP instructions with addressing modes now properly read from memory (even though they discard the value) to trigger page crossing detection

### irq

For proper timing, the CIA needs to be clocked during each CPU cycle.
On a real C64, the timer starts counting on the cycle after the write to CRA with START=1.

Also important: the CIA timer counts down and underflows when it reaches 0 and then counts down one more time (or when it's at 0 and the next clock comes). 

The test is checking the CIA ICR ($DC0D) behavior:  
1. First it sets a timer with 5 cycles delay, reads $DC0D, expects $00 (timer hasn't fired)
1. Then sets timer with 4 cycles delay, reads $DC0D, expects $01 (timer A underflow flag set, but no interrupt bit)
1. Then sets timer with 3 cycles delay, reads $DC0D, expects $81 (timer A underflow + interrupt pending bit 7)

Looking at SETINT:
1. `LDA #0; STA $DC0E` - Stop Timer A
1. `CLC; PLA; ADC #3; STA $DC04` - Set Timer A latch low = (A + 3)
1. `LDA #0; STA $DC05` - Set Timer A latch high = 0
1. Set up IRQ vector to point to IRQ routine
1. `LDA #$35; STA 1` - Enable RAM at $E000-$FFFF (for IRQ vector access)
1. Wait for specific raster line (for consistent timing)
1. `LDA #$19; STA $DC0E` - Start Timer A with one-shot and force load, then RTS

The timer starts on the write to CRA.

But the 6526 has some timing quirks:
1. The timer is in one-shot mode with force load
1. When CRA is written with START=1 and LOAD=1, the latch value is loaded into the counter
1. The timer starts counting on the NEXT phi2 clock

The real 6526 behavior:
- Timer starts counting on the cycle AFTER the write to CRA with START=1
- The timer counts down: N, N-1, ..., 1, 0, underflow

For value N, underflow happens after N+1 clocks.

IRQ line assertion has a 1-cycle delay after the interrupt flag is set.
