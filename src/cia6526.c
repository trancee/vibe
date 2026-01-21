#include <string.h>
#include <assert.h>

#include "cia6526.h"

void cia_init(CIA *cia, uint16_t base_address)
{
    if (cia == NULL) return;
    
    memset(cia, 0, sizeof(CIA));
    cia->base_address = base_address;
    
    // Power-up state
    cia->ddra = 0xFF;  // All port A pins as output
    cia->ddrb = 0xFF;  // All port B pins as output
    
    // TOD starts running by default
    cia->tod_running = true;
    cia->tod_50hz = true;
    
    // Clear all registers
    cia->pra = 0x03;  // Default to VIC bank 0 (bits 0-1 set for active low)
    cia->prb = 0x00;
    cia->icr_data = 0x00;  // Interrupt data register (read-only)
    cia->icr_mask = 0x00;  // Interrupt mask register (write-only)
    cia->cra = 0x00;
    cia->crb = 0x00;
    
    // Clear interrupt state
    cia->irq_pending = false;
    cia->pending_interrupts = 0x00;
}

void cia_reset(CIA *cia)
{
    if (cia == NULL) return;
    cia_init(cia, cia->base_address);
}

uint8_t cia_read(CIA *cia, uint16_t addr)
{
    if (cia == NULL || addr < cia->base_address || addr > cia->base_address + 0x0F) return 0;
    
    uint16_t offset = addr - cia->base_address;
    uint8_t result = 0;
    
    switch (offset) {
        case 0x00:  // PRA - Port A Data Register
            if (cia->ddra == 0xFF) {
                // All pins are outputs, return the written value
                result = cia->pra;
            } else {
                // Some pins are inputs, use callback to get input values
                if (cia->port_a_read) {
                    cia->pra = cia->port_a_read(cia->io_context);
                }
                result = cia->pra;
            }
            break;
            
        case 0x01:  // PRB - Port B Data Register
            if (cia->ddrb == 0xFF) {
                // All pins are outputs, return the written value
                result = cia->prb;
            } else {
                // Some pins are inputs, use callback to get input values
                if (cia->port_b_read) {
                    cia->prb = cia->port_b_read(cia->io_context);
                }
                result = cia->prb;
            }
            break;
            
        case 0x02:  // DDRA - Data Direction Register A
            result = cia->ddra;
            break;
            
        case 0x03:  // DDRB - Data Direction Register B
            result = cia->ddrb;
            break;
            
        case 0x04:  // TALO - Timer A Low Byte
            result = cia->timer_a & 0xFF;
            break;
            
        case 0x05:  // TAHI - Timer A High Byte
            result = (cia->timer_a >> 8) & 0xFF;
            cia->timer_a_underflow = false;  // Clear underflow flag
            break;
            
        case 0x06:  // TBLO - Timer B Low Byte
            result = cia->timer_b & 0xFF;
            break;
            
        case 0x07:  // TBHI - Timer B High Byte
            result = (cia->timer_b >> 8) & 0xFF;
            cia->timer_b_underflow = false;  // Clear underflow flag
            break;
            
        case 0x08:  // TOD10TH - Time of Day 10ths
            result = cia->tod_10th;
            break;
            
        case 0x09:  // TODSEC - Time of Day Seconds
            result = cia->tod_sec;
            break;
            
        case 0x0A:  // TODMIN - Time of Day Minutes
            result = cia->tod_min;
            break;
            
        case 0x0B:  // TODHR - Time of Day Hours
            result = cia->tod_hr;
            cia->icr_data &= ~CIA_ICR_TOD;  // Clear TOD interrupt
            break;
            
        case 0x0C:  // SDR - Serial Data Register
            result = cia->sdr;
            cia->icr_data &= ~CIA_ICR_SDR;  // Clear serial interrupt
            break;
            
        case 0x0D:  // ICR - Interrupt Control Register (DATA - read-only)
            // Reading ICR returns interrupt status and clears it
            result = cia->icr_data;
            if (cia->irq_pending) {
                result |= CIA_ICR_FLAG;  // Bit 7 set if any enabled interrupt occurred
            }
            // Clear all interrupt bits and IRQ after read
            cia->icr_data = 0x00;
            cia->pending_interrupts = 0;
            cia->irq_pending = false;
            break;
            
        case 0x0E:  // CRA - Control Register A
            result = cia->cra;
            break;
            
        case 0x0F:  // CRB - Control Register B
            result = cia->crb;
            break;
            
        default:
            result = 0;
            break;
    }
    
    return result;
}

