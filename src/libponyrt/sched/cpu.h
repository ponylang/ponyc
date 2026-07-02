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
  bool nopin, bool pinasio, bool pinpat, bool pin_tracing_thread,
  uint32_t* tracing_cpu);

void ponyint_cpu_affinity(uint32_t cpu);

void ponyint_cpu_core_pause(uint64_t tsc, uint64_t tsc2, bool yield);

void ponyint_cpu_relax();

uint64_t ponyint_cpu_tick();

uint64_t ponyint_cpu_tick_diff(uint64_t supposedly_earlier,
  uint64_t supposedly_later);

// Elapsed ticks between two ponyint_cpu_tick() readings for a counter that is
// `bits` wide, correct across a single wrap. Exposed (rather than file-local)
// so the wrap arithmetic can be unit-tested at widths other than the host's;
// ponyint_cpu_tick_diff() is the platform-width wrapper around it.
uint64_t ponyint_cpu_tick_diff_bits(uint64_t supposedly_earlier,
  uint64_t supposedly_later, uint32_t bits);

PONY_EXTERN_C_END

#endif
