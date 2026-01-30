/*
 * C64 Emulator - CIA 6526 Implementation
 *
 * Implements:
 * - Two 16-bit timers with various countdown modes
 * - Time of Day clock (BCD)
 * - Keyboard matrix scanning (CIA1)
 * - Interrupt generation
 */

#include <string.h>
#include <stdio.h>
#include "cia6526.h"

static int g_clock_count = 0;

void cia_init(CIA *cia, uint16_t base_addr)
{
    memset(cia, 0, sizeof(CIA));
    cia->base_addr = base_addr;
    cia_reset(cia);
}

void cia_reset(CIA *cia)
{
    /* Port registers - for CIA1, DDRA/DDRB default to 0xFF (output) */
    cia->pra = 0xFF;
    cia->prb = 0xFF;
    cia->ddra = 0xFF; /* All outputs (CIA1 default) */
    cia->ddrb = 0xFF;

    /* Timers */
    cia->ta_counter = 0xFFFF;
    cia->ta_latch = 0xFFFF;
    cia->ta_running = false;
    cia->ta_underflow = false;
    cia->ta_toggle = false;
    cia->ta_delay = 0;

    cia->tb_counter = 0xFFFF;
    cia->tb_latch = 0xFFFF;
    cia->tb_running = false;
    cia->tb_underflow = false;
    cia->tb_toggle = false;
    cia->tb_delay = 0;

    /* Time of Day */
    cia->tod_10ths = 0;
    cia->tod_sec = 0;
    cia->tod_min = 0;
    cia->tod_hr = 1; /* 1:00:00.0 AM */
    cia->alarm_10ths = 0;
    cia->alarm_sec = 0;
    cia->alarm_min = 0;
    cia->alarm_hr = 0;
    cia->tod_latched = false;
    cia->tod_halted = false;
    cia->tod_running = true; /* TOD starts running */
    cia->tod_50hz = true;    /* Default to 50Hz (PAL) */
    cia->tod_tick_count = 0;

    /* Serial port */
    cia->sdr = 0;
    cia->sr_bits = 0;
    cia->serial_output_enabled = false;

    /* Interrupts */
    cia->icr_data = 0;
    cia->icr_mask = 0;
    cia->icr_new = 0;
    cia->irq_pending = false;

    /* Control registers */
    cia->cra = 0;
    cia->crb = 0;

    /* Keyboard */
    memset(cia->keyboard, 0xFF, sizeof(cia->keyboard));
}

/* Read keyboard matrix (CIA1 specific) */
static uint8_t read_keyboard(CIA *cia)
{
    uint8_t result = 0xFF;
    uint8_t col_select = ~cia->pra; /* Columns selected by PRA output */

    for (int col = 0; col < 8; col++)
    {
        if (col_select & (1 << col))
        {
            result &= cia->keyboard[col];
        }
    }

    return result;
}

