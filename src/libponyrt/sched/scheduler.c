#define PONY_WANT_ATOMIC_DEFS

#include "scheduler.h"
#include "cpu.h"
#include "mpmcq.h"
#include "../actor/actor.h"
#include "../gc/cycle.h"
#include "../asio/asio.h"
#include "../mem/pagemap.h"
#include "../mem/pool.h"
#include "ponyassert.h"
#include <dtrace.h>
#include <string.h>
#include "mutemap.h"

#define PONY_SCHED_BLOCK_THRESHOLD 1000000

static DECLARE_THREAD_FN(run_thread);

typedef enum
{
  SCHED_BLOCK = 20,
  SCHED_UNBLOCK = 21,
  SCHED_CNF = 30,
  SCHED_ACK,
  SCHED_TERMINATE = 40,
  SCHED_SUSPEND = 41,
  SCHED_UNMUTE_ACTOR = 50,
  SCHED_NOISY_ASIO = 51,
  SCHED_UNNOISY_ASIO = 52
} sched_msg_t;

// Scheduler global data.
static uint64_t last_cd_tsc;
static uint32_t scheduler_count;
static uint32_t min_scheduler_count;
static uint64_t scheduler_suspend_threshold;
static PONY_ATOMIC(uint32_t) active_scheduler_count;
static PONY_ATOMIC(uint32_t) active_scheduler_count_check;
static scheduler_t* scheduler;
static pony_ctx_t* inject_context;
static PONY_ATOMIC(bool) detect_quiescence;
static bool use_yield;
static mpmcq_t inject;
static __pony_thread_local scheduler_t* this_scheduler;

#if defined(USE_SCHEDULER_SCALING_PTHREADS)
static pthread_mutex_t sched_mut;

static pthread_once_t sched_mut_once = PTHREAD_ONCE_INIT;

void sched_mut_init()
{
    pthread_mutex_init(&sched_mut, NULL);
}
#else
static PONY_ATOMIC(bool) scheduler_count_changing;
#endif

#ifdef USE_MEMTRACK
// holds only size of pthread_cond variables and scheduler_t array
static size_t mem_allocated;
static size_t mem_used;

/** Get the static memory used by the scheduler subsystem.
 */
size_t ponyint_sched_static_mem_size()
{
  return mem_used;
}

/** Get the static memory allocated by the scheduler subsystem.
 */
size_t ponyint_sched_static_alloc_size()
{
  return mem_allocated;
}

size_t ponyint_sched_total_mem_size(pony_ctx_t* ctx)
{
  return
      // memory used for each actor struct
      // + memory used for actormaps for gc acquire/release messages
      ctx->mem_used_actors
      // memory used for mutemap
    + ctx->mem_used;
}

size_t ponyint_sched_total_alloc_size(pony_ctx_t* ctx)
{
  return
      // memory allocated for each actor struct
      // + memory allocated for actormaps for gc acquire/release messages
      ctx->mem_allocated_actors
      // memory allocated for mutemap
    + ctx->mem_allocated;
}
#endif

/**
 * Gets the current active scheduler count
 */
static uint32_t get_active_scheduler_count()
{
   return atomic_load_explicit(&active_scheduler_count, memory_order_relaxed);
}

/**
 * Gets the current active scheduler count check
 */
static uint32_t get_active_scheduler_count_check()
{
   return atomic_load_explicit(&active_scheduler_count_check, memory_order_relaxed);
}

/**
 * Gets the next actor from the scheduler queue.
 */
static pony_actor_t* pop(scheduler_t* sched)
{
  return (pony_actor_t*)ponyint_mpmcq_pop(&sched->q);
}

/**
 * Puts an actor on the scheduler queue.
 */
static void push(scheduler_t* sched, pony_actor_t* actor)
{
  ponyint_mpmcq_push_single(&sched->q, actor);
}

/**
 * Handles the global queue and then pops from the local queue
 */
static pony_actor_t* pop_global(scheduler_t* sched)
{
  pony_actor_t* actor = (pony_actor_t*)ponyint_mpmcq_pop(&inject);

  if(actor != NULL)
    return actor;

  if (sched == NULL)
    return NULL;
  else
    return pop(sched);
}

/**
 * Sends a message to a thread.
 */

static void send_msg(uint32_t from, uint32_t to, sched_msg_t msg, intptr_t arg)
{
  pony_msgi_t* m = (pony_msgi_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(pony_msgi_t)), msg);

#ifdef USE_MEMTRACK_MESSAGES
  this_scheduler->ctx.num_messages--;
  this_scheduler->ctx.mem_used_messages += sizeof(pony_msgi_t);
  this_scheduler->ctx.mem_used_messages -= POOL_ALLOC_SIZE(pony_msgi_t);
#endif

  m->i = arg;
  ponyint_thread_messageq_push(&scheduler[to].mq, &m->msg, &m->msg
#ifdef USE_DYNAMIC_TRACE
    , from, to
#endif
    );
  (void)from;
}

static void send_msg_all_active(uint32_t from, sched_msg_t msg, intptr_t arg)
{
  uint32_t current_active_scheduler_count = get_active_scheduler_count();

  for(uint32_t i = 0; i < current_active_scheduler_count; i++)
    send_msg(from, i, msg, arg);
}

static void send_msg_all(uint32_t from, sched_msg_t msg, intptr_t arg)
{
  for(uint32_t i = 0; i < scheduler_count; i++)
    send_msg(from, i, msg, arg);
}

static void signal_suspended_threads(uint32_t sched_count, int32_t curr_sched_id)
{
  for(uint32_t i = 0; i < sched_count; i++)
  {
    if((int32_t)i != curr_sched_id)
      ponyint_thread_wake(scheduler[i].tid, scheduler[i].sleep_object);
  }
}

