#include "scheduler.h"
#include "cpu.h"
#include "mpmcq.h"
#include "../actor/actor.h"
#include "../gc/cycle.h"
#include "../asio/asio.h"
#include <string.h>
#include <assert.h>

static DECLARE_THREAD_FN(run_thread);

// Scheduler global data.
static uint32_t scheduler_count;
static uint32_t scheduler_waiting;
static scheduler_t* scheduler;
static bool detect_quiescence;
static bool shutdown_on_stop;
static bool terminate;

static mpmcq_t inject;

static __pony_thread_local scheduler_t* this_scheduler;

// Forward declaration.
static void push(scheduler_t* sched, pony_actor_t* actor);

/**
 * Gets the next actor from the scheduler queue.
 */
static pony_actor_t* pop(scheduler_t* sched)
{
#ifdef USE_MPMCQ
  return (pony_actor_t*)mpmcq_pop(&sched->q);
#else
  pony_actor_t* actor = sched->tail;

  if(actor != NULL)
  {
    if(actor != sched->head)
    {
      sched->tail = actor_next(actor);
    } else {
      sched->head = NULL;
      sched->tail = NULL;
    }

    actor_setnext(actor, NULL);
  }

  return actor;
#endif
}

/**
 * Puts an actor on the scheduler queue.
 */
static void push(scheduler_t* sched, pony_actor_t* actor)
{
#ifdef USE_MPMCQ
  mpmcq_push(&sched->q, actor);
#else
  pony_actor_t* head = sched->head;

  if(head != NULL)
  {
    actor_setnext(head, actor);
    sched->head = actor;
  } else {
    sched->head = actor;
    sched->tail = actor;
  }
#endif
}

/**
 * Handles the global queue and then pops from the local queue
 */
static pony_actor_t* pop_global(scheduler_t* sched)
{
  pony_actor_t* actor = (pony_actor_t*)mpmcq_pop(&inject);

  if(actor != NULL)
    return actor;

  return pop(sched);
}

/**
 * If we can terminate, return true. If all schedulers are waiting, one of
 * them will stop the ASIO back end and tell the cycle detector to try to
 * terminate.
 */
static bool quiescent(scheduler_t* sched)
{
  if(!__pony_atomic_load_n(&detect_quiescence, PONY_ATOMIC_RELAXED,
    PONY_ATOMIC_NO_TYPE))
    return false;

  if(__pony_atomic_load_n(&terminate, PONY_ATOMIC_RELAXED,
    PONY_ATOMIC_NO_TYPE))
    return true;

  if(sched->finish)
  {
    uint32_t waiting = __pony_atomic_load_n(&scheduler_waiting,
      PONY_ATOMIC_RELAXED, PONY_ATOMIC_NO_TYPE);

    // If all scheduler threads are waiting and it is possible to stop the ASIO
    // back end, then we can terminate the cycle detector, which will
    // eventually call scheduler_terminate().
    if((waiting == scheduler_count) && asio_stop())
    {
#ifndef USE_MPMCQ
      // It's safe to manipulate our victim, since we know it's paused as well.
      if(sched->victim != NULL)
      {
        __pony_atomic_store_n(&sched->victim->thief, NULL, PONY_ATOMIC_RELEASE,
          PONY_ATOMIC_NO_TYPE);
      }

      __pony_atomic_store_n(&sched->waiting, 0, PONY_ATOMIC_RELEASE,
        PONY_ATOMIC_NO_TYPE);
#endif

      // Under these circumstances, the CD will always go on the current
      // scheduler.
      cycle_terminate(sched->forcecd);

      if(sched->forcecd)
        sched->forcecd = false;
      else
        sched->finish = false;
    }
  }

  return false;
}

static scheduler_t* choose_on_node(scheduler_t* sched, bool on_node)
{
  scheduler_t* victim = sched->last_victim;

  while(true)
  {
    victim--;

    if(victim < scheduler)
      victim = &scheduler[scheduler_count - 1];

    if(victim == sched->last_victim)
      break;

    if((victim == sched) || (on_node && (victim->node != sched->node)))
      continue;

#ifndef USE_MPMCQ
    scheduler_t* thief = NULL;

    if(!__pony_atomic_compare_exchange_n(&victim->thief, &thief,
      sched, false, PONY_ATOMIC_RELAXED, PONY_ATOMIC_RELAXED, intptr_t))
      continue;

    assert(sched->victim == NULL);
    sched->victim = victim;
#endif

    sched->last_victim = victim;
    return victim;
  }

  return NULL;
}

