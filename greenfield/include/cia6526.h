/*
 * C64 Emulator - CIA 6526 Complex Interface Adapter Header
 * 
 * Two CIAs in the C64:
 * - CIA1 ($DC00): Keyboard, joystick, IRQ generation
 * - CIA2 ($DD00): Serial bus, VIC bank selection, NMI generation
 */

#ifndef CIA6526_H
#define CIA6526_H

#include <stdint.h>
#include <stdbool.h>

/* CIA Memory Map */
#define CIA1_BASE       0xDC00
#define CIA2_BASE       0xDD00
#define CIA_SIZE        0x0100

/* CIA Register Offsets */
#define CIA_PRA         0x00    /* Port A Data Register */
#define CIA_PRB         0x01    /* Port B Data Register */
#define CIA_DDRA        0x02    /* Data Direction Register A */
#define CIA_DDRB        0x03    /* Data Direction Register B */
#define CIA_TA_LO       0x04    /* Timer A Low Byte */
#define CIA_TA_HI       0x05    /* Timer A High Byte */
#define CIA_TB_LO       0x06    /* Timer B Low Byte */
#define CIA_TB_HI       0x07    /* Timer B High Byte */
#define CIA_TOD_10THS   0x08    /* TOD 10ths of Seconds */
#define CIA_TOD_SEC     0x09    /* TOD Seconds */
#define CIA_TOD_MIN     0x0A    /* TOD Minutes */
#define CIA_TOD_HR      0x0B    /* TOD Hours */
#define CIA_SDR         0x0C    /* Serial Data Register */
#define CIA_ICR         0x0D    /* Interrupt Control Register */
#define CIA_CRA         0x0E    /* Control Register A */
#define CIA_CRB         0x0F    /* Control Register B */

/* Control Register A Bits */
#define CIA_CRA_START       0x01    /* Start Timer A */
#define CIA_CRA_PBON        0x02    /* PB6 output mode */
#define CIA_CRA_OUTMODE     0x04    /* Toggle/Pulse mode */
#define CIA_CRA_RUNMODE     0x08    /* One-shot/Continuous */
#define CIA_CRA_LOAD        0x10    /* Force load latch into counter */
#define CIA_CRA_INMODE      0x20    /* Count phi2/CNT pulses */
#define CIA_CRA_SPMODE      0x40    /* Serial port mode */
#define CIA_CRA_TODIN       0x80    /* TOD input frequency (50/60Hz) */

/* Control Register A Aliases (for test compatibility) */
#define CIA_CR_START        0x01
#define CIA_CR_PBON         0x02
#define CIA_CR_OUTMODE      0x04
#define CIA_CR_RUNMODE      0x08
#define CIA_CR_LOAD         0x10
#define CIA_CR_INMODE       0x20
#define CIA_CR_SPMODE       0x40
#define CIA_CR_TODIN        0x80

/* Control Register B Bits */
#define CIA_CRB_START       0x01    /* Start Timer B */
#define CIA_CRB_PBON        0x02    /* PB7 output mode */
#define CIA_CRB_OUTMODE     0x04    /* Toggle/Pulse mode */
#define CIA_CRB_RUNMODE     0x08    /* One-shot/Continuous */
#define CIA_CRB_LOAD        0x10    /* Force load latch into counter */
#define CIA_CRB_INMODE0     0x20    /* Timer B input mode bit 0 */
#define CIA_CRB_INMODE1     0x40    /* Timer B input mode bit 1 */
#define CIA_CRB_ALARM       0x80    /* TOD write: 0=set clock, 1=set alarm */

/* Timer B Input Modes (bits 5-6 of CRB) */
#define CIA_TB_COUNT_PHI2   0x00    /* Count phi2 pulses */
#define CIA_TB_COUNT_CNT    0x20    /* Count CNT transitions */
#define CIA_TB_COUNT_TA     0x40    /* Count Timer A underflows */
#define CIA_TB_COUNT_TA_CNT 0x60    /* Count TA underflows when CNT high */

