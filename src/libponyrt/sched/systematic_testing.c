#define PONY_WANT_ATOMIC_DEFS

#include "systematic_testing.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "../tracing/tracing.h"

typedef struct systematic_testing_thread_t
{
#if !defined(PLATFORM_IS_WINDOWS)
  /// Signaled when this slot becomes the active thread. Owned by this
  /// subsystem: created at init, destroyed when the last thread stops.
  pthread_cond_t turn;
#endif
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
// to send SCHED_BLOCK, when a thread becomes eligible to go passive, the cycle
// detector cadence) must therefore run off this deterministic step counter
// instead of ponyint_cpu_tick(), or the spin where a threshold trips moves run
// to run and desyncs the seeded rand() stream that picks the next thread.
//
// The clock advances by a fixed number of pseudo-cycles once per yield (one
// "scheduling step"). The increment scales logical steps onto the existing
// cycle-scale thresholds (block/passive-eligibility at 2,000,000; cycle
// detector at detect_interval * 2,000,000) so those thresholds keep their
// meaning and CLI configurability, now measured in scheduling steps rather
// than CPU cycles.
//
// Only the active thread runs at any moment. That invariant is not free: the
// handoff (pick the next thread, wake it, park self) must be mutually
// exclusive, or the waker and the thread it just woke run concurrently and race
// on the handoff state -- including a use-after-free when the last thread frees
// threads_to_track in the cleanup branch while the waker is still reading it.
// The lock below (pthreads mutex, or the Windows SRW lock) is what enforces the
// invariant: the running thread holds it throughout and releases it only while
// parked in the condition-variable wait, so the woken thread cannot proceed
// until the waker is genuinely waiting.
#define SYSTEMATIC_TESTING_TICK_PER_YIELD 20000
static uint64_t logical_clock = 0;

#if defined(PLATFORM_IS_WINDOWS)
// The Windows arm gets the native analogue of the pthreads mutex +
// per-thread condvars: an SRW lock plus a single shared condition variable. A
// wake targets no particular thread -- WakeAllConditionVariable wakes every
// parked thread and each re-checks `active_thread == self`, so only the
// intended one proceeds and the rest re-park. Both are statically initialized,
// so no setup or teardown is needed. The condition-variable wait is not
// alertable: systematic testing starts no ASIO thread and does no I/O, so
// there are no APCs to service.
static SRWLOCK systematic_testing_win_lock = SRWLOCK_INIT;
static CONDITION_VARIABLE systematic_testing_win_cv = CONDITION_VARIABLE_INIT;
#else
static pthread_mutex_t systematic_testing_mut;

static pthread_once_t systematic_testing_mut_once = PTHREAD_ONCE_INIT;

static void systematic_testing_mut_init()
{
  pthread_mutex_init(&systematic_testing_mut, NULL);
}
#endif

/// Acquire the single-runner lock. Held by the running thread for its
/// whole timeslice and released only while parked.
static void systematic_testing_lock()
{
#if defined(PLATFORM_IS_WINDOWS)
  AcquireSRWLockExclusive(&systematic_testing_win_lock);
#else
  pthread_mutex_lock(&systematic_testing_mut);
#endif
}

static void systematic_testing_unlock()
{
#if defined(PLATFORM_IS_WINDOWS)
  ReleaseSRWLockExclusive(&systematic_testing_win_lock);
#else
  pthread_mutex_unlock(&systematic_testing_mut);
#endif
}

/// Park the calling thread until woken, releasing the single-runner
/// lock while parked and holding it again on return. The wait can
/// return without a matching wake (POSIX allows spurious returns, and
/// the Windows arm wakes every parked thread on purpose); callers
/// re-check `active_thread` and re-park in a loop.
static void systematic_testing_park(systematic_testing_thread_t* self)
{
#if defined(PLATFORM_IS_WINDOWS)
  (void)self;
  SleepConditionVariableSRW(&systematic_testing_win_cv,
    &systematic_testing_win_lock, INFINITE, 0);
#else
  pthread_cond_wait(&self->turn, &systematic_testing_mut);
#endif
}

/// Wake the target thread. The caller holds the single-runner lock, so
/// the woken thread proceeds only once the caller parks or unlocks.
static void systematic_testing_wake(systematic_testing_thread_t* target)
{
#if defined(PLATFORM_IS_WINDOWS)
  (void)target;
  WakeAllConditionVariable(&systematic_testing_win_cv);
#else
  pthread_cond_signal(&target->turn);
#endif
}

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
#if !defined(PLATFORM_IS_WINDOWS)
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

  // initialize thread tracking array (max_threads + 1 to account for the pinned
  // actor thread; the ASIO thread is not run under systematic testing)
  total_threads = max_threads + 1;
  size_t mem_needed = total_threads * sizeof(systematic_testing_thread_t);
  threads_to_track = (systematic_testing_thread_t*)ponyint_pool_alloc_size(
    mem_needed);
  memset(threads_to_track, 0, mem_needed);

#if !defined(PLATFORM_IS_WINDOWS)
  for(uint32_t i = 0; i < total_threads; i++)
    pthread_cond_init(&threads_to_track[i].turn, NULL);
#endif

#ifdef USE_RUNTIMESTATS
  mem_used += (mem_needed);
  mem_allocated += (ponyint_pool_used_size(mem_needed));
#endif

  active_thread = threads_to_track;
}

