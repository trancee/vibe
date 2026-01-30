# C64 Emulator Implementation Plan

Comprehensive plan for building a cycle-exact Commodore 64 emulator.

## Core Architecture: Cycle Accuracy

The emulator uses a centralized "heartbeat" mechanism to ensure all components remain synchronized within a single master clock cycle.

### [c64_tick](file:///home/phil/Projects/Gemini/src/c64.c)
- Central function that advances **VIC-II**, **CIA 1/2**, and **SID** by one cycle.
- Increments the global `sys->cycle_count`.
- Implicitly called by every CPU instruction through memory access.

### [Memory Access Ticking](file:///home/phil/Projects/Gemini/src/memory.c)
- **Automatic Sync**: `mem_read` and `mem_write` call `c64_tick` before processing data.
- This ensures that if an instruction takes 4 cycles (and thus makes multiple memory accesses), the peripheral chips see all 4 timing steps.

### [IRQ Loop Diagnosis]
- Resolved VIC-II clobbering raster target ($D011/$D012) in `vic_step`, causing interrupts every line.
- Implementing separate `raster_irq_line` latch in `C64Vic`.
- Refining CIA ICR bit 7 persistence: it should stay set until READ, reflecting enabled interrupts.
- Improving diagnostic logging in `c64_tick` to use reliable thresholds.

---

## Component Details

### [CPU (6510)](file:///home/phil/Projects/Gemini/src/cpu.c)
### [CPU Core]
- **MISSING OPCODES**: Implement ORA, AND, EOR, BIT, Stack (PHA/PLA/PHP/PLP), Transfer (TAX/TAY/TXA/TYA/TSX/TXS), and Inc/Dec (INC/DEC/INX/DEX/INY/DEY).
- **Correct PC handling**: Ensure all opcodes consume the correct number of bytes and advance PC.
- **Cycle counts**: Ensure each opcode calls `c64_tick` the correct number of times (currently `cpu_step` is simplified, but we must ensure we don't skip logic).
- **BA Stall**: CPU stalls on memory cycles (handled in `mem_read`/`mem_write`) when VIC-II asserts `ba_low`.
- **Decimal Mode**: Full BCD support for `ADC` and `SBC`.
- **Interrupts**: Cycle-exact 7-cycle interrupt sequence. Handled as Level-Sensitive signals.
- **Reset State**: Correctly initializes `I` flag and reserved bits.

### [VIC-II (6569/6567)](file:///home/phil/Projects/Gemini/src/vic.c)
- **Bad Lines**: Accurately latches `bad_line_active` at cycle 12 and asserts `BA` low until cycle 54.
- **Sprite DMA**: Pre-allocates bus slots and asserts `BA` OR-ed with Bad Line demand.
- **Raster Registers**: Correctly updates `$D011` (bit 8) and `$D012` for KERNAL synchronization loops.
- **ANSI Rendering**: Implement primitive text-mode rendering.
    - Fetch 1000 characters from Screen RAM (default `$0400`).
    - Fetch 1000 color nibbles from Color RAM (`$D800`).
    - Use ANSI escape codes (`\e[38;5;...m`) to render the 40x25 grid in the console at the end of each frame (Line 311).

### [SID (6581/8580)](file:///home/phil/Projects/Gemini/src/sid.c)
- **Open Bus**: Reads to write-only registers return the high byte of the address.
- **SVF Filter**: State Variable Filter model implementing Low-pass, Band-pass, and High-pass logic.
- **Dynamic coefficients**: Recalculates `cutoff` and `resonance` on register writes.

### [CIA (6526)](file:///home/phil/Projects/Gemini/src/cia.c)
- **Timers**: Two 16-bit countdown timers with reload and one-shot modes.
- **Keyboard Matrix**: Rows/Columns mapping for input scanning.
- **TOD Clock**: BCD-based Time-of-Day clock with latching (reading Hours freezes values).
- **Interrupt Masking**: Fixed internal interrupt enable logic (ICR).

### [Memory Subsystem](file:///home/phil/Projects/Gemini/src/memory.c)
- **PLA (82S100)**: Full mapping of HIRAM/LORAM/CHAREN bits.
- **Color RAM**: 1KB buffer at `$D800` (returns 4-bit nibbles OR-ed with `$F0`).
- **ROM Support**: Loads `kernal.rom`, `basic.rom`, and `char.rom`.

---

## Verification Plan

### Boot Sequence
- Observe diagnostic logs for `PC` progression.
- Verify that the CPU correctly exits the `$EE5A` raster wait loop (checks `D011`).
- Verify that ROM vectors fetch correctly from `$FFFC`.

### Real-Time Validation
- Throttled output showing Raster Line, Cycles, and BA status once per million master cycles.
