#ifndef CIA_H
#define CIA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// CIA Register Addresses

// CIA1 (at $DC00-$DCFF)
#define CIA1_PRA      0xDC00  // Port A Data Register
#define CIA1_PRB      0xDC01  // Port B Data Register
#define CIA1_DDRA     0xDC02  // Data Direction Register A
#define CIA1_DDRB     0xDC03  // Data Direction Register B
#define CIA1_TALO     0xDC04  // Timer A Low Byte
#define CIA1_TAHI     0xDC05  // Timer A High Byte
#define CIA1_TBLO     0xDC06  // Timer B Low Byte
#define CIA1_TBHI     0xDC07  // Timer B High Byte
#define CIA1_TOD10TH  0xDC08  // Time of Day 10ths of a Second
#define CIA1_TODSEC   0xDC09  // Time of Day Seconds
#define CIA1_TODMIN   0xDC0A  // Time of Day Minutes
#define CIA1_TODHR    0xDC0B  // Time of Day Hours
#define CIA1_SDR      0xDC0C  // Serial Data Register
#define CIA1_ICR      0xDC0D  // Interrupt Control Register
#define CIA1_CRA      0xDC0E  // Control Register A
#define CIA1_CRB      0xDC0F  // Control Register B

// CIA2 (at $DD00-$DDFF)
#define CIA2_PRA      0xDD00  // Port A Data Register
#define CIA2_PRB      0xDD01  // Port B Data Register
#define CIA2_DDRA     0xDD02  // Data Direction Register A
#define CIA2_DDRB     0xDD03  // Data Direction Register B
#define CIA2_TALO     0xDD04  // Timer A Low Byte
#define CIA2_TAHI     0xDD05  // Timer A High Byte
#define CIA2_TBLO     0xDD06  // Timer B Low Byte
#define CIA2_TBHI     0xDD07  // Timer B High Byte
#define CIA2_TOD10TH  0xDD08  // Time of Day 10ths of a Second
#define CIA2_TODSEC   0xDD09  // Time of Day Seconds
#define CIA2_TODMIN   0xDD0A  // Time of Day Minutes
#define CIA2_TODHR    0xDD0B  // Time of Day Hours
#define CIA2_SDR      0xDD0C  // Serial Data Register
#define CIA2_ICR      0xDD0D  // Interrupt Control Register
#define CIA2_CRA      0xDD0E  // Control Register A
#define CIA2_CRB      0xDD0F  // Control Register B

// CIA Control Register A Bits ($DC0E/$DD0E)
#define CIA_CR_START  0x01    // Bit 0: Start timer
#define CIA_CR_PBON   0x02    // Bit 1: Output port B on count
#define CIA_CR_OUTMODE 0x04   // Bit 2: Output mode (0=one-shot, 1=continuous)
#define CIA_CR_RUNMODE 0x08   // Bit 3: Run mode (0=timer, 1= TOD)
#define CIA_CR_LOAD   0x10    // Bit 4: Force load
#define CIA_CR_INMODE 0x20    // Bit 5: Input mode (0=phi2, 0=cnt)
#define CIA_CR_SPMODE 0x40    // Bit 6: Serial port mode
#define CIA_CR_TODIN  0x80    // Bit 7: TOD 50/60 Hz

// CIA Control Register B Bits ($DC0F/$DD0F)
#define CIA_CRB_START 0x01    // Bit 0: Start timer
#define CIA_CRB_PBON  0x02    // Bit 1: Output port B on count
#define CIA_CRB_OUTMODE 0x04  // Bit 2: Output mode (0=one-shot, 1=continuous)
#define CIA_CRB_RUNMODE 0x08  // Bit 3: Run mode (0=timer, 1=alarm)
#define CIA_CRB_LOAD  0x10    // Bit 4: Force load
#define CIA_CRB_ALARM 0x80    // Bit 7: Alarm write enable

// CIA Interrupt Control Register Bits ($DC0D/$DD0D)
#define CIA_ICR_TA    0x01    // Bit 0: Timer A interrupt
#define CIA_ICR_TB    0x02    // Bit 1: Timer B interrupt
#define CIA_ICR_TOD   0x04    // Bit 2: TOD interrupt
#define CIA_ICR_SDR   0x08    // Bit 3: Serial port interrupt
#define CIA_ICR_FLAG  0x80    // Bit 7: Interrupt flag (read-only, 1=IRQ occurred)
#define CIA_ICR_SET   0x80    // Bit 7: Set/clear bit (1=set, 0=clear)

// CIA Port A bits (CIA1 - keyboard matrix)
#define CIA1_PA_ROW0  0x01    // Row 0
#define CIA1_PA_ROW1  0x02    // Row 1
#define CIA1_PA_ROW2  0x04    // Row 2
#define CIA1_PA_ROW3  0x08    // Row 3
#define CIA1_PA_ROW4  0x10    // Row 4
#define CIA1_PA_ROW5  0x20    // Row 5
#define CIA1_PA_ROW6  0x40    // Row 6
#define CIA1_PA_ROW7  0x80    // Row 7

