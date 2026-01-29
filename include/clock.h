#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

#define PHASE_PHI1 0
#define PHASE_PHI2 1

typedef struct
{
    uint64_t clock_currentTime;

    double clock_cyclesPerSecond;
} clock_t;

void clock_init(clock_t *clock, double cyclesPerSecond);
void clock_reset(clock_t *clock);

void clock_step(clock_t *clock);

uint64_t clock_getTime(clock_t *clock, uint32_t phase);
uint32_t clock_getPhase(clock_t *clock);
uint64_t clock_getTimeAndPhase(clock_t *clock);

#endif // CLOCK_H