/*
 * C64 Emulator - Clock Module Implementation
 */

#include "clock.h"

void clock_init(Clock *clock, double cycles_per_second)
{
    clock->current_time = 0;
    clock->cycles_per_second = cycles_per_second;
}

void clock_reset(Clock *clock)
{
    clock->current_time = 0;
}

void clock_step(Clock *clock, int cycles)
{
    clock->current_time += cycles;
}

uint64_t clock_get_time(Clock *clock, uint32_t phase)
{
    /* Each cycle has two phases, so multiply by 2 and add phase */
    return (clock->current_time * 2) + phase;
}

uint32_t clock_get_phase(Clock *clock)
{
    /* Alternate between PHI1 and PHI2 each half-cycle */
    return (clock->current_time & 1) ? PHASE_PHI2 : PHASE_PHI1;
}

uint64_t clock_get_time_and_phase(Clock *clock)
{
    return clock_get_time(clock, clock_get_phase(clock));
}