void cia_write(CIA *cia, uint16_t addr, uint8_t data)
{
    if (cia == NULL || addr < cia->base_address || addr > cia->base_address + 0x0F) return;
    
    uint16_t offset = addr - cia->base_address;
    
    switch (offset) {
        case 0x00:  // PRA - Port A Data Register
            cia->pra = data;
            if (cia->port_a_write) {
                cia->port_a_write(cia->io_context, data & cia->ddra);
            }
            break;
            
        case 0x01:  // PRB - Port B Data Register
            cia->prb = data;
            if (cia->port_b_write) {
                cia->port_b_write(cia->io_context, data & cia->ddrb);
            }
            break;
            
        case 0x02:  // DDRA - Data Direction Register A
            cia->ddra = data;
            break;
            
        case 0x03:  // DDRB - Data Direction Register B
            cia->ddrb = data;
            break;
            
        case 0x04:  // TALO - Timer A Low Byte
            cia->timer_a_latch = (cia->timer_a_latch & 0xFF00) | data;
            if (!(cia->cra & CIA_CR_START)) {
                cia->timer_a = cia->timer_a_latch;
            }
            break;
            
        case 0x05:  // TAHI - Timer A High Byte
            cia->timer_a_latch = (cia->timer_a_latch & 0x00FF) | (data << 8);
            if (!(cia->cra & CIA_CR_START)) {
                cia->timer_a = cia->timer_a_latch;
            }
            break;
            
        case 0x06:  // TBLO - Timer B Low Byte
            cia->timer_b_latch = (cia->timer_b_latch & 0xFF00) | data;
            if (!(cia->crb & CIA_CRB_START)) {
                cia->timer_b = cia->timer_b_latch;
            }
            break;
            
        case 0x07:  // TBHI - Timer B High Byte
            cia->timer_b_latch = (cia->timer_b_latch & 0x00FF) | (data << 8);
            if (!(cia->crb & CIA_CRB_START)) {
                cia->timer_b = cia->timer_b_latch;
            }
            break;
            
        case 0x08:  // TOD10TH - Time of Day 10ths (latch)
            cia->tod_alarm_10th = data;
            break;
            
        case 0x09:  // TODSEC - Time of Day Seconds (latch)
            cia->tod_alarm_sec = data;
            break;
            
        case 0x0A:  // TODMIN - Time of Day Minutes (latch)
            cia->tod_alarm_min = data;
            break;
            
        case 0x0B:  // TODHR - Time of Day Hours (latch)
            cia->tod_alarm_hr = data & 0x9F;  // Only 6 bits used
            if (data & CIA_CRB_ALARM) {
                // Alarm mode - copy latched values to alarm registers
                cia->tod_alarm_hr = cia->tod_alarm_10th;  // This is a bit wrong, but simplified
            }
            break;
            
        case 0x0C:  // SDR - Serial Data Register
            cia->sdr = data;
            cia->icr_data &= ~CIA_ICR_SDR;  // Clear serial interrupt
            break;
            
        case 0x0D:  // ICR - Interrupt Control Register (MASK - write-only)
            if (data & CIA_ICR_SET) {
                // Set interrupt enable bits
                cia->icr_mask |= (data & 0x1F);
            } else {
                // Clear interrupt enable bits
                cia->icr_mask &= ~(data & 0x1F);
            }
            cia_update_irq(cia);
            break;
            
        case 0x0E:  // CRA - Control Register A
            cia->cra = data;
            
            // Handle timer control
            if (data & CIA_CR_LOAD) {
                cia_timer_a_load(cia, cia->timer_a_latch);
            }
            
            if (data & CIA_CR_START) {
                cia_timer_a_start(cia);
            } else {
                cia_timer_a_stop(cia);
            }
            
            cia->timer_a_one_shot = (data & CIA_CR_RUNMODE) != 0;  // Bit 3: 1=one-shot, 0=continuous
            cia->tod_50hz = (data & CIA_CR_TODIN) != 0;
            break;
            
        case 0x0F:  // CRB - Control Register B
            cia->crb = data;
            
            // Handle timer control
            if (data & CIA_CRB_LOAD) {
                cia_timer_b_load(cia, cia->timer_b_latch);
            }
            
            if (data & CIA_CRB_START) {
                cia_timer_b_start(cia);
            } else {
                cia_timer_b_stop(cia);
            }
            
            cia->timer_b_one_shot = (data & CIA_CRB_RUNMODE) != 0;  // Bit 3: 1=one-shot, 0=continuous
            break;
    }
}