static void wake_suspended_threads(int32_t current_scheduler_id)
{
  uint32_t current_active_scheduler_count = get_active_scheduler_count();

  // wake up any sleeping threads
  while ((current_active_scheduler_count = get_active_scheduler_count()) < scheduler_count)
  {
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
    // acquire mutex if using pthreads
    if(!pthread_mutex_lock(&sched_mut))
#else
    // get the bool that controls modifying the active scheduler count variable
    // if using signals
    if(!atomic_load_explicit(&scheduler_count_changing, memory_order_relaxed)
      && !atomic_exchange_explicit(&scheduler_count_changing, true,
      memory_order_acquire))
#endif
    {
      // in case the count changed between the while check and now
      current_active_scheduler_count = get_active_scheduler_count();

      if(current_active_scheduler_count < scheduler_count)
      {
        // set active_scheduler_count to wake all schedulers
        current_active_scheduler_count = scheduler_count;
        atomic_store_explicit(&active_scheduler_count, current_active_scheduler_count,
          memory_order_relaxed);
      }

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
      // unlock the bool that controls modifying the active scheduler count
      // variable if using signals.
      atomic_store_explicit(&scheduler_count_changing, false,
        memory_order_release);
#endif

      // send signals to all scheduler threads that should be awake
      // this is somewhat wasteful if a scheduler thread is already awake
      // NOTE: this intentionally allows for the case where some scheduler
      // threads might miss the signal and not wake up. That is handled in
      // the following while loop
      signal_suspended_threads(current_active_scheduler_count, current_scheduler_id);

#if defined(USE_SCHEDULER_SCALING_PTHREADS)
      // unlock mutex if using pthreads
      pthread_mutex_unlock(&sched_mut);
#endif
    }

    // wait for sleeping threads to wake and update check variable
    while (get_active_scheduler_count() != get_active_scheduler_count_check())
    {
      // send signals to all scheduler threads that should be awake
      // this is somewhat wasteful if a scheduler thread is already awake
      // but is necessary in case the signal to wake a thread was missed
      // NOTE: this intentionally allows for the case where some scheduler
      // threads might miss the signal and not wake up. That is handled in
      // by a combination of the check variable and this while loop
      signal_suspended_threads(current_active_scheduler_count, current_scheduler_id);
    }
  }
}

// start cnf/ack cycle for quiescence if block count >= active_scheduler_count
static void maybe_start_cnf_ack_cycle(scheduler_t* sched)
{
  if(atomic_load_explicit(&detect_quiescence, memory_order_relaxed) &&
    (sched->block_count >= get_active_scheduler_count()))
  {
    // reset ack token count to 0 because dynamic scheduler scaling means
    // that a new thread can wake up changing active_scheduler_count and
    // then block causing block_count >= active_scheduler_count for a
    // second time and if we don't reset, we can think we've received
    // enough acks when we really haven't
    sched->ack_token++;
    sched->ack_count = 0;

    // If we think all threads are blocked, send CNF(token) to everyone.
    send_msg_all_active(sched->index, SCHED_CNF, sched->ack_token);
  }
}

// handle SCHED_BLOCK message
static void handle_sched_block(scheduler_t* sched)
{
  sched->block_count++;
  maybe_start_cnf_ack_cycle(sched);
}

// handle SCHED_UNBLOCK message
static void handle_sched_unblock(scheduler_t* sched)
{
  // Cancel all acks and increment the ack token, so that any pending
  // acks in the queue will be dropped when they are received.
  sched->block_count--;
  sched->ack_token++;
  sched->ack_count = 0;
}

static bool read_msg(scheduler_t* sched)
{
  pony_msgi_t* m;

  bool run_queue_changed = false;

  while((m = (pony_msgi_t*)ponyint_thread_messageq_pop(&sched->mq
#ifdef USE_DYNAMIC_TRACE
    , sched->index
#endif
    )) != NULL)
  {
#ifdef USE_MEMTRACK_MESSAGES
    sched->ctx.num_messages--;
    sched->ctx.mem_used_messages -= sizeof(pony_msgi_t);
    sched->ctx.mem_allocated_messages -= POOL_ALLOC_SIZE(pony_msgi_t);
#endif

    switch(m->msg.id)
    {
      case SCHED_SUSPEND:
      {
        maybe_start_cnf_ack_cycle(sched);
        break;
      }

      case SCHED_BLOCK:
      {
        handle_sched_block(sched);
        break;
      }

      case SCHED_UNBLOCK:
      {
        handle_sched_unblock(sched);
        break;
      }

      case SCHED_CNF:
      {
        // Echo the token back as ACK(token).
        send_msg(sched->index, 0, SCHED_ACK, m->i);
        break;
      }

      case SCHED_ACK:
      {
        // If it's the current token, increment the ack count.
        if(m->i == sched->ack_token)
          sched->ack_count++;
        break;
      }

      case SCHED_TERMINATE:
      {
        sched->terminate = true;
        break;
      }

      case SCHED_UNMUTE_ACTOR:
      {
        if (ponyint_sched_unmute_senders(&sched->ctx, (pony_actor_t*)m->i))
          run_queue_changed = true;

        break;
      }

      case SCHED_NOISY_ASIO:
      {
        // mark asio as being noisy
        sched->asio_noisy = true;
        break;
      }

      case SCHED_UNNOISY_ASIO:
      {
        // mark asio as not being noisy
        sched->asio_noisy = false;
        break;
      }

      default: {}
    }
  }

  return run_queue_changed;
}

/**
 * If we can terminate, return true. If all schedulers are waiting, one of
 * them will stop the ASIO back end and tell the cycle detector to try to
 * terminate.
 */

static bool quiescent(scheduler_t* sched, uint64_t tsc, uint64_t tsc2)
{
  if(sched->terminate)
    return true;

  uint32_t current_active_scheduler_count = get_active_scheduler_count();

  if(sched->ack_count >= current_active_scheduler_count)
  {
    // mark last cycle detector tsc as something huge to ensure
    // cycle detector will not get triggered
    // this is required to ensure scheduler queues are empty
    // upon termination
    uint64_t saved_last_cd_tsc = last_cd_tsc;
    last_cd_tsc = -1;

    if(sched->asio_stoppable && ponyint_asio_stop())
    {
      // successfully stopped ASIO thread
      // tell all scheduler threads to terminate
      send_msg_all(sched->index, SCHED_TERMINATE, 0);

      wake_suspended_threads(sched->index);

      sched->ack_token++;
      sched->ack_count = 0;
    } else if(ponyint_asio_stoppable()) {
      sched->asio_stoppable = true;
      sched->ack_token++;
      sched->ack_count = 0;

      // Run another CNF/ACK cycle.
      send_msg_all_active(sched->index, SCHED_CNF, sched->ack_token);

      // restore last cycle detector tsc to re-enable cycle detector
      // triggering
      last_cd_tsc = saved_last_cd_tsc;
    } else {
      // ASIO is not stoppable
      sched->asio_stoppable = false;

      // restore last cycle detector tsc to re-enable cycle detector
      // triggering
      last_cd_tsc = saved_last_cd_tsc;
    }
  }

  ponyint_cpu_core_pause(tsc, tsc2, use_yield);
  return false;
}



