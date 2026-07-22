#define PONY_WANT_ATOMIC_DEFS

#include "scheduler.h"
#include "systematic_testing.h"
#include "cpu.h"
#include "mpmcq.h"
#include "../actor/actor.h"
#include "../gc/cycle.h"
#include "../asio/asio.h"
#include "../mem/pagemap.h"
#include "../mem/pool.h"
#include "ponyassert.h"
#include <dtrace.h>
#include <inttypes.h>
#include <string.h>
#include "mutemap.h"
#include "../tracing/tracing.h"
#ifdef USE_SYSTEMATIC_TESTING
#include <stdlib.h>
#endif

#ifdef USE_RUNTIMESTATS
#include <stdio.h>
#endif

#define PONY_SCHED_BLOCK_THRESHOLD 2000000

/// How long a passive thread pauses between visits: starts fast,
/// doubles per quiet visit toward the cap, and snaps back to the floor
/// when a visit consumes a message. The pause is the only wait there
/// is — nothing wakes a passive thread; mail is read at its next
/// visit.
#define SCHED_TICK_MIN_NS UINT64_C(10000000)
#define SCHED_TICK_MAX_NS UINT64_C(500000000)

PONY_EXTERN_C_BEGIN

static DECLARE_THREAD_FN(run_thread);

// Scheduler global data.
static uint64_t last_cd_tsc;
static uint32_t scheduler_count;
static uint32_t min_scheduler_count;
static uint64_t scheduler_suspend_threshold;
static scheduler_t* scheduler;
/// How many schedulers are active, as a gauge: each thread adjusts it
/// by one at its own transitions. Advisory — nothing correctness-
/// critical reads it: pony_active_schedulers reports it, the ASIO
/// thread's all-passive check reads it, and the steal loop paces its
/// block decision with it.
static PONY_ATOMIC(uint32_t) active_scheduler_gauge;
static PONY_ATOMIC(bool) runtime_shutdown_initiated;
static bool use_yield;
static mpmcq_t inject;
static scheduler_t* pinned_actor_scheduler;
static __pony_thread_local scheduler_t* this_scheduler;

#ifdef USE_RUNTIMESTATS
// holds only size of the scheduler_t array
static size_t mem_allocated;
static size_t mem_used;
static bool print_stats;
static uint64_t print_stats_interval;

void print_scheduler_stats(scheduler_t* sched)
{
  printf("Scheduler stats for index: %d, "
        "total memory allocated: %" PRId64 ", "
        "total memory used: %" PRId64 ", "
        "created actors counter: %zu, "
        "destroyed actors counter: %zu, "
        "actors app cpu: %zu, "
        "actors gc marking cpu: %zu, "
        "actors gc sweeping cpu: %zu, "
        "actors system cpu: %zu, "
        "scheduler msgs cpu: %zu, "
        "scheduler misc cpu: %zu, "
        "memory used inflight messages: %" PRId64 ", "
        "memory allocated inflight messages: %" PRId64 ", "
        "number of inflight messages: %" PRId64 "\n",
        sched->index,
        sched->ctx.schedulerstats.mem_used + sched->ctx.schedulerstats.mem_used_actors,
        sched->ctx.schedulerstats.mem_allocated + sched->ctx.schedulerstats.mem_allocated_actors,
        sched->ctx.schedulerstats.created_actors_counter,
        sched->ctx.schedulerstats.destroyed_actors_counter,
        sched->ctx.schedulerstats.actor_app_cpu,
        sched->ctx.schedulerstats.actor_gc_mark_cpu,
        sched->ctx.schedulerstats.actor_gc_sweep_cpu,
        sched->ctx.schedulerstats.actor_system_cpu,
        sched->ctx.schedulerstats.msg_cpu,
        sched->ctx.schedulerstats.misc_cpu,
#ifdef USE_RUNTIMESTATS_MESSAGES
        sched->ctx.schedulerstats.mem_used_inflight_messages,
        sched->ctx.schedulerstats.mem_allocated_inflight_messages,
        sched->ctx.schedulerstats.num_inflight_messages
#else
        (int64_t)0,
        (int64_t)0,
        (int64_t)0
#endif
        );
}

/* Get whether stat printing is on */
bool ponyint_sched_print_stats()
{
  return print_stats;
}

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
      ctx->schedulerstats.mem_used_actors
      // memory used for mutemap
    + ctx->schedulerstats.mem_used;
}

size_t ponyint_sched_total_alloc_size(pony_ctx_t* ctx)
{
  return
      // memory allocated for each actor struct
      // + memory allocated for actormaps for gc acquire/release messages
      ctx->schedulerstats.mem_allocated_actors
      // memory allocated for mutemap
    + ctx->schedulerstats.mem_allocated;
}
#endif

/**
 * Gets the current active scheduler count
 */
static uint32_t get_active_scheduler_count()
{
   return atomic_load_explicit(&active_scheduler_gauge, memory_order_relaxed);
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
 * Sends a message to the pinned actor thread.
 */

static void send_msg_pinned_actor_thread(uint32_t from, sched_msg_t msg, intptr_t arg)
{
  pony_msgi_t* m = (pony_msgi_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(pony_msgi_t)), msg);

  TRACING_THREAD_SEND_MESSAGE(m, msg, arg, from, PONY_PINNED_ACTOR_THREAD_INDEX);

#ifdef USE_RUNTIMESTATS_MESSAGES
  this_scheduler->ctx.schedulerstats.num_inflight_messages--;
  this_scheduler->ctx.schedulerstats.mem_used_inflight_messages += sizeof(pony_msgi_t);
  this_scheduler->ctx.schedulerstats.mem_used_inflight_messages -= POOL_ALLOC_SIZE(pony_msgi_t);
#endif

  m->i = arg;

  ponyint_thread_messageq_push(&pinned_actor_scheduler->mq, &m->msg, &m->msg
#ifdef USE_DYNAMIC_TRACE
    , from, PONY_PINNED_ACTOR_THREAD_INDEX
#endif
    );
  (void)from;
}

/**
 * Sends a message to a thread.
 */

static void send_msg(uint32_t from, uint32_t to, sched_msg_t msg, intptr_t arg)
{
  pony_msgi_t* m = (pony_msgi_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(pony_msgi_t)), msg);

  TRACING_THREAD_SEND_MESSAGE(m, msg, arg, from, to);

#ifdef USE_RUNTIMESTATS_MESSAGES
  this_scheduler->ctx.schedulerstats.num_inflight_messages--;
  this_scheduler->ctx.schedulerstats.mem_used_inflight_messages += sizeof(pony_msgi_t);
  this_scheduler->ctx.schedulerstats.mem_used_inflight_messages -= POOL_ALLOC_SIZE(pony_msgi_t);
#endif

  m->i = arg;
  ponyint_thread_messageq_push(&scheduler[to].mq, &m->msg, &m->msg
#ifdef USE_DYNAMIC_TRACE
    , from, to
#endif
    );
  (void)from;
}

static void send_msg_all(uint32_t from, sched_msg_t msg, intptr_t arg)
{
  for(uint32_t i = 0; i < scheduler_count; i++)
    send_msg(from, i, msg, arg);
}

/// How many words the per-scheduler blocked bitmap holds.
static size_t blocked_map_words()
{
  return (scheduler_count + 63) / 64;
}

