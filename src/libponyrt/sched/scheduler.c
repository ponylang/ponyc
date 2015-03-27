#include "scheduler.h"
#include "cpu.h"
#include "mpmcq.h"
#include "../actor/actor.h"
#include "../gc/cycle.h"
#include "../asio/asio.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

static DECLARE_THREAD_FN(run_thread);

// Scheduler global data.
static uint32_t scheduler_count;
static uint32_t scheduler_waiting;
static scheduler_t* scheduler;
static bool detect_quiescence;
static bool shutdown_on_stop;
static bool terminate;
static bool use_mpmcq;

static mpmcq_t inject;

static __pony_thread_local scheduler_t* this_scheduler;

// Forward declaration.
static void push(scheduler_t* sched, pony_actor_t* actor);

/**
 * Gets the next actor from the scheduler queue.
 */
static pony_actor_t* pop(scheduler_t* sched)
{
  if(use_mpmcq)
    return (pony_actor_t*)mpmcq_pop(&sched->q);

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
}

/**
 * Puts an actor on the scheduler queue.
 */
static void push(scheduler_t* sched, pony_actor_t* actor)
{
  if(use_mpmcq)
  {
    mpmcq_push_single(&sched->q, actor);
  } else {
    pony_actor_t* head = sched->head;

    if(head != NULL)
    {
      actor_setnext(head, actor);
      sched->head = actor;
    } else {
      sched->head = actor;
      sched->tail = actor;
    }
  }
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
static bool quiescent(scheduler_t* sched, uint64_t tsc)
{
  // If we should not detect quiscence, pause the core and return false.
  if(!_atomic_load(&detect_quiescence, __ATOMIC_RELAXED))
  {
    cpu_core_pause(tsc);
    return false;
  }

  // If the terminate flag is set, return true immediately.
  if(_atomic_load(&terminate, __ATOMIC_RELAXED))
    return true;

  // If not enough time has elapsed to bother checking, return false.
  if(!cpu_core_pause(tsc))
    return false;

  if(sched->finish)
  {
    uint32_t waiting = _atomic_load(&scheduler_waiting, __ATOMIC_RELAXED);

    // If all scheduler threads are waiting and it is possible to stop the ASIO
    // back end, then we can terminate the cycle detector, which will
    // eventually call scheduler_terminate().
    if((waiting == scheduler_count) && asio_stop())
    {
      if(!use_mpmcq)
      {
        // It's safe to manipulate our victim, since we know it's paused as
        // well.
        if(sched->victim != NULL)
        {
          _atomic_store(&sched->victim->thief, NULL, __ATOMIC_RELEASE);
        }

        _atomic_store(&sched->waiting, 0, __ATOMIC_RELEASE);
      }

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

static scheduler_t* choose_victim(scheduler_t* sched)
{
  scheduler_t* victim = sched->last_victim;

  while(true)
  {
    // Back up one.
    victim--;

    // Wrap around to the end.
    if(victim < scheduler)
      victim = &scheduler[scheduler_count - 1];

    if(victim == sched->last_victim)
    {
      // If we have tried all possible victims, return no victim. Set our last
      // victim to ourself to indicate we've started over.
      sched->last_victim = sched;
      break;
    }

    // Don't try to steal from ourself.
    if(victim == sched)
      continue;

    if(!use_mpmcq)
    {
      scheduler_t* thief = NULL;

      // Mark that we are the thief. If we can't, keep trying.
      if(!_atomic_cas(&victim->thief, &thief, sched,
        __ATOMIC_RELAXED, __ATOMIC_RELAXED))
      {
        continue;
      }

      assert(sched->victim == NULL);
      sched->victim = victim;
    }

    // Record that this is our victim and return it.
    sched->last_victim = victim;
    return victim;
  }

  return NULL;
}

/**
 * Use mpmcqs to allow stealing directly from a victim, without waiting for a
 * response. However, this makes pushing and popping actors on scheduling
 * queues significantly more expensive.
 */
static pony_actor_t* steal(scheduler_t* sched)
{
  uint64_t tsc = cpu_rdtsc();
  pony_actor_t* actor;

  while(true)
  {
    scheduler_t* victim = choose_victim(sched);

    if(victim == NULL)
      victim = sched;

    actor = pop_global(victim);

    if(actor != NULL)
      break;

    _atomic_add32(&scheduler_waiting, 1, __ATOMIC_RELAXED);

    if(quiescent(sched, tsc))
      return NULL;

    _atomic_sub32(&scheduler_waiting, 1, __ATOMIC_RELAXED);
  }

  return actor;
}

/**
 * Wait until we receive a stolen actor. Tight spin at first, falling back to
 * nanosleep. Once we have fallen back, check for quiescence.
 */
static pony_actor_t* request(scheduler_t* sched)
{
  scheduler_t* thief = NULL;

  bool block = _atomic_cas(&sched->thief, &thief, (void*)1,
    __ATOMIC_RELAXED, __ATOMIC_RELAXED);

  _atomic_add32(&scheduler_waiting, 1, __ATOMIC_RELAXED);

  uint64_t tsc = cpu_rdtsc();
  pony_actor_t* actor;

  while(true)
  {
    _atomic_store(&sched->waiting, 1, __ATOMIC_RELEASE);

    scheduler_t* victim = choose_victim(sched);

    if(victim != NULL)
    {
      while(_atomic_load(&sched->waiting, __ATOMIC_ACQUIRE) == 1)
      {
        if(quiescent(sched, tsc))
          return NULL;
      }

      sched->victim = NULL;
    } else {
      if((actor = pop_global(sched)) != NULL)
      {
        _atomic_store(&sched->waiting, 0, __ATOMIC_RELEASE);
        _atomic_sub32(&scheduler_waiting, 1, __ATOMIC_RELAXED);
        break;
      }

      if(quiescent(sched, tsc))
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
    _atomic_cas(&sched->thief, &thief, NULL,
      __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  }

  return actor;
}

/**
 * Check if we have a thief. If we do, try to give it an actor. Signal the
 * thief to continue whether or not we gave it an actor.
 */
static void respond(scheduler_t* sched)
{
  scheduler_t* thief = _atomic_load(&sched->thief, __ATOMIC_RELAXED);

  if(thief <= (scheduler_t*)1)
    return;

  pony_actor_t* actor = pop_global(sched);

  if(actor != NULL)
  {
    assert(thief->waiting == 1);
    push(thief, actor);

    // Decrement the wait count if we give the thief an actor. We know that
    // the scheduler thread is no longer waiting.
    _atomic_sub32(&scheduler_waiting, 1, __ATOMIC_RELAXED);
  }

  _atomic_store(&thief->waiting, 0, __ATOMIC_RELEASE);

  assert(sched->thief == thief);

  _atomic_store(&sched->thief, NULL, __ATOMIC_RELEASE);
}

/**
 * Run a scheduler thread until termination.
 */
static void run(scheduler_t* sched)
{
  pony_actor_t* actor = pop_global(sched);

  while(true)
  {
    if(actor == NULL)
    {
      // We had an empty queue and no rescheduled actor.
      if(use_mpmcq)
        actor = steal(sched);
      else
        actor = request(sched);

      if(actor == NULL)
      {
        // Termination.
        assert(pop(sched) == NULL);
        return;
      }
    }

    // Respond to our thief. We hold an actor for ourself, to make sure we
    // never give away our last actor.
    if(!use_mpmcq)
      respond(sched);

    // Run the current actor and get the next actor.
    bool reschedule = actor_run(actor);
    pony_actor_t* next = pop_global(sched);

    if(reschedule)
    {
      if(next != NULL)
      {
        // If we have a next actor, we go on the back of the queue. Otherwise,
        // we continue to run this actor.
        push(sched, actor);
        actor = next;
      }
    } else {
      // We aren't rescheduling, so run the next actor. This may be NULL if our
      // queue was empty.
      actor = next;
    }
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

  for(uint32_t i = 0; i < scheduler_count; i++)
    mpmcq_destroy(&scheduler[i].q);

  detect_quiescence = false;
  terminate = false;
  scheduler_waiting = 0;

  free(scheduler);
  scheduler = NULL;
  scheduler_count = 0;

  mpmcq_destroy(&inject);
}

void scheduler_init(uint32_t threads, bool forcecd, bool mpmcq)
{
  use_mpmcq = mpmcq;

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
    mpmcq_init(&scheduler[i].q);
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
  _atomic_store(&detect_quiescence, true, __ATOMIC_RELAXED);

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
  // Check for a pending thief.
  if(!use_mpmcq && (this_scheduler != NULL))
    respond(this_scheduler);
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
  _atomic_store(&terminate, true, __ATOMIC_RELAXED);
}

uint32_t scheduler_cores()
{
  return scheduler_count;
}
