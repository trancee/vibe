# Testing Summary

This document summarizes the findings from running the Lorenz C64 Test Suite and the fixes applied to achieve cycle-accurate CPU emulation.

## Lorenz Test Suite

The Lorenz test suite by Wolfgang Lorenz is a comprehensive set of tests for validating C64/6502 CPU emulation accuracy. The tests are located in `tests/lorenz/`.

### cputiming Test

The `cputiming` test validates exact cycle counts for all 256 opcodes using CIA2 timers to measure instruction timing.

## CPU Timing Issues Found and Fixed

### 1. NOP Immediate (Opcode 0x80, 0x82, 0x89, etc.)

**Problem:** The `op_NOP_imm` function only advanced the PC without actually reading the operand byte, which meant the system wasn't ticked for the operand fetch cycle.

**Fix:** Changed from `addr_immediate(cpu)` to `cpu_read(cpu, addr_immediate(cpu))` to properly tick the system.

### 2. Official DEC Instructions (Opcodes 0xC6, 0xCE, 0xD6)

**Problem:** Missing dummy write cycle in Read-Modify-Write (RMW) sequence.

**Fix:** Added the dummy write (writing the original value back before the modified value):
```c
u8 v = cpu_read(cpu, a);
cpu_write(cpu, a, v);  // dummy write (RMW)
v--;
cpu_write(cpu, a, v);
```

### 3. Official INC Instructions (Opcodes 0xE6, 0xEE, 0xF6)

**Problem:** Same as DEC - missing dummy write cycle.

**Fix:** Same pattern as DEC, adding the dummy write before modification.

### 4. Illegal RMW Instructions (DCP, ISC)

**Problem:** All illegal RMW opcodes were missing the dummy write cycle and some used incorrect addressing modes.

**Affected opcodes:**
- DCP: 0xC3, 0xC7, 0xCF, 0xD3, 0xD7
- ISC: 0xE3, 0xE7, 0xEF, 0xF3, 0xF7

**Fix:** Added dummy writes and changed indirect-Y variants to use `addr_indirect_y_rmw()` which always performs the extra cycle.

### 5. STA (ind),Y (Opcode 0x91)

**Problem:** Store instructions with indirect-Y addressing always require a dummy read cycle, even without page crossing.

**Fix:** Changed to use `addr_indirect_y_rmw(cpu)` instead of `addr_indirect_y(cpu, false)`.

### 6. Illegal Store Instructions (SHA, TAS, SHY, SHX)

**Problem:** These quirky illegal store instructions were missing the dummy read cycle.

**Affected opcodes:**
- 0x93: SHA (ind),Y
- 0x9B: TAS abs,Y
- 0x9C: SHY abs,X
- 0x9E: SHX abs,Y
- 0x9F: SHA abs,Y

**Fix:** Added explicit dummy read before the store operation:
```c
cpu_read(cpu, (base & 0xFF00) | (addr & 0x00FF));
```

## Key Timing Concepts

### Read-Modify-Write (RMW) Cycle Pattern

All RMW instructions (ASL, LSR, ROL, ROR, INC, DEC, and their illegal variants) follow this cycle pattern:

1. Opcode fetch
2. Address calculation (varies by addressing mode)
3. Read value from effective address
4. **Dummy write** - write original value back (this is the commonly missed cycle)
5. Write modified value

### Store Instructions with Indexed Addressing

Store instructions (STA, STX, STY, and illegal stores) using indexed addressing modes always take the extra cycle, even when no page boundary is crossed. This differs from load instructions which only take the extra cycle on actual page crossing.

### Addressing Mode Variants

Two versions of indexed addressing functions were created:

- `addr_absolute_x()` / `addr_absolute_y()` - For reads, only adds cycle on page crossing
- `addr_absolute_x_rmw()` / `addr_absolute_y_rmw()` - For RMW/stores, always adds the cycle
- `addr_indirect_y()` - For reads, page crossing check is optional
- `addr_indirect_y_rmw()` - For RMW/stores, always does dummy read

## Test Status

After all fixes, the `cputiming` test passes successfully, indicating cycle-accurate timing for all 256 opcodes.

## IRQ Timing Issues Found and Fixed

The Lorenz `irq` test validates precise interrupt timing for all 256 opcodes. Passing this test required several fixes:

### CIA ICR Acknowledge Mechanism

When the CPU reads the CIA Interrupt Control Register ($DC0D/$DD0D), it acknowledges the interrupt and clears the ICR. However, the 6526 CIA has a 1-cycle delay where the IRQ line isn't immediately deasserted. We added an `icr_ack` flag that inhibits `irq_delay` processing for one cycle after an ICR read, preventing a spurious interrupt edge.

### Age-Based IRQ Pending Threshold

The 6502 samples the IRQ line at phi2 of the penultimate cycle of each instruction. To model this accurately without sub-cycle emulation, we track `irq_pending_age` - the number of cycles since the IRQ became pending.

The decision to take an IRQ uses:
```c
bool take_irq = irq_at_start || (cpu->irq_pending && cpu->irq_pending_age >= threshold);
```

### Special Case: 3-Cycle Taken Branches

Most instructions require `threshold = 1` (IRQ must be pending for at least 1 cycle before the instruction ends). However, 3-cycle taken same-page branches require `threshold = 2`. This is because the branch internally reuses a cycle for branch target calculation, shifting when the IRQ sample effectively occurs.

