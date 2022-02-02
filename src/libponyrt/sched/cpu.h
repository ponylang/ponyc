#ifndef sched_cpu_h
#define sched_cpu_h

#include "scheduler.h"
#include <stdint.h>
#include <stdbool.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

void ponyint_cpu_init();

uint32_t ponyint_cpu_count();

uint32_t ponyint_cpu_assign(uint32_t count, scheduler_t* scheduler,
  bool nopin, bool pinasio);

void ponyint_cpu_affinity(uint32_t cpu);

void ponyint_cpu_core_pause(uint64_t tsc, uint64_t tsc2);

void ponyint_cpu_relax();

uint64_t ponyint_cpu_tick();

uint64_t ponyint_cpu_tick_diff(uint64_t supposedly_earlier,
  uint64_t supposedly_later);

PONY_EXTERN_C_END

#endif
