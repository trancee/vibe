/**
 * cia.c - CIA 6526 chip emulation
 *
 * Implements two CIA chips with timers, TOD clock,
 * keyboard matrix scanning, and serial port.
 */

#include "cia.h"
#include "c64.h"
#include <stdio.h>
#include <string.h>

// Keyboard matrix state (global for simplicity)
static u8 keyboard_matrix[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void cia_init(C64Cia *cia, int cia_num, C64System *sys)
{
    memset(cia, 0, sizeof(C64Cia));
    cia->cia_num = cia_num;
    cia->sys = sys;
}

void cia_reset(C64Cia *cia)
{
    cia->pra = 0;
    cia->prb = 0;
    cia->ddra = 0;
    cia->ddrb = 0;

    cia->timer_a = 0xFFFF;
    cia->timer_b = 0xFFFF;
    cia->timer_a_latch = 0xFFFF;
    cia->timer_b_latch = 0xFFFF;
    cia->timer_b_pb7 = 0xFFFF;

    cia->ta_delay = 0;
    cia->tb_delay = 0;
    cia->pb7_delay = 0;
    cia->ta_started = false;
    cia->tb_started = false;
    cia->ta_load_delay = 0;
    cia->tb_load_delay = 0;
    cia->ta_stop_delay = 0;
    cia->tb_stop_delay = 0;
    
    cia->pb6_out = true;
    cia->pb7_out = true;
    cia->pb6_out_delayed = true;
    cia->pb7_out_delayed = true;
    cia->pb6_pulse = false;
    cia->pb7_pulse = false;
    cia->pb6_pulse_out = false;
    cia->pb7_pulse_out = false;

    cia->cra = 0;
    cia->crb = 0;

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
static void check_irq(C64Cia *cia)
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

void cia_clock(C64Cia *cia)
{
    // Update delayed output to current value (1-cycle delay for toggle output)
    // This makes reads see the value from the previous cycle
    cia->pb6_out_delayed = cia->pb6_out;
    cia->pb7_out_delayed = cia->pb7_out;
    
    // Update pulse output to current pulse state (1-cycle delay)
    // The pulse is visible to reads for the cycle AFTER underflow occurs
    cia->pb6_pulse_out = cia->pb6_pulse;
    cia->pb7_pulse_out = cia->pb7_pulse;
    
    // Clear internal pulses from the previous cycle (pulse mode = high for one cycle only)
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
        // Handle pipeline delay when timer starts
        if (cia->ta_delay > 0)
        {
            cia->ta_delay--;
        }
        else
        {
            bool count = false;

            // Check input mode
            if (!(cia->cra & CIA_CR_INMODE))
            {
                // Count phi2 cycles
                count = true;
            }
            // CNT mode not implemented

            if (count)
            {
                cia->timer_a--;
                if (cia->timer_a == 0xFFFF)
                {
                    // Underflow (wrapped from 0 to 0xFFFF)
                    cia->timer_a = cia->timer_a_latch;

                    // Set interrupt flag
                    cia->icr_data |= CIA_ICR_TA;
                    check_irq(cia);
                    
                    // Timer output to PB6
                    // The toggle flip-flop ALWAYS toggles on underflow, regardless of output mode
                    // OUTMODE only affects how the output is displayed (toggle vs pulse)
                    cia->pb6_out = !cia->pb6_out;
                    
                    // In pulse mode, also set the pulse flag
                    if (!(cia->cra & CIA_CR_OUTMODE))
                    {
                        cia->pb6_pulse = true;
                    }

                    // One-shot mode: stop timer
                    if (cia->cra & CIA_CR_RUNMODE)
                    {
                        cia->cra &= ~CIA_CR_START;
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
        case 1: // CNT (not implemented)
            break;
        case 2: // Timer A underflow
            if (cia->icr_data & CIA_ICR_TA)
            {
                count = true;
            }
            break;
        case 3: // Timer A underflow while CNT high (not implemented)
            break;
        }

        if (count)
        {
            // Count main timer (for register reads) after tb_delay expires
            // Check delay BEFORE decrementing so delay=2 means skip 2 cycles
            if (cia->tb_delay > 0)
            {
                cia->tb_delay--;
            }
            else
            {
                cia->timer_b--;
                if (cia->timer_b == 0xFFFF)
                {
                    // Underflow (wrapped from 0 to 0xFFFF)
                    cia->timer_b = cia->timer_b_latch;
                    
                    // ICR flag is set based on main timer underflow
                    cia->icr_data |= CIA_ICR_TB;
                    check_irq(cia);

                    if (cia->crb & CIA_CR_RUNMODE)
                    {
                        cia->crb &= ~CIA_CR_START;
                    }
                }
            }
            
            // Count PB7 shadow timer (for pulse output) after pb7_delay expires
            if (cia->pb7_delay > 0)
            {
                cia->pb7_delay--;
            }
            else
            {
                cia->timer_b_pb7--;
                if (cia->timer_b_pb7 == 0xFFFF)
                {
                    // PB7 underflow
                    cia->timer_b_pb7 = cia->timer_b_latch;
                    
                    // Timer output to PB7
                    // The toggle flip-flop ALWAYS toggles on underflow
                    cia->pb7_out = !cia->pb7_out;
                    
                    // In pulse mode, also set the pulse flag
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

    // Clear icr_ack at end of cycle (after all interrupt sources processed)
    cia->icr_ack = false;
}

u8 cia_read(C64Cia *cia, u8 reg)
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
        // OUTMODE=0: Pulse mode, OUTMODE=1: Toggle mode
        if (cia->cra & CIA_CR_PBON)
        {
            result &= ~0x40; // Clear PB6
            if (cia->cra & CIA_CR_OUTMODE)
            {
                // Toggle mode: use current toggle state
                if (cia->pb6_out)
                    result |= 0x40;
            }
            else
            {
                // Pulse mode: high during underflow cycle (direct read)
                if (cia->pb6_pulse)
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
                // Toggle mode: use current toggle state
                if (cia->pb7_out)
                    result |= 0x80;
            }
            else
            {
                // Pulse mode: high during underflow cycle (direct read like Timer A)
                if (cia->pb7_pulse)
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
        return cia->timer_a & 0xFF;

    case CIA_TAHI:
        return (cia->timer_a >> 8) & 0xFF;

    case CIA_TBLO:
        return cia->timer_b & 0xFF;

    case CIA_TBHI:
        return (cia->timer_b >> 8) & 0xFF;

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
        // CIA1 clears IRQ, CIA2 clears NMI
        if (cia->cia_num == 1)
        {
            cia->sys->cpu.irq_pending = false;
        }
        else
        {
            cia->sys->cpu.nmi_pending = false;
            cia->sys->cpu.nmi_edge = false; // Allow new NMI edge
        }
        cia->irq_delay = 0;
        cia->icr_ack = true; // Inhibit irq_delay for next cycle
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

void cia_write(C64Cia *cia, u8 reg, u8 value)
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
        if (cia->icr_data & cia->icr_mask & 0x1F)
        {
            cia->icr_data |= CIA_ICR_IR;
            // CIA1 triggers IRQ, CIA2 triggers NMI
            if (cia->cia_num == 1)
            {
                cia->sys->cpu.irq_pending = true;
            }
            else
            {
                cpu_trigger_nmi(&cia->sys->cpu);
            }
        }
        break;

    case CIA_CRA:
    {
        bool was_running = cia->cra & CIA_CR_START;
        bool now_running = value & CIA_CR_START;
        bool force_load = value & CIA_CR_LOAD;
        bool was_toggle = cia->cra & CIA_CR_OUTMODE;
        bool now_toggle = value & CIA_CR_OUTMODE;

        // Force load bit - immediately reload timer from latch
        // Note: Timer A LOAD is immediate, unlike Timer B which has pipeline delay
        if (force_load)
        {
            cia->timer_a = cia->timer_a_latch;
        }

        // Timer starting: add pipeline delay
        // When starting with LOAD bit set, delay is 2 cycles (load + start)
        // When starting without LOAD bit, delay is 1 cycle
        if (!was_running && now_running)
        {
            cia->ta_delay = force_load ? 2 : 1;
            
            // When timer starts, the toggle flip-flop is ALWAYS set HIGH
            // This affects toggle mode output; pulse mode uses pb6_pulse instead
            cia->pb6_out = true;
        }
        
        // Note: The toggle flip-flop state is INDEPENDENT of the PBON/OUTMODE bits
        // Switching from pulse to toggle mode does NOT reset the flip-flop

        cia->cra = value & ~CIA_CR_LOAD; // Load bit not stored
    }
    break;

    case CIA_CRB:
    {
        bool was_running = cia->crb & CIA_CR_START;
        bool now_running = value & CIA_CR_START;
        bool was_toggle = cia->crb & CIA_CR_OUTMODE;
        bool now_toggle = value & CIA_CR_OUTMODE;

        // Force load bit - set delay counter for 2-cycle delay
        // The timer is reloaded after 2 cycles
        bool force_load_b = value & CIA_CR_LOAD;
        if (force_load_b)
        {
            cia->tb_load_delay = 2;
        }

        // Timer starting: add pipeline delay
        // Timer B register uses 2-cycle delay (for cia1tb123 timer read test)
        // PB7 output uses 1-cycle delay (for cia1pb7 timing test)
        if (!was_running && now_running)
        {
            cia->tb_delay = 2;
            cia->pb7_delay = 1;
            
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
    }
    break;
    }
}

void cia_set_key(C64Cia *cia, int row, int col, bool pressed)
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

u8 cia_read_keyboard(C64Cia *cia)
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
