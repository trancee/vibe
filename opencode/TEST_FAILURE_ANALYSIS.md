# Test Failure Analysis and Implementation Guidance

This document analyzes the current test failures and provides specific implementation guidance for fixing them.

## CPU Test Failures

### 1. Branch Timing (cpu_branch_timing)
**Error**: `Taken branch should take 3 cycles (expected: 3, actual: 2)`

**Root Cause**: The CPU implementation doesn't add the extra cycle for taken branches.

**Fix Needed** in `cpu.c`:
```c
// In branch instruction implementation (around addressing modes)
static u16 addr_mode_relative(C64* sys) {
    s8 offset = (s8)cpu_fetch_byte(sys);
    u16 target_addr = sys->cpu.pc + offset;
    
    // Add 1 cycle if branch taken
    if (condition_is_true) {
        sys->cpu.extra_cycles++;
        
        // Add another cycle if page boundary crossed
        if ((sys->cpu.pc & 0xFF00) != (target_addr & 0xFF00)) {
            sys->cpu.extra_cycles++;
        }
    }
    
    return target_addr;
}
```

### 2. Page Cross Timing (cpu_page_cross_timing)
**Error**: `A should load value from page crossed address (expected: 66, actual: 0)`

**Root Cause**: Indexed addressing modes don't handle page crossing correctly.

**Fix Needed** in memory addressing:
```c
// For absolute X/Y addressing
static u16 addr_mode_absolute_x(C64* sys) {
    u16 base = cpu_fetch_word(sys);
    u16 addr = base + sys->cpu.x;
    
    // Check page cross and add penalty cycle
    if ((base & 0xFF00) != (addr & 0xFF00)) {
        sys->cpu.page_crossed = true;
        sys->cpu.extra_cycles++;
    }
    
    return addr;
}
```

### 3. Stack Operations (cpu_stack_operations)
**Error**: `PC should return after RTS (expected: 4099, actual: 4098)`

**Root Cause**: RTS increments PC after pulling from stack, but implementation might not handle this correctly.

**Fix Needed** in RTS implementation:
```c
case 0x60: // RTS
    sys->cpu.pc = cpu_pull_word(sys);
    sys->cpu.pc++;  // RTS increments PC after return
    break;
```

### 4. Compare Instructions (cpu_compare_instructions)
**Error**: `Carry should be set for CPX equal`

**Root Cause**: Compare instructions not setting carry flag correctly.

**Fix Needed** in CPX implementation:
```c
case 0xE0: // CPX #$imm
case 0xE4: // CPX $zp  
case 0xEC: // CPX $abs
    u8 result = sys->cpu.x - operand;
    sys->cpu.status &= ~(FLAG_C | FLAG_Z | FLAG_N);
    if (sys->cpu.x >= operand) sys->cpu.status |= FLAG_C;
    if (result == 0) sys->cpu.status |= FLAG_Z;
    if (result & 0x80) sys->cpu.status |= FLAG_N;
    break;
```

### 5. Transfer Instructions (cpu_transfer_instructions)
**Error**: `Negative should be set`

**Root Cause**: Transfer instructions not updating flags correctly.

**Fix Needed** in transfer instructions:
```c
case 0xA8: // TAY
    sys->cpu.y = sys->cpu.a;
    sys->cpu.status &= ~(FLAG_Z | FLAG_N);
    if (sys->cpu.y == 0) sys->cpu.status |= FLAG_Z;
    if (sys->cpu.y & 0x80) sys->cpu.status |= FLAG_N;
    break;
```

## CIA Test Failures

### 1. Timer Underflow (cia_timer_underflow)
**Error**: `Timer A underflow flag should be set`

**Root Cause**: Timer underflow detection not implemented correctly.

**Fix Needed** in `cia.c`:
```c
void cia_clock(C64CIA* cia) {
    // Timer A
    if (cia->cra & 0x01) { // Timer started
        if (cia->ta_delay > 0) {
            cia->ta_delay--;
        } else {
            cia->timer_a--;
            if (cia->timer_a == 0xFFFF) { // Underflow
                cia->timer_a = cia->timer_a_latch; // Reload
                cia->icr_data |= 0x01; // Set underflow flag
                cia->icr_irq_flag_set = true; // Mark for ICR bit 7
                cia->irq_pending = (cia->icr_data & cia->imr) != 0;
            }
        }
    }
}
```

