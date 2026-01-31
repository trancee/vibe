# C64 Emulator Comprehensive Test Suite

This document describes the comprehensive test suite implemented for the C64 emulator project.

## Overview

The test suite provides thorough coverage of all major C64 hardware components with a focus on **cycle-exact accuracy** and **hardware state verification**. The suite is designed to validate compliance with known reference tests like the Lorenz test suite.

## Test Framework

### Core Components

- **`test_suite.h`**: Enhanced test framework with assertion macros and helper functions
- **`test_suite.c`**: Implementation of test runner, timing helpers, and CPU instruction table
- **`test_runner.c`**: Main test executable with command-line interface

### Key Features

- **Comprehensive Assertions**: `TEST_ASSERT`, `TEST_ASSERT_EQ`, `TEST_ASSERT_EQ_HEX`, `TEST_ASSERT_RANGE`
- **Timing Verification**: Cycle-accurate timing tests with `TimingInfo` helpers
- **Component Isolation**: Tests can run individual components or full integration scenarios
- **Detailed Reporting**: Pass/fail statistics with optional verbose output

## Test Categories

### 1. CPU Tests (`test_cpu.c`)

#### Core Instruction Tests
- **ADC Immediate**: Tests addition with carry and flag updates
- **Branch Timing**: Validates branch penalty cycles (+1 taken, +2 page cross)
- **Page Cross Timing**: Tests additional cycle penalties for indexed addressing
- **Stack Operations**: Validates PHA/PLA, JSR/RTS, and stack pointer behavior
- **Flag Operations**: Tests CLC/SEC, CLD/SED, CLI/SEI, CLV instructions
- **Compare Instructions**: Validates CMP, CPX, CPY with proper flag setting
- **Transfer Instructions**: Tests TAX, TAY, TSX, TXA, TXS, TYA

#### Edge Cases
- **Illegal Opcodes**: Framework ready for comprehensive illegal opcode testing
- **Decimal Mode**: Framework ready for BCD arithmetic testing
- **Interrupt Handling**: Placeholder for IRQ/NMI timing tests

#### CPU Instruction Table
Complete 256-entry instruction table with:
- Opcode mnemonics
- Cycle timing ranges
- Flag effects
- Addressing mode penalties

### 2. CIA Tests (`test_cia_comprehensive.c`)

#### Timer Functionality
- **Basic Timer Operation**: Timer countdown and latch reload
- **Timer Underflow**: Interrupt flag generation and timer reload
- **Pipeline Delay**: Validates 2-4 cycle startup delay
- **Timer B**: Tests second timer channel

#### Interrupt System
- **Lorenz IRQ Test**: Implements the critical reference test with timer values 8,7,6
- **ICR Timing**: Tests interrupt control register bit 7 delay
- **Multiple Interrupt Sources**: Validates interrupt flag accumulation

#### Additional Features
- **Time-of-Day Clock**: BCD clock and alarm functionality
- **Keyboard Matrix**: Row/column scanning simulation
- **Serial Port**: CIA2 serial port operations

### 3. VIC-II Tests (`test_vic.c`)

#### Display Timing
- **Bad Line Detection**: Validates cycle 12-54 BA assertion logic
- **Sprite DMA**: Tests sprite pre-allocation and DMA cycles
- **Raster Interrupts**: Line comparison and register updates

#### Memory Fetching
- **Address Calculation**: Tests screen/character memory base calculation
- **Bank Selection**: Validates 16K bank switching via CIA2
- **Display Modes**: Tests text, bitmap, and extended text modes

#### Graphics Features
- **Color Registers**: Border, background, sprite, and multicolor handling
- **Sprite Processing**: Position, priority, expansion, multicolor
- **Register Access**: Validates all VIC register behaviors

### 4. Memory Tests (`test_memory.c`)

#### Core Functionality
- **Basic Read/Write**: Validates RAM access patterns
- **Stack Area**: Tests $0100-$01FF stack operations
- **Zero Page**: Tests zero page and indirect addressing modes

#### Memory Management
- **PLA Mapping**: Tests HIRAM/LORAM/CHAREN bit combinations
- **I/O Mapping**: Validates VIC, SID, CIA, color RAM mapping
- **Color RAM**: Tests 4-bit nibble handling with $F0 masking

#### ROM System
- **ROM Loading**: File validation and checksum verification
- **Reset Vector**: Tests KERNAL reset vector validation
- **ROM Integrity**: Validates BASIC/KERNAL/CHAR ROM structure

### 5. SID Tests (`test_sid.c`)

#### Voice Generation
- **Voice Registers**: Tests frequency, pulse width, control for all 3 voices
- **Control Bits**: Validates GATE, SYNC, RING MOD, TEST, waveform select
- **Envelope Generator**: Tests attack/decay/sustain/release parameters

#### Audio Processing
- **Filter System**: Tests low-pass, band-pass, high-pass, notch modes
- **Filter Parameters**: Validates cutoff frequency and resonance control
- **Volume Control**: Tests master volume and voice mixing

#### Special Features
- **Voice 3 Output**: Tests digi-sound capabilities
- **Potentiometer Inputs**: Tests paddle input reading
- **Audio Generation**: Framework ready for comprehensive audio testing