/* Interrupt Control Register Bits */
#define CIA_ICR_TA          0x01    /* Timer A underflow */
#define CIA_ICR_TB          0x02    /* Timer B underflow */
#define CIA_ICR_ALARM       0x04    /* TOD alarm */
#define CIA_ICR_TOD         0x04    /* Alias for TOD alarm */
#define CIA_ICR_SDR         0x08    /* Serial port full/empty */
#define CIA_ICR_FLG         0x10    /* FLAG pin edge (hardware signal) */
#define CIA_ICR_FLAG        0x80    /* Interrupt occurred (read-only, bit 7) */
#define CIA_ICR_IR          0x80    /* Interrupt occurred (read) / Set/Clear (write) */
#define CIA_ICR_SET         0x80    /* Set/clear bit for write (same as IR) */

/* CIA1 Absolute Address Definitions (for test compatibility) */
#define CIA1_PRA        0xDC00
#define CIA1_PRB        0xDC01
#define CIA1_DDRA       0xDC02
#define CIA1_DDRB       0xDC03
#define CIA1_TALO       0xDC04
#define CIA1_TAHI       0xDC05
#define CIA1_TBLO       0xDC06
#define CIA1_TBHI       0xDC07
#define CIA1_TOD10TH    0xDC08
#define CIA1_TODSEC     0xDC09
#define CIA1_TODMIN     0xDC0A
#define CIA1_TODHR      0xDC0B
#define CIA1_SDR        0xDC0C
#define CIA1_ICR        0xDC0D
#define CIA1_CRA        0xDC0E
#define CIA1_CRB        0xDC0F

/* CIA2 Absolute Address Definitions */
#define CIA2_PRA        0xDD00
#define CIA2_PRB        0xDD01
#define CIA2_DDRA       0xDD02
#define CIA2_DDRB       0xDD03
#define CIA2_TALO       0xDD04
#define CIA2_TAHI       0xDD05
#define CIA2_TBLO       0xDD06
#define CIA2_TBHI       0xDD07
#define CIA2_TOD10TH    0xDD08
#define CIA2_TODSEC     0xDD09
#define CIA2_TODMIN     0xDD0A
#define CIA2_TODHR      0xDD0B
#define CIA2_SDR        0xDD0C
#define CIA2_ICR        0xDD0D
#define CIA2_CRA        0xDD0E
#define CIA2_CRB        0xDD0F

/* Port A/B read/write callback types */
typedef uint8_t (*cia_port_read_t)(void *context);
typedef void (*cia_port_write_t)(void *context, uint8_t data);

/* CIA State Structure */
typedef struct {
    /* Port registers */
    uint8_t pra;            /* Port A data */
    uint8_t prb;            /* Port B data */
    uint8_t ddra;           /* Port A data direction */
    uint8_t ddrb;           /* Port B data direction */
    
    /* Timer A */
    uint16_t ta_counter;    /* Current counter value */
    uint16_t ta_latch;      /* Latch (reload value) */
    bool ta_running;        /* Timer running */
    bool ta_underflow;      /* Underflow occurred this cycle */
    bool ta_toggle;         /* Toggle state for PB6 output */
    uint8_t ta_delay;       /* Cycles until timer starts counting (pipeline delay) */
    
    /* Timer B */
    uint16_t tb_counter;    /* Current counter value */
    uint16_t tb_latch;      /* Latch (reload value) */
    bool tb_running;        /* Timer running */
    bool tb_underflow;      /* Underflow occurred this cycle */
    bool tb_toggle;         /* Toggle state for PB7 output */
    uint8_t tb_delay;       /* Cycles until timer starts counting (pipeline delay) */
    
    /* Time of Day */
    uint8_t tod_10ths;      /* 10ths of seconds (0-9) */
    uint8_t tod_sec;        /* Seconds (0-59) */
    uint8_t tod_min;        /* Minutes (0-59) */
    uint8_t tod_hr;         /* Hours (1-12 + AM/PM) */
    
    uint8_t alarm_10ths;    /* Alarm 10ths */
    uint8_t alarm_sec;      /* Alarm seconds */
    uint8_t alarm_min;      /* Alarm minutes */
    uint8_t alarm_hr;       /* Alarm hours */
    
    uint8_t tod_latch_10ths;/* Latched values (when reading hours) */
    uint8_t tod_latch_sec;
    uint8_t tod_latch_min;
    uint8_t tod_latch_hr;
    bool tod_latched;       /* TOD values are latched */
    bool tod_halted;        /* TOD clock halted (writing hours) */
    bool tod_running;       /* TOD is running (for test compatibility) */
    bool tod_50hz;          /* TOD uses 50Hz input (for test compatibility) */
    uint32_t tod_tick_count;/* Counter for TOD timing */
    
    /* Serial port */
    uint8_t sdr;            /* Serial data register */
    uint8_t sr_bits;        /* Bits remaining in shift register */
    bool serial_output_enabled; /* Serial output mode enabled */
    
    /* Interrupt control */
    uint8_t icr_data;       /* Interrupt flags (read) */
    uint8_t icr_mask;       /* Interrupt mask (write) */
    uint8_t icr_new;        /* New interrupt flags (not yet visible in bit 7) */
    uint8_t irq_delay;      /* Cycles until IRQ is asserted (0 = can assert now) */
    
    /* Control registers */
    uint8_t cra;            /* Control Register A */
    uint8_t crb;            /* Control Register B */
    
    /* External interface */
    bool irq_pending;       /* IRQ output line */
    uint16_t base_addr;     /* Base address ($DC00 or $DD00) */
    
    /* Keyboard matrix (CIA1 only) */
    uint8_t keyboard[8];    /* 8 rows of keyboard state */
    
    /* Port callbacks (for test compatibility) */
    void *io_context;
    cia_port_read_t port_a_read;
    cia_port_write_t port_a_write;
    cia_port_read_t port_b_read;
    cia_port_write_t port_b_write;
} CIA;

