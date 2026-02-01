/**
 * cia.h - CIA 6526 chip emulation header
 */

#ifndef C64_CIA_H
#define C64_CIA_H

#include "types.h"

// Forward declaration
typedef struct C64System C64System;

// CIA register offsets
#define CIA_PRA 0x00    // Port A data
#define CIA_PRB 0x01    // Port B data
#define CIA_DDRA 0x02   // Port A direction
#define CIA_DDRB 0x03   // Port B direction
#define CIA_TALO 0x04   // Timer A low
#define CIA_TAHI 0x05   // Timer A high
#define CIA_TBLO 0x06   // Timer B low
#define CIA_TBHI 0x07   // Timer B high
#define CIA_TOD_10 0x08 // TOD 1/10 sec
#define CIA_TOD_S 0x09  // TOD seconds
#define CIA_TOD_M 0x0A  // TOD minutes
#define CIA_TOD_H 0x0B  // TOD hours
#define CIA_SDR 0x0C    // Serial data
#define CIA_ICR 0x0D    // Interrupt control
#define CIA_CRA 0x0E    // Control reg A
#define CIA_CRB 0x0F    // Control reg B

// Timer control bits (CRA/CRB)
#define CIA_CR_START 0x01
#define CIA_CR_PBON 0x02
#define CIA_CR_OUTMODE 0x04
#define CIA_CR_RUNMODE 0x08 // 0=continuous, 1=one-shot
#define CIA_CR_LOAD 0x10
#define CIA_CR_INMODE 0x20 // Timer A: 0=phi2, 1=CNT
                           // Timer B: 0/1=phi2/CNT, 2/3=Timer A underflow
#define CIA_CR_SPMODE 0x40 // CRA: SP output mode
#define CIA_CR_TODIN 0x80  // CRB: TOD write = alarm

// ICR bits
#define CIA_ICR_TA 0x01   // Timer A underflow
#define CIA_ICR_TB 0x02   // Timer B underflow
#define CIA_ICR_TOD 0x04  // TOD alarm
#define CIA_ICR_SDR 0x08  // Serial complete
#define CIA_ICR_FLAG 0x10 // FLAG pin
#define CIA_ICR_IR 0x80   // Interrupt occurred

// CIA state structure
typedef struct
{
    // Port registers
    u8 pra;  // Port A data register
    u8 prb;  // Port B data register
    u8 ddra; // Port A data direction
    u8 ddrb; // Port B data direction

    // Timers
    u16 timer_a;       // Timer A current value (internal, updated during tick)
    u16 timer_b;       // Timer B current value (internal, updated during tick)
    u16 timer_a_read;  // Timer A value visible to CPU reads (captured at start of tick)
    u16 timer_b_read;  // Timer B value visible to CPU reads (captured at start of tick)
    u16 timer_a_latch; // Timer A reload value
    u16 timer_b_latch; // Timer B reload value
    u16 timer_a_pb6;   // Shadow counter for PB6 output (counts immediately)
    u16 timer_b_pb7;   // Shadow counter for PB7 output (1 cycle ahead of timer_b)

    // Timer pipeline delays (for cycle accuracy)
    u8 ta_delay;     // Cycles before Timer A register starts counting
    u8 pb6_delay;    // Cycles before PB6 output starts reacting (separate from ta_delay)
    u8 tb_delay;     // Cycles before Timer B starts counting
    u8 pb7_delay;    // Cycles before PB7 output starts reacting (separate from tb_delay)
    bool ta_started; // Timer A has started
    bool tb_started; // Timer B has started
    
    // Post-reload skip flags (COUNT3 pipeline effect)
    // After underflow+reload, the timer skips ONE decrement cycle
    bool ta_reload_skip; // Timer A will skip next decrement
    bool pb6_reload_skip; // Timer A PB6 shadow will skip next decrement
    bool tb_reload_skip; // Timer B will skip next decrement
    bool pb7_reload_skip; // Timer B PB7 shadow will skip next decrement
    
    // Pending load operations (2-cycle delay for force load)
    // 0 = no pending, 1 = load next cycle, 2 = load in 2 cycles
    u8 ta_load_delay; // Timer A force load delay counter
    u8 tb_load_delay; // Timer B force load delay counter
    
    // Pending stop operations (timer counts more cycles before stopping)
    // 0 = no pending, 1 = stop next cycle, 2 = stop in 2 cycles
    u8 ta_stop_delay; // Timer A will stop after this many cycles
    u8 tb_stop_delay; // Timer B will stop after this many cycles
    
    // Timer output to Port B (PB6 for Timer A, PB7 for Timer B)
    bool pb6_out; // Timer A output state (toggle mode) - internal
    bool pb7_out; // Timer B output state (toggle mode) - internal
    bool pb6_out_delayed; // Timer A output visible to reads (1 cycle delay)
    bool pb7_out_delayed; // Timer B output visible to reads (1 cycle delay)
    bool pb6_pulse; // Timer A pulse will be active (internal)
    bool pb7_pulse; // Timer B pulse will be active (internal)
    bool pb6_pulse_out; // Timer A pulse visible to reads
    bool pb7_pulse_out; // Timer B pulse visible to reads
    
    // Timer A underflow signal for cascade mode (Timer B counting Timer A)
    bool ta_underflow; // Timer A underflowed this cycle
    bool ta_underflow_delay; // Timer A underflow signal delayed by 1 cycle

    // Control registers
    u8 cra; // Control register A
    u8 crb; // Control register B

    // Interrupt registers
    u8 icr_data; // Actual interrupt flags
    u8 icr_mask; // Interrupt enable mask
    bool irq_pending;
    u8 irq_delay; // Counter for ICR bit 7 delay (2 cycles)
    bool icr_ack; // ICR was just read, inhibit irq_delay for 1 cycle

    // TOD clock (BCD format)
    u8 tod_10ths;
    u8 tod_sec;
    u8 tod_min;
    u8 tod_hr;

    // TOD latched values (for reading)
    u8 tod_latch_10ths;
    u8 tod_latch_sec;
    u8 tod_latch_min;
    u8 tod_latch_hr;
    bool tod_latched;

    // TOD alarm
    u8 alarm_10ths;
    u8 alarm_sec;
    u8 alarm_min;
    u8 alarm_hr;

    // TOD counter (divides phi2 to 10Hz)
    u32 tod_divider;

    // Serial data register
    u8 sdr;
    u8 sdr_bits;

    // Is this CIA1 or CIA2?
    int cia_num; // 1 or 2

    // Reference to system
    C64System *sys;
} C64Cia;

// CIA functions
void cia_init(C64Cia *cia, int cia_num, C64System *sys);
void cia_reset(C64Cia *cia);
void cia_clock(C64Cia *cia); // Advance one phi2 cycle

// Register access
u8 cia_read(C64Cia *cia, u8 reg);
void cia_write(C64Cia *cia, u8 reg, u8 value);

// Keyboard matrix (CIA1 specific)
void cia_set_key(C64Cia *cia, int row, int col, bool pressed);
u8 cia_read_keyboard(C64Cia *cia);

#endif // C64_CIA_H