uint8_t cia_read(CIA *cia, uint16_t addr)
{
    uint8_t reg = addr & 0x0F;

    switch (reg)
    {
    case CIA_PRA:
        /* Output bits from port register, input bits from callback or external */
        if (cia->port_a_read)
        {
            uint8_t external = cia->port_a_read(cia->io_context);
            return (cia->pra & cia->ddra) | (external & ~cia->ddra);
        }
        return (cia->pra | ~cia->ddra) & 0xFF;

    case CIA_PRB:
        /* Output bits from port register, input bits from callback or keyboard */
        if (cia->port_b_read)
        {
            uint8_t external = cia->port_b_read(cia->io_context);
            return (cia->prb & cia->ddrb) | (external & ~cia->ddrb);
        }
        if (cia->base_addr == CIA1_BASE)
        {
            return read_keyboard(cia);
        }
        return (cia->prb | ~cia->ddrb) & 0xFF;

    case CIA_DDRA:
        return cia->ddra;

    case CIA_DDRB:
        return cia->ddrb;

    case CIA_TA_LO:
        return cia->ta_counter & 0xFF;

    case CIA_TA_HI:
        return (cia->ta_counter >> 8) & 0xFF;

    case CIA_TB_LO:
        return cia->tb_counter & 0xFF;

    case CIA_TB_HI:
        return (cia->tb_counter >> 8) & 0xFF;

    case CIA_TOD_10THS:
        /* Reading 10ths unlatches TOD */
        if (cia->tod_latched)
        {
            cia->tod_latched = false;
            return cia->tod_latch_10ths;
        }
        return cia->tod_10ths;

    case CIA_TOD_SEC:
        if (cia->tod_latched)
        {
            return cia->tod_latch_sec;
        }
        return cia->tod_sec;

    case CIA_TOD_MIN:
        if (cia->tod_latched)
        {
            return cia->tod_latch_min;
        }
        return cia->tod_min;

    case CIA_TOD_HR:
        /* Reading hours latches TOD */
        if (!cia->tod_latched)
        {
            cia->tod_latched = true;
            cia->tod_latch_10ths = cia->tod_10ths;
            cia->tod_latch_sec = cia->tod_sec;
            cia->tod_latch_min = cia->tod_min;
            cia->tod_latch_hr = cia->tod_hr;
        }
        return cia->tod_latch_hr;

    case CIA_SDR:
        return cia->sdr;

    case CIA_ICR:
    {
        /* Reading ICR clears it and acknowledges interrupts */
        uint8_t result = cia->icr_data;
        /* Bit 7 reflects the IRQ line state (has 1-cycle delay after flag) */
        if (cia->irq_pending)
        {
            result |= CIA_ICR_IR;
        }
        cia->icr_data = 0;
        cia->irq_pending = false;
        return result;
    }

    case CIA_CRA:
        return cia->cra;

    case CIA_CRB:
        return cia->crb;
    }

    return 0xFF;
}

void cia_write(CIA *cia, uint16_t addr, uint8_t data)
{
    uint8_t reg = addr & 0x0F;

    switch (reg)
    {
    case CIA_PRA:
        cia->pra = data;
        if (cia->port_a_write)
        {
            /* Only output bits (where DDR=1) affect external devices */
            cia->port_a_write(cia->io_context, data & cia->ddra);
        }
        break;

    case CIA_PRB:
        cia->prb = data;
        if (cia->port_b_write)
        {
            /* Only output bits (where DDR=1) affect external devices */
            cia->port_b_write(cia->io_context, data & cia->ddrb);
        }
        break;

    case CIA_DDRA:
        cia->ddra = data;
        break;

    case CIA_DDRB:
        cia->ddrb = data;
        break;

    case CIA_TA_LO:
        cia->ta_latch = (cia->ta_latch & 0xFF00) | data;
        break;

    case CIA_TA_HI:
        cia->ta_latch = (cia->ta_latch & 0x00FF) | (data << 8);
        /* If timer not running, load counter immediately */
        if (!(cia->cra & CIA_CRA_START))
        {
            cia->ta_counter = cia->ta_latch;
        }
        break;

    case CIA_TB_LO:
        cia->tb_latch = (cia->tb_latch & 0xFF00) | data;
        break;

    case CIA_TB_HI:
        cia->tb_latch = (cia->tb_latch & 0x00FF) | (data << 8);
        /* If timer not running, load counter immediately */
        if (!(cia->crb & CIA_CRB_START))
        {
            cia->tb_counter = cia->tb_latch;
        }
        break;

    case CIA_TOD_10THS:
        if (cia->crb & CIA_CRB_ALARM)
        {
            cia->alarm_10ths = data & 0x0F;
        }
        else
        {
            cia->tod_10ths = data & 0x0F;
            cia->tod_halted = false; /* Writing 10ths starts TOD */
        }
        break;

    case CIA_TOD_SEC:
        if (cia->crb & CIA_CRB_ALARM)
        {
            cia->alarm_sec = data & 0x7F;
        }
        else
        {
            cia->tod_sec = data & 0x7F;
        }
        break;

    case CIA_TOD_MIN:
        if (cia->crb & CIA_CRB_ALARM)
        {
            cia->alarm_min = data & 0x7F;
        }
        else
        {
            cia->tod_min = data & 0x7F;
        }
        break;

    case CIA_TOD_HR:
        if (cia->crb & CIA_CRB_ALARM)
        {
            cia->alarm_hr = data & 0x9F;
        }
        else
        {
            cia->tod_hr = data & 0x9F;
            cia->tod_halted = true; /* Writing hours halts TOD */
        }
        break;

    case CIA_SDR:
        cia->sdr = data;
        if (cia->cra & CIA_CRA_SPMODE)
        {
            /* Output mode: start serial transmission */
            cia->sr_bits = 8;
        }
        break;

    case CIA_ICR:
        if (data & CIA_ICR_IR)
        {
            /* Bit 7 set: Set bits specified by bits 0-4 */
            cia->icr_mask |= (data & 0x1F);
        }
        else
        {
            /* Bit 7 clear: Clear bits specified by bits 0-4 */
            cia->icr_mask &= ~(data & 0x1F);
        }
        /* Check if any newly enabled interrupt is pending */
        if (cia->icr_data & cia->icr_mask)
        {
            cia->irq_pending = true;
        }
        break;

    case CIA_CRA:
    {
        bool was_running = cia->ta_running;
        cia->cra = data; /* Store full value for test compatibility */
        bool should_run = (data & CIA_CRA_START) != 0;

        /* TODIN bit controls 50/60Hz mode */
        cia->tod_50hz = (data & CIA_CR_TODIN) != 0;

        /* Force load */
        if (data & CIA_CRA_LOAD)
        {
            cia->ta_counter = cia->ta_latch;
        }

        /* Starting timer from stopped state - set delay for pipeline */
        if (!was_running && should_run)
        {
            /* Timer has 4-cycle pipeline delay before counting */
            cia->ta_delay = 4;
        }

        cia->ta_running = should_run;
    }
    break;

    case CIA_CRB:
    {
        bool was_running = cia->tb_running;
        cia->crb = data; /* Store full value for test compatibility */
        bool should_run = (data & CIA_CRB_START) != 0;

        /* Force load */
        if (data & CIA_CRB_LOAD)
        {
            cia->tb_counter = cia->tb_latch;
        }

        /* Starting timer from stopped state - set delay for pipeline */
        if (!was_running && should_run)
        {
            /* Timer has 2-cycle pipeline delay before counting */
            cia->tb_delay = 2;
        }

        cia->tb_running = should_run;
    }
    break;
    }
}