void cia_clock(CIA *cia)
{
    if (cia == NULL) return;
    
    // Clock Timer A if running
    if (cia->timer_a_running) {
        if (cia->timer_a == 0) {
            cia->timer_a = cia->timer_a_latch;
            cia_timer_a_underflow(cia);
            
            if (cia->timer_a_one_shot) {
                cia->timer_a_running = false;
                cia->cra &= ~CIA_CR_START;
            }
        } else {
            cia->timer_a--;
        }
    }
    
    // Clock Timer B if running
    if (cia->timer_b_running) {
        if (cia->timer_b == 0) {
            cia->timer_b = cia->timer_b_latch;
            cia_timer_b_underflow(cia);
            
            if (cia->timer_b_one_shot) {
                cia->timer_b_running = false;
                cia->crb &= ~CIA_CRB_START;
            }
        } else {
            cia->timer_b--;
        }
    }
    
    // Handle serial port
    if (cia->serial_output_enabled) {
        cia_clock_serial(cia);
    }
}

void cia_clock_phi2(CIA *cia)
{
    if (cia == NULL) return;
    
    // Clock Time of Day at 50/60 Hz
    if (cia->tod_running) {
        cia_clock_tod(cia);
    }
}

bool cia_get_irq(CIA *cia)
{
    if (cia == NULL) return false;
    return cia->irq_pending;
}

// Timer functions
void cia_timer_a_start(CIA *cia)
{
    if (cia == NULL) return;
    cia->timer_a_running = true;
    cia->cra |= CIA_CR_START;
}

void cia_timer_a_stop(CIA *cia)
{
    if (cia == NULL) return;
    cia->timer_a_running = false;
    cia->cra &= ~CIA_CR_START;
}

void cia_timer_a_load(CIA *cia, uint16_t value)
{
    if (cia == NULL) return;
    cia->timer_a_latch = value;
    cia->timer_a = value;
}

void cia_timer_b_start(CIA *cia)
{
    if (cia == NULL) return;
    cia->timer_b_running = true;
    cia->crb |= CIA_CRB_START;
}

void cia_timer_b_stop(CIA *cia)
{
    if (cia == NULL) return;
    cia->timer_b_running = false;
    cia->crb &= ~CIA_CRB_START;
}

void cia_timer_b_load(CIA *cia, uint16_t value)
{
    if (cia == NULL) return;
    cia->timer_b_latch = value;
    cia->timer_b = value;
}

void cia_timer_a_underflow(CIA *cia)
{
    if (cia == NULL) return;
    
    cia->timer_a_underflow = true;
    cia->pending_interrupts |= CIA_ICR_TA;
    cia->icr_data |= CIA_ICR_TA;
    cia_update_irq(cia);
    
    // Toggle port B bit 6 if PBON is set
    if (cia->crb & CIA_CRB_PBON) {
        cia->prb ^= 0x40;
    }
}