static scheduler_t* choose_victim(scheduler_t* sched)
{
  scheduler_t* victim = sched->last_victim;

  while(true)
  {
    // Schedulers are laid out sequentially in memory

    // Back up one.
    victim--;

    uint32_t current_active_scheduler_count = get_active_scheduler_count();

    if(victim < scheduler)
      // victim is before the first scheduler location
      // wrap around to the end.
      victim = &scheduler[current_active_scheduler_count - 1];

    if((victim == sched->last_victim) || (current_active_scheduler_count == 1))
    {
      // If we have tried all possible victims, return no victim. Set our last
      // victim to ourself to indicate we've started over.
      sched->last_victim = sched;
      break;
    }

    // Don't try to steal from ourself.
    if(victim == sched)
      continue;

    // Record that this is our victim and return it.
    sched->last_victim = victim;
    return victim;
  }

  return NULL;
}

/**
 * Suspend this thread for some time, including no sleep at all if
 * pop_global() can give us an actor immediately.
 *
 * WARNING: suspend_scheduler must be called in critical section
 *          protected by sched_mut/scheduler_count_changing,
 *          and we return with that mechanism:
 *            * Pthreads: locked, because pthread_thread_suspend() does
 *              not permit a choice to avoid reacquiring the mutex.
 *            * Non-Pthreads: unlocked, because after the suspend,
 *              we only need to reacquire in special case of
 *              sched->index==0.
 */
static pony_actor_t* suspend_scheduler(scheduler_t* sched,
  uint32_t current_active_scheduler_count)
{
  pony_actor_t* actor = NULL;

  // decrement active_scheduler_count so other schedulers know we're
  // sleeping
  uint32_t sched_count = get_active_scheduler_count();

  // make sure the scheduler count didn't change
  // if it did, then another thread resumed and it may not be
  // appropriate for us to suspend any longer, so don't suspend
  if(sched_count != current_active_scheduler_count) {
#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
    atomic_store_explicit(&scheduler_count_changing, false,
      memory_order_release);
#endif
    return actor;
  }

  atomic_store_explicit(&active_scheduler_count, sched_count - 1,
    memory_order_relaxed);

  // decrement active_scheduler_count_check
  uint32_t sched_count_check = get_active_scheduler_count_check();

  atomic_store_explicit(&active_scheduler_count_check, sched_count_check - 1,
    memory_order_relaxed);

  // ensure main active scheduler count and check variable match
  pony_assert(sched_count == sched_count_check);

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
  // unlock the bool that controls modifying the active scheduler count
  // variable if using signals
  atomic_store_explicit(&scheduler_count_changing, false,
    memory_order_release);
#endif

  // let sched 0 know we're suspending only after decrementing
  // active_scheduler_count to avoid a race condition between
  // when we update active_scheduler_count and scheduler 0 processes
  // the SCHED_SUSPEND message we send it. If we don't do this,
  // and scheduler 0 processes the SCHED_SUSPEND message before we
  // decrement active_scheduler_count, it could think that
  // active_scheduler_count > block_count and not start the CNF/ACK
  // process for termination and potentiall hang the runtime instead
  // of allowing it to reach quiescence.
  if(sched->index != 0)
    send_msg(sched->index, 0, SCHED_SUSPEND, 0);

  // dtrace suspend notification
  DTRACE1(THREAD_SUSPEND, (uintptr_t)sched);

  while(get_active_scheduler_count() <= (uint32_t)sched->index)
  {
    // if we're scheduler 0 with noisy actors check to make
    // sure inject queue is empty to avoid race condition
    // between thread 0 sleeping and the ASIO thread getting a
    // new event
    if(sched->index == 0)
    {
      actor = pop_global(NULL);
      if(actor != NULL)
        break;

      if(read_msg(sched))
      {
        // An actor was unmuted and added to our run queue. Pop it and return.
        // Effectively, we are "stealing" from ourselves. We need to verify that
        // popping succeeded (actor != NULL) as some other scheduler might have
        // stolen the newly scheduled actor from us already. Schedulers, what a
        // bunch of thieving bastards!
        actor = pop_global(sched);
        if(actor != NULL)
          break;
      }

      // if ASIO is no longer noisy due to reading a message from the ASIO
      // thread
      if(!sched->asio_noisy)
        break;
    }

    // sleep waiting for signal to wake up again
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
    ponyint_thread_suspend(sched->sleep_object, &sched_mut);
#else
    ponyint_thread_suspend(sched->sleep_object);
#endif
  }

  // dtrace resume notification
  DTRACE1(THREAD_RESUME, (uintptr_t)sched);

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
  // When using signals, need to acquire sched count changing variable
  while (true)
  {
    // get the bool that controls modifying the active scheduler
    // count variable if using signals
    if(!atomic_load_explicit(&scheduler_count_changing, memory_order_relaxed)
      && !atomic_exchange_explicit(&scheduler_count_changing, true,
      memory_order_acquire))
    {
#endif
      // get active_scheduler_count
      sched_count = get_active_scheduler_count();

      // make sure active_scheduler_count == 1 if it is 0
      // and we're scheduler 0 with noisy actors
      // and we just pulled an actor off the inject queue
      // or the ASIO is no longer noisy
      // and we broke out of the earlier loop before suspending
      // to return the actor
      if(sched_count == 0)
      {
        // set active_scheduler_count to 1
        sched_count = 1;
        atomic_store_explicit(&active_scheduler_count,
          sched_count, memory_order_relaxed);
      }

      // increment active_scheduler_count_check
      sched_count_check = get_active_scheduler_count_check();

      atomic_store_explicit(&active_scheduler_count_check,
        sched_count_check + 1, memory_order_relaxed);

      // ensure main active scheduler count and check variable match
      // pony_assert(sched_count == sched_count_check);

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
      // unlock the bool that controls modifying the active scheduler count
      // variable if using signals
      atomic_store_explicit(&scheduler_count_changing, false,
        memory_order_release);

      // break while loop
      break;
    }
  }
#endif

  return actor;
}