#if !defined(PONY_NDEBUG) || defined(PONY_ALWAYS_ASSERT)
/// Population count of a scheduler's blocked map. Debug-only caller;
/// scheduler 0's map and its block_count are fed by the same message
/// stream, so they must always agree.
static uint32_t blocked_map_count(scheduler_t* sched)
{
  uint32_t count = 0;

  for(size_t i = 0; i < blocked_map_words(); i++)
  {
    count += __pony_popcount((uint32_t)sched->blocked_map[i]);
    count += __pony_popcount((uint32_t)(sched->blocked_map[i] >> 32));
  }

  return count;
}
#endif

// handle SCHED_BLOCK message
static void handle_sched_block(scheduler_t* sched)
{
  sched->block_count++;
  pony_assert(sched->block_count <= scheduler_count);

  // reset ack token and count because block count changed
  sched->ack_token++;
  sched->asio_stoppable = false;
  sched->ack_count = scheduler_count;
  pony_assert(sched->ack_count > 0);

  // start cnf/ack cycle for quiescence if block count == scheduler_count
  // only if there are no noisy actors subscribed with the ASIO subsystem
  // and the mutemap is empty
  if(!sched->asio_noisy &&
    ponyint_mutemap_size(&sched->mute_mapping) == 0 &&
    sched->block_count == scheduler_count)
  {
      // If we think all threads are blocked, send CNF(token) to everyone.
      // and to the pinned actor thread
      sched->ack_count = scheduler_count + 1;
      send_msg_all(sched->index, SCHED_CNF, sched->ack_token);
      send_msg_pinned_actor_thread(sched->index, SCHED_CNF, sched->ack_token);
  }
}

// handle SCHED_UNBLOCK message
static void handle_sched_unblock(scheduler_t* sched)
{
  // Cancel all acks and increment the ack token, so that any pending
  // acks in the queue will be dropped when they are received.
  sched->block_count--;
  sched->ack_token++;
  sched->asio_stoppable = false;
  sched->ack_count = scheduler_count;
  pony_assert(sched->ack_count > 0);
  pony_assert(sched->block_count <= scheduler_count);
}

/// A quiescence probe recorded during a visit's message read, to be
/// answered only after the visit's work check comes up empty.
typedef struct recorded_cnf_t
{
  bool recorded;
  intptr_t token;
} recorded_cnf_t;

/**
 * Reads the message queue to completion. With recorded_cnf NULL, a
 * CNF is ACKed inline when the caller is empty-handed (actor is
 * NULL), as the active loops require. A passive or pinned-thread
 * visit passes recorded_cnf instead: the token is recorded and the
 * caller answers only if its work check comes up empty — never ACK a
 * probe with work in hand. consumed, when non-NULL, reports whether
 * any message was read at all; the visit's tick backoff resets on it.
 */
static bool read_msg(scheduler_t* sched, pony_actor_t* actor,
  recorded_cnf_t* recorded_cnf, bool* consumed)
{
#ifdef USE_RUNTIMESTATS
    uint64_t used_cpu = ponyint_sched_cpu_used(&sched->ctx);
    sched->ctx.schedulerstats.misc_cpu += used_cpu;
#endif

  pony_msgi_t* m;

  bool run_queue_changed = false;

  while((m = (pony_msgi_t*)ponyint_thread_messageq_pop(&sched->mq
#ifdef USE_DYNAMIC_TRACE
    , sched->index
#endif
    )) != NULL)
  {
#ifdef USE_RUNTIMESTATS_MESSAGES
    sched->ctx.schedulerstats.num_inflight_messages--;
    sched->ctx.schedulerstats.mem_used_inflight_messages -= sizeof(pony_msgi_t);
    sched->ctx.schedulerstats.mem_allocated_inflight_messages -= POOL_ALLOC_SIZE(pony_msgi_t);
#endif

    TRACING_THREAD_RECEIVE_MESSAGE(m, (sched_msg_t)m->msg.id, m->i);

    if(consumed != NULL)
      *consumed = true;

    switch(m->msg.id)
    {
      case SCHED_BLOCK:
      {
        sched->blocked_map[m->i / 64] |= (UINT64_C(1) << (m->i % 64));

        if(0 == sched->index)
        {
          handle_sched_block(sched);
          pony_assert(blocked_map_count(sched) == sched->block_count);
        }
        break;
      }

      case SCHED_UNBLOCK:
      {
        sched->blocked_map[m->i / 64] &= ~(UINT64_C(1) << (m->i % 64));

        // The sender took work in hand, so it is a fresh activation
        // target again.
        sched->poked_map[m->i / 64] &= ~(UINT64_C(1) << (m->i % 64));

        if(0 == sched->index)
        {
          handle_sched_unblock(sched);
          pony_assert(blocked_map_count(sched) == sched->block_count);
        }
        break;
      }

      case SCHED_ACTIVATE:
      {
        pony_assert(PONY_UNKNOWN_SCHEDULER_INDEX != sched->index);
        pony_assert(PONY_PINNED_ACTOR_THREAD_INDEX != sched->index);

        sched->activate_requested = true;
        break;
      }

      case SCHED_CNF:
      {
        pony_assert(PONY_UNKNOWN_SCHEDULER_INDEX != sched->index);

        if(recorded_cnf != NULL)
        {
          // A later CNF carries a newer token; the stale one would be
          // dropped by the token check on scheduler 0 anyway.
          recorded_cnf->recorded = true;
          recorded_cnf->token = m->i;
        }
        else if(NULL == actor)
        {
          // Echo the token back as ACK(token) only if we don't have an actor to run.
          send_msg(sched->index, 0, SCHED_ACK, m->i);
        }
        break;
      }

      case SCHED_ACK:
      {
        pony_assert(0 == sched->index);

        // If it's the current token, decrement the ack count for # of schedulers
        // to expect an ACK from.
        if(m->i == sched->ack_token)
          sched->ack_count--;
        break;
      }

      case SCHED_TERMINATE:
      {
        pony_assert(PONY_UNKNOWN_SCHEDULER_INDEX != sched->index);
        sched->terminate = true;
        break;
      }

      case SCHED_UNMUTE_ACTOR:
      {
        pony_assert(PONY_UNKNOWN_SCHEDULER_INDEX != sched->index);

        if (ponyint_sched_unmute_senders(&sched->ctx, (pony_actor_t*)m->i))
          run_queue_changed = true;

        break;
      }

      case SCHED_NOISY_ASIO:
      {
        pony_assert(PONY_UNKNOWN_SCHEDULER_INDEX != sched->index);
        pony_assert(PONY_PINNED_ACTOR_THREAD_INDEX != sched->index);

        // mark asio as being noisy
        sched->asio_noisy++;
        break;
      }

      case SCHED_UNNOISY_ASIO:
      {
        pony_assert(PONY_UNKNOWN_SCHEDULER_INDEX != sched->index);
        pony_assert(PONY_PINNED_ACTOR_THREAD_INDEX != sched->index);

        // mark asio as not being noisy
        sched->asio_noisy--;
        pony_assert(sched->asio_noisy >= 0);
        break;
      }

      default: {}
    }
  }

#ifdef USE_RUNTIMESTATS
    used_cpu = ponyint_sched_cpu_used(&sched->ctx);
    sched->ctx.schedulerstats.msg_cpu += used_cpu;
#endif

  return run_queue_changed;
}

/**
 * If we can terminate, return true. If all schedulers are waiting, one of
 * them will stop the ASIO back end and tell the cycle detector to try to
 * terminate.
 */

