/**
 * cia.c - CIA 6526 chip emulation
 *
 * Implements two CIA chips with timers, TOD clock,
 * keyboard matrix scanning, and serial port.
 * 
 * Known Limitations:
 * - Lorenz cia1ta test: Fails 1 of ~14,000 test cases (I4=30, B4=20, IE=$11, BE=$00)
 *   The timer read timing in this edge case differs by 1 cycle from real hardware.
 * - Lorenz cia1tb/cia2tb tests: Fail 1 of ~14,000 test cases (I4=30, B4=9, IE=$10, BE=$19)
 *   The one-shot mode START bit clearing happens 1 cycle later than real hardware
 *   expects when the timer underflows on the exact cycle of a CRB read.
 * 
 * These edge cases involve conflicting timing requirements between:
 * - Timer register reads (which need delayed counting via tb_delay)
 * - ICR/CRB underflow detection (which needs faster counting for proper flag timing)
 * The current implementation prioritizes the timer register read timing.
 */

#include "cia.h"
#include "c64.h"
#include <stdio.h>
#include <string.h>

// Keyboard matrix state (global for simplicity)
static u8 keyboard_matrix[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void cia_init(CIA *cia, int cia_num, C64 *sys)
{
    memset(cia, 0, sizeof(CIA));
    cia->cia_num = cia_num;
    cia->sys = sys;
}

void cia_reset(CIA *cia)
{
    cia->pra = 0;
    cia->prb = 0;
    cia->ddra = 0;
    cia->ddrb = 0;

    cia->timer_a = 0xFFFF;
    cia->timer_b = 0xFFFF;
    cia->timer_a_read = 0xFFFF;
    cia->timer_b_read = 0xFFFF;
    cia->timer_a_latch = 0xFFFF;
    cia->timer_b_latch = 0xFFFF;
    cia->timer_a_pb6 = 0xFFFF;
    cia->timer_b_pb7 = 0xFFFF;

    cia->ta_delay = 0;
    cia->pb6_delay = 0;
    cia->tb_delay = 0;
    cia->pb7_delay = 0;
    cia->ta_started = false;
    cia->tb_started = false;
    cia->ta_reload_skip = false;
    cia->pb6_reload_skip = false;
    cia->tb_reload_skip = false;
    cia->pb7_reload_skip = false;
    cia->ta_load_delay = 0;
    cia->tb_load_delay = 0;
    cia->ta_stop_delay = 0;
    cia->tb_stop_delay = 0;
    cia->ta_cnt_delay = 0;
    cia->tb_cnt_delay = 0;
    
    cia->pb6_out = true;
    cia->pb7_out = true;
    cia->pb6_out_delayed = true;
    cia->pb7_out_delayed = true;
    cia->pb6_pulse = false;
    cia->pb7_pulse = false;
    cia->pb6_pulse_out = false;
    cia->pb7_pulse_out = false;
    
    cia->ta_underflow = false;
    cia->ta_underflow_delay = false;

    cia->cra = 0;
    cia->crb = 0;
    
    // RUNMODE 2-stage pipeline - all stages are 0 (continuous mode)
    cia->runmode_a = false;
    cia->runmode_a_next = false;
    cia->runmode_a_pending = false;
    cia->runmode_b = false;
    cia->runmode_b_next = false;
    cia->runmode_b_pending = false;

    cia->icr_data = 0;
    cia->icr_mask = 0;
    cia->irq_pending = false;
    cia->irq_delay = 0;
    cia->icr_ack = false;

    cia->tod_10ths = 0;
    cia->tod_sec = 0;
    cia->tod_min = 0;
    cia->tod_hr = 0;
    cia->tod_latched = false;
    cia->tod_divider = 0;

    cia->sdr = 0;
    cia->sdr_bits = 0;
}

// Check and set ICR bit 7 with proper delay
static void check_irq(CIA *cia)
{
    // Check if any enabled interrupt occurred
    // Only trigger if:
    // 1. ICR was not just read (icr_ack is false)
    // 2. There's an enabled interrupt pending
    // 3. Bit 7 is not already set (haven't already triggered the delay)
    // 4. irq_delay is not already pending
    if (!cia->icr_ack &&
        !cia->irq_delay &&
        !(cia->icr_data & CIA_ICR_IR) &&
        (cia->icr_data & cia->icr_mask & 0x1F))
    {
        // Use 1-cycle delay for ICR bit 7 and interrupt triggering
        // The interrupt line goes low on the NEXT cycle after the condition is met
        cia->irq_delay = 1;
    }
}

void cia_clock(CIA *cia)
{
    // Clear icr_ack from previous cycle's read at the START of this cycle
    // This way, a read sets icr_ack to prevent interrupts from that same cycle,
    // but the next cycle starts fresh
    cia->icr_ack = false;
    
    // Advance 2-stage RUNMODE pipeline - the one-shot decision uses the value from 2 cycles ago
    // Stage: runmode_a (used) <- runmode_a_next <- runmode_a_pending (written)
    // This ensures that a RUNMODE write on cycle N doesn't affect underflow until cycle N+2
    cia->runmode_a = cia->runmode_a_next;
    cia->runmode_a_next = cia->runmode_a_pending;
    cia->runmode_b = cia->runmode_b_next;
    cia->runmode_b_next = cia->runmode_b_pending;
    
    // Capture Timer A value for reads (1-cycle delay)
    // CPU reads see the value from the PREVIOUS cycle
    cia->timer_a_read = cia->timer_a;
    
    // Capture Timer B value for reads (1-cycle delay)
    // This is separate from tb_delay which controls counting
    cia->timer_b_read = cia->timer_b;
    
    // Copy previous cycle's underflow to delayed signal (for Timer B cascade)
    // Timer B should see Timer A underflow 1 cycle after it happens
    cia->ta_underflow_delay = cia->ta_underflow;
    
    // Clear Timer A underflow for this cycle
    cia->ta_underflow = false;
    
    // Update delayed output to current value (1-cycle delay for toggle output)
    // This makes reads see the value from the previous cycle
    cia->pb6_out_delayed = cia->pb6_out;
    cia->pb7_out_delayed = cia->pb7_out;
    
    // Update pulse output to current pulse state (1-cycle delay)
    // The pulse is visible to reads for the cycle AFTER underflow occurs
    cia->pb6_pulse_out = cia->pb6_pulse;
    cia->pb7_pulse_out = cia->pb7_pulse;
    
    // Clear internal pulses from previous cycle
    cia->pb6_pulse = false;
    cia->pb7_pulse = false;
    
    // Process pending timer loads with delay counter
    // Load happens when counter reaches 0
    if (cia->ta_load_delay > 0)
    {
        cia->ta_load_delay--;
        if (cia->ta_load_delay == 0)
        {
            cia->timer_a = cia->timer_a_latch;
            cia->timer_a_pb6 = cia->timer_a_latch;
            // When LOAD completes, add 1 cycle delay before timer can count
            // This ensures the loaded value is stable for one cycle
            // We set to 2 because the timer counting logic will decrement it
            // in the same cia_clock() call
            if ((cia->cra & CIA_CR_START) && cia->ta_delay <= 1)
            {
                cia->ta_delay = 2;
            }
        }
    }
    if (cia->tb_load_delay > 0)
    {
        cia->tb_load_delay--;
        if (cia->tb_load_delay == 0)
        {
            cia->timer_b = cia->timer_b_latch;
            cia->timer_b_pb7 = cia->timer_b_latch;
            // When LOAD completes, add 1 cycle delay before timer can count
            // This ensures the loaded value is stable for one cycle
            // We set to 2 because the timer counting logic will decrement it
            // in the same cia_clock() call
            if ((cia->crb & CIA_CR_START) && cia->tb_delay <= 1)
            {
                cia->tb_delay = 2;
            }
        }
    }

    // Process IRQ delay at start of cycle
    if (cia->irq_delay > 0)
    {
        cia->irq_delay--;
        if (cia->irq_delay == 0)
        {
            cia->icr_data |= CIA_ICR_IR;
            cia->irq_pending = true;
            // Both CIA1 and CIA2 trigger their interrupts after the delay
            if (cia->cia_num == 1)
            {
                cia->sys->cpu.irq_pending = true;
                cia->sys->cpu.irq_pending_age = 0;
            }
            else
            {
                cpu_trigger_nmi(&cia->sys->cpu);
            }
        }
    }

    // Timer A
    if (cia->cra & CIA_CR_START)
    {
        bool count = false;

        // Check input mode
        if (!(cia->cra & CIA_CR_INMODE))
        {
            // Count phi2 cycles
            count = true;
        }
        else if (cia->ta_cnt_delay > 0)
        {
            // Mode just switched from phi2 to CNT - continue counting during delay
            cia->ta_cnt_delay--;
            count = true;
        }
        // CNT mode (after delay expires) - not counting

        if (count)
        {
            // Count main timer (for register reads) after ta_delay expires
            if (cia->ta_delay > 0)
            {
                cia->ta_delay--;
            }
            else
            {
                cia->timer_a--;
                if (cia->timer_a == 0xFFFF)
                {
                    // Underflow - reload from latch
                    cia->timer_a = cia->timer_a_latch;

                    // Set underflow signal for cascade mode (Timer B counts Timer A underflows)
                    cia->ta_underflow = true;

                    // Set interrupt flag (based on main timer)
                    cia->icr_data |= CIA_ICR_TA;
                    check_irq(cia);

                    // One-shot mode: stop timer
                    // Use OR of current CRA and pipelined RUNMODE:
                    // - SET one-shot takes effect immediately (current CRA)
                    // - CLR one-shot is delayed (pipelined value preserves old state)
                    bool oneshot = (cia->cra & CIA_CR_RUNMODE) || cia->runmode_a;
                    if (oneshot)
                    {
                        cia->cra &= ~CIA_CR_START;
                    }
                }
            }
            
            // Count PB6 shadow timer (for pulse output) after pb6_delay expires
            if (cia->pb6_delay > 0)
            {
                cia->pb6_delay--;
            }
            else
            {
                cia->timer_a_pb6--;
                if (cia->timer_a_pb6 == 0xFFFF)
                {
                    // PB6 underflow
                    cia->timer_a_pb6 = cia->timer_a_latch;
                    
                    // Timer output to PB6
                    cia->pb6_out = !cia->pb6_out;
                
                    // In pulse mode, set the pulse flag (visible next cycle)
                    if (!(cia->cra & CIA_CR_OUTMODE))
                    {
                        cia->pb6_pulse = true;
                    }
                }
            }
        }
    }

    // Timer B
    if (cia->crb & CIA_CR_START)
    {
        // Check if we should count (based on input mode)
        bool count = false;
        u8 inmode = (cia->crb >> 5) & 0x03;

        switch (inmode)
        {
        case 0: // phi2
            count = true;
            break;
        case 1: // CNT (not implemented - would need CNT pin input)
            break;
        case 2: // Timer A underflow
            // Use delayed underflow signal (1 cycle after TA underflows)
            // This matches real hardware behavior where cascade has 1-cycle delay
            if (cia->ta_underflow_delay)
            {
                count = true;
            }
            break;
        case 3: // Timer A underflow while CNT high
            // CNT line is high by default (pulled high externally)
            // Use delayed underflow signal for cascade timing
            if (cia->ta_underflow_delay)
            {
                count = true;
            }
            break;
        }

        if (count)
        {
            // Count main timer (for register reads)
            // tb_delay only applies to phi2 mode, not cascade mode
            bool can_count_main = (inmode != 0) || (cia->tb_delay == 0);
            
            if (cia->tb_delay > 0 && inmode == 0)
            {
                cia->tb_delay--;
            }
            
            if (can_count_main)
            {
                cia->timer_b--;
                if (cia->timer_b == 0xFFFF)
                {
                    // Underflow (wrapped from 0 to 0xFFFF)
                    cia->timer_b = cia->timer_b_latch;
                    
                    // In cascade mode, update timer_b_read immediately so reads
                    // see the reloaded value on the same cycle as underflow
                    if (inmode != 0)
                    {
                        cia->timer_b_read = cia->timer_b_latch;
                    }
                    
                    // ICR flag is set based on main timer underflow
                    cia->icr_data |= CIA_ICR_TB;
                    check_irq(cia);

                    // One-shot mode: stop timer
                    // Use OR of current CRB and pipelined RUNMODE:
                    // - SET one-shot takes effect immediately (current CRB)
                    // - CLR one-shot is delayed (pipelined value preserves old state)
                    bool oneshot = (cia->crb & CIA_CR_RUNMODE) || cia->runmode_b;
                    if (oneshot)
                    {
                        cia->crb &= ~CIA_CR_START;
                    }
                }
            }
            
            // Count PB7 shadow timer (for pulse output)
            // pb7_delay only applies to phi2 mode, not cascade mode
            bool can_count_pb7 = (inmode != 0) || (cia->pb7_delay == 0);
            
            if (cia->pb7_delay > 0 && inmode == 0)
            {
                cia->pb7_delay--;
            }
            
            if (can_count_pb7)
            {
                cia->timer_b_pb7--;
                if (cia->timer_b_pb7 == 0xFFFF)
                {
                    // PB7 underflow
                    cia->timer_b_pb7 = cia->timer_b_latch;
                    
                    // Timer output to PB7
                    // The toggle flip-flop ALWAYS toggles on underflow
                    cia->pb7_out = !cia->pb7_out;
                    
                    // In cascade mode, update pb7_out_delayed immediately so reads
                    // see the toggled value on the same cycle as underflow
                    if (inmode != 0)
                    {
                        cia->pb7_out_delayed = cia->pb7_out;
                    }
                    
                    // In pulse mode, set the pulse flag (visible next cycle)
                    if (!(cia->crb & CIA_CR_OUTMODE))
                    {
                        cia->pb7_pulse = true;
                    }
                }
            }
        }
    }

    // Process pending stop operations after timer counting
    // This allows the timer to count more cycles before stopping
    if (cia->ta_stop_delay > 0)
    {
        cia->ta_stop_delay--;
        if (cia->ta_stop_delay == 0)
        {
            cia->cra &= ~CIA_CR_START;
        }
    }
    if (cia->tb_stop_delay > 0)
    {
        cia->tb_stop_delay--;
        if (cia->tb_stop_delay == 0)
        {
            cia->crb &= ~CIA_CR_START;
        }
    }

    // TOD clock (runs at ~10Hz)
    cia->tod_divider++;
    if (cia->tod_divider >= (C64_CPU_FREQ / 10))
    {
        cia->tod_divider = 0;

        // Increment 1/10 seconds
        cia->tod_10ths++;
        if (cia->tod_10ths >= 10)
        {
            cia->tod_10ths = 0;

            // Increment seconds (BCD)
            u8 sec_lo = (cia->tod_sec & 0x0F) + 1;
            u8 sec_hi = (cia->tod_sec >> 4);
            if (sec_lo >= 10)
            {
                sec_lo = 0;
                sec_hi++;
            }
            if (sec_hi >= 6)
            {
                sec_hi = 0;

                // Increment minutes
                u8 min_lo = (cia->tod_min & 0x0F) + 1;
                u8 min_hi = (cia->tod_min >> 4);
                if (min_lo >= 10)
                {
                    min_lo = 0;
                    min_hi++;
                }
                if (min_hi >= 6)
                {
                    min_hi = 0;

                    // Increment hours
                    u8 hr = cia->tod_hr & 0x1F;
                    bool pm = cia->tod_hr & 0x80;

                    u8 hr_lo = (hr & 0x0F) + 1;
                    u8 hr_hi = (hr >> 4);
                    if (hr_lo >= 10)
                    {
                        hr_lo = 0;
                        hr_hi++;
                    }
                    hr = (hr_hi << 4) | hr_lo;

                    if (hr >= 0x12)
                    {
                        hr = 0;
                        pm = !pm;
                    }

                    cia->tod_hr = hr | (pm ? 0x80 : 0);
                }
                cia->tod_min = (min_hi << 4) | min_lo;
            }
            cia->tod_sec = (sec_hi << 4) | sec_lo;
        }

        // Check alarm
        if (cia->tod_10ths == cia->alarm_10ths &&
            cia->tod_sec == cia->alarm_sec &&
            cia->tod_min == cia->alarm_min &&
            cia->tod_hr == cia->alarm_hr)
        {
            cia->icr_data |= CIA_ICR_TOD;
            check_irq(cia);
        }
    }
}

u8 cia_read(CIA *cia, u8 reg)
{
    switch (reg)
    {
    case CIA_PRA:
        if (cia->cia_num == 1)
        {
            // CIA1 Port A: Keyboard columns / Joystick 2
            return cia_read_keyboard(cia);
        }
        else
        {
            // CIA2 Port A: VIC bank, serial bus
            return (cia->pra & cia->ddra) | (~cia->ddra & 0xFF);
        }

    case CIA_PRB:
    {
        u8 result;
        if (cia->cia_num == 1)
        {
            // CIA1 Port B: Keyboard rows / Joystick 1
            // Output bits read back the PRB value, input bits read external state
            // When no keys pressed, external state is 0xFF
            result = (cia->prb & cia->ddrb) | (~cia->ddrb & 0xFF);
        }
        else
        {
            // CIA2 Port B: User port
            result = (cia->prb & cia->ddrb) | (~cia->ddrb & 0xFF);
        }
        
        // Timer A output to PB6 (when PBON is set)
        // OUTMODE=0: Pulse mode (LOW normally, HIGH for 1 cycle on underflow)
        // OUTMODE=1: Toggle mode (toggles on each underflow)
        if (cia->cra & CIA_CR_PBON)
        {
            result &= ~0x40; // Clear PB6
            if (cia->cra & CIA_CR_OUTMODE)
            {
                // Toggle mode: use delayed toggle state (1-cycle delay)
                if (cia->pb6_out_delayed)
                    result |= 0x40;
            }
            else
            {
                // Pulse mode: LOW normally, HIGH during pulse
                // pulse_out = true means we're in the HIGH pulse period
                if (cia->pb6_pulse_out)
                    result |= 0x40;
            }
        }
        
        // Timer B output to PB7 (when PBON is set)
        // OUTMODE=0: Pulse mode, OUTMODE=1: Toggle mode
        if (cia->crb & CIA_CR_PBON)
        {
            result &= ~0x80; // Clear PB7
            if (cia->crb & CIA_CR_OUTMODE)
            {
                // Toggle mode: use delayed toggle state (1-cycle delay)
                if (cia->pb7_out_delayed)
                    result |= 0x80;
            }
            else
            {
                // Pulse mode: LOW normally, HIGH during pulse
                if (cia->pb7_pulse_out)
                    result |= 0x80;
            }
        }
        
        return result;
    }

    case CIA_DDRA:
        return cia->ddra;

    case CIA_DDRB:
        return cia->ddrb;

    case CIA_TALO:
    {
        // Use captured value from start of cycle (1-cycle delay for reads)
        // When timer is 0 and running, return latch value instead
        u16 value = cia->timer_a_read;
        if (value == 0 && (cia->cra & CIA_CR_START))
        {
            value = cia->timer_a_latch;
        }
        return value & 0xFF;
    }

    case CIA_TAHI:
    {
        u16 value = cia->timer_a_read;
        if (value == 0 && (cia->cra & CIA_CR_START))
        {
            value = cia->timer_a_latch;
        }
        return (value >> 8) & 0xFF;
    }

    case CIA_TBLO:
    {
        // Timer B read behavior depends on input mode
        // Phi2 mode (inmode=0): tb_delay controls counting, read current value
        // Cascade mode (inmode=2): immediate counting, read captured (delayed) value
        u8 inmode = (cia->crb >> 5) & 0x03;
        u16 value = (inmode == 0) ? cia->timer_b : cia->timer_b_read;
        // In phi2 mode, timer reloads immediately so reads never see 0
        // In cascade mode with delayed reads, we CAN see 0 (it's 1 cycle behind)
        if (inmode == 0 && value == 0 && (cia->crb & CIA_CR_START))
        {
            value = cia->timer_b_latch;
        }
        return value & 0xFF;
    }

    case CIA_TBHI:
    {
        u8 inmode = (cia->crb >> 5) & 0x03;
        u16 value = (inmode == 0) ? cia->timer_b : cia->timer_b_read;
        // In phi2 mode, timer reloads immediately so reads never see 0
        // In cascade mode with delayed reads, we CAN see 0 (it's 1 cycle behind)
        if (inmode == 0 && value == 0 && (cia->crb & CIA_CR_START))
        {
            value = cia->timer_b_latch;
        }
        return (value >> 8) & 0xFF;
    }

    case CIA_TOD_10:
    {
        u8 result = cia->tod_latched ? cia->tod_latch_10ths : cia->tod_10ths;
        cia->tod_latched = false; // Unlatch after reading 10ths
        return result;
    }

    case CIA_TOD_S:
        return cia->tod_latched ? cia->tod_latch_sec : cia->tod_sec;

    case CIA_TOD_M:
        return cia->tod_latched ? cia->tod_latch_min : cia->tod_min;

    case CIA_TOD_H:
        // Reading hours latches TOD
        if (!cia->tod_latched)
        {
            cia->tod_latch_10ths = cia->tod_10ths;
            cia->tod_latch_sec = cia->tod_sec;
            cia->tod_latch_min = cia->tod_min;
            cia->tod_latch_hr = cia->tod_hr;
            cia->tod_latched = true;
        }
        return cia->tod_latch_hr;

    case CIA_SDR:
        return cia->sdr;

    case CIA_ICR:
    {
        u8 result = cia->icr_data;
        // Reading ICR clears it and sets acknowledgement flag
        cia->icr_data = 0;
        cia->irq_pending = false;
        // CIA1 clears IRQ, CIA2 clears NMI
        if (cia->cia_num == 1)
        {
            cia->sys->cpu.irq_pending = false;
        }
        else
        {
            // If NMI was pending (bit 7 set) AND we actually have an NMI pending,
            // preserve that fact so it still fires at the end of this instruction.
            // Don't set this if NMI was already taken (nmi_pending already false).
            if ((result & 0x80) && cia->sys->cpu.nmi_pending)
            {
                cia->sys->cpu.nmi_triggered_this_insn = true;
            }
            cia->sys->cpu.nmi_pending = false;
            cia->sys->cpu.nmi_edge = false; // Allow new NMI edge
        }
        cia->irq_delay = 0;
        cia->icr_ack = true; // Inhibit irq_delay for rest of this cycle
        return result;
    }

    case CIA_CRA:
        return cia->cra;

    case CIA_CRB:
        return cia->crb;

    default:
        return 0xFF;
    }
}

void cia_write(CIA *cia, u8 reg, u8 value)
{
    switch (reg)
    {
    case CIA_PRA:
        cia->pra = value;
        break;

    case CIA_PRB:
        cia->prb = value;
        break;

    case CIA_DDRA:
        cia->ddra = value;
        break;

    case CIA_DDRB:
        cia->ddrb = value;
        break;

    case CIA_TALO:
        cia->timer_a_latch = (cia->timer_a_latch & 0xFF00) | value;
        break;

    case CIA_TAHI:
        cia->timer_a_latch = (cia->timer_a_latch & 0x00FF) | (value << 8);
        // If timer not running, writing high byte also loads timer
        if (!(cia->cra & CIA_CR_START))
        {
            cia->timer_a = cia->timer_a_latch;
            cia->timer_a_pb6 = cia->timer_a_latch;
            cia->timer_a_read = cia->timer_a_latch;
        }
        break;

    case CIA_TBLO:
        cia->timer_b_latch = (cia->timer_b_latch & 0xFF00) | value;
        break;

    case CIA_TBHI:
        cia->timer_b_latch = (cia->timer_b_latch & 0x00FF) | (value << 8);
        if (!(cia->crb & CIA_CR_START))
        {
            cia->timer_b = cia->timer_b_latch;
            cia->timer_b_pb7 = cia->timer_b_latch;
            cia->timer_b_read = cia->timer_b_latch;
        }
        break;

    case CIA_TOD_10:
        if (cia->crb & CIA_CR_TODIN)
        {
            cia->alarm_10ths = value;
        }
        else
        {
            cia->tod_10ths = value & 0x0F;
        }
        break;

    case CIA_TOD_S:
        if (cia->crb & CIA_CR_TODIN)
        {
            cia->alarm_sec = value;
        }
        else
        {
            cia->tod_sec = value;
        }
        break;

    case CIA_TOD_M:
        if (cia->crb & CIA_CR_TODIN)
        {
            cia->alarm_min = value;
        }
        else
        {
            cia->tod_min = value;
        }
        break;

    case CIA_TOD_H:
        if (cia->crb & CIA_CR_TODIN)
        {
            cia->alarm_hr = value;
        }
        else
        {
            cia->tod_hr = value;
        }
        break;

    case CIA_SDR:
        cia->sdr = value;
        break;

    case CIA_ICR:
        // Bit 7: Set or clear mode
        if (value & 0x80)
        {
            // Set bits
            cia->icr_mask |= (value & 0x1F);
        }
        else
        {
            // Clear bits
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

    case CIA_CRA:
    {
        bool was_running = cia->cra & CIA_CR_START;
        bool now_running = value & CIA_CR_START;
        bool force_load = value & CIA_CR_LOAD;
        bool was_cnt_mode = cia->cra & CIA_CR_INMODE;
        bool now_cnt_mode = value & CIA_CR_INMODE;

        // Force load bit - schedule reload from latch
        // Timer A LOAD has a 1-cycle delay before the load takes effect
        if (force_load)
        {
            cia->ta_load_delay = 1;
        }
        
        // Mode switch while timer is running
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

        // Timer starting: set up pipeline delays
        if (!was_running && now_running)
        {
            // Main timer (for register reads) has 1-cycle delay
            if (force_load)
            {
                cia->ta_delay = 1;  // Force load: 1 cycle delay before counting
            }
            else
            {
                cia->ta_delay = 1;
            }
            
            // PB6 shadow timer counts immediately for correct pulse timing (pb6 test)
            cia->pb6_delay = 0;
            
            // Sync the PB6 shadow timer with the main timer
            cia->timer_a_pb6 = cia->timer_a;
            
            // Clear reload skip - fresh start shouldn't inherit old skip state
            cia->ta_reload_skip = false;
            cia->pb6_reload_skip = false;
            
            // When timer starts, the toggle flip-flop is ALWAYS set HIGH
            // This affects toggle mode output; pulse mode uses pb6_pulse instead
            cia->pb6_out = true;
        }
        
        // Note: The toggle flip-flop state is INDEPENDENT of the PBON/OUTMODE bits
        // Switching from pulse to toggle mode does NOT reset the flip-flop

        cia->cra = value & ~CIA_CR_LOAD; // Load bit not stored
        
        // Update RUNMODE 2-stage pipeline - write to pending, takes effect in 2 cycles
        cia->runmode_a_pending = (value & CIA_CR_RUNMODE) != 0;
    }
    break;

    case CIA_CRB:
    {
        bool was_running = cia->crb & CIA_CR_START;
        bool now_running = value & CIA_CR_START;

        // Force load bit - set delay counter for 2-cycle delay
        // The timer is reloaded after 2 cycles
        bool force_load_b = value & CIA_CR_LOAD;
        if (force_load_b)
        {
            cia->tb_load_delay = 2;
        }

        // Timer starting: add pipeline delays
        // Register reads need 2-cycle delay (tb123 test)
        // Pulse output needs immediate counting (pb7 test)
        if (!was_running && now_running)
        {
            // Timer B register always has 2-cycle delay for proper read timing
            cia->tb_delay = 2;
            // Pulse output (PB7) counts immediately for correct pulse timing
            cia->pb7_delay = 0;
            
            // Sync the PB7 shadow timer with the main timer
            cia->timer_b_pb7 = cia->timer_b;
            
            // When timer starts, the toggle flip-flop is ALWAYS set HIGH
            // This affects toggle mode output; pulse mode uses pb7_pulse instead
            cia->pb7_out = true;
        }
        
        // Note: The toggle flip-flop state is INDEPENDENT of the PBON/OUTMODE bits
        // Switching from pulse to toggle mode does NOT reset the flip-flop

        // When stopping a running timer, use delayed stop
        // The timer will count 2 more cycles before actually stopping
        if (was_running && !now_running)
        {
            // Set stop delay - the actual stop happens after 2 cycles
            cia->tb_stop_delay = 2;
            // Don't clear START bit yet - it will be cleared by stop_delay processing
            cia->crb = (value | CIA_CR_START) & ~CIA_CR_LOAD;
        }
        else
        {
            cia->crb = value & ~CIA_CR_LOAD;
        }
        
        // Update RUNMODE 2-stage pipeline - write to pending, takes effect in 2 cycles
        cia->runmode_b_pending = (value & CIA_CR_RUNMODE) != 0;
    }
    break;
    }
}

void cia_set_key(CIA *cia, int row, int col, bool pressed)
{
    (void)cia; // Uses global keyboard matrix
    if (row < 0 || row > 7 || col < 0 || col > 7)
        return;

    if (pressed)
    {
        keyboard_matrix[row] &= ~(1 << col);
    }
    else
    {
        keyboard_matrix[row] |= (1 << col);
    }
}

u8 cia_read_keyboard(CIA *cia)
{
    // Port A is keyboard columns (directly from matrix when scanning)
    // Port B selects which row to read

    u8 cols = 0xFF;
    u8 rows = ~(cia->prb & cia->ddrb); // Active low

    for (int r = 0; r < 8; r++)
    {
        if (rows & (1 << r))
        {
            cols &= keyboard_matrix[r];
        }
    }

    return cols;
}
