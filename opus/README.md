### Key Features Implemented

- **CPU (6510)**: All legal opcodes, addressing modes, decimal mode ADC/SBC, interrupt handling, CPU port ($00/$01) with proper floating state
- **Memory**: PLA bank switching logic, ROM visibility control, Color RAM, I/O mapping
- **VIC-II**: Raster timing, bad line detection, BA stall, raster IRQ, ANSI console rendering
- **CIA**: Timer A/B with pipeline delays, ICR bit 7 timing, TOD clock, keyboard matrix scanning
- **SID**: Register access, envelope generator, oscillators, filter (simplified), open bus behavior
- **System**: Central tick mechanism ensuring all components stay synchronized

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

1. test_cpu.c - **86 CPU tests** covering:  
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