// Clock source for the scheduler's clock-gated control flow (block/suspend
// thresholds and the cycle-detector cadence). Under systematic testing we read
// the deterministic logical clock instead of the wall clock so those decisions
// — and therefore the seeded thread-selection stream — reproduce run to run.
// The matching ponyint_cpu_tick_diff() calls are left as-is: on x86-64 and
// Windows (the only platforms systematic testing builds on) PONY_CPU_TICK_BITS
// is 64, so tick_diff(a, b) is exactly b - a, correct for a monotonic clock.
static uint64_t sched_clock(void)
{
#if defined(USE_SYSTEMATIC_TESTING)
  return SYSTEMATIC_TESTING_CLOCK();
#else
  return ponyint_cpu_tick();
#endif
}

static bool quiescent(scheduler_t* sched, uint64_t tsc, uint64_t tsc2)
{
  if(sched->terminate)
    return true;

  // only scheduler 0 can initiate shutdown (it is the ony that gets all the
  // ACK messages as part of the CNF/ACK coordination for shutdown)
  // only if there are no noisy actors registered with the ASIO subsystem
  // and the mutemap is empty...
  if(0 == sched->index && !sched->asio_noisy && ponyint_mutemap_size(&sched->mute_mapping) == 0)
  {
    // 0 means that all schedulers have ACK'd and we can proceed with shutdown..
    // if any scheduler threads block/unblock before we get ACKs from them all
    // then the ack_token is incremented and the ack_count is reset and we start
    // the countdown to `ack_count == 0` all over again
    if(0 == sched->ack_count)
    {
      if(sched->asio_stoppable && ponyint_asio_stop())
      {
        // successfully stopped ASIO thread
        // tell all scheduler threads to terminate
        send_msg_all(sched->index, SCHED_TERMINATE, 0);
        send_msg_pinned_actor_thread(sched->index, SCHED_TERMINATE, 0);

        sched->ack_token++;
        sched->ack_count = scheduler_count;
        pony_assert(sched->ack_count > 0);
      } else if(ponyint_asio_stoppable()) {
        sched->asio_stoppable = true;
        sched->ack_token++;

        // Run another CNF/ACK cycle.
        // send CNF(token) to everyone.
        // and to the pinned actor thread
        sched->ack_count = scheduler_count + 1;
        send_msg_all(sched->index, SCHED_CNF, sched->ack_token);
        send_msg_pinned_actor_thread(sched->index, SCHED_CNF, sched->ack_token);
      } else {
        // reset ack_token/count for shutdown coordination
        sched->ack_token++;
        sched->asio_stoppable = false;
        sched->ack_count = scheduler_count;
        pony_assert(sched->ack_count > 0);
      }
    }
  }

#if defined(USE_SYSTEMATIC_TESTING)
  (void)tsc;
  (void)tsc2;
  SYSTEMATIC_TESTING_YIELD();
#else
  ponyint_cpu_core_pause(tsc, tsc2, use_yield);
#endif

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

    if(victim < scheduler)
      // victim is before the first scheduler location
      // wrap around to the end.
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

    // Skip victims this thread believes are blocked: their queues are
    // empty. The map can lag — a stale bit costs one missed steal
    // opportunity until the unblock broadcast is read, never a lost
    // one.
    uint32_t victim_index = (uint32_t)victim->index;
    if(sched->blocked_map[victim_index / 64] &
      (UINT64_C(1) << (victim_index % 64)))
      continue;

    // Record that this is our victim and return it.
    sched->last_victim = victim;
    return victim;
  }

  return NULL;
}

/**
 * Pause a passive thread for one tick. Under systematic testing the
 * pause is a bare scheduling point: execution is serialized to one
 * thread at a time, so there is no real time to wait out, and a
 * passive thread must stay selectable for the seeded interleaving to
 * reach its visits.
 */
static void sched_tick_pause(uint64_t ns)
{
#if defined(USE_SYSTEMATIC_TESTING)
  (void)ns;
  SYSTEMATIC_TESTING_YIELD();
#else
  ponyint_cpu_sleep_ns(ns);
#endif
}

/**
 * Wait passively until there is a reason to run again. A passive
 * thread runs no actors and steals nothing, but it is not asleep: on
 * every visit it reads its message queue to completion, reclaims
 * foreign frees delivered to its allocator inbox, and pops the global
 * inject queue once. Between visits it pauses for a tick, backing off
 * while nothing happens. Nothing wakes a passive thread; everything
 * sent to one is found within a tick.
 *
 * The inject pop is the backstop that keeps every race on advisory
 * state (the blocked maps, the gauge) harmless: work stranded in
 * inject with nobody active is found within a tick.
 *
 * Returns an actor popped from inject — work in hand, which the
 * caller answers with the latch-guarded unblock — or NULL to resume
 * stealing empty-handed: an ACTIVATE was consumed, scheduler 0 lost
 * its noisy events, or termination was requested (the caller's
 * quiescent check reads sched->terminate).
 */
static pony_actor_t* passive_wait(scheduler_t* sched)
{
  atomic_fetch_sub_explicit(&active_scheduler_gauge, 1,
    memory_order_relaxed);

  // dtrace suspend notification
  DTRACE1(THREAD_SUSPEND, (uintptr_t)sched);
  TRACING_THREAD_SUSPEND();

  uint64_t tick_ns = SCHED_TICK_MIN_NS;
  pony_actor_t* actor = NULL;

  while(true)
  {
    recorded_cnf_t cnf = {false, 0};
    bool consumed = false;

    // A passive thread's mutemap is empty — emptiness gated the
    // transition, and only running actors can grow it — so no unmute
    // read here can reschedule anything onto the run queue.
    bool run_queue_changed = read_msg(sched, NULL, &cnf, &consumed);
    pony_assert(!run_queue_changed);
    (void)run_queue_changed;

    // Take back what other threads freed into this thread's allocator
    // inbox while it paused.
    ponyint_pool_drain();

    actor = pop_global(NULL);

    if(sched->terminate)
    {
      pony_assert(actor == NULL);
      break;
    }

    // Work in hand: go active and run it. Taking work unblocks, and
    // that token bump on scheduler 0 invalidates any probe answer, so
    // this path leaves the probe unanswered on purpose.
    if(actor != NULL)
      break;

    // No work in hand: answer a recorded quiescence probe. This holds
    // whether the thread stays passive or leaves below to service an
    // ACTIVATE — a work-free leave does not unblock, so the probe is
    // still this thread's to answer, and no fresh probe would replace
    // it while the round's token stands. A stale answer, if the thread
    // then takes work and unblocks, is dropped by scheduler 0's token
    // check.
    if(cnf.recorded)
      send_msg(sched->index, 0, SCHED_ACK, cnf.token);

    // Asked to go active, or, for scheduler 0, the noisy events that
    // allowed it to be passive are gone and the quiescence backstop
    // must run again.
    if(sched->activate_requested ||
      ((sched->index == 0) && !sched->asio_noisy))
      break;

    // Deliver pending foreign frees so their owners can reclaim while
    // this thread pauses.
    ponyint_pool_suspend_flush();

    // Once the tick has backed off to its cap, this thread has been idle long
    // enough that holding its freed memory no longer pays: hand it back to the
    // OS. Gating on the backoff keeps a thread that briefly parks and resumes
    // from thrashing decommits; it rewarms its cache and pages when it runs.
    if(tick_ns >= SCHED_TICK_MAX_NS)
      ponyint_pool_return_idle();

    if(consumed)
      tick_ns = SCHED_TICK_MIN_NS;

    sched_tick_pause(tick_ns);

    tick_ns *= 2;
    if(tick_ns > SCHED_TICK_MAX_NS)
      tick_ns = SCHED_TICK_MAX_NS;
  }

  // Leaving passive for any reason makes this thread active again.
  // The blocked latch is untouched here: only work in hand unblocks,
  // and the caller owns that.
  sched->activate_requested = false;
  atomic_fetch_add_explicit(&active_scheduler_gauge, 1,
    memory_order_relaxed);

  // dtrace resume notification
  DTRACE1(THREAD_RESUME, (uintptr_t)sched);
  TRACING_THREAD_RESUME();

  return actor;
}