static pony_actor_t* perhaps_suspend_scheduler(
  scheduler_t* sched, uint32_t current_active_scheduler_count,
  bool* block_sent, uint32_t* steal_attempts, bool sched_is_blocked)
{
  // if we're the highest active scheduler thread
  // and there are more active schedulers than the minimum requested
  // and we're not terminating
  // and active scheduler count matchs the check variable indicating all
  // threads that should be awake are awake
  if ((current_active_scheduler_count > min_scheduler_count)
    && (sched == &scheduler[current_active_scheduler_count - 1])
    && (!sched->terminate)
    && (current_active_scheduler_count == get_active_scheduler_count_check())
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
    // try to acquire mutex if using pthreads
    && !pthread_mutex_trylock(&sched_mut)
#else
    // try and get the bool that controls modifying the active scheduler count
    // variable if using signals
    && (!atomic_load_explicit(&scheduler_count_changing, memory_order_relaxed)
      && !atomic_exchange_explicit(&scheduler_count_changing, true,
      memory_order_acquire))
#endif
    )
  {
    pony_actor_t* actor = NULL;

    // can only sleep if we're scheduler > 0 or if we're scheduler 0 and
    // there is at least one noisy actor registered
    if((sched->index > 0) || ((sched->index == 0) && sched->asio_noisy))
    {
      if (!sched_is_blocked)
      {
        // unblock before suspending to ensure cnf/ack cycle works as expected
        if(sched->index == 0)
          handle_sched_unblock(sched);
        else
          send_msg(sched->index, 0, SCHED_UNBLOCK, 0);

        *block_sent = false;
      }
      actor = suspend_scheduler(sched, current_active_scheduler_count);
      // reset steal_attempts so we try to steal from all other schedulers
      // prior to suspending again
      *steal_attempts = 0;
    }
    else
    {
      pony_assert(sched->index == 0);
      pony_assert(!sched->asio_noisy);
#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
      // suspend_scheduler() would have unlocked for us,
      // but we didn't call it, so unlock now.
      atomic_store_explicit(&scheduler_count_changing, false,
        memory_order_release);
#endif
      if (sched_is_blocked)
      {
        // send block message if there are no noisy actors registered
        // with the ASIO thread and this is scheduler 0
        handle_sched_block(sched);
        *block_sent = true;
      }
    }
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
    // unlock mutex if using pthreads
    pthread_mutex_unlock(&sched_mut);
#endif
    if(actor != NULL)
      return actor;
  }
  return NULL;
}

/**
 * Use mpmcqs to allow stealing directly from a victim, without waiting for a
 * response.
 */
static pony_actor_t* steal(scheduler_t* sched)
{
  bool block_sent = false;
  uint32_t steal_attempts = 0;
  uint64_t tsc = ponyint_cpu_tick();
  pony_actor_t* actor;
  scheduler_t* victim = NULL;

  while(true)
  {
    victim = choose_victim(sched);

    actor = pop_global(victim);
    if(actor != NULL)
      break;

    uint64_t tsc2 = ponyint_cpu_tick();

    if(read_msg(sched))
    {
      // An actor was unmuted and added to our run queue. Pop it and return.
      // Effectively, we are "stealing" from ourselves. We need to verify that
      // popping succeeded (actor != NULL) as some other scheduler might have
      // stolen the newly scheduled actor from us already. Schedulers, what a
      // bunch of thieving bastards!
      actor = pop_global(sched);
      if(actor != NULL)
        break;
    }

    if(quiescent(sched, tsc, tsc2))
    {
      DTRACE2(WORK_STEAL_FAILURE, (uintptr_t)sched, (uintptr_t)victim);
      return NULL;
    }

    // Determine if we are blocked.
    //
    // Note, "blocked" means we have no more work to do and we believe that we
    // should check to see if we can terminate the program.
    //
    // To be blocked, we have to:
    //
    // 1. Not have any noisy actors registered with the ASIO thread/subsystem.
    //    If we have any noisy actors then, while we might not have any work
    //    to do, we aren't blocked. Blocked means we can't make forward
    //    progress and the program might be ready to terminate. Noisy actors
    //    means that no, the program isn't ready to terminate becuase one of
    //    noisy actors could receive a message from an external source (timer,
    //    network, etc).
    // 2. Not have any muted actors. If we are holding any muted actors then,
    //    while we might not have any work to do, we aren't blocked. Blocked
    //    means we can't make forward progress and the program might be ready
    //    to terminate. Muted actors means that no, the program isn't ready
    //    to terminate.
    // 3. We have attempted to steal from every other scheduler and failed to
    //    get any work. In the process of stealing from every other scheduler,
    //    we will have also tried getting work off the ASIO inject queue
    //    multiple times
    // 4. We've been trying to steal for at least PONY_SCHED_BLOCK_THRESHOLD
    //    cycles (currently 1000000).
    //    In many work stealing scenarios, we immediately get steal an actor.
    //    Sending a block/unblock pair in that scenario is very wasteful.
    //    Same applies to other "quick" steal scenarios.
    //    1 million cycles is roughly 1 millisecond, depending on clock speed.
    //    By waiting 1 millisecond before sending a block message, we are going to
    //    delay quiescence by a small amount of time but also optimize work
    //    stealing for generating far fewer block/unblock messages.
    uint32_t current_active_scheduler_count = get_active_scheduler_count();
    uint64_t clocks_elapsed = tsc2 - tsc;

    if (!block_sent)
    {
      // make sure thread scaling order is still valid. we should never be
      // active if the active_scheduler_count isn't larger than our index.
      pony_assert(current_active_scheduler_count > (uint32_t)sched->index);

      if (steal_attempts < current_active_scheduler_count)
      {
        steal_attempts++;
      }
      else if ((clocks_elapsed > PONY_SCHED_BLOCK_THRESHOLD) &&
        (ponyint_mutemap_size(&sched->mute_mapping) == 0))
      {
        // only try and suspend if enough time has passed
        if(clocks_elapsed > scheduler_suspend_threshold)
        {
          // in case active scheduler count changed
          current_active_scheduler_count = get_active_scheduler_count();

          actor = perhaps_suspend_scheduler(sched, current_active_scheduler_count,
            &block_sent, &steal_attempts, true);
          if (actor != NULL)
            break;
        }

        if(!sched->asio_noisy)
        {
          // Only send block messages if there are no noisy actors registered
          // with the ASIO thread
          if(sched->index == 0)
            handle_sched_block(sched);
          else
            send_msg(sched->index, 0, SCHED_BLOCK, 0);

          block_sent = true;
        }
      }
    }
    else
    {
      // block sent and no work to do. We should try and suspend if we can now
      // if we do suspend, we'll send a unblock message first to ensure cnf/ack
      // cycle works as expected

      // make sure thread scaling order is still valid. we should never be
      // active if the active_scheduler_count isn't larger than our index.
      pony_assert(current_active_scheduler_count > (uint32_t)sched->index);

      // only try and suspend if enough time has passed
      if(clocks_elapsed > scheduler_suspend_threshold)
      {
        actor = perhaps_suspend_scheduler(sched, current_active_scheduler_count,
          &block_sent, &steal_attempts, false);
        if (actor != NULL)
          break;
      }
    }

    // if we're scheduler 0 and cycle detection is enabled
    if(!ponyint_actor_getnoblock() && (sched->index == 0))
    {
      // trigger cycle detector by sending it a message if it is time
      uint64_t current_tsc = ponyint_cpu_tick();
      if(ponyint_cycle_check_blocked(last_cd_tsc, current_tsc))
      {
        last_cd_tsc = current_tsc;

        // cycle detector should now be on the queue
        actor = pop_global(sched);
        if(actor != NULL)
          break;
      }
    }
  }

  if(block_sent)
  {
    // Only send unblock message if a corresponding block message was sent
    if(sched->index == 0)
      handle_sched_unblock(sched);
    else
      send_msg(sched->index, 0, SCHED_UNBLOCK, 0);
  }
  DTRACE3(WORK_STEAL_SUCCESSFUL, (uintptr_t)sched, (uintptr_t)victim, (uintptr_t)actor);
  return actor;
}