void cia_timer_b_underflow(CIA *cia)
{
    if (cia == NULL) return;
    
    cia->timer_b_underflow = true;
    cia->pending_interrupts |= CIA_ICR_TB;
    cia->icr_data |= CIA_ICR_TB;
    cia_update_irq(cia);
    
    // Toggle port B bit 7 if PBON is set
    if (cia->crb & CIA_CRB_PBON) {
        cia->prb ^= 0x80;
    }
}

// TOD functions
void cia_tod_start(CIA *cia)
{
    if (cia == NULL) return;
    cia->tod_running = true;
}

void cia_tod_stop(CIA *cia)
{
    if (cia == NULL) return;
    cia->tod_running = false;
}

void cia_tod_set(CIA *cia, uint8_t hr, uint8_t min, uint8_t sec, uint8_t tenth)
{
    if (cia == NULL) return;
    
    cia->tod_hr = hr & 0x9F;  // 6 bits only
    cia->tod_min = min;
    cia->tod_sec = sec;
    cia->tod_10th = tenth;
}

void cia_tod_set_alarm(CIA *cia, uint8_t hr, uint8_t min, uint8_t sec, uint8_t tenth)
{
    if (cia == NULL) return;
    
    cia->tod_alarm_hr = hr & 0x9F;
    cia->tod_alarm_min = min;
    cia->tod_alarm_sec = sec;
    cia->tod_alarm_10th = tenth;
}

void cia_clock_tod(CIA *cia)
{
    if (cia == NULL || !cia->tod_running) return;
    
    // Check for alarm BEFORE incrementing time
    if (cia->tod_hr == cia->tod_alarm_hr &&
        cia->tod_min == cia->tod_alarm_min &&
        cia->tod_sec == cia->tod_alarm_sec &&
        cia->tod_10th == cia->tod_alarm_10th) {
        
        cia->pending_interrupts |= CIA_ICR_TOD;
        cia->icr_data |= CIA_ICR_TOD;
        cia_update_irq(cia);
    }
    
    // Increment 10ths (BCD: 0-9)
    cia->tod_10th++;
    if (cia->tod_10th > 9) {
        cia->tod_10th = 0;
        
        // Increment seconds (BCD: 00-59)
        uint8_t sec_lo = cia->tod_sec & 0x0F;
        uint8_t sec_hi = (cia->tod_sec >> 4) & 0x07;
        sec_lo++;
        if (sec_lo > 9) {
            sec_lo = 0;
            sec_hi++;
        }
        if (sec_hi > 5) {
            sec_hi = 0;
            sec_lo = 0;
            
            // Increment minutes (BCD: 00-59)
            uint8_t min_lo = cia->tod_min & 0x0F;
            uint8_t min_hi = (cia->tod_min >> 4) & 0x07;
            min_lo++;
            if (min_lo > 9) {
                min_lo = 0;
                min_hi++;
            }
            if (min_hi > 5) {
                min_hi = 0;
                min_lo = 0;
                
                // Increment hours (BCD: 01-12, with AM/PM in bit 7)
                uint8_t hr_lo = cia->tod_hr & 0x0F;
                uint8_t hr_hi = (cia->tod_hr >> 4) & 0x01;
                uint8_t pm = cia->tod_hr & 0x80;
                
                hr_lo++;
                if (hr_lo > 9) {
                    hr_lo = 0;
                    hr_hi++;
                }
                
                // Handle 12-hour rollover
                uint8_t hour_val = hr_hi * 10 + hr_lo;
                if (hour_val == 12) {
                    pm ^= 0x80;  // Toggle AM/PM
                } else if (hour_val > 12) {
                    hr_hi = 0;
                    hr_lo = 1;
                }
                
                cia->tod_hr = (pm | (hr_hi << 4) | hr_lo);
            }
            cia->tod_min = (min_hi << 4) | min_lo;
        }
        cia->tod_sec = (sec_hi << 4) | sec_lo;
    }
}

// Serial port functions
void cia_serial_write(CIA *cia, uint8_t data)
{
    if (cia == NULL) return;
    
    cia->sdr = data;
    cia->serial_shift_reg = data;
    cia->serial_bit_count = 0;
    cia->serial_output_enabled = true;
}