// CIA Port B bits (CIA1 - keyboard matrix)
#define CIA1_PB_COL0  0x01    // Column 0
#define CIA1_PB_COL1  0x02    // Column 1
#define CIA1_PB_COL2  0x04    // Column 2
#define CIA1_PB_COL3  0x08    // Column 3
#define CIA1_PB_COL4  0x10    // Column 4
#define CIA1_PB_COL5  0x20    // Column 5
#define CIA1_PB_COL6  0x40    // Column 6
#define CIA1_PB_COL7  0x80    // Column 7

// CIA2 Port A bits (VIC memory bank control)
#define CIA2_PA_VM0   0x01    // Bit 0: VIC memory bank bit 0
#define CIA2_PA_VM1   0x02    // Bit 1: VIC memory bank bit 1
#define CIA2_PA_VM12  0x03    // Bits 0-1: VIC memory bank
#define CIA2_PA_VM13  0x04    // Bit 2: VIC memory bank bit 2 (13)
#define CIA2_PA_CASS  0x08    // Bit 3: Cassette motor
#define CIA2_PA_CASSW 0x10    // Bit 4: Cassette write line
#define CIA2_PA_RS232 0x20    // Bit 5: RS-232 TXD output
#define CIA2_PA_CLK   0x40    // Bit 6: Serial clock output
#define CIA2_PA_DATA  0x80    // Bit 7: Serial data output

typedef struct {
    // CIA Registers
    uint8_t pra;      // Port A Data Register
    uint8_t prb;      // Port B Data Register
    uint8_t ddra;     // Data Direction Register A
    uint8_t ddrb;     // Data Direction Register B
    
    uint16_t timer_a_latch;    // Timer A latch
    uint16_t timer_a;          // Timer A current value
    uint16_t timer_b_latch;    // Timer B latch
    uint16_t timer_b;          // Timer B current value
    
    uint8_t tod_10th;  // Time of Day 10ths
    uint8_t tod_sec;   // Time of Day seconds
    uint8_t tod_min;   // Time of Day minutes
    uint8_t tod_hr;    // Time of Day hours
    uint8_t tod_alarm_10th;  // TOD alarm 10ths
    uint8_t tod_alarm_sec;   // TOD alarm seconds
    uint8_t tod_alarm_min;   // TOD alarm minutes
    uint8_t tod_alarm_hr;    // TOD alarm hours
    
    uint8_t sdr;      // Serial Data Register
    uint8_t icr;      // Interrupt Control Register
    uint8_t cra;      // Control Register A
    uint8_t crb;      // Control Register B
    
    // Internal state
    bool timer_a_running;
    bool timer_b_running;
    bool timer_a_one_shot;
    bool timer_b_one_shot;
    bool timer_a_underflow;
    bool timer_b_underflow;
    
    bool tod_running;
    bool tod_50hz;
    
    uint8_t serial_shift_reg;
    uint8_t serial_bit_count;
    bool serial_output_enabled;
    
    // Interrupt state
    bool irq_pending;
    uint8_t pending_interrupts;
    
    // Callbacks for I/O
    uint8_t (*port_a_read)(void *context);
    void (*port_a_write)(void *context, uint8_t data);
    uint8_t (*port_b_read)(void *context);
    void (*port_b_write)(void *context, uint8_t data);
    void *io_context;
    
    uint16_t base_address;  // CIA base address (0xDC00 or 0xDD00)
} CIA;

// CIA Functions
void cia_init(CIA *cia, uint16_t base_address);
void cia_reset(CIA *cia);
uint8_t cia_read(CIA *cia, uint16_t addr);
void cia_write(CIA *cia, uint16_t addr, uint8_t data);
void cia_clock(CIA *cia);
void cia_clock_phi2(CIA *cia);
bool cia_get_irq(CIA *cia);

// Timer functions
void cia_timer_a_start(CIA *cia);
void cia_timer_a_stop(CIA *cia);
void cia_timer_a_load(CIA *cia, uint16_t value);
void cia_timer_b_start(CIA *cia);
void cia_timer_b_stop(CIA *cia);
void cia_timer_b_load(CIA *cia, uint16_t value);

// TOD functions
void cia_tod_start(CIA *cia);
void cia_tod_stop(CIA *cia);
void cia_tod_set(CIA *cia, uint8_t hr, uint8_t min, uint8_t sec, uint8_t tenth);
void cia_tod_set_alarm(CIA *cia, uint8_t hr, uint8_t min, uint8_t sec, uint8_t tenth);

// Serial port functions
void cia_serial_write(CIA *cia, uint8_t data);
uint8_t cia_serial_read(CIA *cia);

// I/O callbacks
void cia_set_io_callbacks(CIA *cia, void *context,
                          uint8_t (*port_a_read)(void *context),
                          void (*port_a_write)(void *context, uint8_t data),
                          uint8_t (*port_b_read)(void *context),
                          void (*port_b_write)(void *context, uint8_t data));

// Internal functions
void cia_timer_a_underflow(CIA *cia);
void cia_timer_b_underflow(CIA *cia);
void cia_clock_tod(CIA *cia);
void cia_clock_serial(CIA *cia);
void cia_update_irq(CIA *cia);

// CIA1 specific functions
uint8_t cia1_keyboard_scan(CIA *cia1);
uint8_t cia1_joystick_read(CIA *cia1, uint8_t joystick_num);

// CIA2 specific functions
uint8_t cia2_get_vic_bank(CIA *cia2);
void cia2_set_vic_bank(CIA *cia2, uint8_t bank);

#endif // CIA_H