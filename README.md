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
│   ├── opcode_table.c    # Complete opcode table
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

- `ADDR_IMP` - Implied
- `ADDR_IMM` - Immediate
- `ADDR_ZP` - Zero Page
- `ADDR_ZPX` - Zero Page, X
- `ADDR_ZPY` - Zero Page, Y
- `ADDR_ABS` - Absolute
- `ADDR_ABSX` - Absolute, X
- `ADDR_ABSY` - Absolute, Y
- `ADDR_IND` - Indirect
- `ADDR_INDX` - Indexed Indirect
- `ADDR_INDY` - Indirect Indexed
- `ADDR_REL` - Relative

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