static scheduler_t* choose_victim(scheduler_t* sched)
{
  if(scheduler_count == 1)
    return NULL;

#ifndef USE_MPMCQ
#ifdef USE_NUMA
  scheduler_t* victim = choose_on_node(sched, true);

  if(victim != NULL)
    return victim;
#endif
#endif

  return choose_on_node(sched, false);
}

#ifdef USE_MPMCQ

/**
 * Use mpmcqs to allow stealing directly from a victim, without waiting for a
 * response. However, this makes pushing and popping actors on scheduling
 * queues significantly more expensive.
 */
static pony_actor_t* request(scheduler_t* sched)
{
  uint64_t tsc = cpu_rdtsc();
  pony_actor_t* actor;

  while(true)
  {
    scheduler_t* victim = choose_victim(sched);

    if(victim != NULL)
    {
      actor = pop(victim);

      if(actor != NULL)
        break;
    }

    __pony_atomic_fetch_add(&scheduler_waiting, 1, PONY_ATOMIC_RELAXED,
      uint32_t);

    if(cpu_core_pause(tsc) && quiescent(sched))
      return NULL;

    __pony_atomic_fetch_sub(&scheduler_waiting, 1, PONY_ATOMIC_RELAXED,
      uint32_t);
  }

  return actor;
}

#else

/**
 * Wait until we receive a stolen actor. Tight spin at first, falling back to
 * nanosleep. Once we have fallen back, check for quiescence.
 */
static pony_actor_t* request(scheduler_t* sched)
{
  scheduler_t* thief = NULL;

  bool block = __pony_atomic_compare_exchange_n(&sched->thief, &thief,
    (void*)1, false, PONY_ATOMIC_RELAXED, PONY_ATOMIC_RELAXED, intptr_t);

  __pony_atomic_fetch_add(&scheduler_waiting, 1, PONY_ATOMIC_RELAXED,
    uint32_t);

  uint64_t tsc = cpu_rdtsc();
  pony_actor_t* actor;

  while(true)
  {
    __pony_atomic_store_n(&sched->waiting, 1, PONY_ATOMIC_RELEASE,
      PONY_ATOMIC_NO_TYPE);

    scheduler_t* victim = choose_victim(sched);

    if(victim != NULL)
    {
      while(__pony_atomic_load_n(&sched->waiting, PONY_ATOMIC_ACQUIRE,
        PONY_ATOMIC_NO_TYPE) == 1)
      {
        if(cpu_core_pause(tsc) && quiescent(sched))
          return NULL;
      }

      sched->victim = NULL;
    } else {
      if(cpu_core_pause(tsc) && quiescent(sched))
        return NULL;
    }

    if((actor = pop(sched)) != NULL)
      break;
  }

  if(block)
  {
    thief = (scheduler_t*)1;

    /** MSVC++ throws warning here - non-void expression with no effect.
     *  GCC/clang would throw a warning for unused result, hence disable the
     *  MSVC++ warning.
     *
     *  http://msdn.microsoft.com/en-us/library/577cze47.aspx
     */
#if defined(PLATFORM_IS_WINDOWS)
#  pragma warning(disable:4552)
#endif
    __pony_atomic_compare_exchange_n(&sched->thief, &thief, NULL, false,
      PONY_ATOMIC_RELAXED, PONY_ATOMIC_RELAXED, intptr_t);
  }

  return actor;
}

/**
 * Check if we have a thief. If we do, try to give it an actor. Signal the
 * thief to continue whether or not we gave it an actor.
 */
static void respond(scheduler_t* sched)
{
  scheduler_t* thief = (scheduler_t*)__pony_atomic_load_n(&sched->thief,
    PONY_ATOMIC_RELAXED, PONY_ATOMIC_NO_TYPE);

  if(thief <= (scheduler_t*)1)
    return;

  pony_actor_t* actor = pop_global(sched);

  if(actor != NULL)
  {
    assert(thief->waiting == 1);
    push(thief, actor);

    // Decrement the wait count if we give the thief an actor. We know that
    // scheduler thread is no longer waiting.
    __pony_atomic_fetch_sub(&scheduler_waiting, 1, PONY_ATOMIC_RELAXED,
      uint32_t);
  }

  __pony_atomic_store_n(&thief->waiting, 0, PONY_ATOMIC_RELEASE,
    PONY_ATOMIC_NO_TYPE);

  assert(sched->thief == thief);

  __pony_atomic_store_n(&sched->thief, NULL, PONY_ATOMIC_RELEASE,
    PONY_ATOMIC_NO_TYPE);
}

#endif

/**
 * Run a scheduler thread until termination.
 */
