#ifndef sched_systematic_testing_h
#define sched_systematic_testing_h

#include <platform.h>
#include "scheduler.h"

#if defined(USE_SYSTEMATIC_TESTING)
#if !defined(PLATFORM_IS_WINDOWS) && !defined(USE_SCHEDULER_SCALING_PTHREADS)
pony_static_assert(false, "Systematic testing requires pthreads (USE_SCHEDULER_SCALING_PTHREADS) to be enabled!");
#endif

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

void ponyint_systematic_testing_init(uint64_t random_seed, uint32_t max_threads, uint32_t verbosity);
void ponyint_systematic_testing_start(scheduler_t* schedulers, pony_thread_id_t asio_thread, pony_signal_event_t asio_signal);
void ponyint_systematic_testing_wait_start(pony_thread_id_t thread, pony_signal_event_t signal);
void ponyint_systematic_testing_yield();
bool ponyint_systematic_testing_asio_stopped();
void ponyint_systematic_testing_stop_thread();
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
void ponyint_systematic_testing_suspend(pthread_mutex_t* mut);
#else
void ponyint_systematic_testing_suspend();
#endif
uint32_t ponyint_systematic_testing_verbose_level();

#define SYSTEMATIC_TESTING_PRINTF(level, f_, ...) if((uint32_t)level <= ponyint_systematic_testing_verbose_level()) fprintf(stderr, (f_), ##__VA_ARGS__)
#define SYSTEMATIC_TESTING_INIT(RANDOM_SEED, MAX_THREADS, VERBOSITY) ponyint_systematic_testing_init(RANDOM_SEED, MAX_THREADS, VERBOSITY)
#define SYSTEMATIC_TESTING_START(SCHEDULERS, ASIO_THREAD, ASIO_SLEEP_OBJECT) ponyint_systematic_testing_start(SCHEDULERS, ASIO_THREAD, ASIO_SLEEP_OBJECT)
#define SYSTEMATIC_TESTING_WAIT_START(THREAD, SIGNAL) ponyint_systematic_testing_wait_start(THREAD, SIGNAL)
#define SYSTEMATIC_TESTING_YIELD() ponyint_systematic_testing_yield()
#define SYSTEMATIC_TESTING_ASIO_STOPPED() ponyint_systematic_testing_asio_stopped()
#define SYSTEMATIC_TESTING_STOP_THREAD() ponyint_systematic_testing_stop_thread()
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
#define SYSTEMATIC_TESTING_SUSPEND(MUT) ponyint_systematic_testing_suspend(MUT)
#else
#define SYSTEMATIC_TESTING_SUSPEND() ponyint_systematic_testing_suspend()
#endif

PONY_EXTERN_C_END

#else

#define SYSTEMATIC_TESTING_PRINTF(level, f_, ...)
#define SYSTEMATIC_TESTING_INIT(RANDOM_SEED, MAX_THREADS, VERBOSITY)
#define SYSTEMATIC_TESTING_START(SCHEDULERS, ASIO_THREAD, ASIO_SLEEP_OBJECT)
#define SYSTEMATIC_TESTING_WAIT_START(THREAD, SIGNAL)
#define SYSTEMATIC_TESTING_YIELD()
#define SYSTEMATIC_TESTING_ASIO_STOPPED()
#define SYSTEMATIC_TESTING_STOP_THREAD()
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
#define SYSTEMATIC_TESTING_SUSPEND(MUT)
#else
#define SYSTEMATIC_TESTING_SUSPEND()
#endif

#endif

#endif