/**
 * Use mpmcqs to allow stealing directly from a victim, without waiting for a
 * response.
 */
static pony_actor_t* steal(scheduler_t* sched)
{
  bool suspend_eligible = false;
  uint32_t steal_attempts = 0;
  uint64_t tsc = sched_clock();
  pony_actor_t* actor;
  scheduler_t* victim = NULL;

  while(true)
  {
    victim = choose_victim(sched);

    actor = pop_global(victim);
    if(actor != NULL)
      break;

    uint64_t tsc2 = sched_clock();

    if(read_msg(sched, actor, NULL, NULL))
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

    // An ACTIVATE consumed while already active asks for nothing;
    // clear it so it cannot ride into a later passive stretch.
    sched->activate_requested = false;

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
    //    cycles (currently 2000000).
    //    In many work stealing scenarios, we immediately get steal an actor.
    //    Sending a block/unblock pair in that scenario is very wasteful.
    //    Same applies to other "quick" steal scenarios.
    //    2 million cycles is roughly 1 millisecond, depending on clock speed.
    //    By waiting 1 millisecond before sending a block message, we are going
    //    to delay quiescence by a small amount of time but also optimize work
    //    stealing for generating far fewer block/unblock messages.
    uint32_t current_active_scheduler_count = get_active_scheduler_count();
    uint64_t clocks_elapsed = ponyint_cpu_tick_diff(tsc, tsc2);

    // Once we've been trying to steal long enough to be eligible to go
    // passive, stay eligible. tsc is a baseline that a passive stretch
    // can push more than ponyint_cpu_tick()'s wrap period into the
    // past, making clocks_elapsed read small again; latching keeps an
    // idle scheduler transitioning promptly instead of busy-spinning
    // until the elapsed count grows back past the threshold.
    if(clocks_elapsed > scheduler_suspend_threshold)
      suspend_eligible = true;

    if(!sched->blocked)
    {
      if(steal_attempts < current_active_scheduler_count)
      {
        steal_attempts++;
      }
      else if((clocks_elapsed > PONY_SCHED_BLOCK_THRESHOLD) &&
        (ponyint_mutemap_size(&sched->mute_mapping) == 0))
      {
        // only considered blocked if we're scheduler > 0 or if we're scheduler
        // 0 and there are no noiisy actors registered
        if((sched->index > 0) || ((sched->index == 0) && !sched->asio_noisy))
        {
          send_msg_all(sched->index, SCHED_BLOCK, sched->index);
          sched->blocked = true;
        }
      }
    }

    // Go passive once the idle clock has run down, on separate gates
    // from the block decision: an index above zero needs the blocked
    // latch; scheduler 0 goes passive — latched or not — only while
    // ASIO is noisy and its mutemap is empty, and is otherwise the one
    // thread that never goes passive, the quiescence backstop. Indices
    // below the minimum thread count never go passive, which is how
    // the minimum stays staffed with zero coordination. A passive
    // thread stays counted as blocked the whole time, which is what
    // lets quiescence trigger while threads are passive.
    if(suspend_eligible && ((uint32_t)sched->index >= min_scheduler_count) &&
      ((sched->index > 0) ? sched->blocked :
        ((sched->asio_noisy != 0) &&
          (ponyint_mutemap_size(&sched->mute_mapping) == 0))))
    {
      actor = passive_wait(sched);

      if(actor != NULL)
        break;

      // Resumed empty-handed: reset the idle clock so this thread
      // steals for a full threshold window before it may go passive
      // again, and re-tries every victim before blocking.
      tsc = sched_clock();
      suspend_eligible = false;
      steal_attempts = 0;
    }

    // if we're scheduler 0 and cycle detection is enabled
    if(!ponyint_actor_getnoblock() && (sched->index == 0))
    {
      // trigger cycle detector by sending it a message if it is time
      uint64_t current_tsc = sched_clock();
      if(ponyint_cycle_check_blocked(last_cd_tsc, current_tsc))
      {
        last_cd_tsc = current_tsc;

        // cycle detector should now be on the queue
        actor = pop_global(sched);

        if(actor != NULL)
        {
          // if we were able to get the cycle detector and we're in the final
          // round of the CNF/ACK protocol (because asio_stoppable is true)
          // run the cycle detector manually without unblocking
          if(ponyint_is_cycle(actor) && sched->asio_stoppable)
          {
            // Run the cycle detector and get the next actor
            DTRACE2(ACTOR_SCHEDULED, (uintptr_t)sched, (uintptr_t)actor);
            bool reschedule = ponyint_actor_run(&sched->ctx, actor);
            sched->ctx.current = NULL;
            pony_actor_t* next = pop_global(sched);

            if(reschedule || next != NULL)
            {
              if(next != NULL)
              {
                // If we have a next actor, push the cycle detector to the back
                // of the queue only if it needs to be rescheduled
                if(reschedule)
                  push(sched, actor);
                DTRACE2(ACTOR_DESCHEDULED, (uintptr_t)sched, (uintptr_t)actor);
                actor = next;
              }

              // if there's more work for the cycle detector to do or there is
              // another actor to run, break out of the while loop to return the
              // actor to be run normally after this scheduler unblocks
              break;
            } else {
              // if there's no more work for the cycle detector to do and no next
              // actor, reset actor to NULL and keep looping in steal waiting for
              // termination
              actor = NULL;
            }
          } else {
            // otherwise break out of the while loop to return the actor to be
            // run normally after this scheduler unblocks
            break;
          }
        }
      }
    }
  }

  // Work in hand: leave the blocked tally before running it. The
  // inline cycle-detector run above stays exempt — it runs while
  // blocked on purpose, and only a follow-on actor reaches here.
  if(sched->blocked)
  {
    send_msg_all(sched->index, SCHED_UNBLOCK, sched->index);
    sched->blocked = false;
  }

  DTRACE3(WORK_STEAL_SUCCESSFUL, (uintptr_t)sched, (uintptr_t)victim, (uintptr_t)actor);
  return actor;
}

#ifdef USE_RUNTIMESTATS
uint64_t ponyint_sched_cpu_used(pony_ctx_t* ctx)
{
  uint64_t last_tsc = ctx->last_tsc;
  uint64_t current_tsc = ponyint_cpu_tick();
  ctx->last_tsc = current_tsc;
  return ponyint_cpu_tick_diff(last_tsc, current_tsc);
}
#endif

/**
 * Ask one blocked scheduler to go active, picked from this thread's
 * own blocked map. A poked bit per target debounces repeat requests
 * until that target's next unblock broadcast arrives. Every targeting
 * mistake is self-correcting: an ACTIVATE landing on an active thread
 * is cleared unread, doubled requests collapse in the target's
 * mailbox, and a thread activated for no reason finds nothing and
 * goes passive again on its own.
 */
