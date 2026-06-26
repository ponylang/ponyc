#define PONY_WANT_ATOMIC_DEFS

#include "systematic_testing.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "ponyassert.h"
#include "../tracing/tracing.h"

typedef struct systematic_testing_thread_t
{
  pony_thread_id_t tid;
  pony_signal_event_t sleep_object;
  bool stopped;
} systematic_testing_thread_t;

static systematic_testing_thread_t* active_thread = NULL;
static systematic_testing_thread_t* threads_to_track = NULL;
static uint32_t total_threads = 0;
static uint32_t stopped_threads = 0;
static PONY_ATOMIC(uint32_t) waiting_to_start_count;

// Logical clock for systematic testing.
//
// Under systematic testing the wall clock is meaningless because execution is
// serialized to one thread at a time; a thread sits parked waiting its turn
// while real time keeps advancing. The scheduler's clock-gated decisions (when
// to send SCHED_BLOCK, when a thread becomes suspend-eligible, the cycle
// detector cadence) must therefore run off this deterministic step counter
// instead of ponyint_cpu_tick(), or the spin where a threshold trips moves run
// to run and desyncs the seeded rand() stream that picks the next thread.
//
// The clock advances by a fixed number of pseudo-cycles once per yield (one
// "scheduling step"). The increment scales logical steps onto the existing
// cycle-scale thresholds (block/suspend at 2,000,000; cycle detector at
// detect_interval * 2,000,000) so those thresholds keep their meaning and CLI
// configurability, now measured in scheduling steps rather than CPU cycles.
//
// Only the active thread runs at any moment, so no synchronization is needed.
#define SYSTEMATIC_TESTING_TICK_PER_YIELD 20000
static uint64_t logical_clock = 0;

#if defined(USE_SCHEDULER_SCALING_PTHREADS)
static pthread_mutex_t systematic_testing_mut;

static pthread_once_t systematic_testing_mut_once = PTHREAD_ONCE_INIT;

void systematic_testing_mut_init()
{
  pthread_mutex_init(&systematic_testing_mut, NULL);
}
#endif

#ifdef USE_RUNTIMESTATS
// holds only size of systematic_testing_thread_t array
static size_t mem_allocated;
static size_t mem_used;

/** Get the static memory used by the systematic testing subsystem.
 */
size_t ponyint_systematic_testing_static_mem_size()
{
  return mem_used;
}

/** Get the static memory allocated by the systematic testing subsystem.
 */
size_t ponyint_systematic_testing_static_alloc_size()
{
  return mem_allocated;
}
#endif

void ponyint_systematic_testing_init(uint64_t random_seed, uint32_t max_threads)
{
  #if defined(USE_SCHEDULER_SCALING_PTHREADS)
  pthread_once(&systematic_testing_mut_once, systematic_testing_mut_init);
#endif

  atomic_store_explicit(&waiting_to_start_count, 0, memory_order_relaxed);

  logical_clock = 0;

  // NB: when no seed is given (random_seed == 0) we intentionally read the real
  // wall clock for entropy. The logical clock is 0 here and would make the
  // auto-chosen seed always 0; reproducibility is moot when no seed was asked
  // for. A given seed never reaches this branch, so determinism is unaffected.
  if(0 == random_seed)
    random_seed = ponyint_cpu_tick();

  SYSTEMATIC_TESTING_PRINTF("Systematic testing using seed: %" PRIu64 "...\n", random_seed);
  SYSTEMATIC_TESTING_PRINTF("(rerun with `<app> --ponysystematictestingseed %" PRIu64 "` to reproduce)\n", random_seed);
  TRACING_SYSTEMATIC_TESTING_CONFIG(random_seed);

  // TODO systematic testing: maybe replace with better RNG?
  // probably best to replace this with a RNG that has the exact same behavior
  // on all platform so we can deterministically reproduce the same thread
  // interleaving everywhere if needed
  srand((int)random_seed);

  // initialize thead tracking array (should be max_threads + 2 to account for asio and pinned actor threads)
  total_threads = max_threads + 2;
  size_t mem_needed = total_threads * sizeof(systematic_testing_thread_t);
  threads_to_track = (systematic_testing_thread_t*)ponyint_pool_alloc_size(
    mem_needed);
  memset(threads_to_track, 0, mem_needed);

#ifdef USE_RUNTIMESTATS
  mem_used += (mem_needed);
  mem_allocated += (ponyint_pool_used_size(mem_needed));
#endif

  active_thread = threads_to_track;
}

