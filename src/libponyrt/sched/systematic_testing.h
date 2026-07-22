#ifndef sched_systematic_testing_h
#define sched_systematic_testing_h

#include <platform.h>
#include "scheduler.h"

#if defined(USE_SYSTEMATIC_TESTING)

#include <stdio.h>

PONY_EXTERN_C_BEGIN

#ifdef USE_RUNTIMESTATS
/** Get the static memory used by the systematic testing subsystem.
 */
size_t ponyint_systematic_testing_static_mem_size();

/** Get the static memory allocated by the systematic testing subsystem.
 */
size_t ponyint_systematic_testing_static_alloc_size();
#endif

void ponyint_systematic_testing_init(uint64_t random_seed, uint32_t max_threads);
void ponyint_systematic_testing_start();
void ponyint_systematic_testing_wait_start(int32_t scheduler_index);
void ponyint_systematic_testing_yield();
// Current value of the systematic-testing logical clock (see the .c file). Used
// by the scheduler in place of ponyint_cpu_tick() on the clock-gated control
// paths so those decisions are a deterministic function of the seed.
uint64_t ponyint_systematic_testing_clock();
void ponyint_systematic_testing_stop_thread();

// TODO systematic testing: maybe make output conditional based on a `verbosity` argument that is CLI driven?
#define SYSTEMATIC_TESTING_PRINTF(f_, ...) fprintf(stderr, (f_), ##__VA_ARGS__)
#define SYSTEMATIC_TESTING_INIT(RANDOM_SEED, MAX_THREADS) ponyint_systematic_testing_init(RANDOM_SEED, MAX_THREADS)
#define SYSTEMATIC_TESTING_START() ponyint_systematic_testing_start()
#define SYSTEMATIC_TESTING_WAIT_START(SCHEDULER_INDEX) ponyint_systematic_testing_wait_start(SCHEDULER_INDEX)
#define SYSTEMATIC_TESTING_YIELD() ponyint_systematic_testing_yield()
#define SYSTEMATIC_TESTING_CLOCK() ponyint_systematic_testing_clock()
#define SYSTEMATIC_TESTING_STOP_THREAD() ponyint_systematic_testing_stop_thread()

PONY_EXTERN_C_END

#else

#define SYSTEMATIC_TESTING_PRINTF(f_, ...)
#define SYSTEMATIC_TESTING_INIT(RANDOM_SEED, MAX_THREADS)
#define SYSTEMATIC_TESTING_START()
#define SYSTEMATIC_TESTING_WAIT_START(SCHEDULER_INDEX)
#define SYSTEMATIC_TESTING_YIELD()
#define SYSTEMATIC_TESTING_CLOCK() (0)
#define SYSTEMATIC_TESTING_STOP_THREAD()

#endif

#endif