static void sched_maybe_activate(scheduler_t* sched)
{
  for(size_t w = 0; w < blocked_map_words(); w++)
  {
    uint64_t candidates = sched->blocked_map[w] & ~sched->poked_map[w];

    // Broadcasts include the sender, so a thread's own blocked bit can
    // be stale; never pick self.
    if(w == ((uint32_t)sched->index / 64))
      candidates &= ~(UINT64_C(1) << ((uint32_t)sched->index % 64));

    if(candidates == 0)
      continue;

    uint32_t bit = __pony_ffsll(candidates) - 1;

    sched->poked_map[w] |= (UINT64_C(1) << bit);
    send_msg(sched->index, (uint32_t)(w * 64) + bit, SCHED_ACTIVATE, 0);
    return;
  }
}

/**
 * Run a scheduler thread until termination.
 */
static void run(scheduler_t* sched)
{
#ifdef USE_RUNTIMESTATS
  uint64_t last_stats_print_tsc = ponyint_cpu_tick();
  sched->ctx.last_tsc = ponyint_cpu_tick();
#endif

  // sleep thread until we're ready to start processing
  SYSTEMATIC_TESTING_WAIT_START(sched->index);

  if(sched->index == 0) {
    last_cd_tsc = 0;
  }

  pony_actor_t* actor = pop_global(sched);

  if (DTRACE_ENABLED(ACTOR_SCHEDULED) && actor != NULL) {
    DTRACE2(ACTOR_SCHEDULED, (uintptr_t)sched, (uintptr_t)actor);
  }

  while(true)
  {
#ifdef USE_RUNTIMESTATS
    if(ponyint_sched_print_stats())
    {
      // convert to cycles for use with ponyint_cpu_tick()
      // 1 second = 2000000000 cycles (approx.)
      // based on same scale as ponyint_cpu_core_pause() uses
      uint64_t new_tsc = ponyint_cpu_tick();
      if(ponyint_cpu_tick_diff(last_stats_print_tsc, new_tsc) >
        print_stats_interval)
      {
        last_stats_print_tsc = new_tsc;
        print_scheduler_stats(sched);
      }
    }
#endif

    // if we're scheduler 0
    if(sched->index == 0)
    {
      // if cycle detection is enabled
      if(!ponyint_actor_getnoblock())
      {
        // trigger cycle detector by sending it a message if it is time
        uint64_t current_tsc = sched_clock();
        if(ponyint_cycle_check_blocked(last_cd_tsc, current_tsc))
        {
          last_cd_tsc = current_tsc;

          // cycle detector should now be on the queue
          if(actor == NULL)
            actor = pop_global(sched);
        }
      }
    }

    // In response to reading a message, we might have unmuted an actor and
    // added it back to our queue. if we don't have an actor to run, we want
    // to pop from our queue to check for a recently unmuted actor
    if(read_msg(sched, actor, NULL, NULL) && actor == NULL)
    {
      actor = pop_global(sched);
    }

    // An ACTIVATE consumed while already active asks for nothing;
    // clear it so it cannot ride into a later passive stretch.
    sched->activate_requested = false;

    // if it's a pinned actor, send it to the pinned_actor_scheduler and get
    // another actor to process. The pinned thread's own visit finds it
    // within a tick.
    while(NULL != actor && ponyint_actor_is_pinned(actor))
    {
      ponyint_mpmcq_push(&pinned_actor_scheduler->q, actor);
      actor = pop_global(sched);
    }

    while(actor == NULL)
    {
      // We had an empty queue and no rescheduled actor.
      actor = steal(sched);

      // if it's a pinned actor, send it to the pinned_actor_scheduler and get
      // another actor to process. The pinned thread's own visit finds
      // it within a tick.
      if(NULL != actor && ponyint_actor_is_pinned(actor))
      {
        ponyint_mpmcq_push(&pinned_actor_scheduler->q, actor);
        actor = NULL;
        // try and steal again
        continue;
      }

      if(actor == NULL)
      {
#ifdef USE_RUNTIMESTATS
        uint64_t used_cpu = ponyint_sched_cpu_used(&sched->ctx);
        sched->ctx.schedulerstats.misc_cpu += used_cpu;
        if(ponyint_sched_print_stats())
          print_scheduler_stats(sched);
#endif

        // Termination.
        pony_assert(pop(sched) == NULL);
        SYSTEMATIC_TESTING_STOP_THREAD();
        return;
      }
      DTRACE2(ACTOR_SCHEDULED, (uintptr_t)sched, (uintptr_t)actor);
    }

    // We have at least one muted actor...
    // Try and activate a passive scheduler thread to help with the load.
    // This is to err on the side of caution and activate more threads in case
    // of muted actors rather than potentially not activate enough threads.
    // If there isn't enough work, they'll go passive again.
    // NOTE: This could result in a pathological case where only one thread
    // has a muted actor but there is only one overloaded actor. In this case
    // the extra scheduler threads would keep being activated and then go
    // passive over and over again.
    if(ponyint_mutemap_size(&sched->mute_mapping) > 0)
      sched_maybe_activate(sched);

    pony_assert(!ponyint_actor_is_pinned(actor));

    // Run the current actor and get the next actor.
    bool reschedule = ponyint_actor_run(&sched->ctx, actor);
    sched->ctx.current = NULL;
    SYSTEMATIC_TESTING_YIELD();
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
        // theory to have another scheduler thread be activated to do work in
        // parallel.
        // Try and activate a passive scheduler thread to help with the load.
        // If there isn't enough work, they'll go passive again.
        sched_maybe_activate(sched);
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
  TRACING_THREAD_START(this_scheduler);

  run(sched);

  TRACING_THREAD_STOP();
  ponyint_pool_thread_cleanup();

  return 0;
}


/**
 * Run a custom scheduler thread for pinned actors until termination.
 * This thread does not partiticpate in most normal scheduler messaging
 * like CNF/ACK/block/unblock/noisy/unnoisy. it does participate in
 * muting messages and termination messages.
 *
 * It fits the passive model with one twist: it is the only thread
 * whose run queue other threads write — schedulers hand pinned actors
 * over by pushing onto it — so its own run queue is its activation
 * signal, and no ACTIVATE message is involved. It broadcasts nothing,
 * keeps no blocked map, and adjusts no gauge: nobody steals from it
 * and nobody activates it.
 */
static void run_pinned_actors()
{
  pony_assert(PONY_PINNED_ACTOR_THREAD_INDEX == this_scheduler->index);

  scheduler_t* sched = this_scheduler;

#if defined(USE_SYSTEMATIC_TESTING)
  // start processing
  SYSTEMATIC_TESTING_START();
#endif

#ifdef USE_RUNTIMESTATS
  uint64_t last_stats_print_tsc = ponyint_cpu_tick();
  sched->ctx.last_tsc = ponyint_cpu_tick();
#endif

  pony_actor_t* actor = NULL;
  uint64_t tick_ns = SCHED_TICK_MIN_NS;
  bool passive = false;

  // The pinned thread's recorded quiescence probe persists across
  // iterations: unlike a scheduler thread, it takes work without ever
  // unblocking, so a probe recorded in an iteration that then runs a
  // pinned actor would be lost — and since block_count does not change,
  // no fresh probe would replace it, stalling termination one ACK
  // short. Keeping the token until an empty-handed visit answers it (or
  // a newer probe overwrites it) closes that gap. Answering a stale
  // token is safe: scheduler 0's token check drops it.
  recorded_cnf_t cnf = {false, 0};

  while(true)
  {
#ifdef USE_RUNTIMESTATS
    if(ponyint_sched_print_stats())
    {
      // convert to cycles for use with ponyint_cpu_tick()
      // 1 second = 2000000000 cycles (approx.)
      // based on same scale as ponyint_cpu_core_pause() uses
      uint64_t new_tsc = ponyint_cpu_tick();
      if(ponyint_cpu_tick_diff(last_stats_print_tsc, new_tsc) >
        print_stats_interval)
      {
        last_stats_print_tsc = new_tsc;
        print_scheduler_stats(sched);
      }
    }
#endif

    bool consumed = false;

    // process pending messages; this might add an actor to the inject queue
    // due to an unmuted actor but that is for other scheduler threads to deal with
    // technically, this is inefficient since any actor unmuted by the pinned actor
    // scheduler should be handled by the pinned actor scheduler but for the moment
    // that is how things work and the actor will eventually come back to this thread
    // to be run anyways.
    // A quiescence probe is recorded, not answered: the answer waits
    // on the run-queue check below, so a probe is never ACKed with
    // work in hand.
    read_msg(sched, actor, &cnf, &consumed);

    // Termination. all the normal scheduler threads have decided there is no
    // more work to do so we can shutdown
    if(sched->terminate)
    {
#ifdef USE_RUNTIMESTATS
      uint64_t used_cpu = ponyint_sched_cpu_used(&sched->ctx);
      sched->ctx.schedulerstats.misc_cpu += used_cpu;
      if(ponyint_sched_print_stats())
        print_scheduler_stats(sched);
#endif

      pony_assert(pop(sched) == NULL);

      if(passive)
      {
        // dtrace resume notification
        DTRACE1(THREAD_RESUME, (uintptr_t)sched);
        TRACING_THREAD_RESUME();
      }

      SYSTEMATIC_TESTING_STOP_THREAD();
      return;
    }

    // get the next pinned actor to run if we don't already have one
    if(actor == NULL)
      actor = pop(sched);

    // if it's a not pinned actor, send it to a normal scheduler and get
    // another pinned actor to process; these are likely the result of pinned
    // actors sending messages to non-pinned actors. If every scheduler
    // is passive, their visits' inject pops find it within a tick.
    while(NULL != actor && !ponyint_actor_is_pinned(actor))
    {
      // Put on the shared mpmcq.
      ponyint_mpmcq_push(&inject, actor);
      actor = pop(sched);
    }

    if(actor == NULL)
    {
      // Empty-handed visit: answer a recorded quiescence probe, then
      // pause for a tick. Nothing wakes this thread; a pinned actor
      // made runnable while it pauses waits for the next visit.
      if(!passive)
      {
        passive = true;

        // dtrace suspend notification
        DTRACE1(THREAD_SUSPEND, (uintptr_t)sched);
        TRACING_THREAD_SUSPEND();
      }

      if(cnf.recorded)
      {
        send_msg(sched->index, 0, SCHED_ACK, cnf.token);
        cnf.recorded = false;
      }

      // Deliver pending foreign frees so their owners can reclaim
      // while this thread pauses.
      ponyint_pool_suspend_flush();

      if(consumed)
        tick_ns = SCHED_TICK_MIN_NS;

      sched_tick_pause(tick_ns);

      tick_ns *= 2;
      if(tick_ns > SCHED_TICK_MAX_NS)
        tick_ns = SCHED_TICK_MAX_NS;

      // Take back what other threads freed into this thread's
      // allocator inbox while it paused.
      ponyint_pool_drain();
    } else {
      if(passive)
      {
        passive = false;
        tick_ns = SCHED_TICK_MIN_NS;

        // dtrace resume notification
        DTRACE1(THREAD_RESUME, (uintptr_t)sched);
        TRACING_THREAD_RESUME();
      }

      pony_assert(ponyint_actor_is_pinned(actor));

      // Run the current actor and get the next actor.
      bool reschedule = ponyint_actor_run(&sched->ctx, actor);
      sched->ctx.current = NULL;
      SYSTEMATIC_TESTING_YIELD();

      pony_actor_t* next = pop(sched);

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
}

static void ponyint_sched_shutdown()
{
  uint32_t start = 0;

  atomic_store_explicit(&runtime_shutdown_initiated, true,
    memory_order_release);

  // Every scheduler thread reads TERMINATE within a tick and exits on
  // its own; nothing needs waking.
  while(start < scheduler_count)
  {
    if (ponyint_thread_join(scheduler[start].tid))
      start++;
  }

  DTRACE0(RT_END);
  ponyint_cycle_terminate(&this_scheduler->ctx);

  for(uint32_t i = 0; i < scheduler_count; i++)
  {
    while(ponyint_thread_messageq_pop(&scheduler[i].mq
#ifdef USE_DYNAMIC_TRACE
      , i
#endif
      ) != NULL) { ; }
    ponyint_mutemap_destroy(&scheduler[i].mute_mapping);
    ponyint_messageq_destroy(&scheduler[i].mq, false);
    ponyint_mpmcq_destroy(&scheduler[i].q);
  }

  size_t blocked_map_size = blocked_map_words() * sizeof(uint64_t);

  for(uint32_t i = 0; i < scheduler_count; i++)
  {
    ponyint_pool_free_size(blocked_map_size, scheduler[i].blocked_map);
    ponyint_pool_free_size(blocked_map_size, scheduler[i].poked_map);
  }

  atomic_store_explicit(&active_scheduler_gauge, 0, memory_order_relaxed);
  ponyint_pool_free_size(scheduler_count * sizeof(scheduler_t), scheduler);
#ifdef USE_RUNTIMESTATS
  mem_used -= (scheduler_count * sizeof(scheduler_t));
  mem_allocated -= (ponyint_pool_used_size(scheduler_count
    * sizeof(scheduler_t)));
#endif
  scheduler = NULL;
  pinned_actor_scheduler = NULL;
  scheduler_count = 0;

  ponyint_mpmcq_destroy(&inject);
}

uint64_t ponyint_sched_stats_interval_cycles(uint32_t interval_seconds)
{
  // Compute at 64-bit width so the multiply cannot overflow: an interval of a
  // few seconds already exceeds 2^32 cycles, and a 32-bit multiply would
  // truncate it (storing the result in a 32-bit type would too -- hence the
  // uint64_t print_stats_interval). 1 second is ~2000000000 cycles, the same
  // approximation the other scheduler timing thresholds use.
  return (uint64_t)interval_seconds * 2000000000;
}

pony_ctx_t* ponyint_sched_init(uint32_t threads, bool noyield, bool pin,
  bool pinasio, bool pinpat, uint32_t min_threads, uint32_t thread_suspend_threshold,
  uint32_t stats_interval, bool pin_tracing_thread
#if defined(USE_SYSTEMATIC_TESTING)
  , uint64_t systematic_testing_seed)
#else
  )
#endif
{
  ponyint_register_thread();

#ifdef USE_RUNTIMESTATS
  if(stats_interval != UINT32_MAX)
  {
    print_stats_interval = ponyint_sched_stats_interval_cycles(stats_interval);
    print_stats = true;
  }
  else
    print_stats = false;
#else
  (void)stats_interval;
#endif

  use_yield = !noyield;

  // clamp thread suspend threshold to documented range [1, 1000] ms
  if(thread_suspend_threshold < 1)
    thread_suspend_threshold = 1;
  if(thread_suspend_threshold > 1000)
    thread_suspend_threshold = 1000;

  // If no thread count is specified, use the available physical core count.
  if(threads == 0)
    threads = ponyint_cpu_count();

  // If minimum thread count is > thread count, cap it at thread count
  if(min_threads > threads)
    min_threads = threads; // this becomes the equivalent of --ponynoscale

  // convert to cycles for use with ponyint_cpu_tick()
  // 1 second = 2000000000 cycles (approx.)
  // based on same scale as ponyint_cpu_core_pause() uses
  scheduler_suspend_threshold = (uint64_t)thread_suspend_threshold * 2000000;

  scheduler_count = threads;
  min_scheduler_count = min_threads;
  scheduler = (scheduler_t*)ponyint_pool_alloc_size(
    scheduler_count * sizeof(scheduler_t));
#ifdef USE_RUNTIMESTATS
  mem_used += (scheduler_count * sizeof(scheduler_t));
  mem_allocated += (ponyint_pool_used_size(scheduler_count
    * sizeof(scheduler_t)));
#endif
  memset(scheduler, 0, scheduler_count * sizeof(scheduler_t));

  atomic_store_explicit(&active_scheduler_gauge, scheduler_count,
    memory_order_relaxed);

  size_t blocked_map_size = blocked_map_words() * sizeof(uint64_t);

  for(uint32_t i = 0; i < scheduler_count; i++)
  {
    scheduler[i].blocked_map =
      (uint64_t*)ponyint_pool_alloc_size(blocked_map_size);
    memset(scheduler[i].blocked_map, 0, blocked_map_size);
    scheduler[i].poked_map =
      (uint64_t*)ponyint_pool_alloc_size(blocked_map_size);
    memset(scheduler[i].poked_map, 0, blocked_map_size);
  }

  uint32_t tracing_cpu = -1;

  uint32_t asio_cpu = ponyint_cpu_assign(scheduler_count, scheduler, pin,
    pinasio, pinpat, pin_tracing_thread, &tracing_cpu);

  // make sure tracing knows how mant schedulers there are
  TRACING_SCHEDULERS_INIT(scheduler_count, tracing_cpu);

  for(uint32_t i = 0; i < scheduler_count; i++)
  {
    scheduler[i].ctx.scheduler = &scheduler[i];
    scheduler[i].last_victim = &scheduler[i];
    scheduler[i].index = i;
    scheduler[i].asio_noisy = 0;
    scheduler[i].ack_count = scheduler_count;
    pony_assert(scheduler[i].ack_count > 0);
    ponyint_messageq_init(&scheduler[i].mq);
    ponyint_mpmcq_init(&scheduler[i].q);
  }

  ponyint_mpmcq_init(&inject);
  ponyint_asio_init(asio_cpu);

  // set up main thread as scheduler for running pinned actors
  this_scheduler->ctx.scheduler = this_scheduler;
  this_scheduler->last_victim = this_scheduler;
  this_scheduler->index = PONY_PINNED_ACTOR_THREAD_INDEX;
  this_scheduler->asio_noisy = false;
  ponyint_messageq_init(&this_scheduler->mq);
  ponyint_mpmcq_init(&this_scheduler->q);

  pinned_actor_scheduler = this_scheduler;

  TRACING_THREAD_START(this_scheduler);

  // initialize systematic testing after starting the tracing thread
  SYSTEMATIC_TESTING_INIT(systematic_testing_seed, scheduler_count);

  return pony_ctx();
}

bool ponyint_sched_start()
{
  ponyint_register_thread();

  if(!ponyint_asio_start())
    return false;

  DTRACE0(RT_START);
  uint32_t start = 0;

  for(uint32_t i = start; i < scheduler_count; i++)
  {
    if(!ponyint_thread_create(&scheduler[i].tid, run_thread, scheduler[i].cpu,
      &scheduler[i]))
      return false;
  }

  // custom run loop for pinned actors
  run_pinned_actors();

  ponyint_sched_shutdown();

  TRACING_THREAD_STOP();
  ponyint_pool_thread_cleanup();

  while(ponyint_thread_messageq_pop(&this_scheduler->mq
#ifdef USE_DYNAMIC_TRACE
    , PONY_PINNED_ACTOR_THREAD_INDEX
#endif
    ) != NULL) { ; }
  ponyint_mutemap_destroy(&this_scheduler->mute_mapping);
  ponyint_messageq_destroy(&this_scheduler->mq, false);
  ponyint_mpmcq_destroy(&this_scheduler->q);

  return true;
}

void ponyint_sched_add(pony_ctx_t* ctx, pony_actor_t* actor)
{
  pony_assert(NULL != ctx->scheduler);

  // Add to the current scheduler thread.
  push(ctx->scheduler, actor);
}

void ponyint_sched_add_inject(pony_actor_t* actor)
{
  // Put on the shared mpmcq.
  ponyint_mpmcq_push(&inject, actor);
}

PONY_API schedulerstats_t* pony_scheduler_stats()
{
#ifdef USE_RUNTIMESTATS
  pony_ctx_t* ctx = pony_ctx();
  return &ctx->schedulerstats;
#else
  return NULL;
#endif
}

PONY_API int32_t pony_scheduler_index()
{
  pony_ctx_t* ctx = pony_ctx();
  return ctx->scheduler->index;
}

PONY_API uint32_t pony_schedulers()
{
  return scheduler_count;
}

PONY_API uint32_t pony_active_schedulers()
{
  return get_active_scheduler_count();
}

PONY_API uint32_t pony_min_schedulers()
{
  return min_scheduler_count;
}

PONY_API bool pony_scheduler_yield()
{
  return use_yield;
}

void ponyint_register_thread()
{
  if(this_scheduler != NULL)
    return;

  // Create a scheduler_t, even though we will only use the pony_ctx_t.
  this_scheduler = POOL_ALLOC(scheduler_t);
  memset(this_scheduler, 0, sizeof(scheduler_t));
  this_scheduler->tid = ponyint_thread_self();
  this_scheduler->index = PONY_UNKNOWN_SCHEDULER_INDEX;
  this_scheduler->ctx.scheduler = this_scheduler;
}

void ponyint_unregister_thread()
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

void ponyint_register_asio_thread()
{
  ponyint_register_thread();
  this_scheduler->index = PONY_ASIO_SCHEDULER_INDEX;
  TRACING_THREAD_START(this_scheduler);
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

void ponyint_sched_activate_if_all_passive(int32_t from)
{
  // Scheduler 0 is the natural target: it is only ever passive while
  // ASIO has noisy events, which is exactly when this is called.
  if(get_active_scheduler_count() == 0)
    send_msg((uint32_t)from, 0, SCHED_ACTIVATE, 0);
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
#ifdef USE_RUNTIMESTATS
    ctx->schedulerstats.mem_used += sizeof(muteref_t);
    ctx->schedulerstats.mem_allocated += POOL_ALLOC_SIZE(muteref_t);
    int64_t old_mmap_mem_size = ponyint_mutemap_mem_size(&sched->mute_mapping);
    int64_t old_mmap_alloc_size =
      ponyint_mutemap_alloc_size(&sched->mute_mapping);
#endif
    ponyint_mutemap_putindex(&sched->mute_mapping, mref, index);
#ifdef USE_RUNTIMESTATS
    int64_t new_mmap_mem_size = ponyint_mutemap_mem_size(&sched->mute_mapping);
    int64_t new_mmap_alloc_size =
      ponyint_mutemap_alloc_size(&sched->mute_mapping);
    ctx->schedulerstats.mem_used += (new_mmap_mem_size - old_mmap_mem_size);
    ctx->schedulerstats.mem_allocated += (new_mmap_alloc_size - old_mmap_alloc_size);
#endif
  }

  size_t index2 = HASHMAP_UNKNOWN;
  pony_actor_t* r = ponyint_muteset_get(&mref->value, sender, &index2);
  if(r == NULL)
  {
    // This is safe because an actor can only ever be in a single scheduler's
    // mutemap
#ifdef USE_RUNTIMESTATS
    int64_t old_mset_mem_size = ponyint_muteset_mem_size(&mref->value);
    int64_t old_mset_alloc_size = ponyint_muteset_alloc_size(&mref->value);
#endif
    ponyint_muteset_putindex(&mref->value, sender, index2);
    sender->muted++;
#ifdef USE_RUNTIMESTATS
    int64_t new_mset_mem_size = ponyint_muteset_mem_size(&mref->value);
    int64_t new_mset_alloc_size = ponyint_muteset_alloc_size(&mref->value);
    ctx->schedulerstats.mem_used += (new_mset_mem_size - old_mset_mem_size);
    ctx->schedulerstats.mem_allocated += (new_mset_alloc_size - old_mset_alloc_size);
    pony_assert(ctx->schedulerstats.mem_used >= 0);
    pony_assert(ctx->schedulerstats.mem_allocated >= 0);
#endif
  }
#ifdef USE_RUNTIMESTATS
  pony_assert(ctx->schedulerstats.mem_used ==
    (int64_t)ponyint_mutemap_total_mem_size(&sched->mute_mapping));
  pony_assert(ctx->schedulerstats.mem_allocated ==
    (int64_t)ponyint_mutemap_total_alloc_size(&sched->mute_mapping));
#endif
}

void ponyint_sched_start_global_unmute(uint32_t from, pony_actor_t* actor)
{
  // Passive receivers hold no muted actors, so the message is a no-op
  // for them, read at their next visit.
  send_msg_all(from, SCHED_UNMUTE_ACTOR, (intptr_t)actor);
  send_msg_pinned_actor_thread(from, SCHED_UNMUTE_ACTOR, (intptr_t)actor);
}

DECLARE_STACK(ponyint_actorstack, actorstack_t, pony_actor_t);
DEFINE_STACK(ponyint_actorstack, actorstack_t, pony_actor_t);

#ifdef USE_SYSTEMATIC_TESTING
// Order two actors by their stable creation-order id so unmuted actors are
// rescheduled in a layout-independent order rather than in the muteset's
// pointer-hash iteration order (which depends on ASLR). ids are unique, so
// there are no ties and qsort's instability cannot reintroduce a layout
// dependence.
static int sched_actor_systematic_testing_id_cmp(const void* a, const void* b)
{
  uint64_t ia = (*(pony_actor_t* const*)a)->systematic_testing_id;
  uint64_t ib = (*(pony_actor_t* const*)b)->systematic_testing_id;
  return (ia > ib) - (ia < ib);
}
#endif

// Reschedule one no-longer-muted actor. Both unmute arms in
// ponyint_sched_unmute_senders go through this so they stay behaviorally
// identical; they differ only in the order they walk the actors.
static void sched_reschedule_unmuted(pony_ctx_t* ctx, pony_actor_t* actor)
{
  ponyint_unmute_actor(actor);
  ponyint_sched_add(ctx, actor);
  DTRACE2(ACTOR_SCHEDULED, (uintptr_t)ctx->scheduler, (uintptr_t)actor);
  ponyint_sched_start_global_unmute(ctx->scheduler->index, actor);
}

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
#ifdef USE_SYSTEMATIC_TESTING
    size_t needs_unmuting_count = 0;
#endif

#ifdef USE_RUNTIMESTATS
    ctx->schedulerstats.mem_used -= sizeof(muteref_t);
    ctx->schedulerstats.mem_allocated -= POOL_ALLOC_SIZE(muteref_t);
    ctx->schedulerstats.mem_used -= ponyint_muteset_mem_size(&mref->value);
    ctx->schedulerstats.mem_allocated -= ponyint_muteset_alloc_size(&mref->value);
    pony_assert(ctx->schedulerstats.mem_used >= 0);
    pony_assert(ctx->schedulerstats.mem_allocated >= 0);
#endif

    // Find and collect any actors that need to be unmuted
    while((muted = ponyint_muteset_next(&mref->value, &i)) != NULL)
    {
      // This is safe because an actor can only ever be in a single scheduler's
      // mutemap
      pony_assert(muted->muted > 0);
      muted->muted--;

      // If muted->muted is 0 after we decremented it; then the actor
      // is longer muted
      if(muted->muted == 0)
      {
        needs_unmuting = ponyint_actorstack_push(needs_unmuting, muted);
#ifdef USE_SYSTEMATIC_TESTING
        needs_unmuting_count++;
#endif
      }
    }

    ponyint_mutemap_removeindex(&sched->mute_mapping, index);
    ponyint_muteref_free(mref);

    // Reschedule the actors that are no longer muted. Under systematic testing
    // we sort them by stable creation-order id so the run-queue order (and thus
    // the replayed interleaving) does not depend on the muteset's pointer-hash
    // iteration order, i.e. on memory layout; otherwise we walk the stack
    // directly. Both arms reschedule via sched_reschedule_unmuted.
#ifdef USE_SYSTEMATIC_TESTING
    if(needs_unmuting_count > 0)
    {
      pony_actor_t** unmuting = (pony_actor_t**)ponyint_pool_alloc_size(
        needs_unmuting_count * sizeof(pony_actor_t*));
      size_t k = 0;

      while(needs_unmuting != NULL)
        needs_unmuting = ponyint_actorstack_pop(needs_unmuting, &unmuting[k++]);

      qsort(unmuting, needs_unmuting_count, sizeof(pony_actor_t*),
        sched_actor_systematic_testing_id_cmp);

      for(k = 0; k < needs_unmuting_count; k++)
      {
        sched_reschedule_unmuted(ctx, unmuting[k]);
        actors_rescheduled++;
      }

      ponyint_pool_free_size(needs_unmuting_count * sizeof(pony_actor_t*),
        unmuting);
    }
#else
    pony_actor_t* to_unmute;

    while(needs_unmuting != NULL)
    {
      needs_unmuting = ponyint_actorstack_pop(needs_unmuting, &to_unmute);
      sched_reschedule_unmuted(ctx, to_unmute);
      actors_rescheduled++;
    }
#endif
  }

#ifdef USE_RUNTIMESTATS
  pony_assert(ctx->schedulerstats.mem_used ==
    (int64_t)ponyint_mutemap_total_mem_size(&sched->mute_mapping));
  pony_assert(ctx->schedulerstats.mem_allocated ==
    (int64_t)ponyint_mutemap_total_alloc_size(&sched->mute_mapping));
#endif

  return actors_rescheduled > 0;
}

// Return the scheduler's index
PONY_API int32_t pony_sched_index(pony_ctx_t* ctx)
{
  return ctx->scheduler->index;
}

PONY_EXTERN_C_END