/**
 * Run a scheduler thread until termination.
 */
static void run(scheduler_t* sched)
{
  if(sched->index == 0)
    last_cd_tsc = 0;

  pony_actor_t* actor = pop_global(sched);

  if (DTRACE_ENABLED(ACTOR_SCHEDULED) && actor != NULL) {
    DTRACE2(ACTOR_SCHEDULED, (uintptr_t)sched, (uintptr_t)actor);
  }

  while(true)
  {
    // if we're scheduler 0
    if(sched->index == 0)
    {
      // if cycle detection is enabled
      if(!ponyint_actor_getnoblock())
      {
        // trigger cycle detector by sending it a message if it is time
        uint64_t current_tsc = ponyint_cpu_tick();
        if(ponyint_cycle_check_blocked(last_cd_tsc, current_tsc))
        {
          last_cd_tsc = current_tsc;

          // cycle detector should now be on the queue
          if(actor == NULL)
            actor = pop_global(sched);
        }
      }

      uint32_t current_active_scheduler_count = get_active_scheduler_count();
      uint32_t current_active_scheduler_count_check = get_active_scheduler_count_check();

      // if not all threads that should be awake are awake due to a missed signal
      if(current_active_scheduler_count != current_active_scheduler_count_check)
      {
        // send signals to all scheduler threads that should be awake
        // this is somewhat wasteful if a scheduler thread is already awake
        // but is necessary in case the signal to wake a thread was missed
        signal_suspended_threads(current_active_scheduler_count, sched->index);
      }
    }

    // In response to reading a message, we might have unmuted an actor and
    // added it back to our queue. if we don't have an actor to run, we want
    // to pop from our queue to check for a recently unmuted actor
    if(read_msg(sched) && actor == NULL)
    {
      actor = pop_global(sched);
    }

    if(actor == NULL)
    {
      // We had an empty queue and no rescheduled actor.
      actor = steal(sched);

      if(actor == NULL)
      {
        // Termination.
        pony_assert(pop(sched) == NULL);
        return;
      }
      DTRACE2(ACTOR_SCHEDULED, (uintptr_t)sched, (uintptr_t)actor);
    }

    // We have at least one muted actor...
    // Try and wake up a sleeping scheduler thread to help with the load.
    // This is to err on the side of caution and wake up more threads in case
    // of muted actors rather than potentially not wake up enough threads.
    // If there isn't enough work, they'll go back to sleep.
    // NOTE: This could result in a pathological case where only one thread
    // has a muted actor but there is only one overloaded actor. In this case
    // the extra scheduler threads would keep being woken up and then go back
    // to sleep over and over again.
    if(ponyint_mutemap_size(&sched->mute_mapping) > 0)
      ponyint_sched_maybe_wakeup(sched->index);

    // Run the current actor and get the next actor.
    bool reschedule = ponyint_actor_run(&sched->ctx, actor, false);
    pony_actor_t* next = pop_global(sched);

    if(reschedule)
    {
      if(next != NULL)
      {
        // If we have a next actor, we go on the back of the queue. Otherwise,
        // we continue to run this actor.
        push(sched, actor);
        DTRACE2(ACTOR_DESCHEDULED, (uintptr_t)sched, (uintptr_t)actor);
        actor = next;
        DTRACE2(ACTOR_SCHEDULED, (uintptr_t)sched, (uintptr_t)actor);

        // We have at least two actors worth of work; the one we just finished
        // running a batch for that needs to be rescheduled and the next one we
        // just `pop_global`d. This indicates that there is enough work in
        // theory to have another scheduler thread be woken up to do work in
        // parallel.
        // Try and wake up a sleeping scheduler thread to help with the load.
        // If there isn't enough work, they'll go back to sleep.
        ponyint_sched_maybe_wakeup(sched->index);
      }
    } else {
      // We aren't rescheduling, so run the next actor. This may be NULL if our
      // queue was empty.
      DTRACE2(ACTOR_DESCHEDULED, (uintptr_t)sched, (uintptr_t)actor);
      actor = next;
      if (DTRACE_ENABLED(ACTOR_SCHEDULED) && actor != NULL) {
        DTRACE2(ACTOR_SCHEDULED, (uintptr_t)sched, (uintptr_t)actor);
      }
    }
  }
}