uint8_t cia_serial_read(CIA *cia)
{
    if (cia == NULL) return 0;
    return cia->sdr;
}

void cia_clock_serial(CIA *cia)
{
    if (cia == NULL || !cia->serial_output_enabled) return;
    
    cia->serial_bit_count++;
    
    if (cia->serial_bit_count >= 8) {
        cia->serial_output_enabled = false;
        cia->pending_interrupts |= CIA_ICR_SDR;
        cia->icr_data |= CIA_ICR_SDR;
        cia_update_irq(cia);
    }
}

// I/O callbacks
void cia_set_io_callbacks(CIA *cia, void *context,
                          uint8_t (*port_a_read)(void *context),
                          void (*port_a_write)(void *context, uint8_t data),
                          uint8_t (*port_b_read)(void *context),
                          void (*port_b_write)(void *context, uint8_t data))
{
    if (cia == NULL) return;
    
    cia->io_context = context;
    cia->port_a_read = port_a_read;
    cia->port_a_write = port_a_write;
    cia->port_b_read = port_b_read;
    cia->port_b_write = port_b_write;
}

// CIA1 specific functions
uint8_t cia1_keyboard_scan(CIA *cia1)
{
    if (cia1 == NULL) return 0;
    
    // Simple keyboard matrix scanning implementation
    // This would need to be connected to actual keyboard matrix data
    uint8_t rows = ~cia1->pra & 0xFF;  // Rows being scanned (active low)
    uint8_t cols = 0xFF;              // Default to no keys pressed
    
    // This is a simplified implementation
    // In reality, you'd have a keyboard matrix table
    if (rows & 0x01) cols &= 0xFE;  // Row 0
    if (rows & 0x02) cols &= 0xFD;  // Row 1
    if (rows & 0x04) cols &= 0xFB;  // Row 2
    if (rows & 0x08) cols &= 0xF7;  // Row 3
    if (rows & 0x10) cols &= 0xEF;  // Row 4
    if (rows & 0x20) cols &= 0xDF;  // Row 5
    if (rows & 0x40) cols &= 0xBF;  // Row 6
    if (rows & 0x80) cols &= 0x7F;  // Row 7
    
    return cols;
}

uint8_t cia1_joystick_read(CIA *cia1, uint8_t joystick_num)
{
    if (cia1 == NULL) return 0xFF;
    
    // C64 joystick reading:
    // Joystick 1 uses CIA1 Port B bits 0-4 ($DC01)
    // Joystick 2 uses CIA1 Port A bits 0-4 ($DC00)
    // Bits are active-low (0 = pressed)
    
    if (joystick_num == 1) {
        // Read from port B, return as-is (already active-low from port read)
        return cia_read(cia1, cia1->base_address + 0x01) & 0x1F;
    } else if (joystick_num == 2) {
        // Read from port A, return as-is (already active-low from port read)
        return cia_read(cia1, cia1->base_address + 0x00) & 0x1F;
    }
    
    return 0xFF;
}

// CIA2 specific functions
uint8_t cia2_get_vic_bank(CIA *cia2)
{
    if (cia2 == NULL) return 0;
    
    // VIC bank is determined by bits 0-1 of port A (active low)
    uint8_t bank = (~(cia2->pra) & 0x03);
    return bank;
}

void cia2_set_vic_bank(CIA *cia2, uint8_t bank)
{
    if (cia2 == NULL || bank > 3) return;
    
    // Set VIC bank bits (active low)
    uint8_t vm_bits = (~bank & 0x03);
    cia2->pra = (cia2->pra & ~0x03) | vm_bits;
    
    // Call the port A write callback if available
    if (cia2->port_a_write) {
        cia2->port_a_write(cia2->io_context, cia2->pra);
    }
}

void cia_update_irq(CIA *cia)
{
    if (cia == NULL) return;
    
    // IRQ is pending if any enabled interrupt source has occurred
    cia->irq_pending = (cia->icr_data & cia->icr_mask & 0x1F) != 0;
}