### 2. Lorenz IRQ Test (cia_lorenz_irq)
**Error**: Timer values < 8 should underflow immediately

**Root Cause**: Pipeline delay implementation not matching expected behavior.

**Fix Needed**: The pipeline delay might need adjustment or the timer start value handling.

### 3. Timer Pipeline Delay (cia_timer_pipeline)
**Error**: `Timer should not decrement on first cycle due to pipeline delay`

**Root Cause**: Pipeline delay logic not working correctly.

**Fix Needed**:
```c
// When starting timer
cia->ta_delay = 2; // Start with 2-3 cycle delay
cia->timer_a_started = true;
```

### 4. ICR Timing (cia_icr_timing)
**Error**: `Underflow flag should be set immediately`

**Root Cause**: ICR bit 7 timing delay not implemented.

**Fix Needed**:
```c
// In cia_clock()
if (cia->icr_irq_flag_set) {
    // After 1 cycle, set bit 7 in ICR
    cia->icr_data |= 0x80;
    cia->icr_irq_flag_set = false;
}
```

### 5. TOD Clock (cia_tod_clock)
**Error**: `Tenth should increment after 10 cycles (expected: 8, actual: 7)`

**Root Cause**: TOD clock not advancing at correct rate.

**Fix Needed**:
```c
void cia_clock(C64CIA* cia) {
    static int tod_counter = 0;
    tod_counter++;
    
    if (tod_counter >= 10) { // Every 10 cycles = 1/10 second
        tod_counter = 0;
        cia->tod_tenth++;
        if (cia->tod_tenth >= 10) {
            cia->tod_tenth = 0;
            cia->tod_seconds++;
            // ... handle minute/hour carry
        }
    }
}
```

## Priority Implementation Order

### High Priority (Critical for functionality)
1. **CPU Flag Operations** - Affects almost all instructions
2. **CPU Compare Instructions** - Used extensively
3. **CPU Transfer Instructions** - Fundamental operations
4. **CIA Timer Underflow** - Core timing functionality

### Medium Priority (Important for accuracy)
5. **CPU Branch Timing** - Cycle accuracy
6. **CPU Page Cross Timing** - Cycle accuracy
7. **CPU Stack Operations** - Subroutine calls
8. **CIA Pipeline Delay** - Timing accuracy

### Lower Priority (Advanced features)
9. **CIA ICR Timing** - Interrupt accuracy
10. **CIA TOD Clock** - Real-time clock functionality

## Testing Strategy

### 1. Fix One Test at a Time
Start with the fundamental failures and work up:

```bash
# Fix CPU flags first
./test_suite --cpu | grep "FAIL.*flag"

# Then compare instructions  
./test_suite --cpu | grep "FAIL.*compare"

# Then transfer instructions
./test_suite --cpu | grep "FAIL.*transfer"
```

### 2. Use Individual Test Functions
The test suite supports running individual functions by commenting out others in the test runner.

### 3. Validate Incrementally
After each fix, re-run tests to ensure you're making progress without breaking other functionality.

### 4. Reference Documentation
- **6502 datasheet** for CPU instruction details
- **6526 CIA datasheet** for timer behavior
- **Implementation plan document** for specific requirements

## Expected vs Actual Behavior

The test failures indicate that the current implementation is **functionally working** but not **cycle-accurate**. This is actually good progress! The core structure is there, but the timing details need refinement.

### What's Working:
- Basic instruction execution
- Memory access
- Component structure
- Test framework integration

### What Needs Work:
- Cycle-accurate timing
- Flag updates
- Interrupt generation
- Hardware edge cases

## Next Steps

1. **Immediate**: Fix CPU flag and compare instructions
2. **Short-term**: Address CIA timer underflow
3. **Medium-term**: Implement proper timing for branches and page crosses
4. **Long-term**: Complete cycle-accurate implementation

The test suite is working exactly as intended - it's identifying the specific areas where the emulator needs improvement to achieve cycle-exact accuracy!