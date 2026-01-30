/*
 * C64 Emulator - Clock Module
 * 
 * Provides cycle-accurate timing for the emulator.
 * PAL C64 runs at 985248 Hz, NTSC at 1022727 Hz.
 */

#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

#define PHASE_PHI1 0
#define PHASE_PHI2 1

typedef struct {
    uint64_t current_time;      // Current cycle count
    double cycles_per_second;   // Clock frequency
} Clock;

/* Initialize clock with given frequency */
void clock_init(Clock *clock, double cycles_per_second);

/* Reset clock to initial state */
void clock_reset(Clock *clock);

/* Advance clock by given number of cycles */
void clock_step(Clock *clock, int cycles);

/* Get current time for given phase */
uint64_t clock_get_time(Clock *clock, uint32_t phase);

/* Get current phase (PHI1 or PHI2) */
uint32_t clock_get_phase(Clock *clock);

/* Get time and phase combined */
uint64_t clock_get_time_and_phase(Clock *clock);

#endif /* CLOCK_H */