void cia_clock(CIA *cia)
{
    if (cia->base_addr == CIA1_BASE)
        g_clock_count++;

    /* Clear underflow flags from previous cycle */
    cia->ta_underflow = false;
    cia->tb_underflow = false;

    /* Timer A */
    if (cia->ta_running)
    {
        /* Handle pipeline delay when timer just started */
        if (cia->ta_delay > 0)
        {
            cia->ta_delay--;
        }
        else
        {
            bool count = false;

            if (cia->cra & CIA_CRA_INMODE)
            {
                /* Count CNT transitions - not implemented, assume phi2 */
                count = true;
            }
            else
            {
                /* Count phi2 pulses */
                count = true;
            }

            if (count)
            {
                if (cia->ta_counter == 0)
                {
                    /* Underflow */
                    cia->ta_underflow = true;
                    cia->icr_data |= CIA_ICR_TA; /* Flag is immediately visible */
                                                 /* Underflow */
                    cia->ta_underflow = true;
                    cia->icr_data |= CIA_ICR_TA; /* Flag is immediately visible */

                    /* Reload from latch */
                    cia->ta_counter = cia->ta_latch;

                    /* Reload from latch */
                    cia->ta_counter = cia->ta_latch;

                    /* Toggle for PB6 output */
                    if (cia->cra & CIA_CRA_PBON)
                    {
                        if (cia->cra & CIA_CRA_OUTMODE)
                        {
                            cia->ta_toggle = !cia->ta_toggle;
                        }
                    }

                    /* One-shot mode: stop timer */
                    if (cia->cra & CIA_CRA_RUNMODE)
                    {
                        cia->ta_running = false;
                        cia->cra &= ~CIA_CRA_START;
                    }
                }
                else
                {
                    cia->ta_counter--;
                }
            }
        }
    }

    /* Timer B */
    if (cia->tb_running)
    {
        /* Handle pipeline delay when timer just started */
        if (cia->tb_delay > 0)
        {
            cia->tb_delay--;
        }
        else
        {
            bool count = false;
            uint8_t mode = cia->crb & (CIA_CRB_INMODE0 | CIA_CRB_INMODE1);

            switch (mode)
            {
            case CIA_TB_COUNT_PHI2:
                count = true;
                break;
            case CIA_TB_COUNT_CNT:
                /* CNT transitions - not implemented */
                count = true;
                break;
            case CIA_TB_COUNT_TA:
                /* Count Timer A underflows */
                count = cia->ta_underflow;
                break;
            case CIA_TB_COUNT_TA_CNT:
                /* Count TA underflows when CNT high */
                count = cia->ta_underflow; /* Simplified */
                break;
            }

            if (count)
            {
                if (cia->tb_counter == 0)
                {
                    /* Underflow */
                    cia->tb_underflow = true;
                    cia->icr_data |= CIA_ICR_TB; /* Flag is immediately visible */

                    /* Reload from latch */
                    cia->tb_counter = cia->tb_latch;

                    /* Toggle for PB7 output */
                    if (cia->crb & CIA_CRB_PBON)
                    {
                        if (cia->crb & CIA_CRB_OUTMODE)
                        {
                            cia->tb_toggle = !cia->tb_toggle;
                        }
                    }

                    /* One-shot mode: stop timer */
                    if (cia->crb & CIA_CRB_RUNMODE)
                    {
                        cia->tb_running = false;
                        cia->crb &= ~CIA_CRB_START;
                    }
                }
                else
                {
                    cia->tb_counter--;
                }
            }
        }
    }
}