### 6. Integration Tests (`test_integration.c`)

#### System Boot
- **KERNAL Boot**: Tests reset vector fetching and system initialization
- **BASIC Interpreter**: Tests BASIC program storage and variable handling
- **Component Interaction**: Validates cross-component dependencies

#### Reference Compliance
- **Lorenz Suite**: Comprehensive reference test compliance
- **CPU Timing**: Validates branch and page cross penalties
- **Memory Timing**: Tests access timing differences
- **Peripheral Interaction**: Tests CIA/VIC/SID coordination

## Usage

### Build Commands

```bash
# Build comprehensive test suite
make test-suite

# Run all tests
./test_suite

# Run specific categories
./test_suite --cpu
./test_suite --cia
./test_suite --vic
./test_suite --memory
./test_suite --sid
./test_suite --integration

# Verbose output
./test_suite --verbose

# Individual component tests
make test-cpu
make test-cia
make test-vic
make test-memory
make test-sid
make test-all
```

### Test Output

```
🚀 C64 Emulator Comprehensive Test Suite
==========================================

🧪 Running: cpu_adc_immediate (ADC immediate instruction)
  ✅ PASS: ADC immediate instruction test passed

🧪 Running: cpu_branch_timing (Branch instruction timing)
  ❌ FAIL: src/test_cpu.c:59 - Taken branch should take 3 cycles (expected: 3, actual: 2)
❌ cpu_branch_timing FAILED

==================================================
TEST SUMMARY
==================================================
Total:  47
Passed: 35
Failed: 12
Skipped: 0
❌ 12 TESTS FAILED
==================================================
```

## Documentation Alignment

### Edge Cases Covered

#### CPU Edge Cases
1. **Branch Penalties**: +1 cycle for taken branches, +1 for page cross
2. **Page Cross Timing**: Indexed addressing across page boundaries
3. **Interrupt Timing**: 7-cycle interrupt sequence
4. **Decimal Mode**: BCD arithmetic edge cases
5. **Stack Overflow/Underflow**: Boundary conditions

#### CIA Edge Cases
1. **Pipeline Delays**: 2-4 cycle timer startup delay
2. **ICR Bit 7 Timing**: 1-cycle delay after interrupt flag set
3. **Interrupt Persistence**: Flags accumulate until ICR read
4. **Timer Precision**: Underflow timing and latch behavior
5. **Time-of-Day**: BCD clock edge cases

#### VIC Edge Cases
1. **Bad Lines**: Cycle 12-54 BA assertion conditions
2. **Sprite DMA**: Pre-allocation and priority handling
3. **Raster Timing**: Line comparison and register update timing
4. **Memory Fetching**: Screen/character/color RAM access patterns
5. **Display State**: VBLANK and display enable timing

#### Memory Edge Cases
1. **PLA State Transitions**: HIRAM/LORAM/CHAREN bit combinations
2. **Color RAM Nibbles**: 4-bit handling with $F0 masking
3. **ROM Validation**: File integrity and checksum verification
4. **Stack Boundaries**: $0100-$01FF limits
5. **Zero Page Wrapping**: Indirect addressing page boundary handling

#### SID Edge Cases
1. **Voice Synchronization**: Ring modulation and sync effects
2. **Filter Resonance**: SVF filter implementation
3. **Envelope Curves**: Attack/decay/sustain/release timing
4. **Voice 3 Digi**: Digital sound sampling
5. **Potentiometer**: Analog input reading

### Reference Test Compliance

#### Lorenz Test Suite
- **CPU Port Floating**: External pull-ups on bits 0-5 behavior
- **CPU Timing**: Branch and page cross cycle penalties
- **IRQ Test**: CIA timer/ICR cycle-accurate timing (8,7,6 timer values)
- **Memory Timing**: Different access patterns for RAM/ROM/IO

#### Implementation Requirements
- **Cycle-Exact Synchronization**: All components advance on `c64_tick()`
- **Memory Access Timing**: Automatic `c64_tick()` calls on read/write
- **Interrupt Handling**: Level-sensitive IRQ/NMI with proper masking

## Future Enhancements

### CPU
- Complete 6502 opcode coverage (all 256 opcodes)
- Illegal opcode implementation and testing
- Decimal mode BCD arithmetic
- Interrupt sequence timing validation

### CIA
- Complete Time-of-Day clock testing
- Serial port communication testing
- Interrupt prioritization
- Timer B comprehensive testing

### VIC
- Visual output verification
- Sprite collision detection
- Advanced display modes
- Light pen support

### SID
- Audio output verification
- Advanced filter modeling
- Voice interaction testing
- Complete envelope shaping

### Integration
- Full ROM compatibility testing
- Software compatibility validation
- Performance benchmarking
- Regression testing framework

## Notes

- Tests focus on **hardware accuracy** over software functionality
- Some tests may fail until corresponding hardware implementation is complete
- Framework is designed for easy extension and modification
- Timing tests require cycle-exact implementation for full validation
- Reference tests (Lorenz) are critical for compatibility validation