static DECLARE_THREAD_FN(run_thread)
{
  scheduler_t* sched = (scheduler_t*) arg;
  this_scheduler = sched;
  ponyint_cpu_affinity(sched->cpu);

#if !defined(PLATFORM_IS_WINDOWS) && !defined(USE_SCHEDULER_SCALING_PTHREADS)
  // Make sure we block signals related to scheduler sleeping/waking
  // so they queue up to avoid race conditions
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, PONY_SCHED_SLEEP_WAKE_SIGNAL);
  pthread_sigmask(SIG_BLOCK, &set, NULL);
#endif

  run(sched);
  ponyint_pool_thread_cleanup();

  return 0;
}

static void ponyint_sched_shutdown()
{
  uint32_t start;

  start = 0;

  for(uint32_t i = start; i < scheduler_count; i++)
    ponyint_thread_join(scheduler[i].tid);

  DTRACE0(RT_END);
  ponyint_cycle_terminate();

  for(uint32_t i = 0; i < scheduler_count; i++)
  {
    while(ponyint_thread_messageq_pop(&scheduler[i].mq
#ifdef USE_DYNAMIC_TRACE
      , i
#endif
      ) != NULL) { ; }
    ponyint_messageq_destroy(&scheduler[i].mq);
    ponyint_mpmcq_destroy(&scheduler[i].q);

#if defined(PLATFORM_IS_WINDOWS)
    // close wait event objects
    CloseHandle(scheduler[i].sleep_object);
#elif defined(USE_SCHEDULER_SCALING_PTHREADS)
    // destroy pthread condition object
    pthread_cond_destroy(scheduler[i].sleep_object);
#ifdef USE_MEMTRACK
    mem_used -= sizeof(pthread_cond_t);
    mem_allocated -= POOL_ALLOC_SIZE(pthread_cond_t);
#endif
    POOL_FREE(pthread_cond_t, scheduler[i].sleep_object);
    // set sleep condition object to NULL
    scheduler[i].sleep_object = NULL;
#endif
  }

  ponyint_pool_free_size(scheduler_count * sizeof(scheduler_t), scheduler);
#ifdef USE_MEMTRACK
  mem_used -= (scheduler_count * sizeof(scheduler_t));
  mem_allocated -= (ponyint_pool_used_size(scheduler_count
    * sizeof(scheduler_t)));
#endif
  scheduler = NULL;
  inject_context = NULL;
  scheduler_count = 0;
  atomic_store_explicit(&active_scheduler_count, 0, memory_order_relaxed);

  ponyint_mpmcq_destroy(&inject);
}

pony_ctx_t* ponyint_sched_init(uint32_t threads, bool noyield, bool pin,
  bool pinasio, uint32_t min_threads, uint32_t thread_suspend_threshold)
{
  pony_register_thread();

  use_yield = !noyield;

  // if thread suspend threshold is less then 1, then ensure it is 1
  if(thread_suspend_threshold < 1)
    thread_suspend_threshold = 1;

  // If no thread count is specified, use the available physical core count.
  if(threads == 0)
    threads = ponyint_cpu_count();

  // If minimum thread count is > thread count, cap it at thread count
  if(min_threads > threads)
    min_threads = threads; // this becomes the equivalent of --ponynoscale

  // convert to cycles for use with ponyint_cpu_tick()
  // 1 second = 2000000000 cycles (approx.)
  // based on same scale as ponyint_cpu_core_pause() uses
  scheduler_suspend_threshold = thread_suspend_threshold * 1000000;

  scheduler_count = threads;
  min_scheduler_count = min_threads;
  atomic_store_explicit(&active_scheduler_count, scheduler_count,
    memory_order_relaxed);
  atomic_store_explicit(&active_scheduler_count_check, scheduler_count,
    memory_order_relaxed);
  scheduler = (scheduler_t*)ponyint_pool_alloc_size(
    scheduler_count * sizeof(scheduler_t));
#ifdef USE_MEMTRACK
  mem_used += (scheduler_count * sizeof(scheduler_t));
  mem_allocated += (ponyint_pool_used_size(scheduler_count
    * sizeof(scheduler_t)));
#endif
  memset(scheduler, 0, scheduler_count * sizeof(scheduler_t));

  uint32_t asio_cpu = ponyint_cpu_assign(scheduler_count, scheduler, pin,
    pinasio);

#if !defined(PLATFORM_IS_WINDOWS) && defined(USE_SCHEDULER_SCALING_PTHREADS)
  pthread_once(&sched_mut_once, sched_mut_init);
#endif

  for(uint32_t i = 0; i < scheduler_count; i++)
  {
#if defined(PLATFORM_IS_WINDOWS)
    // create wait event objects
    scheduler[i].sleep_object = CreateEvent(NULL, FALSE, FALSE, NULL);
#elif defined(USE_SCHEDULER_SCALING_PTHREADS)
    // create pthread condition object
#ifdef USE_MEMTRACK
    mem_used += sizeof(pthread_cond_t);
    mem_allocated += POOL_ALLOC_SIZE(pthread_cond_t);
#endif
    scheduler[i].sleep_object = POOL_ALLOC(pthread_cond_t);
    int ret = pthread_cond_init(scheduler[i].sleep_object, NULL);
    if(ret != 0)
    {
      // if it failed, set `sleep_object` to `NULL` for error
#ifdef USE_MEMTRACK
      mem_used -= sizeof(pthread_cond_t);
      mem_allocated -= POOL_ALLOC_SIZE(pthread_cond_t);
#endif
      POOL_FREE(pthread_cond_t, scheduler[i].sleep_object);
      scheduler[i].sleep_object = NULL;
    }
#else
    scheduler[i].sleep_object = PONY_SCHED_SLEEP_WAKE_SIGNAL;
#endif

    scheduler[i].ctx.scheduler = &scheduler[i];
    scheduler[i].last_victim = &scheduler[i];
    scheduler[i].index = i;
    scheduler[i].asio_noisy = false;
    ponyint_messageq_init(&scheduler[i].mq);
    ponyint_mpmcq_init(&scheduler[i].q);
  }

  ponyint_mpmcq_init(&inject);
  ponyint_asio_init(asio_cpu);

  inject_context = pony_ctx();

  return inject_context;
}