/* Called at 50/60 Hz for TOD updates */
void cia_tod_tick(CIA *cia)
{
    if (cia->tod_halted)
        return;

    /* Check alarm BEFORE incrementing (alarm triggers when TOD equals alarm) */
    if (cia->tod_10ths == cia->alarm_10ths &&
        cia->tod_sec == cia->alarm_sec &&
        cia->tod_min == cia->alarm_min &&
        cia->tod_hr == cia->alarm_hr)
    {
        cia->icr_data |= CIA_ICR_ALARM;
        if (cia->icr_mask & CIA_ICR_ALARM)
        {
            cia->irq_pending = true;
        }
    }

    /* Increment 10ths of seconds */
    cia->tod_10ths++;
    if (cia->tod_10ths >= 10)
    {
        cia->tod_10ths = 0;

        /* Increment seconds (BCD) */
        cia->tod_sec++;
        if ((cia->tod_sec & 0x0F) >= 10)
        {
            cia->tod_sec = (cia->tod_sec & 0xF0) + 0x10;
        }
        if (cia->tod_sec >= 0x60)
        {
            cia->tod_sec = 0;

            /* Increment minutes (BCD) */
            cia->tod_min++;
            if ((cia->tod_min & 0x0F) >= 10)
            {
                cia->tod_min = (cia->tod_min & 0xF0) + 0x10;
            }
            if (cia->tod_min >= 0x60)
            {
                cia->tod_min = 0;

                /* Increment hours (BCD, 12-hour format) */
                uint8_t hr = cia->tod_hr & 0x1F;
                bool pm = cia->tod_hr & 0x80;

                hr++;
                if ((hr & 0x0F) >= 10)
                {
                    hr = (hr & 0xF0) + 0x10;
                }
                if (hr >= 0x12)
                {
                    if (hr == 0x12)
                    {
                        pm = !pm;
                    }
                    else
                    {
                        hr = 0x01;
                    }
                }

                cia->tod_hr = hr | (pm ? 0x80 : 0);
            }
        }
    }

    /* Update IRQ pending state from this cycle's interrupts */
    /* Must be AFTER all timer/interrupt logic so icr_data is current */
    if (cia->icr_data & cia->icr_mask)
    {
        cia->irq_pending = true;
    }
}

bool cia_get_irq(CIA *cia)
{
    return cia->irq_pending;
}

