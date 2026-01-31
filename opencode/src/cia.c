#include "c64.h"
#include <string.h>

void cia_init(C64CIA* cia) {
    memset(cia, 0, sizeof(C64CIA));
    cia->ta_delay = 0; // Initialize to no delay
    cia->tb_delay = 0;
}

void cia_clock(C64CIA* cia) {
    // Handle ICR bit 7 delay: set bit 7 on cycle AFTER interrupt flag was set
    if (cia->icr_irq_flag_set) {
        cia->icr_data |= 0x80;
        cia->icr_irq_flag_set = false;
    }
    cia->irq_pending = false;
    
    if (cia->ta_delay > 0) {
        cia->ta_delay--;
        if (cia->ta_delay == 0 && (cia->cra & 0x01)) {
            cia->timer_a = cia->timer_a_latch;
        }
    }
    
    if ((cia->cra & 0x01) && cia->ta_delay == 0) {
        u16 old_timer = cia->timer_a;
        cia->timer_a--;
        if (old_timer == 0 && cia->timer_a == 0xFFFF) {
            cia->timer_a = cia->timer_a_latch;
            cia->icr_data |= 0x01;
            cia->irq_pending = true;
            cia->icr_irq_flag_set = true; // Flag for ICR bit 7 delay
            if (cia->cra & 0x08) {
                cia->cra &= ~0x01;
            }
        }
    }
    
    if (cia->tb_delay > 0) {
        cia->tb_delay--;
        if (cia->tb_delay == 0 && (cia->crb & 0x01)) {
            cia->timer_b = cia->timer_b_latch;
        }
    }
    
    if ((cia->crb & 0x01) && cia->tb_delay == 0) {
        u16 old_timer = cia->timer_b;
        cia->timer_b--;
        if (old_timer == 0 && cia->timer_b == 0xFFFF) {
            cia->timer_b = cia->timer_b_latch;
            cia->icr_data |= 0x02;
            cia->irq_pending = true;
            cia->icr_irq_flag_set = true; // Flag for ICR bit 7 delay
            if (cia->crb & 0x08) {
                cia->crb &= ~0x01;
            }
        }
    }
    
    if (cia->tod_running) {
        cia->tod_tenth++;
        if (cia->tod_tenth >= 10) {
            cia->tod_tenth = 0;
            cia->tod_seconds++;
            if (cia->tod_seconds >= 60) {
                cia->tod_seconds = 0;
                cia->tod_minutes++;
                if (cia->tod_minutes >= 60) {
                    cia->tod_minutes = 0;
                    cia->tod_hours++;
                    if (cia->tod_hours >= 24) {
                        cia->tod_hours = 0;
                    }
                }
            }
        }
        
        if (cia->tod_tenth == cia->tod_alarm_tenth &&
            cia->tod_seconds == cia->tod_alarm_seconds &&
            cia->tod_minutes == cia->tod_alarm_minutes &&
            cia->tod_hours == cia->tod_alarm_hours) {
            cia->icr_data |= 0x04;
            cia->irq_pending = true;
            cia->icr_irq_flag_set = true; // Flag for ICR bit 7 delay
        }
    }
}

u8 cia_read_reg(C64CIA* cia, u8 reg) {
    switch (reg) {
        case 0x00:
            return cia->pra;
        case 0x01:
            return cia->prb;
        case 0x02:
            return cia->timer_a & 0xFF;
        case 0x03:
            return (cia->timer_a >> 8) & 0xFF;
        case 0x04:
            return cia->timer_b & 0xFF;
        case 0x05:
            return (cia->timer_b >> 8) & 0xFF;
        case 0x06:
        case 0x07:
            cia->tod_latch = true;
            if (reg == 0x06) {
                return (cia->tod_tenth & 0x0F) | ((cia->tod_seconds & 0x0F) << 4);
            } else {
                return (cia->tod_seconds & 0xF0) | ((cia->tod_minutes & 0x0F) << 4);
            }
        case 0x08:
        case 0x09:
            if (reg == 0x08) {
                u8 result = cia->tod_latch ? 
                    ((cia->tod_minutes & 0xF0) | ((cia->tod_hours & 0x0F) << 4)) :
                    ((cia->tod_minutes & 0xF0) | ((cia->tod_hours & 0x0F) << 4));
                cia->tod_latch = false;
                return result;
            } else {
                return cia->tod_latch ? 
                    (cia->tod_hours & 0x80) : (cia->tod_hours & 0x80);
            }
        case 0x0A:
            return cia->icr_data; // Bit 7 is now handled in cia_clock() with proper delay
        case 0x0B:
            return cia->cra;
        case 0x0C:
            return cia->crb;
        case 0x0D:
            return cia->sdr;
        case 0x0E:
            return cia->data_dir_a;
        case 0x0F:
            return cia->data_dir_b;
        default:
            return 0xFF;
    }
}

void cia_write_reg(C64CIA* cia, u8 reg, u8 data) {
    switch (reg) {
        case 0x00:
            cia->data_dir_a = data;
            break;
        case 0x01:
            cia->pra = data;
            break;
        case 0x02:
            cia->timer_a_latch = (cia->timer_a_latch & 0xFF00) | data;
            break;
        case 0x03:
            cia->timer_a_latch = (cia->timer_a_latch & 0x00FF) | (data << 8);
            break;
        case 0x04:
            cia->timer_b_latch = (cia->timer_b_latch & 0xFF00) | data;
            break;
        case 0x05:
            cia->timer_b_latch = (cia->timer_b_latch & 0x00FF) | (data << 8);
            break;
        case 0x08:
            cia->tod_hours = data & 0x9F;
            if (!(data & 0x80)) {
                cia->tod_running = false;
            }
            break;
        case 0x09:
            cia->tod_minutes = data & 0x7F;
            break;
        case 0x0A:
            cia->tod_seconds = data & 0x7F;
            break;
        case 0x0B:
            cia->tod_tenth = data & 0x0F;
            cia->tod_alarm_hours = cia->tod_hours;
            cia->tod_alarm_minutes = cia->tod_minutes;
            cia->tod_alarm_seconds = cia->tod_seconds;
            cia->tod_alarm_tenth = cia->tod_tenth;
            if (data & 0x80) {
                cia->tod_running = true;
            }
            break;
        case 0x0C:
            cia->imr = data;
            break;
        case 0x0D:
            u8 old_icr_data = cia->icr_data;
            cia->icr_data &= ~data;
            if (cia->icr_data != old_icr_data) {
                cia->irq_pending = false;
            }
            break;
        case 0x0E:
            cia->cra = data;
            if (data & 0x10) {
                cia->timer_a = cia->timer_a_latch;
                cia->ta_delay = 1; // Minimal delay for forced load
            }
            if (data & 0x01) {
                if (cia->ta_delay == 0) {
                    cia->ta_delay = 1; // Minimal delay for start
                }
            }
            break;
        case 0x0F:
            cia->crb = data;
            if (data & 0x10) {
                cia->timer_b = cia->timer_b_latch;
                cia->tb_delay = 2;
            }
            if (data & 0x01) {
                if (cia->tb_delay == 0) {
                    cia->tb_delay = 2;
                }
            }
            break;
        case 0x10:
            cia->data_dir_b = data;
            break;
        case 0x11:
            cia->prb = data;
            break;
        case 0x12:
            cia->sdr = data;
            break;
    }
}