bool ponyint_sched_start(bool library)
{
  pony_register_thread();

  if(!ponyint_asio_start())
    return false;

  atomic_store_explicit(&detect_quiescence, !library, memory_order_relaxed);

  DTRACE0(RT_START);
  uint32_t start = 0;

  for(uint32_t i = start; i < scheduler_count; i++)
  {
#if defined(PLATFORM_IS_WINDOWS) || defined(USE_SCHEDULER_SCALING_PTHREADS)
    // there was an error creating a wait event or a pthread condition object
    if(scheduler[i].sleep_object == NULL)
      return false;
#endif

    if(!ponyint_thread_create(&scheduler[i].tid, run_thread, scheduler[i].cpu,
      &scheduler[i]))
      return false;
  }

  if(!library)
  {
    ponyint_sched_shutdown();
  }

  return true;
}

void ponyint_sched_stop()
{
  atomic_store_explicit(&detect_quiescence, true, memory_order_relaxed);
  ponyint_sched_shutdown();
}

void ponyint_sched_add(pony_ctx_t* ctx, pony_actor_t* actor)
{
  if(ctx->scheduler != NULL)
  {
    // Add to the current scheduler thread.
    push(ctx->scheduler, actor);
  } else {
    // Put on the shared mpmcq.
    ponyint_mpmcq_push(&inject, actor);
  }
}

uint32_t ponyint_sched_cores()
{
  return scheduler_count;
}

uint32_t ponyint_active_sched_count()
{
  return get_active_scheduler_count();
}

PONY_API void pony_register_thread()
{
  if(this_scheduler != NULL)
    return;

  // Create a scheduler_t, even though we will only use the pony_ctx_t.
  this_scheduler = POOL_ALLOC(scheduler_t);
  memset(this_scheduler, 0, sizeof(scheduler_t));
  this_scheduler->tid = ponyint_thread_self();
  this_scheduler->index = -1;
}

PONY_API void pony_unregister_thread()
{
  if(this_scheduler == NULL)
    return;

  POOL_FREE(scheduler_t, this_scheduler);
  this_scheduler = NULL;

  ponyint_pool_thread_cleanup();
}

PONY_API pony_ctx_t* pony_ctx()
{
  pony_assert(this_scheduler != NULL);
  return &this_scheduler->ctx;
}

// Tell all scheduler threads that asio is noisy
void ponyint_sched_noisy_asio(int32_t from)
{
  send_msg_all(from, SCHED_NOISY_ASIO, 0);
}

// Tell all scheduler threads that asio is not noisy
void ponyint_sched_unnoisy_asio(int32_t from)
{
  send_msg_all(from, SCHED_UNNOISY_ASIO, 0);
}

// Maybe wake up a scheduler thread if possible
void ponyint_sched_maybe_wakeup_if_all_asleep(int32_t current_scheduler_id)
{
  uint32_t current_active_scheduler_count = get_active_scheduler_count();

  // wake up threads if the current active count is 0
  // keep trying until successful to avoid deadlock
  while((current_active_scheduler_count = get_active_scheduler_count()) == 0)
  {
    ponyint_sched_maybe_wakeup(current_scheduler_id);

    current_active_scheduler_count = get_active_scheduler_count();

    if(current_active_scheduler_count >= 1)
    {
      // wait for sleeping threads to wake and update check variable
      // to ensure that we have at least one fully awake scheduler thread
      while (get_active_scheduler_count() != get_active_scheduler_count_check())
      {
        // send signals to all scheduler threads that should be awake
        // this is somewhat wasteful if a scheduler thread is already awake
        // but is necessary in case the signal to wake a thread was missed
        // NOTE: this intentionally allows for the case where some scheduler
        // threads might miss the signal and not wake up. That is handled in
        // by a combination of the check variable and this while loop
        signal_suspended_threads(current_active_scheduler_count, current_scheduler_id);
      }
    }
  }
}

pony_ctx_t* ponyint_sched_get_inject_context() {
  return inject_context;
}

// Maybe wake up a scheduler thread if possible
void ponyint_sched_maybe_wakeup(int32_t current_scheduler_id)
{
  uint32_t current_active_scheduler_count = get_active_scheduler_count();

  // if we have some schedulers that are sleeping, wake one up
  if((current_active_scheduler_count < scheduler_count) &&
#if defined(USE_SCHEDULER_SCALING_PTHREADS)
    // try to acquire mutex if using pthreads
    !pthread_mutex_trylock(&sched_mut)
#else
    // try and get the bool that controls modifying the active scheduler count
    // variable if using signals
    (!atomic_load_explicit(&scheduler_count_changing, memory_order_relaxed)
      && !atomic_exchange_explicit(&scheduler_count_changing, true,
      memory_order_acquire))
#endif
    )
  {
    // in case the count changed between the while check and now
    current_active_scheduler_count = get_active_scheduler_count();

    if(current_active_scheduler_count < scheduler_count)
    {
      // increment active_scheduler_count to wake a new scheduler up
      current_active_scheduler_count++;
      atomic_store_explicit(&active_scheduler_count, current_active_scheduler_count,
        memory_order_relaxed);
    }

#if !defined(USE_SCHEDULER_SCALING_PTHREADS)
    // unlock the bool that controls modifying the active scheduler count
    // variable if using signals.
    atomic_store_explicit(&scheduler_count_changing, false,
      memory_order_release);
#endif

    // send signals to all scheduler threads that should be awake
    // this is somewhat wasteful if a scheduler thread is already awake
    // NOTE: this intentionally allows for the case where some scheduler
    // threads might miss the signal and not wake up. That is handled as
    // part of the beginning of the `run` loop and the while loop in
    // ponyint_sched_maybe_wakeup_if_all_asleep
    signal_suspended_threads(current_active_scheduler_count, current_scheduler_id);

#if defined(USE_SCHEDULER_SCALING_PTHREADS)
    // unlock mutex if using pthreads
    pthread_mutex_unlock(&sched_mut);
#endif
  }
}

// Manage a scheduler's mute map
//
// When an actor attempts to send to an overloaded actor, it will be added
// to the mute map for this scheduler. The mute map is in the form of:
//
// overloaded receiving actor => [sending actors]
//
// - A given actor will only existing as a sending actor in the map for
//   a given scheduler.
// - Receiving actors can exist as a mute map key in the mute map of more
//   than one scheduler
//
// Because muted sending actors only exist in a single scheduler's mute map
// and because they aren't scheduled when they are muted, any manipulation
// that we do on their state (for example incrementing or decrementing their
// mute count) is thread safe as only a single scheduler thread will be
// accessing the information.