void cia_set_key(CIA *cia, int row, int col, bool pressed)
{
    if (row < 0 || row >= 8 || col < 0 || col >= 8)
        return;

    if (pressed)
    {
        cia->keyboard[col] &= ~(1 << row);
    }
    else
    {
        cia->keyboard[col] |= (1 << row);
    }
}

uint8_t cia_get_port_a(CIA *cia)
{
    return (cia->pra & cia->ddra) | (~cia->ddra & 0xFF);
}

uint8_t cia_get_port_b(CIA *cia)
{
    return (cia->prb & cia->ddrb) | (~cia->ddrb & 0xFF);
}

void cia_set_port_a(CIA *cia, uint8_t value)
{
    /* Input bits come from external value */
    cia->pra = (cia->pra & cia->ddra) | (value & ~cia->ddra);
}

void cia_set_port_b(CIA *cia, uint8_t value)
{
    cia->prb = (cia->prb & cia->ddrb) | (value & ~cia->ddrb);
}

void cia_set_io_callbacks(CIA *cia, void *context,
                          cia_port_read_t port_a_read, cia_port_write_t port_a_write,
                          cia_port_read_t port_b_read, cia_port_write_t port_b_write)
{
    cia->io_context = context;
    cia->port_a_read = port_a_read;
    cia->port_a_write = port_a_write;
    cia->port_b_read = port_b_read;
    cia->port_b_write = port_b_write;
}

void cia_clock_serial(CIA *cia)
{
    if (!cia->serial_output_enabled)
        return;

    /* Shift out one bit */
    if (cia->sr_bits > 0)
    {
        cia->sr_bits--;
        if (cia->sr_bits == 0)
        {
            /* Transmission complete - trigger interrupt */
            cia->icr_data |= CIA_ICR_SDR;
            if (cia->icr_mask & CIA_ICR_SDR)
            {
                cia->irq_pending = true;
            }
        }
    }
}

void cia_clock_phi2(CIA *cia)
{
    /* Alias for cia_tod_tick - increments TOD 10ths */
    cia_tod_tick(cia);
}

void cia_tod_set(CIA *cia, uint8_t hr, uint8_t min, uint8_t sec, uint8_t tenths)
{
    cia->tod_hr = hr;
    cia->tod_min = min;
    cia->tod_sec = sec;
    cia->tod_10ths = tenths;
}

void cia_tod_set_alarm(CIA *cia, uint8_t hr, uint8_t min, uint8_t sec, uint8_t tenths)
{
    cia->alarm_hr = hr;
    cia->alarm_min = min;
    cia->alarm_sec = sec;
    cia->alarm_10ths = tenths;
}

uint8_t cia1_joystick_read(CIA *cia, int joy_num)
{
    /* Joystick 1 is on CIA1 Port B, Joystick 2 on Port A */
    /* Return the value read from the appropriate port (bits 0-4 are joystick) */
    if (joy_num == 1)
    {
        /* Joystick 1 on Port B */
        if (cia->port_b_read)
        {
            return cia->port_b_read(cia->io_context) & 0x1F;
        }
        return cia->prb & 0x1F;
    }
    else
    {
        /* Joystick 2 on Port A */
        if (cia->port_a_read)
        {
            return cia->port_a_read(cia->io_context) & 0x1F;
        }
        return cia->pra & 0x1F;
    }
}

void cia2_set_vic_bank(CIA *cia, uint8_t bank)
{
    /* VIC bank is controlled by bits 0-1 of CIA2 Port A (active low) */
    /* Bank 0 = 0xC000-0xFFFF, Bank 1 = 0x8000-0xBFFF, etc. */
    /* Set DDRA to only have bits 0-1 as output for VIC bank control */
    cia->ddra = 0x03;
    uint8_t value = (cia->pra & 0xFC) | ((~bank) & 0x03);
    cia_write(cia, CIA_PRA, value);
}

uint8_t cia2_get_vic_bank(CIA *cia)
{
    /* VIC bank is bits 0-1 of Port A, active low */
    return (~cia->pra) & 0x03;
}
