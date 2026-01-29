#include "clock.h"

#define MAX_TIME 9223372036854775807
#define MIN_TIME -9223372036854775807

void clock_init(clock_t *clock, double cyclesPerSecond)
{
    clock->clock_cyclesPerSecond = cyclesPerSecond;

    clock_reset(clock);
}

void clock_reset(clock_t *clock)
{
    clock->clock_currentTime = 0;
}

void clock_step(clock_t *clock)
{
      uint64_t time = clock_getTimeAndPhase(clock);

}

uint64_t clock_getTime(clock_t *clock, uint32_t phase)
{
    return (clock->clock_currentTime + (phase == PHASE_PHI1 ? 1 : 0)) / 2;
}

uint32_t clock_getPhase(clock_t *clock) {
  return (clock->clock_currentTime & 1) == 0 ? PHASE_PHI1 : PHASE_PHI2;
}

uint64_t clock_getTimeAndPhase(clock_t *clock) {
  return clock->clock_currentTime;
}