void ponyint_systematic_testing_wait_start(pony_thread_id_t thread, pony_signal_event_t signal)
{
  TRACING_SYSTEMATIC_TESTING_WAITING_TO_START_BEGIN();

#if defined(USE_SCHEDULER_SCALING_PTHREADS)
  pthread_mutex_lock(&systematic_testing_mut);
#endif

  atomic_fetch_add_explicit(&waiting_to_start_count, 1, memory_order_relaxed);

  // sleep until it is this threads turn to do some work
#if defined(PLATFORM_IS_WINDOWS)
  while(active_thread->tid != thread)
#else
  while(0 == pthread_equal(active_thread->tid, thread))
#endif
  {
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
    ponyint_thread_suspend(signal, &systematic_testing_mut);
#else
    ponyint_thread_suspend(signal);
#endif
  }

  TRACING_SYSTEMATIC_TESTING_WAITING_TO_START_END();
  TRACING_SYSTEMATIC_TESTING_TIMESLICE_BEGIN();
}

void ponyint_systematic_testing_start(scheduler_t* schedulers, pony_thread_id_t asio_thread, pony_signal_event_t asio_signal, pony_thread_id_t pinned_actor_thread, pony_signal_event_t pinned_actor_signal)
{
  threads_to_track[0].tid = pinned_actor_thread;
  threads_to_track[0].sleep_object = pinned_actor_signal;
  threads_to_track[0].stopped = false;

  threads_to_track[1].tid = asio_thread;
  threads_to_track[1].sleep_object = asio_signal;
  threads_to_track[1].stopped = false;

  for(uint32_t i = 2; i < total_threads; i++)
  {
    threads_to_track[i].tid = schedulers[i-2].tid;
    threads_to_track[i].sleep_object = schedulers[i-2].sleep_object;
    threads_to_track[i].stopped = false;
  }

  TRACING_SYSTEMATIC_TESTING_WAITING_TO_START_BEGIN();

  while((total_threads - 1) != atomic_load_explicit(&waiting_to_start_count, memory_order_relaxed))
  {
    ponyint_cpu_core_pause(1, 10000002, true);
  }

#if defined(USE_SCHEDULER_SCALING_PTHREADS)
  pthread_mutex_lock(&systematic_testing_mut);
#endif

  // always start the first scheduler thread (not asio which is 1 nor the pinned actor thread which is 0)
  active_thread = &threads_to_track[2];

  TRACING_SYSTEMATIC_TESTING_STARTED();

  ponyint_thread_wake(active_thread->tid, active_thread->sleep_object);

#if defined(USE_SCHEDULER_SCALING_PTHREADS)
  ponyint_thread_suspend(threads_to_track[0].sleep_object, &systematic_testing_mut);
#else
  ponyint_thread_suspend(threads_to_track[0].sleep_object);
#endif

  TRACING_SYSTEMATIC_TESTING_WAITING_TO_START_END();
  TRACING_SYSTEMATIC_TESTING_TIMESLICE_BEGIN();
}

uint64_t ponyint_systematic_testing_clock()
{
  return logical_clock;
}

