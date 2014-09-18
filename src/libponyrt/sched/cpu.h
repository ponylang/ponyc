#ifndef sched_cpu_h
#define sched_cpu_h

#include <stdint.h>
#include <stdbool.h>

void cpu_count(uint32_t* physical, uint32_t* logical);

bool cpu_physical(uint32_t cpu);

void cpu_affinity(uint32_t cpu);

uint64_t cpu_rdtsc();

bool cpu_core_pause(uint64_t tsc);

#endif
