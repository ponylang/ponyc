#define PONY_WANT_ATOMIC_DEFS

#include "systematic_testing.h"

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

  if(0 == random_seed)
    random_seed = ponyint_cpu_tick();

  SYSTEMATIC_TESTING_PRINTF("Systematic testing using seed: %lu...\n", random_seed);
  SYSTEMATIC_TESTING_PRINTF("(rerun with `<app> --ponysystematictestingseed %lu` to reproduce)\n", random_seed);
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
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
      ponyint_thread_suspend(current_thread->sleep_object, &systematic_testing_mut);
#else
      ponyint_thread_suspend(current_thread->sleep_object);
#endif
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