static uint32_t get_next_index()
{
  uint32_t active_scheduler_count = pony_active_schedulers();
  bool pinned_actor_scheduler_suspended = ponyint_get_pinned_actor_scheduler_suspended();
  uint32_t active_count = active_scheduler_count + 1; // account for asio thread
  // account for pinned actor thread if it is not suspended
  if(!pinned_actor_scheduler_suspended)
    active_count = active_count + 1;

  uint32_t next_index = -1;
  do
  {
    next_index = rand() % active_count;

    // skip over pinned actor thread index if it is suspended
    if(pinned_actor_scheduler_suspended)
      next_index = next_index + 1;

    pony_assert(next_index <= total_threads);
  }
  while (threads_to_track[next_index].stopped);

  return next_index;
}


void ponyint_systematic_testing_yield()
{
  // Advance the logical clock once per scheduling step. This is the single
  // choke point every yield flows through, so it is the natural place to tick.
  logical_clock += SYSTEMATIC_TESTING_TICK_PER_YIELD;

  if(stopped_threads == total_threads)
  {
    size_t mem_needed = total_threads * sizeof(systematic_testing_thread_t);
    ponyint_pool_free_size(mem_needed, threads_to_track);
#ifdef USE_RUNTIMESTATS
    mem_used -= (mem_needed);
    mem_allocated -= (ponyint_pool_used_size(mem_needed));
#endif
    active_thread = NULL;
    threads_to_track = NULL;
    total_threads = 0;
    stopped_threads = 0;
    logical_clock = 0;
    atomic_store_explicit(&waiting_to_start_count, 0, memory_order_relaxed);

#if defined(USE_SCHEDULER_SCALING_PTHREADS)
    pthread_mutex_unlock(&systematic_testing_mut);
#endif

    TRACING_SYSTEMATIC_TESTING_TIMESLICE_END();
    SYSTEMATIC_TESTING_PRINTF("Systematic testing successfully finished!\n");
    TRACING_SYSTEMATIC_TESTING_FINISHED();

    return;
  }

  uint32_t next_index = get_next_index();

  systematic_testing_thread_t *next_thread = &threads_to_track[next_index];
  systematic_testing_thread_t *current_thread = active_thread;

#if defined(PLATFORM_IS_WINDOWS)
  if(active_thread->tid != next_thread->tid)
#else
  if(0 == pthread_equal(active_thread->tid, next_thread->tid))
#endif
  {
    active_thread = next_thread;

    TRACING_SYSTEMATIC_TESTING_TIMESLICE_END();

    ponyint_thread_wake(active_thread->tid, active_thread->sleep_object);

    if(!current_thread->stopped)
    {
      // Park until it is genuinely this thread's turn again. The underlying
      // wait can return without a matching wake -- pthread_cond_wait is allowed
      // to wake spuriously (POSIX), and a stray wake is more likely under load.
      // Returning here while active_thread still points at another thread would
      // let this thread run out of turn and desync the single-runner handoff,
      // deadlocking every thread. Re-check and re-park, as wait_start does.
      while(active_thread != current_thread)
      {
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
        ponyint_thread_suspend(current_thread->sleep_object, &systematic_testing_mut);
#else
        ponyint_thread_suspend(current_thread->sleep_object);
#endif
      }
      TRACING_SYSTEMATIC_TESTING_TIMESLICE_BEGIN();
    }
    else
    {
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
      pthread_mutex_unlock(&systematic_testing_mut);
#endif
    }
  }
}

#if defined(USE_SCHEDULER_SCALING_PTHREADS)
void ponyint_systematic_testing_suspend(pthread_mutex_t* mut)
#else
void ponyint_systematic_testing_suspend()
#endif
{
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
  // unlock mutex as `pthread_suspend` would before suspend
  pthread_mutex_unlock(mut);
#endif

  ponyint_systematic_testing_yield();

#if defined(USE_SCHEDULER_SCALING_PTHREADS)
  // lock mutex as `pthread_suspend` would after resume
  pthread_mutex_lock(mut);
#endif
}

bool ponyint_systematic_testing_asio_stopped()
{
  // asio is always the second thread
  return threads_to_track[1].stopped;
}

void ponyint_systematic_testing_stop_thread()
{
  active_thread->stopped = true;
  stopped_threads++;

  ponyint_systematic_testing_yield();
}