static void run(scheduler_t* sched)
{
  while(true)
  {
    // Get an actor from our queue.
    pony_actor_t* actor = pop_global(sched);

    if(actor == NULL)
    {
      // Wait until we get an actor.
      actor = request(sched);

      // Termination.
      if(actor == NULL)
        return;
    }
#ifndef USE_MPMCQ
    else
    {
      // Respond to our thief. We hold an actor for ourself, to make sure we
      // never give away our last actor.
      respond(sched);
    }
#endif

    // If this returns true, reschedule the actor on our queue.
    if(actor_run(actor))
      push(sched, actor);
  }
}

static DECLARE_THREAD_FN(run_thread)
{
  scheduler_t* sched = (scheduler_t*) arg;
  this_scheduler = sched;
  cpu_affinity(sched->cpu);
  run(sched);

  return 0;
}

static void scheduler_shutdown()
{
  uint32_t start;

  if(scheduler[0].tid == pony_thread_self())
    start = 1;
  else
    start = 0;

  for(uint32_t i = start; i < scheduler_count; i++)
    pony_thread_join(scheduler[i].tid);

#ifdef USE_MPMCQ
  for(uint32_t i = 0; i < scheduler_count; i++)
    mpmcq_destroy(&scheduler[i].q);
#endif

  __pony_atomic_store_n(&detect_quiescence, false, PONY_ATOMIC_RELAXED,
    PONY_ATOMIC_NO_TYPE);

  __pony_atomic_store_n(&terminate, false, PONY_ATOMIC_RELAXED,
    PONY_ATOMIC_NO_TYPE);

  __pony_atomic_store_n(&scheduler_waiting, 0, PONY_ATOMIC_RELAXED,
    PONY_ATOMIC_NO_TYPE);

  free(scheduler);
  scheduler = NULL;
  scheduler_count = 0;

  mpmcq_destroy(&inject);
}

void scheduler_init(uint32_t threads, bool forcecd)
{
  // If no thread count is specified, use the available physical core count.
  if(threads == 0)
    threads = cpu_count();

  scheduler_count = threads;
  scheduler_waiting = 0;
  scheduler = (scheduler_t*)calloc(scheduler_count, sizeof(scheduler_t));

  cpu_assign(scheduler_count, scheduler);

  scheduler[0].finish = true;
  scheduler[0].forcecd = forcecd;

  for(uint32_t i = 0; i < scheduler_count; i++)
  {
    scheduler[i].last_victim = &scheduler[i];

#ifdef USE_MPMCQ
    mpmcq_init(&scheduler[i].q);
#endif
  }

  mpmcq_init(&inject);
  asio_init();
}

bool scheduler_start(pony_termination_t termination)
{
  if(!asio_start())
    return false;

  detect_quiescence = termination == PONY_DONT_WAIT;
  shutdown_on_stop = termination == PONY_ASYNC_WAIT;

  uint32_t start;

  if(termination == PONY_ASYNC_WAIT)
  {
    start = 0;
  } else {
    start = 1;
    scheduler[0].tid = pony_thread_self();
  }

  for(uint32_t i = start; i < scheduler_count; i++)
  {
    if(!pony_thread_create(&scheduler[i].tid, run_thread, scheduler[i].cpu,
      &scheduler[i]))
      return false;
  }

  if(termination != PONY_ASYNC_WAIT)
  {
    run_thread(&scheduler[0]);
    scheduler_shutdown();
  }

  return true;
}

void scheduler_stop()
{
  __pony_atomic_store_n(&detect_quiescence, true, PONY_ATOMIC_RELAXED,
    PONY_ATOMIC_NO_TYPE);

  if(shutdown_on_stop)
    scheduler_shutdown();
}

pony_actor_t* scheduler_worksteal()
{
  // TODO: is this right?
  return pop_global(this_scheduler);
}

void scheduler_add(pony_actor_t* actor)
{
  if(this_scheduler != NULL)
  {
    // Add to the current scheduler thread.
    push(this_scheduler, actor);
  } else {
    // Put on the shared mpmcq.
    mpmcq_push(&inject, actor);
  }
}

void scheduler_respond()
{
#ifndef USE_MPMCQ
  // Check for a pending thief.
  if(this_scheduler != NULL)
    respond(this_scheduler);
#endif
}

void scheduler_offload()
{
  scheduler_t* sched = this_scheduler;

  if(sched == NULL)
    return;

  pony_actor_t* actor;

  while((actor = pop(sched)) != NULL)
    mpmcq_push(&inject, actor);
}

void scheduler_terminate()
{
  __pony_atomic_store_n(&terminate, true, PONY_ATOMIC_RELAXED,
    PONY_ATOMIC_NO_TYPE);
}

uint32_t scheduler_cores()
{
  return scheduler_count;
}
