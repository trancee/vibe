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
void cpu_trigger_nmi(CPU *cpu) {
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
if (cpu->nmi_pending && cpu->nmi_pending_age >= 1 && vector != NMI_VECTOR) {
    vector = NMI_VECTOR;  // Redirect to NMI vector
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

## CIA Timer B Pipeline Delays (cia1tb123/cia2tb123)

The Lorenz `cia1tb123` and `cia2tb123` tests validate Timer B's specific pipeline behavior, which differs from Timer A in several ways.

### Timer B LOAD Pipeline (2-cycle delay)

When the LOAD bit (bit 4) is set in Control Register B ($DC0F/$DD0F), Timer B does NOT reload immediately. Instead, there is a 2-cycle pipeline delay:

```c
// Timer B LOAD has 2-cycle pipeline delay (unlike Timer A which is immediate)
if (force_load)
{
    cia->tb_load_delay = 2;
}
```

The delay is processed in `cia_clock()`:
```c
if (cia->tb_load_delay > 0) {
    cia->tb_load_delay--;
    if (cia->tb_load_delay == 0) {
        cia->timer_b = cia->timer_b_latch;
    }
}
```

**Key insight:** Timer A LOAD is immediate, while Timer B LOAD has a 2-cycle delay. This asymmetry is critical for passing the tests.

### Timer B STOP Pipeline (2-cycle delay)

When Timer B is stopped (START bit cleared) after a forced underflow, there's a 2-cycle delay before the timer actually stops:

```c
// Timer B: stopping after force-underflow has a 2-cycle delay
if (!start && crb_old_start)
{
    cia->tb_stop_delay = 2;
    // Don't clear START bit yet - it will be cleared by stop_delay processing
}
```

During the stop delay, the START bit remains set in the internal state, allowing the timer to continue counting for 2 more cycles.

### Timer A vs Timer B Pipeline Differences

| Operation | Timer A | Timer B |
|-----------|---------|---------|
| START     | 2-cycle delay | 2-cycle delay |
| LOAD      | Immediate | 2-cycle delay |
| STOP      | Immediate | 2-cycle delay |

These differences reflect the real 6526 CIA's internal pipeline architecture. Timer A is on the "fast path" for common timing operations, while Timer B has additional pipeline stages.

## CIA Timer Output to Port B (PB6/PB7)

The Lorenz `cia1pb6`, `cia1pb7`, `cia2pb6`, and `cia2pb7` tests validate the CIA's ability to output timer underflow signals directly to Port B pins.

### Overview

The 6526 CIA can output timer signals to Port B:
- **Timer A → PB6** (bit 6 of Port B)
- **Timer B → PB7** (bit 7 of Port B)

This is controlled by the PBON bit (bit 1) in the respective control registers (CRA for Timer A, CRB for Timer B).

### Output Modes

The OUTMODE bit (bit 2) controls how timer underflow appears on the output:

| OUTMODE | Mode | Behavior |
|---------|------|----------|
| 0 | Pulse | Output goes HIGH for exactly one cycle on each underflow |
| 1 | Toggle | Output inverts (toggles) on each underflow |

### Key Implementation Details

#### Toggle Flip-Flop Is Independent

The toggle flip-flop is internal to the timer and always toggles on every underflow, **regardless of the PBON and OUTMODE settings**. This is critical for passing the tests that verify "toggle state is not independent."

```c
// Timer output - toggle flip-flop ALWAYS toggles on underflow
// regardless of output mode (pulse vs toggle) or PBON setting
cia->pb6_out = !cia->pb6_out;  // Toggle

// Pulse mode sets the pulse flag
cia->pb6_pulse = true;
```

#### Flip-Flop Set HIGH on Timer Start

When a timer is started (by setting the START bit in the control register), the corresponding flip-flop is set HIGH:

```c
if (start && !cra_old_start) {
    // Timer starting
    cia->pb6_out = true;  // Set flip-flop HIGH when timer starts
}
```

#### Timer B Shadow Counter for PB7 Timing

A key challenge was that the Timer B register read tests (`cia1tb123`, `cia2tb123`) require a 2-cycle delay before Timer B starts counting, but the PB7 pulse timing tests require a 1-cycle delay for correct output.

The solution uses a **shadow counter** (`timer_b_pb7`) that runs 1 cycle ahead of the main timer:

```c
// Main timer uses tb_delay=2 (for register read timing)
// Shadow counter uses pb7_delay=1 (for PB7 output timing)
if (cia->pb7_delay > 0) {
    cia->pb7_delay--;
} else {
    // Shadow counter for PB7 output
    cia->timer_b_pb7--;
    if (cia->timer_b_pb7 == 0xFFFF) {
        cia->pb7_out = !cia->pb7_out;  // Toggle flip-flop
        cia->pb7_pulse = true;
    }
}
```

#### One-Cycle Output Delay

Both toggle and pulse outputs have a one-cycle delay before they appear on the port:

```c
// Update delayed outputs at the start of each cycle
cia->pb6_out_delayed = cia->pb6_out;
cia->pb7_out_delayed = cia->pb7_out;
cia->pb6_pulse_out = cia->pb6_pulse;
cia->pb7_pulse_out = cia->pb7_pulse;
```

### Port B Read Logic

When reading Port B with PBON set, the timer output overrides the corresponding bit:

```c
u8 val = (cia->prb & cia->ddrb) | (~cia->ddrb & 0xFF);

if (cia->cra & CIA_CR_PBON) {
    // Timer A outputs to PB6
    bool pb6_val = (cia->cra & CIA_CR_OUTMODE) 
        ? cia->pb6_out_delayed   // Toggle mode
        : cia->pb6_pulse_out;    // Pulse mode
    val = (val & ~0x40) | (pb6_val ? 0x40 : 0);
}

if (cia->crb & CIA_CR_PBON) {
    // Timer B outputs to PB7
    bool pb7_val = (cia->crb & CIA_CR_OUTMODE)
        ? cia->pb7_out_delayed   // Toggle mode  
        : cia->pb7_pulse_out;    // Pulse mode
    val = (val & ~0x80) | (pb7_val ? 0x80 : 0);
}
```

### Test Status

After implementation, all PB6/PB7 tests pass:
- ✅ cia1pb6
- ✅ cia2pb6
- ✅ cia1pb7
- ✅ cia2pb7

## CIA Timer Cascade Mode (cia1tab/cia2tab)

The Lorenz `cia1tab` test validates Timer B's cascade mode, where Timer B counts Timer A underflows instead of system clock cycles. This mode is enabled by setting bits 5-6 of CRB to `01` (INMODE = Timer A underflow).

### Test Configuration

The test sets both Timer A and Timer B latches to 2, starts Timer B in cascade mode, then starts Timer A. It reads all four values (TA, TB, PB, ICR) at 12 different timing offsets and compares against expected values.

### Key Findings

#### 1. Cascade Signal Delay (1 cycle)

When Timer A underflows, Timer B should count on the NEXT cycle, not immediately. This models the internal 6526 pipeline where the underflow signal propagates with a 1-cycle delay:

```c
// At start of cia_clock():
cia->ta_underflow_delay = cia->ta_underflow;  // Copy previous cycle's underflow
cia->ta_underflow = false;                     // Clear for this cycle

// Timer B cascade mode uses the delayed signal:
case 2: // Timer A underflow
    if (cia->ta_underflow_delay) {
        count = true;
    }
    break;
```

#### 2. Timer B Reads in Cascade Mode

In cascade mode, Timer B reads need to return a "captured" value (`timer_b_read`) that is updated at the start of each cycle. This gives a 1-cycle delay on reads, matching real hardware:

```c
// At start of cia_clock():
cia->timer_b_read = cia->timer_b;

// When reading Timer B in cascade mode:
u8 inmode = (cia->crb >> 5) & 0x03;
u16 value = (inmode == 0) ? cia->timer_b : cia->timer_b_read;
```

#### 3. Zero Value Visibility in Cascade Mode

In phi2 mode (counting system clocks), Timer B reloads immediately on underflow, so reads never see value 0. However, in cascade mode with the delayed read mechanism, the value 0 IS visible for exactly 1 cycle:

```c
// Only phi2 mode substitutes 0 with latch value
if (inmode == 0 && value == 0 && (cia->crb & CIA_CR_START))
{
    value = cia->timer_b_latch;
}
```

#### 4. Immediate Read Update on Cascade Underflow

When Timer B underflows in cascade mode, `timer_b_read` must be immediately updated to the reloaded latch value so reads on that same cycle see the new value:

```c
if (cia->timer_b == 0xFFFF)
{
    cia->timer_b = cia->timer_b_latch;
    
    // In cascade mode, update timer_b_read immediately
    if (inmode != 0)
    {
        cia->timer_b_read = cia->timer_b_latch;
    }
    // ... ICR flag setting, etc.
}
```

#### 5. PB7 Output in Cascade Mode

The PB7 output (controlled by Timer B) must also update immediately on underflow in cascade mode, rather than being delayed by the normal 1-cycle output pipeline:

```c
// In cascade mode, update pb7_out_delayed immediately
if (inmode != 0)
{
    cia->pb7_out_delayed = cia->pb7_out;
}
```

#### 6. No Delay for PB7 Shadow Timer in Cascade Mode

In phi2 mode, the PB7 shadow timer (`timer_b_pb7`) uses `pb7_delay` to control when counting starts. In cascade mode, this delay should be bypassed:

```c
bool can_count_pb7 = (inmode != 0) || (cia->pb7_delay == 0);

if (cia->pb7_delay > 0 && inmode == 0)
{
    cia->pb7_delay--;
}

if (can_count_pb7)
{
    cia->timer_b_pb7--;
    // ...
}
```

### Summary: Cascade Mode Timing Differences

| Aspect | Phi2 Mode | Cascade Mode |
|--------|-----------|--------------|
| Count signal | Every cycle | 1 cycle after TA underflow |
| Timer read value | Current `timer_b` | Captured `timer_b_read` |
| Value 0 visible | No (shows latch) | Yes (for 1 cycle) |
| PB7 output delay | 1 cycle | Immediate on underflow |
| Start counting delay | Uses `tb_delay` | No delay |

### Test Status

After all fixes:
- ✅ cia1tab - PASSING

## CIA Timer Input Mode Switching (cnto2)

The Lorenz `cnto2` test validates the timing behavior when switching between the two Timer A input modes: phi2 (system clock) and CNT (external clock pin).

### Test Configuration

The test:
1. Starts Timer A in CNT mode (not counting since CNT pin isn't pulsed)
2. Switches to phi2 mode and reads the timer (expects specific value)
3. Switches back to CNT mode and reads again (expects specific value)

The test verifies that mode switching has a 1-cycle pipeline delay in both directions.

### Key Finding: Mode Switch Delay

When switching between CNT and phi2 modes while the timer is running, there is a **1-cycle delay** before the mode change takes effect:

| Transition | Behavior |
|------------|----------|
| CNT → phi2 | Timer delays 1 cycle before starting to count |
| phi2 → CNT | Timer continues counting for 1 more cycle before stopping |

### Implementation

Two new delay counters were added to track mode transitions:

```c
// In cia.h
u8 ta_cnt_delay;  // Timer A counting continues after phi2→CNT switch
u8 tb_cnt_delay;  // Timer B counting continues after phi2→CNT switch
```

The mode switch is detected in the CRA write handler:

```c
if (now_running)
{
    if (was_cnt_mode && !now_cnt_mode)
    {
        // CNT → phi2: 1-cycle delay before counting starts
        cia->ta_delay = 1;
    }
    else if (!was_cnt_mode && now_cnt_mode)
    {
        // phi2 → CNT: timer continues counting for 1 more cycle
        cia->ta_cnt_delay = 1;
    }
}
```

The counting logic uses the delay counter to continue counting during the transition:

```c
if (!(cia->cra & CIA_CR_INMODE))
{
    // phi2 mode - count
    count = true;
}
else if (cia->ta_cnt_delay > 0)
{
    // CNT mode but still in transition - continue counting
    cia->ta_cnt_delay--;
    count = true;
}
// else: CNT mode after delay - not counting
```

### Test Status

After implementing mode switch delays:
- ✅ cnto2 - PASSING

## CIA ICR Read and NMI Triggering (icr01)

The Lorenz `icr01` test validates the precise interaction between reading the CIA2 Interrupt Control Register ($DD0D) and NMI triggering. This test examines several edge cases around when NMI should and shouldn't fire based on the timing of ICR reads.

### Test Scenarios

The test validates four key scenarios for CIA2 Timer A with NMI enabled:

1. **Read ICR when value is $01** (timer fired, but bit 7 not yet set): First read should see $01, second read should see $00 (cleared), and NMI should NOT fire.

2. **Read ICR when value is $81** (NMI already triggered): First read should see $81, second read should see $00 (cleared), and NMI MUST fire after the instruction completes.

3. **Read ICR when value is $00** (timer hasn't fired yet): First read should see $00, second read should see $81 (timer fired between reads), and NMI MUST fire after the second instruction.

### Key Findings

#### 1. ICR Acknowledge Timing (icr_ack)

The `icr_ack` flag was being cleared at the **end** of `cia_clock()`, but this caused it to persist into the next cycle. If an ICR read happened on one cycle and the timer underflowed on the next, the stale `icr_ack` would incorrectly inhibit the interrupt from triggering.

**Fix:** Move `icr_ack` clearing to the **start** of `cia_clock()`:

```c
void cia_clock(CIA *cia)
{
    // Clear icr_ack from previous cycle's read at the START of this cycle
    // This way, a read sets icr_ack to prevent interrupts from that same cycle,
    // but the next cycle starts fresh
    cia->icr_ack = false;
    
    // ... rest of cia_clock
}
```

This ensures that an ICR read only inhibits interrupt triggering for the remainder of that same cycle, not the following cycle.

#### 2. NMI Preservation on ICR Read (nmi_triggered_this_insn)

When reading ICR with bit 7 set ($81), the read clears both `nmi_pending` and `nmi_edge`. However, if the CPU was in the middle of an instruction when this happened, the NMI that was pending should still fire at the end of that instruction.

**Fix:** Add a flag to track NMI triggering within an instruction:

```c
// In cpu.h
bool nmi_triggered_this_insn;  // NMI was triggered during this instruction

// In cia_read() when reading CIA2 ICR:
if (result & 0x80)
{
    cia->sys->cpu.nmi_triggered_this_insn = true;
}
cia->sys->cpu.nmi_pending = false;
cia->sys->cpu.nmi_edge = false;

// In cpu_step() at instruction start:
cpu->nmi_triggered_this_insn = false;

// In cpu_step() for take_nmi decision:
bool take_nmi = nmi_at_start || cpu->nmi_triggered_this_insn || 
                (cpu->nmi_pending && cpu->nmi_pending_age >= nmi_threshold);
```

This ensures that if NMI was pending at the moment of ICR read (bit 7 was set), the CPU still takes the NMI at the end of the instruction even though the ICR read cleared `nmi_pending`.

### Timing Analysis

For the "read ICR=$81" case with timer latch=1:

| Cycle | Event |
|-------|-------|
| STA $DD0E (cycle 4) | Timer starts, force load, ta_delay=2 |
| LDA $DD0D (cycle 1) | ta_delay 2→1 |
| LDA $DD0D (cycle 2) | ta_delay 1→0 |
| LDA $DD0D (cycle 3) | Timer counts 1→0 |
| LDA $DD0D (cycle 4) | Timer 0→FFFF (underflow), irq_delay=1, reload |
| After LDA | irq_delay fires, ICR bit 7 set, NMI triggered |
| LDX $DD0D (cycle 4) | Reads ICR=$81, sets nmi_triggered_this_insn |
| After LDX | NMI taken because nmi_triggered_this_insn is true |

### Test Status

After both fixes:
- ✅ icr01 - PASSING

## CIA ICR Mask Write Timing (imr)

The Lorenz `imr` test validates the timing of interrupt triggering when writing to the CIA Interrupt Control Register ($DC0D/$DD0D) to enable an interrupt that already has a pending source.

### Test Configuration

The test:
1. Starts Timer A in one-shot mode with latch=0 (causes immediate underflow)
2. Timer underflows and sets ICR bit 0, but interrupt mask is disabled
3. Later, writes $81 to ICR to enable Timer A interrupt
4. Checks that IRQ does NOT fire on "clock 2" (2nd cycle after the write)
5. Checks that IRQ DOES fire on "clock 3" (3rd cycle after the write)

### Key Finding: 2-Cycle Delay for ICR Mask Write

When writing to the ICR mask register to enable an interrupt that already has a pending source (e.g., timer underflow already set ICR bit 0), there is a **2-cycle delay** before the interrupt line goes low.

This differs from the 1-cycle delay used when a timer underflows with the interrupt already enabled. The extra cycle accounts for the fact that:
1. The ICR mask write happens **after** `cia_clock()` runs for that cycle
2. The `irq_delay` counter is processed at the **start** of the next `cia_clock()`

### Implementation

```c
case CIA_ICR:
    // Bit 7: Set or clear mode
    if (value & 0x80)
    {
        cia->icr_mask |= (value & 0x1F);
    }
    else
    {
        cia->icr_mask &= ~(value & 0x1F);
    }

    // Check if we now have enabled pending interrupt
    // When writing to ICR mask, if a pending interrupt exists, use 2-cycle delay
    // because the write happens after cia_clock runs for this cycle
    if (!cia->icr_ack &&
        !cia->irq_delay &&
        !(cia->icr_data & CIA_ICR_IR) &&
        (cia->icr_data & cia->icr_mask & 0x1F))
    {
        cia->irq_delay = 2;
    }
    break;
```

### Timing Comparison

| Interrupt Source | Delay Cycles |
|------------------|--------------|
| Timer underflow (interrupt already enabled) | 1 cycle |
| ICR mask write (enabling pending interrupt) | 2 cycles |

### Test Status

After implementing the 2-cycle delay:
- ✅ imr - PASSING

## One-Shot Mode Switching Timing (flipos)

The flipos test validates the timing of switching between one-shot and continuous modes at precise moments relative to timer underflow.

### Test Cases

1. **SET ONESHOT AT T-1**: Start in continuous mode, switch to one-shot 1 cycle before underflow. Timer should STOP.
2. **CLR ONESHOT AT T-1**: Start in one-shot mode, switch to continuous 1 cycle before underflow. Timer should STOP.
3. **SET ONESHOT AT T**: Switch to one-shot exactly at underflow. Timer may or may not stop (implementation-defined).
4. **CLR ONESHOT AT T-2**: Switch from one-shot to continuous 2 cycles before underflow. Timer may not stop.

### Key Finding: Asymmetric RUNMODE Behavior

The one-shot mode has asymmetric timing:
- **Setting one-shot** takes effect immediately (current cycle)
- **Clearing one-shot** has a 2-cycle delay (the old one-shot state persists)

This means:
- If you switch TO one-shot mode just before underflow, the timer stops
- If you switch FROM one-shot mode just before underflow, the timer still stops (because the change hasn't taken effect yet)

### Implementation: 2-Stage RUNMODE Pipeline

We implement this with a 2-stage pipeline for the RUNMODE bit:
```c
// At start of cia_clock():
cia->runmode_a = cia->runmode_a_next;
cia->runmode_a_next = cia->runmode_a_pending;

// When writing CRA:
cia->runmode_a_pending = (value & CIA_CR_RUNMODE) != 0;
```

At underflow, we use OR logic to combine both immediate and pipelined values:
```c
bool oneshot = (cia->cra & CIA_CR_RUNMODE) || cia->runmode_a;
if (oneshot) {
    cia->cra &= ~CIA_CR_START;  // Stop timer
}
```

This ensures:
- Setting one-shot: `cra & CIA_CR_RUNMODE` is true immediately → timer stops
- Clearing one-shot: `runmode_a` retains the old value for 2 cycles → timer stops

### Test Status

After implementing the asymmetric RUNMODE pipeline:
- ✅ flipos - PASSING

## CNT Default State (cntdef)

The cntdef test validates that the CNT (counter input) pin is high by default.

### Background

Timer B supports four input modes (INMODE bits 6-5 of CRB):
- 00: Count phi2 clock
- 01: Count CNT positive edges
- 10: Count Timer A underflows
- 11: Count Timer A underflows while CNT is high

Mode 11 (cascade with CNT gate) requires the CNT line to be high for Timer B to count Timer A underflows.

### Key Finding: CNT is High by Default

On real hardware, the CNT pin is typically pulled high externally. The test verifies this by setting Timer B to mode 11 and checking if it counts.

### Implementation

We assume CNT is always high (since we don't emulate external CNT input):
```c
case 3: // Timer A underflow while CNT high
    // CNT line is high by default (pulled high externally)
    if (cia->ta_underflow_delay) {
        count = true;
    }
    break;
```

### Test Status

After implementing CNT high by default:
- ✅ cntdef - PASSING

## Known Limitations

### CIA Timer A Timing (cia1ta)

The Lorenz `cia1ta` test fails 1 of approximately 14,000 test cases.

**Failing Case:** I4=$1E (30), B4=$14 (20), IE=$11, BE=$00

**Root Cause:** Timer read timing differs by 1 cycle from real hardware in this specific edge case. The test sets up Timer A with various latch values (I4) and initial timer values (B4), then reads the timer and interrupt status at precise cycle offsets. In this particular case, our emulation returns the timer value 1 cycle differently than expected.

**Why It Cannot Be Fixed:** The timer implementation uses `ta_delay` to correctly model the delay before a timer starts counting after being loaded. This delay is essential for passing the vast majority of test cases. However, in this specific edge case, the delay causes the timer read to return a value that differs from real hardware by 1 cycle.

### CIA Timer B One-Shot Mode (cia1tb, cia2tb)

The Lorenz `cia1tb` and `cia2tb` tests each fail 1 of approximately 14,000 test cases.

**Failing Case:** I4=$1E (30), B4=$09 (9), IE=$10, BE=$19

**Observed Behavior:**
- Timer value reads correctly: A4=$08, AD=$00 (matches expected R4, RD)
- CRB reads incorrectly: AE=$09 vs expected RE=$08

The test expects the START bit (bit 0 of CRB) to be cleared when reading CRB, but our emulation still shows it set.

**Root Cause Analysis:**

Through detailed cycle tracing, the issue was identified:

| Cycle | Timer B Value | Event |
|-------|---------------|-------|
| ... | 9→8→7→...→1→0 | Timer counting down |
| N | 0 | CRB READ happens, returns $09 (START still set) |
| N+1 | 0→$FFFF | Underflow detected, timer reloads, START bit cleared |

The problem is that **Timer B underflows and clears the START bit on the cycle AFTER the CRB read**. Real hardware apparently clears the START bit before or during the read on cycle N, but our emulation clears it on cycle N+1.

**Why It Cannot Be Fixed:**

The timer implementation uses `tb_delay` to correctly model the delay before Timer B starts counting after a force load. This delay is critical for many test cases:

1. Without `tb_delay`, timer register reads return incorrect values
2. With `tb_delay`, the underflow/START-clearing happens one cycle later than some edge cases expect

Attempted fixes:
1. **Check underflow before decrement**: Same result - the timing relationship is unchanged
2. **Remove tb_delay reset after force load**: Fixed cia1tb but broke other test cases (B4=$14 variants)

The fundamental conflict is that timer register reads need the delayed counting (`tb_delay`) for correct values, but ICR/CRB underflow detection would need faster counting for correct flag timing in these edge cases. The implementation prioritizes correct timer register reads since those are far more common in real software.

## Current Test Status Summary

After all fixes, the following 18 CIA/interrupt-related tests pass:
- ✅ irq - IRQ timing
- ✅ nmi - NMI timing
- ✅ cia1tb123 - Timer B cascade modes
- ✅ cia2tb123 - Timer B cascade modes (CIA2)
- ✅ cia1pb6 - Timer A PB6 output
- ✅ cia1pb7 - Timer B PB7 output
- ✅ cia2pb6 - Timer A PB6 output (CIA2)
- ✅ cia2pb7 - Timer B PB7 output (CIA2)
- ✅ cia1tab - Timer A→B cascade
- ✅ loadth - Timer force load
- ✅ cnto2 - Timer counting
- ✅ icr01 - ICR read timing
- ✅ imr - ICR mask write timing
- ✅ flipos - One-shot mode switching
- ✅ oneshot - Basic one-shot mode
- ✅ cntdef - CNT default state

Known failing tests (documented limitations):
- ⚠️ cia1ta - 1 of ~14,000 test cases fails (timer read timing edge case)
- ⚠️ cia1tb - 1 of ~14,000 test cases fails (one-shot START bit timing edge case)
- ⚠️ cia2tb - Same issue as cia1tb (CIA2 Timer B has identical timing characteristics)

## CIA Interrupt Pending Flag Fix

The unit tests `icr_irq_pending` and `cia2_generates_nmi` were failing because the CIA's internal `irq_pending` flag was not being properly maintained.

### Problem

The CIA struct has an `irq_pending` flag to track whether the CIA is asserting an interrupt. This flag was:
1. Never set to `true` when a timer underflow triggered an interrupt
2. Never cleared when the ICR was read (acknowledging the interrupt)

The code was correctly setting `sys->cpu.irq_pending` (the CPU-level interrupt) but not `cia->irq_pending` (the CIA-level status flag).

### Fix

In `cia_clock()`, when the IRQ delay expires and triggers an interrupt:
```c
if (cia->irq_delay == 0) {
    cia->icr_data |= CIA_ICR_IR;
    cia->irq_pending = true;  // Added
    // ... trigger CPU interrupt ...
}
```

In `cia_read()`, when the ICR register is read:
```c
case CIA_ICR: {
    u8 result = cia->icr_data;
    cia->icr_data = 0;
    cia->irq_pending = false;  // Added
    // ... clear CPU interrupt ...
}
```

## NMI Edge Detection Fix

During debugging of the cia2ta test, an issue was found with double-NMI triggering.

### Problem

When reading the CIA2 ICR register mid-instruction, the code would set `nmi_triggered_this_insn = true` to preserve the NMI for execution at the end of the instruction. However, this was happening even when the NMI had already been taken, causing a spurious second NMI.

### Fix

In `cia_read()` for CIA2 ICR read:
```c
// Only set nmi_triggered_this_insn if NMI is actually still pending
if ((result & 0x80) && cia->sys->cpu.nmi_pending) {
    cia->sys->cpu.nmi_triggered_this_insn = true;
}
```

In `cpu_step()` when taking the NMI:
```c
if (take_nmi) {
    cpu->nmi_pending = false;
    cpu->nmi_triggered_this_insn = false;  // Added - NMI is being taken
    // ...
}
```

## Test Harness Improvements

The Lorenz test harness required several fixes to properly run the CIA timer tests:

### Screen Memory Initialization

The tests use KERNAL routines that clear the screen. Without proper initialization, the screen clear would corrupt page 3 vectors ($0314-$0319):

```c
// Initialize HIBASE ($0288) to point screen at $0400
mem_write_raw(&sys.mem, 0x0288, 0x04);

// Initialize screen line table ($D9-$F1)
for (int row = 0; row < 25; row++) {
    u16 line_addr = 0x0400 + (row * 40);
    mem_write_raw(&sys.mem, 0xD9 + row, (line_addr >> 8) | 0x80);
}
```

### IRQ Handler in RAM

The IRQ handler at $FF48 jumps through the vector at $0314/$0315. When ROM is visible, writes to $EABF (where the default handler would be) go to RAM but reads still come from ROM. The fix places the IRQ return stub at $0270 in the cassette buffer area, which is always RAM regardless of ROM banking.

### STOP Key Handler

The tests call the KERNAL STOP routine to check for user abort. A stub at $0290 returns with Z=0 (no STOP key pressed) to allow tests to continue.

## References

- [6502 Instruction Timing](http://www.oxyron.de/html/opcodes02.html)
- [Extra Instructions of the 65xx Series CPU](http://www.ffd2.com/fridge/docs/6502-NMOS.extra.opcodes)
- [CIA 6526 Datasheet](http://archive.6502.org/datasheets/mos_6526_cia_recreated.pdf)
- Wolfgang Lorenz C64 Test Suite