Branch opcodes are identified by `(opcode & 0x1F) == 0x10`, and a 3-cycle branch is one that was taken without crossing a page boundary.

### I Flag Sampling at Instruction Start

The 6502 commits to taking an interrupt based on the I flag state at the start of the instruction, not the end. This means:
- SEI sets I=1 but doesn't block an IRQ that was already going to be taken
- CLI sets I=0 but enables interrupts starting from the next instruction

We sample `i_flag_at_start` before the opcode fetch and use it for the final interrupt decision:
```c
bool i_flag_at_start = (cpu->P & FLAG_I) != 0;
// ... execute instruction ...
if (take_irq && !i_flag_at_start) { do_interrupt(...); }
```

### IRQ Pending Age Tracking

The age counter is incremented in `c64_tick()` before clocking the CIAs:
```c
if (sys->cpu.irq_pending) sys->cpu.irq_pending_age++;
```

When `irq_pending` becomes true in the CIA, we reset the age to 0. This ensures the threshold check accurately reflects real hardware timing.

## NMI Timing Issues Found and Fixed

The Lorenz `nmi` test validates precise Non-Maskable Interrupt timing, including edge detection, BRK hijacking, and the interaction between CIA timers and NMI triggering. This was one of the most challenging tests due to the cycle-exact timing requirements.

### Edge-Triggered NMI Detection

Unlike IRQ (which is level-triggered), NMI is edge-triggered. The CPU detects a high-to-low transition on the NMI line. We track this with an `nmi_edge` flag:

```c
void cpu_trigger_nmi(C64Cpu *cpu) {
    if (!cpu->nmi_edge) {
        cpu->nmi_pending = true;
        cpu->nmi_pending_age = 0;
        cpu->nmi_edge = true;
    }
}
```

The `nmi_edge` flag is only cleared when the CIA's Interrupt Control Register (ICR) is read, which acknowledges the interrupt source.

### CIA Timer Pipeline Delays

The 6526 CIA has specific timing delays that must be accurately emulated:

1. **Timer Start Delay (2 cycles):** When a timer is started by writing to the control register, counting doesn't begin until 2 cycles later. This is tracked with `ta_delay`.

2. **Interrupt Trigger Delay (1 cycle):** When a timer underflows and sets bit 0 of the ICR, there's a 1-cycle delay before the interrupt line (IRQ for CIA1, NMI for CIA2) actually goes low. This is tracked with `irq_delay`.

These delays are critical for the NMI test, which precisely times when NMI should occur relative to a BRK instruction.

### NMI Sampling at Instruction End

The 6502 samples the NMI line at specific points during instruction execution. After extensive testing, we found that NMI should be checked at the **end** of instruction execution (after the instruction has completed) rather than at the start:

```c
// At end of instruction execution:
bool take_nmi = nmi_at_start || (cpu->nmi_pending && cpu->nmi_pending_age >= 1);
```

This ensures the correct PC is pushed to the stack - the address of the next instruction after the one that just completed.

### BRK Special Case: No NMI at End

When a BRK instruction completes, we don't take an NMI at the end of the instruction because BRK is itself an interrupt sequence. Taking NMI at the end of BRK would cause incorrect behavior:

```c
if (opcode == 0x00) {  // BRK
    take_nmi = false;
}
```

### NMI Hijacking of BRK/IRQ

If an NMI becomes pending during the execution of a BRK instruction (or during an IRQ sequence), it can "hijack" the interrupt. The hijack check occurs after the processor status is pushed (cycle 5 of the interrupt sequence):

```c
// In do_interrupt(), after push P:
if (cpu->nmi_pending && cpu->nmi_pending_age >= 1 && vector != 0xFFFA) {
    vector = 0xFFFA;  // Redirect to NMI vector
    cpu->nmi_pending = false;
    // Keep nmi_edge set - prevents spurious second NMI
}
```

Key points:
- The hijack requires `nmi_pending_age >= 1` to ensure NMI was pending before the current cycle
- We don't clear `nmi_edge` on hijack - it stays set until the ICR is read
- The BRK instruction's B flag is still set in the pushed status, even though NMI is taken

### Age-Based NMI Threshold

Similar to IRQ, we track `nmi_pending_age` to determine if NMI was pending long enough to be sampled:

- **At instruction start:** Use `age >= 2` threshold to check if NMI was pending at the end of the previous instruction
- **At instruction end:** Use `age >= 1` threshold for normal NMI taking
- **For hijack:** Use `age >= 1` threshold

### Test Scenarios Covered

The NMI test validates timing with different timer values (CLOCK 0-9), testing:

1. **CLOCK 4-9:** NMI triggers after BRK completes, taken at the end of the subsequent NOP instruction
2. **CLOCK 0-3:** NMI triggers during BRK execution and hijacks the BRK, redirecting to NMI vector with B flag set

## References

- [6502 Instruction Timing](http://www.oxyron.de/html/opcodes02.html)
- [Extra Instructions of the 65xx Series CPU](http://www.ffd2.com/fridge/docs/6502-NMOS.extra.opcodes)
- [CIA 6526 Datasheet](http://archive.6502.org/datasheets/mos_6526_cia_recreated.pdf)
- Wolfgang Lorenz C64 Test Suite