void ponyint_sched_mute(pony_ctx_t* ctx, pony_actor_t* sender, pony_actor_t* recv)
{
  pony_assert(sender != recv);
  scheduler_t* sched = ctx->scheduler;
  size_t index = HASHMAP_UNKNOWN;
  muteref_t key;
  key.key = recv;

  muteref_t* mref = ponyint_mutemap_get(&sched->mute_mapping, &key, &index);
  if(mref == NULL)
  {
    mref = ponyint_muteref_alloc(recv);
#ifdef USE_MEMTRACK
    ctx->mem_used += sizeof(muteref_t);
    ctx->mem_allocated += POOL_ALLOC_SIZE(muteref_t);
    int64_t old_mmap_mem_size = ponyint_mutemap_mem_size(&sched->mute_mapping);
    int64_t old_mmap_alloc_size =
      ponyint_mutemap_alloc_size(&sched->mute_mapping);
#endif
    ponyint_mutemap_putindex(&sched->mute_mapping, mref, index);
#ifdef USE_MEMTRACK
    int64_t new_mmap_mem_size = ponyint_mutemap_mem_size(&sched->mute_mapping);
    int64_t new_mmap_alloc_size =
      ponyint_mutemap_alloc_size(&sched->mute_mapping);
    ctx->mem_used += (new_mmap_mem_size - old_mmap_mem_size);
    ctx->mem_allocated += (new_mmap_alloc_size - old_mmap_alloc_size);
#endif
  }

  size_t index2 = HASHMAP_UNKNOWN;
  pony_actor_t* r = ponyint_muteset_get(&mref->value, sender, &index2);
  if(r == NULL)
  {
    // This is safe because an actor can only ever be in a single scheduler's
    // mutemap
#ifdef USE_MEMTRACK
    int64_t old_mset_mem_size = ponyint_muteset_mem_size(&mref->value);
    int64_t old_mset_alloc_size = ponyint_muteset_alloc_size(&mref->value);
#endif
    ponyint_muteset_putindex(&mref->value, sender, index2);
    atomic_fetch_add_explicit(&sender->muted, 1, memory_order_relaxed);
#ifdef USE_MEMTRACK
    int64_t new_mset_mem_size = ponyint_muteset_mem_size(&mref->value);
    int64_t new_mset_alloc_size = ponyint_muteset_alloc_size(&mref->value);
    ctx->mem_used += (new_mset_mem_size - old_mset_mem_size);
    ctx->mem_allocated += (new_mset_alloc_size - old_mset_alloc_size);
    pony_assert(ctx->mem_used >= 0);
    pony_assert(ctx->mem_allocated >= 0);
#endif
  }
#ifdef USE_MEMTRACK
  pony_assert(ctx->mem_used ==
    (int64_t)ponyint_mutemap_total_mem_size(&sched->mute_mapping));
  pony_assert(ctx->mem_allocated ==
    (int64_t)ponyint_mutemap_total_alloc_size(&sched->mute_mapping));
#endif
}

void ponyint_sched_start_global_unmute(uint32_t from, pony_actor_t* actor)
{
  send_msg_all_active(from, SCHED_UNMUTE_ACTOR, (intptr_t)actor);
}

DECLARE_STACK(ponyint_actorstack, actorstack_t, pony_actor_t);
DEFINE_STACK(ponyint_actorstack, actorstack_t, pony_actor_t);

bool ponyint_sched_unmute_senders(pony_ctx_t* ctx, pony_actor_t* actor)
{
  size_t actors_rescheduled = 0;
  scheduler_t* sched = ctx->scheduler;
  size_t index = HASHMAP_UNKNOWN;
  muteref_t key;
  key.key = actor;

  muteref_t* mref = ponyint_mutemap_get(&sched->mute_mapping, &key, &index);

  if(mref != NULL)
  {
    size_t i = HASHMAP_UNKNOWN;
    pony_actor_t* muted = NULL;
    actorstack_t* needs_unmuting = NULL;

#ifdef USE_MEMTRACK
    ctx->mem_used -= sizeof(muteref_t);
    ctx->mem_allocated -= POOL_ALLOC_SIZE(muteref_t);
    ctx->mem_used -= ponyint_muteset_mem_size(&mref->value);
    ctx->mem_allocated -= ponyint_muteset_alloc_size(&mref->value);
    pony_assert(ctx->mem_used >= 0);
    pony_assert(ctx->mem_allocated >= 0);
#endif

    // Find and collect any actors that need to be unmuted
    while((muted = ponyint_muteset_next(&mref->value, &i)) != NULL)
    {
      // This is safe because an actor can only ever be in a single scheduler's
      // mutemap
      size_t muted_count = atomic_fetch_sub_explicit(&muted->muted, 1, memory_order_relaxed);
      pony_assert(muted_count > 0);

      // If muted_count used to be 1 before we decremented it; then the actor
      // is longer muted
      if(muted_count == 1)
      {
        needs_unmuting = ponyint_actorstack_push(needs_unmuting, muted);
      }
    }

    ponyint_mutemap_removeindex(&sched->mute_mapping, index);
    ponyint_muteref_free(mref);

    // Unmute any actors that need to be unmuted
    pony_actor_t* to_unmute;

    while(needs_unmuting != NULL)
    {
      needs_unmuting = ponyint_actorstack_pop(needs_unmuting, &to_unmute);

      if(!has_flag(to_unmute, FLAG_UNSCHEDULED))
      {
        ponyint_unmute_actor(to_unmute);
        // TODO: we don't want to reschedule if our queue is empty.
        // That's wasteful.
        ponyint_sched_add(ctx, to_unmute);
        DTRACE2(ACTOR_SCHEDULED, (uintptr_t)sched, (uintptr_t)to_unmute);
        actors_rescheduled++;
      }

      ponyint_sched_start_global_unmute(ctx->scheduler->index, to_unmute);
    }
  }

#ifdef USE_MEMTRACK
  pony_assert(ctx->mem_used ==
    (int64_t)ponyint_mutemap_total_mem_size(&sched->mute_mapping));
  pony_assert(ctx->mem_allocated ==
    (int64_t)ponyint_mutemap_total_alloc_size(&sched->mute_mapping));
#endif

  return actors_rescheduled > 0;
}

// Return the scheduler's index
PONY_API int32_t pony_sched_index(pony_ctx_t* ctx)
{
  return ctx->scheduler->index;
}