void ponyint_systematic_testing_wait_start(int32_t scheduler_index)
{
  // Slot 0 is the pinned actor thread; slots 1..N are the scheduler
  // threads. The ASIO thread is not run under systematic testing, so
  // it gets no slot.
  systematic_testing_thread_t* self =
    &threads_to_track[scheduler_index + 1];

  TRACING_SYSTEMATIC_TESTING_WAITING_TO_START_BEGIN();

  systematic_testing_lock();

  atomic_fetch_add_explicit(&waiting_to_start_count, 1, memory_order_relaxed);

  // Park until it is truly this thread's turn. The wait can return
  // without a matching wake -- re-check `active_thread` and re-park on
  // each wake. Converting this loop to a bare `if` lets a spuriously-
  // woken thread run out of turn and desync the single-runner handoff,
  // deadlocking every thread. The yield handoff and the coordinator
  // park below do the same.
  while(active_thread != self)
    systematic_testing_park(self);

  TRACING_SYSTEMATIC_TESTING_WAITING_TO_START_END();
  TRACING_SYSTEMATIC_TESTING_TIMESLICE_BEGIN();
}

void ponyint_systematic_testing_start()
{
  TRACING_SYSTEMATIC_TESTING_WAITING_TO_START_BEGIN();

  while((total_threads - 1) != atomic_load_explicit(&waiting_to_start_count, memory_order_relaxed))
  {
    ponyint_cpu_core_pause(1, 10000002, true);
  }

  systematic_testing_lock();

  // always start the first scheduler thread (slot 1; slot 0 is the pinned actor
  // thread).
  active_thread = &threads_to_track[1];

  TRACING_SYSTEMATIC_TESTING_STARTED();

  systematic_testing_wake(active_thread);

  // Park until it is genuinely this (pinned actor) thread's turn. As in the
  // yield handoff and `wait_start`, the wait can return without a matching
  // wake. Returning here while `active_thread` still points at another thread
  // would let this thread run out of turn and desync the single-runner
  // handoff, deadlocking every thread. Re-check and re-park.
  while(active_thread != &threads_to_track[0])
    systematic_testing_park(&threads_to_track[0]);

  TRACING_SYSTEMATIC_TESTING_WAITING_TO_START_END();
  TRACING_SYSTEMATIC_TESTING_TIMESLICE_BEGIN();
}

uint64_t ponyint_systematic_testing_clock()
{
  return logical_clock;
}

static uint32_t get_next_index()
{
  // Every tracked thread that has not stopped is eligible, passive
  // ones included: a passive thread does protocol work on its visits,
  // and its timed pause is a bare yield, always resumable.
  uint32_t next_index;

  do
  {
    next_index = (uint32_t)rand() % total_threads;
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
#if !defined(PLATFORM_IS_WINDOWS)
    for(uint32_t i = 0; i < total_threads; i++)
      pthread_cond_destroy(&threads_to_track[i].turn);
#endif

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

    systematic_testing_unlock();

    TRACING_SYSTEMATIC_TESTING_TIMESLICE_END();
    SYSTEMATIC_TESTING_PRINTF("Systematic testing successfully finished!\n");
    TRACING_SYSTEMATIC_TESTING_FINISHED();

    return;
  }

  uint32_t next_index = get_next_index();

  systematic_testing_thread_t *next_thread = &threads_to_track[next_index];
  systematic_testing_thread_t *current_thread = active_thread;

  if(next_thread != current_thread)
  {
    active_thread = next_thread;

    TRACING_SYSTEMATIC_TESTING_TIMESLICE_END();

    systematic_testing_wake(next_thread);

    if(!current_thread->stopped)
    {
      // Park until it is genuinely this thread's turn again. The underlying
      // wait can return without a matching wake. Returning here while
      // active_thread still points at another thread would let this thread
      // run out of turn and desync the single-runner handoff, deadlocking
      // every thread. Re-check and re-park, as wait_start does.
      while(active_thread != current_thread)
        systematic_testing_park(current_thread);

      TRACING_SYSTEMATIC_TESTING_TIMESLICE_BEGIN();
    }
    else
    {
      systematic_testing_unlock();
    }
  }
}

void ponyint_systematic_testing_stop_thread()
{
  active_thread->stopped = true;
  stopped_threads++;

  ponyint_systematic_testing_yield();
}