/* Struct member aliases for test compatibility */
#define timer_a         ta_counter
#define timer_a_latch   ta_latch
#define timer_a_running ta_running
#define timer_a_underflow ta_underflow
#define timer_b         tb_counter
#define timer_b_latch   tb_latch
#define timer_b_running tb_running
#define timer_b_underflow tb_underflow
#define base_address    base_addr
#define tod_10th        tod_10ths
#define tod_alarm_hr    alarm_hr
#define tod_alarm_min   alarm_min
#define tod_alarm_sec   alarm_sec
#define tod_alarm_10th  alarm_10ths

/* CIA Functions */
void cia_init(CIA *cia, uint16_t base_addr);
void cia_reset(CIA *cia);

uint8_t cia_read(CIA *cia, uint16_t addr);
void cia_write(CIA *cia, uint16_t addr, uint8_t data);

/* Clock the CIA by one cycle */
void cia_clock(CIA *cia);

/* Clock the serial port (for test compatibility) */
void cia_clock_serial(CIA *cia);

/* Clock phi2 (alias for cia_tod_tick for test compatibility) */
void cia_clock_phi2(CIA *cia);

/* TOD clock tick (call at 50/60 Hz) */
void cia_tod_tick(CIA *cia);

/* Set TOD time directly (for test compatibility) */
void cia_tod_set(CIA *cia, uint8_t hr, uint8_t min, uint8_t sec, uint8_t tenths);

/* Set TOD alarm (for test compatibility) */
void cia_tod_set_alarm(CIA *cia, uint8_t hr, uint8_t min, uint8_t sec, uint8_t tenths);

/* Read joystick (for test compatibility - CIA1 only) */
uint8_t cia1_joystick_read(CIA *cia, int joy_num);

/* VIC bank control (CIA2 only) */
void cia2_set_vic_bank(CIA *cia, uint8_t bank);
uint8_t cia2_get_vic_bank(CIA *cia);

/* Process IRQ delay at instruction boundary */
void cia_finalize_irq(CIA *cia);

/* Get IRQ/NMI status */
bool cia_get_irq(CIA *cia);

/* Set keyboard state (for CIA1) */
void cia_set_key(CIA *cia, int row, int col, bool pressed);

/* Get/set port values (for external connections) */
uint8_t cia_get_port_a(CIA *cia);
uint8_t cia_get_port_b(CIA *cia);
void cia_set_port_a(CIA *cia, uint8_t value);
void cia_set_port_b(CIA *cia, uint8_t value);

/* Set I/O port callbacks (for test compatibility) */
void cia_set_io_callbacks(CIA *cia, void *context,
                          cia_port_read_t port_a_read, cia_port_write_t port_a_write,
                          cia_port_read_t port_b_read, cia_port_write_t port_b_write);

#endif /* CIA6526_H */
