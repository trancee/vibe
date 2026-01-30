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

void cia_init(C64Cia *cia, int cia_num, C64System *sys) {
    memset(cia, 0, sizeof(C64Cia));
    cia->cia_num = cia_num;
    cia->sys = sys;
}

void cia_reset(C64Cia *cia) {
    cia->pra = 0;
    cia->prb = 0;
    cia->ddra = 0;
    cia->ddrb = 0;
    
    cia->timer_a = 0xFFFF;
    cia->timer_b = 0xFFFF;
    cia->timer_a_latch = 0xFFFF;
    cia->timer_b_latch = 0xFFFF;
    
    cia->ta_delay = 0;
    cia->tb_delay = 0;
    cia->ta_started = false;
    cia->tb_started = false;
    
    cia->cra = 0;
    cia->crb = 0;
    
    cia->icr_data = 0;
    cia->icr_mask = 0;
    cia->irq_pending = false;
    cia->irq_delay = false;
    
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
static void check_irq(C64Cia *cia) {
    // Check if any enabled interrupt occurred
    if (cia->icr_data & cia->icr_mask & 0x1F) {
        // IRQ delay: bit 7 not set until next cycle
        cia->irq_delay = true;
    }
}

void cia_clock(C64Cia *cia) {
    // Process IRQ delay from previous cycle
    if (cia->irq_delay) {
        cia->icr_data |= CIA_ICR_IR;
        cia->irq_pending = true;
        cia->irq_delay = false;
    }
    
    // Timer A
    if (cia->cra & CIA_CR_START) {
        // Handle pipeline delay when timer starts
        if (cia->ta_delay > 0) {
            cia->ta_delay--;
        } else {
            bool count = false;
            
            // Check input mode
            if (!(cia->cra & CIA_CR_INMODE)) {
                // Count phi2 cycles
                count = true;
            }
            // CNT mode not implemented
            
            if (count) {
                if (cia->timer_a == 0) {
                    // Underflow
                    cia->timer_a = cia->timer_a_latch;
                    
                    // Set interrupt flag
                    cia->icr_data |= CIA_ICR_TA;
                    check_irq(cia);
                    
                    // One-shot mode: stop timer
                    if (cia->cra & CIA_CR_RUNMODE) {
                        cia->cra &= ~CIA_CR_START;
                    }
                } else {
                    cia->timer_a--;
                }
            }
        }
    }
    
    // Timer B
    if (cia->crb & CIA_CR_START) {
        if (cia->tb_delay > 0) {
            cia->tb_delay--;
        } else {
            bool count = false;
            u8 inmode = (cia->crb >> 5) & 0x03;
            
            switch (inmode) {
                case 0: // phi2
                    count = true;
                    break;
                case 1: // CNT (not implemented)
                    break;
                case 2: // Timer A underflow
                    if (cia->icr_data & CIA_ICR_TA) {
                        count = true;
                    }
                    break;
                case 3: // Timer A underflow while CNT high (not implemented)
                    break;
            }
            
            if (count) {
                if (cia->timer_b == 0) {
                    // Underflow
                    cia->timer_b = cia->timer_b_latch;
                    
                    cia->icr_data |= CIA_ICR_TB;
                    check_irq(cia);
                    
                    if (cia->crb & CIA_CR_RUNMODE) {
                        cia->crb &= ~CIA_CR_START;
                    }
                } else {
                    cia->timer_b--;
                }
            }
        }
    }
    
    // TOD clock (runs at ~10Hz)
    cia->tod_divider++;
    if (cia->tod_divider >= (C64_CPU_FREQ / 10)) {
        cia->tod_divider = 0;
        
        // Increment 1/10 seconds
        cia->tod_10ths++;
        if (cia->tod_10ths >= 10) {
            cia->tod_10ths = 0;
            
            // Increment seconds (BCD)
            u8 sec_lo = (cia->tod_sec & 0x0F) + 1;
            u8 sec_hi = (cia->tod_sec >> 4);
            if (sec_lo >= 10) {
                sec_lo = 0;
                sec_hi++;
            }
            if (sec_hi >= 6) {
                sec_hi = 0;
                
                // Increment minutes
                u8 min_lo = (cia->tod_min & 0x0F) + 1;
                u8 min_hi = (cia->tod_min >> 4);
                if (min_lo >= 10) {
                    min_lo = 0;
                    min_hi++;
                }
                if (min_hi >= 6) {
                    min_hi = 0;
                    
                    // Increment hours
                    u8 hr = cia->tod_hr & 0x1F;
                    bool pm = cia->tod_hr & 0x80;
                    
                    u8 hr_lo = (hr & 0x0F) + 1;
                    u8 hr_hi = (hr >> 4);
                    if (hr_lo >= 10) {
                        hr_lo = 0;
                        hr_hi++;
                    }
                    hr = (hr_hi << 4) | hr_lo;
                    
                    if (hr >= 0x12) {
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
            cia->tod_hr == cia->alarm_hr) {
            cia->icr_data |= CIA_ICR_TOD;
            check_irq(cia);
        }
    }
}

u8 cia_read(C64Cia *cia, u8 reg) {
    switch (reg) {
        case CIA_PRA:
            if (cia->cia_num == 1) {
                // CIA1 Port A: Keyboard columns / Joystick 2
                return cia_read_keyboard(cia);
            } else {
                // CIA2 Port A: VIC bank, serial bus
                return (cia->pra & cia->ddra) | (~cia->ddra & 0xFF);
            }
        
        case CIA_PRB:
            if (cia->cia_num == 1) {
                // CIA1 Port B: Keyboard rows / Joystick 1
                return 0xFF;  // No keys pressed
            } else {
                // CIA2 Port B: User port
                return (cia->prb & cia->ddrb) | (~cia->ddrb & 0xFF);
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
            cia->tod_latched = false;
            return cia->tod_latched ? cia->tod_latch_10ths : cia->tod_10ths;
        
        case CIA_TOD_S:
            return cia->tod_latched ? cia->tod_latch_sec : cia->tod_sec;
        
        case CIA_TOD_M:
            return cia->tod_latched ? cia->tod_latch_min : cia->tod_min;
        
        case CIA_TOD_H:
            // Reading hours latches TOD
            if (!cia->tod_latched) {
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
                // Reading ICR clears it
                cia->icr_data = 0;
                cia->irq_pending = false;
                cia->irq_delay = false;
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

void cia_write(C64Cia *cia, u8 reg, u8 value) {
    switch (reg) {
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
            if (!(cia->cra & CIA_CR_START)) {
                cia->timer_a = cia->timer_a_latch;
            }
            break;
        
        case CIA_TBLO:
            cia->timer_b_latch = (cia->timer_b_latch & 0xFF00) | value;
            break;
        
        case CIA_TBHI:
            cia->timer_b_latch = (cia->timer_b_latch & 0x00FF) | (value << 8);
            if (!(cia->crb & CIA_CR_START)) {
                cia->timer_b = cia->timer_b_latch;
            }
            break;
        
        case CIA_TOD_10:
            if (cia->crb & CIA_CR_TODIN) {
                cia->alarm_10ths = value;
            } else {
                cia->tod_10ths = value & 0x0F;
            }
            break;
        
        case CIA_TOD_S:
            if (cia->crb & CIA_CR_TODIN) {
                cia->alarm_sec = value;
            } else {
                cia->tod_sec = value;
            }
            break;
        
        case CIA_TOD_M:
            if (cia->crb & CIA_CR_TODIN) {
                cia->alarm_min = value;
            } else {
                cia->tod_min = value;
            }
            break;
        
        case CIA_TOD_H:
            if (cia->crb & CIA_CR_TODIN) {
                cia->alarm_hr = value;
            } else {
                cia->tod_hr = value;
            }
            break;
        
        case CIA_SDR:
            cia->sdr = value;
            break;
        
        case CIA_ICR:
            // Bit 7: Set or clear mode
            if (value & 0x80) {
                // Set bits
                cia->icr_mask |= (value & 0x1F);
            } else {
                // Clear bits
                cia->icr_mask &= ~(value & 0x1F);
            }
            
            // Check if we now have enabled pending interrupt
            if (cia->icr_data & cia->icr_mask & 0x1F) {
                cia->icr_data |= CIA_ICR_IR;
                cia->irq_pending = true;
            }
            break;
        
        case CIA_CRA:
            {
                bool was_running = cia->cra & CIA_CR_START;
                bool now_running = value & CIA_CR_START;
                
                // Force load bit
                if (value & CIA_CR_LOAD) {
                    cia->timer_a = cia->timer_a_latch;
                }
                
                // Timer starting: add pipeline delay
                if (!was_running && now_running) {
                    cia->ta_delay = 2;  // 2-cycle delay before counting
                }
                
                cia->cra = value & ~CIA_CR_LOAD;  // Load bit not stored
            }
            break;
        
        case CIA_CRB:
            {
                bool was_running = cia->crb & CIA_CR_START;
                bool now_running = value & CIA_CR_START;
                
                if (value & CIA_CR_LOAD) {
                    cia->timer_b = cia->timer_b_latch;
                }
                
                if (!was_running && now_running) {
                    cia->tb_delay = 2;
                }
                
                cia->crb = value & ~CIA_CR_LOAD;
            }
            break;
    }
}

void cia_set_key(C64Cia *cia, int row, int col, bool pressed) {
    (void)cia;  // Uses global keyboard matrix
    if (row < 0 || row > 7 || col < 0 || col > 7) return;
    
    if (pressed) {
        keyboard_matrix[row] &= ~(1 << col);
    } else {
        keyboard_matrix[row] |= (1 << col);
    }
}

u8 cia_read_keyboard(C64Cia *cia) {
    // Port A is keyboard columns (directly from matrix when scanning)
    // Port B selects which row to read
    
    u8 cols = 0xFF;
    u8 rows = ~(cia->prb & cia->ddrb);  // Active low
    
    for (int r = 0; r < 8; r++) {
        if (rows & (1 << r)) {
            cols &= keyboard_matrix[r];
        }
    }
    
    return cols;
}
