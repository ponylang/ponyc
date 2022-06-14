#include "systematic_testing.h"

#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "ponyassert.h"

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

#if defined(USE_SCHEDULER_SCALING_PTHREADS)
static pthread_mutex_t systematic_testing_mut;

static pthread_once_t systematic_testing_mut_once = PTHREAD_ONCE_INIT;

void systematic_testing_mut_init()
{
  pthread_mutex_init(&systematic_testing_mut, NULL);
}
#endif

void ponyint_systematic_testing_init(uint64_t random_seed, uint32_t max_threads)
{
  #if defined(USE_SCHEDULER_SCALING_PTHREADS)
  pthread_once(&systematic_testing_mut_once, systematic_testing_mut_init);
#endif

  if(0 == random_seed)
    random_seed = ponyint_cpu_tick();

  SYSTEMATIC_TESTING_PRINTF("Systematic testing using seed: %lu...\n", random_seed);
  SYSTEMATIC_TESTING_PRINTF("(rerun with `<app> --ponysystematictestingseed %lu` to reproduce)\n", random_seed);

  // TODO: maybe replace with better RNG?
  srand((int)random_seed);

  // initialize thead tracking array (should be max_threads + 1 to account for asio)
  // TODO: memtrack tracking for these allocations
  total_threads = max_threads + 1;
  threads_to_track = (systematic_testing_thread_t*)ponyint_pool_alloc_size(
    total_threads * sizeof(systematic_testing_thread_t));;
  memset(threads_to_track, 0, total_threads * sizeof(systematic_testing_thread_t));

  active_thread = threads_to_track;
}

void ponyint_systematic_testing_wait_start(pony_thread_id_t thread, pony_signal_event_t signal)
{
  SYSTEMATIC_TESTING_PRINTF("thread %lu: waiting to start...\n", thread);

  // sleep until it is this threads turn to do some work
#if defined(PLATFORM_IS_WINDOWS)
  while(active_thread->tid == thread)
#else
  while(0 == pthread_equal(active_thread->tid, thread))
#endif
  {
    SYSTEMATIC_TESTING_PRINTF("thread %lu: still waiting for it's turn... active thread: %lu\n", thread, active_thread->tid);
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
    ponyint_thread_suspend(signal, &systematic_testing_mut);
#else
    ponyint_thread_suspend(signal);
#endif
  }

  SYSTEMATIC_TESTING_PRINTF("thread %lu: started...\n", thread);
}

void ponyint_systematic_testing_start(scheduler_t* schedulers, pony_thread_id_t asio_thread, pony_signal_event_t asio_signal)
{
  threads_to_track[0].tid = asio_thread;
  threads_to_track[0].sleep_object = asio_signal;
  threads_to_track[0].stopped = false;

  for(uint32_t i = 1; i < total_threads; i++)
  {
    threads_to_track[i].tid = schedulers[i-1].tid;
    threads_to_track[i].sleep_object = schedulers[i-1].sleep_object;
    threads_to_track[i].stopped = false;
  }

  // always start the first scheduler thread (not asio which is 0)
  active_thread = &threads_to_track[1];

  SYSTEMATIC_TESTING_PRINTF("Starting systematic testing with thread %lu...\n", active_thread->tid);

  ponyint_thread_wake(active_thread->tid, active_thread->sleep_object);
}

static uint32_t get_next_index()
{
  uint32_t active_scheduler_count = pony_active_schedulers();
  uint32_t active_count = active_scheduler_count + 1; // account for asio
  uint32_t next_index = 0;
  do
  {
    next_index = rand() % active_count;
    pony_assert(next_index <= total_threads);
  }
  while (threads_to_track[next_index].stopped);

  return next_index;
}


void ponyint_systematic_testing_yield()
{
  if(stopped_threads == total_threads)
  {
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
    pthread_mutex_unlock(&systematic_testing_mut);
#endif
    return;
  }

  uint32_t next_index = get_next_index();

  systematic_testing_thread_t *next_thread = &threads_to_track[next_index];
  systematic_testing_thread_t *current_thread = active_thread;

#if defined(PLATFORM_IS_WINDOWS)
  if(active_thread->tid == next_thread->tid)
#else
  if(0 == pthread_equal(active_thread->tid, next_thread->tid))
#endif
  {
    active_thread = next_thread;

    SYSTEMATIC_TESTING_PRINTF("thread %lu: yielding to thread: %lu.. next_index: %u\n", current_thread->tid, active_thread->tid, next_index);

    ponyint_thread_wake(active_thread->tid, active_thread->sleep_object);

    if(!current_thread->stopped)
    {
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
      ponyint_thread_suspend(current_thread->sleep_object, &systematic_testing_mut);
#else
      ponyint_thread_suspend(current_thread->sleep_object);
#endif
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
  // asio is always the first thread
  return threads_to_track[0].stopped;
}

void ponyint_systematic_testing_stop_thread()
{
  active_thread->stopped = true;
  stopped_threads++;
  SYSTEMATIC_TESTING_PRINTF("thread %lu: stopped...\n", active_thread->tid);

  ponyint_systematic_testing_yield();
}
