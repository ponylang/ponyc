#define PONY_WANT_ATOMIC_DEFS

#include "actor.h"
#include "../sched/scheduler.h"
#include "../sched/cpu.h"
#include "../mem/pool.h"
#include "../gc/cycle.h"
#include "../gc/trace.h"
#include "ponyassert.h"
#include <assert.h>
#include <string.h>
#include <dtrace.h>

#ifdef USE_RUNTIMESTATS
#include <stdio.h>
#endif

#ifdef USE_VALGRIND
#include <valgrind/helgrind.h>
#endif

// default actor batch size
#define PONY_SCHED_BATCH 100

PONY_EXTERN_C_BEGIN

// Ignore padding at the end of the type.
pony_static_assert((offsetof(pony_actor_t, gc) + sizeof(gc_t)) ==
   sizeof(pony_actor_pad_t), "Wrong actor pad size!");

static bool actor_noblock = false;

enum
{
  FLAG_BLOCKED = 1 << 0,
  FLAG_BLOCKED_SENT = 1 << 1,
  FLAG_SYSTEM = 1 << 2,
  FLAG_UNSCHEDULED = 1 << 3,
  FLAG_CD_CONTACTED = 1 << 4,
  FLAG_RC_OVER_ZERO_SEEN = 1 << 5,
  FLAG_PINNED = 1 << 6,
};

enum
{
  SYNC_FLAG_PENDINGDESTROY = 1 << 0,
  SYNC_FLAG_OVERLOADED = 1 << 1,
  SYNC_FLAG_UNDER_PRESSURE = 1 << 2,
  SYNC_FLAG_MUTED = 1 << 3,
};

#ifdef USE_RUNTIMESTATS
void print_actor_stats(pony_actor_t* actor)
{
  printf("Actor stats for actor: %zu, "
        "heap memory allocated: %ld, "
        "heap memory used: %ld, "
        "heap num allocated: %ld, "
        "heap realloc counter: %ld, "
        "heap alloc counter: %ld, "
        "heap free counter: %ld, "
        "heap gc counter: %ld, "
        "system cpu: %ld, "
        "app cpu: %ld, "
        "garbage collection marking cpu: %ld, "
        "garbage collection sweeping cpu: %ld, "
        "messages sent counter: %ld, "
        "system messages processed counter: %ld, "
        "app messages processed counter: %ld\n",
        (uintptr_t)actor,
        actor->actorstats.heap_mem_allocated,
        actor->actorstats.heap_mem_used,
        actor->actorstats.heap_num_allocated,
        actor->actorstats.heap_realloc_counter,
        actor->actorstats.heap_alloc_counter,
        actor->actorstats.heap_free_counter,
        actor->actorstats.heap_gc_counter,
        actor->actorstats.system_cpu,
        actor->actorstats.app_cpu,
        actor->actorstats.gc_mark_cpu,
        actor->actorstats.gc_sweep_cpu,
        actor->actorstats.messages_sent_counter,
        actor->actorstats.system_messages_processed_counter,
        actor->actorstats.app_messages_processed_counter
        );
}
#endif

// The sync flags of a given actor cannot be mutated from more than one actor at
// once, so these operations need not be atomic RMW.
static bool has_sync_flag_any(pony_actor_t* actor, uint8_t check_flags)
{
  uint8_t flags = atomic_load_explicit(&actor->sync_flags, memory_order_acquire);
  return (flags & check_flags) != 0;
}

static bool has_sync_flag(pony_actor_t* actor, uint8_t flag)
{
  return has_sync_flag_any(actor, flag);
}

static void set_sync_flag(pony_actor_t* actor, uint8_t flag)
{
  uint8_t flags = atomic_load_explicit(&actor->sync_flags, memory_order_acquire);
  atomic_store_explicit(&actor->sync_flags, flags | flag, memory_order_release);
}

static void unset_sync_flag(pony_actor_t* actor, uint8_t flag)
{
  uint8_t flags = atomic_load_explicit(&actor->sync_flags, memory_order_acquire);
  atomic_store_explicit(&actor->sync_flags, flags & (uint8_t)~flag,
    memory_order_release);
}

// The internal flags of a given actor are only ever read or written by a
// single scheduler at a time and so need not be synchronization safe (atomics).
static bool has_internal_flag(pony_actor_t* actor, uint8_t flag)
{
  return (actor->internal_flags & flag) != 0;
}

static void set_internal_flag(pony_actor_t* actor, uint8_t flag)
{
  actor->internal_flags = actor->internal_flags | flag;
}

static void unset_internal_flag(pony_actor_t* actor, uint8_t flag)
{
  actor->internal_flags = actor->internal_flags & (uint8_t)~flag;
}

//
// Mute/Unmute/Check mute status functions
//
// For backpressure related muting and unmuting to work correctly, the following
// rules have to be maintained.
//
// 1. Across schedulers, an actor should never been seen as muted when it is not
// in fact muted.
// 2. It's ok for a muted actor to be seen as unmuted in a transient fashion
// across actors
//
// If rule #1 is violated, we might end up deadlocking because an actor was
// muted for sending to an actor that might never be unmuted (because it isn't
// muted). The actor muted actor would continue to remain muted and the actor
// incorrectly seen as muted became actually muted and then unmuted.
//
// If rule #2 is violated, then a muted actor will receive from 1 to a few
// additional messages and the sender won't be muted. As this is a transient
// situtation that should be shortly rectified, there's no harm done.
//
// Our handling of atomic operations in `mute_actor`
// and `unmute_actor` are to assure that both rules aren't violated.

static void mute_actor(pony_actor_t* actor)
{
  set_sync_flag(actor, SYNC_FLAG_MUTED);
  DTRACE1(ACTOR_MUTED, (uintptr_t)actor);
}

void ponyint_unmute_actor(pony_actor_t* actor)
{
  unset_sync_flag(actor, SYNC_FLAG_MUTED);
  DTRACE1(ACTOR_UNMUTED, (uintptr_t)actor);
}

static bool triggers_muting(pony_actor_t* actor)
{
  return has_sync_flag_any(actor, SYNC_FLAG_OVERLOADED |
    SYNC_FLAG_UNDER_PRESSURE | SYNC_FLAG_MUTED);
}

static void actor_setoverloaded(pony_actor_t* actor)
{
  pony_assert(!ponyint_is_cycle(actor));
  set_sync_flag(actor, SYNC_FLAG_OVERLOADED);
  DTRACE1(ACTOR_OVERLOADED, (uintptr_t)actor);
}

static void actor_unsetoverloaded(pony_actor_t* actor)
{
  pony_ctx_t* ctx = pony_ctx();
  unset_sync_flag(actor, SYNC_FLAG_OVERLOADED);
  DTRACE1(ACTOR_OVERLOADED_CLEARED, (uintptr_t)actor);
  if (!has_sync_flag(actor, SYNC_FLAG_UNDER_PRESSURE))
  {
    ponyint_sched_start_global_unmute(ctx->scheduler->index, actor);
  }
}

static void maybe_mark_should_mute(pony_ctx_t* ctx, pony_actor_t* to)
{
  if(ctx->current != NULL)
  {
    // only mute a sender IF:
    // 1. the receiver is overloaded/under pressure/muted
    // AND
    // 2. the sender isn't overloaded or under pressure
    // AND
    // 3. we are sending to another actor (as compared to sending to self)
    if(triggers_muting(to) &&
      !has_sync_flag_any(ctx->current, SYNC_FLAG_OVERLOADED |
        SYNC_FLAG_UNDER_PRESSURE) &&
      ctx->current != to)
    {
      ponyint_sched_mute(ctx, ctx->current, to);
    }
  }
}

#ifndef PONY_NDEBUG
static bool well_formed_msg_chain(pony_msg_t* first, pony_msg_t* last)
{
  // A message chain is well formed if last is reachable from first and is the
  // end of the chain. first should also be the start of the chain but we can't
  // verify that.

  if((first == NULL) || (last == NULL) ||
    (atomic_load_explicit(&last->next, memory_order_relaxed) != NULL))
    return false;

  pony_msg_t* m1 = first;
  pony_msg_t* m2 = first;

  while((m1 != NULL) && (m2 != NULL))
  {
    if(m2 == last)
      return true;

    m2 = atomic_load_explicit(&m2->next, memory_order_relaxed);

    if(m2 == last)
      return true;
    if(m2 == NULL)
      return false;

    m1 = atomic_load_explicit(&m1->next, memory_order_relaxed);
    m2 = atomic_load_explicit(&m2->next, memory_order_relaxed);

    if(m1 == m2)
      return false;
  }

  return false;
}
#endif

static void try_gc(pony_ctx_t* ctx, pony_actor_t* actor)
{
  if(!ponyint_heap_startgc(&actor->heap
#ifdef USE_RUNTIMESTATS
  , actor))
#else
  ))
#endif
    return;

#ifdef USE_RUNTIMESTATS
    uint64_t used_cpu = ponyint_sched_cpu_used(ctx);
    ctx->schedulerstats.misc_cpu += used_cpu;
#endif

  DTRACE2(GC_START, (uintptr_t)ctx->scheduler, (uintptr_t)actor);

  ponyint_gc_mark(ctx);

  if(actor->type->trace != NULL)
    actor->type->trace(ctx, actor);

  ponyint_mark_done(ctx);

#ifdef USE_RUNTIMESTATS
    used_cpu = ponyint_sched_cpu_used(ctx);
    ctx->schedulerstats.actor_gc_mark_cpu += used_cpu;
    actor->actorstats.gc_mark_cpu += used_cpu;
#endif

  ponyint_heap_endgc(&actor->heap
#ifdef USE_RUNTIMESTATS
  , actor);
#else
  );
#endif

  DTRACE2(GC_END, (uintptr_t)ctx->scheduler, (uintptr_t)actor);

#ifdef USE_RUNTIMESTATS
    used_cpu = ponyint_sched_cpu_used(ctx);
    ctx->schedulerstats.actor_gc_sweep_cpu += used_cpu;
    actor->actorstats.gc_sweep_cpu += used_cpu;
#endif
}

static void send_unblock(pony_actor_t* actor)
{
  // Send unblock before continuing.
  unset_internal_flag(actor, FLAG_BLOCKED | FLAG_BLOCKED_SENT);
  ponyint_cycle_unblock(actor);
}

static void send_block(pony_ctx_t* ctx, pony_actor_t* actor)
{
  pony_assert(ctx->current == actor);

  // Try and run GC because we're blocked and sending a block message
  // to the CD. This will try and free any memory the actor has in its
  // heap that wouldn't get freed otherwise until the actor is
  // destroyed or happens to receive more work via application messages
  // that eventually trigger a GC which may not happen for a long time
  // (or ever). Do this BEFORE sending the message or else we might be
  // GCing while the CD destroys us.
  // only if `gc.rc > 0` because if `gc.rc == 0` then the actor is a zombie
  // and the cycle detector will destroy it upon receiving the block message
  if(actor->gc.rc > 0)
  {
    pony_triggergc(ctx);
    try_gc(ctx, actor);
  }

  // We're blocked, send block message.
  set_internal_flag(actor, FLAG_BLOCKED_SENT);
  set_internal_flag(actor, FLAG_CD_CONTACTED);
  ponyint_cycle_block(actor, &actor->gc);
}

static bool handle_message(pony_ctx_t* ctx, pony_actor_t* actor,
  pony_msg_t* msg)
{
#ifdef USE_RUNTIMESTATS_MESSAGES
  ctx->schedulerstats.num_inflight_messages--;
#endif

  switch(msg->id)
  {
    case ACTORMSG_ACQUIRE:
    {
#ifdef USE_RUNTIMESTATS_MESSAGES
      ctx->schedulerstats.mem_used_inflight_messages -= sizeof(pony_msgp_t);
      ctx->schedulerstats.mem_allocated_inflight_messages -= POOL_ALLOC_SIZE(pony_msgp_t);
#endif

      pony_assert(!ponyint_is_cycle(actor));
      pony_msgp_t* m = (pony_msgp_t*)msg;

#ifdef USE_RUNTIMESTATS
      ctx->schedulerstats.mem_used_actors -= (sizeof(actorref_t)
        + ponyint_objectmap_total_mem_size(&((actorref_t*)m->p)->map));
      ctx->schedulerstats.mem_allocated_actors -= (POOL_ALLOC_SIZE(actorref_t)
        + ponyint_objectmap_total_alloc_size(&((actorref_t*)m->p)->map));
#endif

      if(ponyint_gc_acquire(&actor->gc, (actorref_t*)m->p) &&
        has_internal_flag(actor, FLAG_BLOCKED_SENT))
      {
        // send unblock if we've sent a block
        send_unblock(actor);
      }

      return false;
    }

    case ACTORMSG_RELEASE:
    {
#ifdef USE_RUNTIMESTATS_MESSAGES
      ctx->schedulerstats.mem_used_inflight_messages -= sizeof(pony_msgp_t);
      ctx->schedulerstats.mem_allocated_inflight_messages -= POOL_ALLOC_SIZE(pony_msgp_t);
#endif

      pony_assert(!ponyint_is_cycle(actor));
      pony_msgp_t* m = (pony_msgp_t*)msg;

#ifdef USE_RUNTIMESTATS
      ctx->schedulerstats.mem_used_actors -= (sizeof(actorref_t)
        + ponyint_objectmap_total_mem_size(&((actorref_t*)m->p)->map));
      ctx->schedulerstats.mem_allocated_actors -= (POOL_ALLOC_SIZE(actorref_t)
        + ponyint_objectmap_total_alloc_size(&((actorref_t*)m->p)->map));
#endif

      if(ponyint_gc_release(&actor->gc, (actorref_t*)m->p) &&
        has_internal_flag(actor, FLAG_BLOCKED_SENT))
      {
        // send unblock if we've sent a block
        send_unblock(actor);
      }

      return false;
    }

    case ACTORMSG_ACK:
    {
#ifdef USE_RUNTIMESTATS_MESSAGES
      ctx->schedulerstats.mem_used_inflight_messages -= sizeof(pony_msgi_t);
      ctx->schedulerstats.mem_allocated_inflight_messages -= POOL_ALLOC_SIZE(pony_msgi_t);
#endif

      pony_assert(ponyint_is_cycle(actor));
      DTRACE3(ACTOR_MSG_RUN, (uintptr_t)ctx->scheduler, (uintptr_t)actor, msg->id);
      actor->type->dispatch(ctx, actor, msg);
      return false;
    }

    case ACTORMSG_CONF:
    {
#ifdef USE_RUNTIMESTATS_MESSAGES
      ctx->schedulerstats.mem_used_inflight_messages -= sizeof(pony_msgi_t);
      ctx->schedulerstats.mem_allocated_inflight_messages -= POOL_ALLOC_SIZE(pony_msgi_t);
#endif

      pony_assert(!ponyint_is_cycle(actor));
      if(has_internal_flag(actor, FLAG_BLOCKED_SENT))
      {
        // We've sent a block message, send confirm.
        pony_msgi_t* m = (pony_msgi_t*)msg;
        ponyint_cycle_ack(m->i);
      }

      return false;
    }

    case ACTORMSG_ISBLOCKED:
    {
#ifdef USE_RUNTIMESTATS_MESSAGES
      ctx->schedulerstats.mem_used_inflight_messages -= sizeof(pony_msg_t);
      ctx->schedulerstats.mem_allocated_inflight_messages -= POOL_ALLOC_SIZE(pony_msg_t);
#endif

      // this actor should not already be marked as pendingdestroy
      // or else we could end up double freeing it
      // this assertion is to ensure this invariant is not
      // accidentally broken due to code changes
      pony_assert(!ponyint_actor_pendingdestroy(actor));

      pony_assert(!ponyint_is_cycle(actor));
      if(has_internal_flag(actor, FLAG_BLOCKED)
        && !has_internal_flag(actor, FLAG_BLOCKED_SENT)
        && (actor->gc.rc > 0))
      {
        // We're blocked, send block message if:
        // - the actor hasn't already sent one
        // - the actor aren't a zombie aka rc == 0
        //
        // Sending multiple "i'm blocked" messages to the cycle detector
        // will result in actor potentially being freed more than once.
        send_block(ctx, actor);
      }

      return false;
    }

    case ACTORMSG_BLOCK:
    {
      // runtimestats messages tracked in cycle detector

      pony_assert(ponyint_is_cycle(actor));
      DTRACE3(ACTOR_MSG_RUN, (uintptr_t)ctx->scheduler, (uintptr_t)actor, msg->id);
      actor->type->dispatch(ctx, actor, msg);
      return false;
    }

    case ACTORMSG_UNBLOCK:
    {
#ifdef USE_RUNTIMESTATS_MESSAGES
      ctx->schedulerstats.mem_used_inflight_messages -= sizeof(pony_msgp_t);
      ctx->schedulerstats.mem_allocated_inflight_messages -= POOL_ALLOC_SIZE(pony_msgp_t);
#endif

      pony_assert(ponyint_is_cycle(actor));
      DTRACE3(ACTOR_MSG_RUN, (uintptr_t)ctx->scheduler, (uintptr_t)actor, msg->id);
      actor->type->dispatch(ctx, actor, msg);
      return false;
    }

    case ACTORMSG_CHECKBLOCKED:
    {
#ifdef USE_RUNTIMESTATS_MESSAGES
      ctx->schedulerstats.mem_used_inflight_messages -= sizeof(pony_msg_t);
      ctx->schedulerstats.mem_allocated_inflight_messages -= POOL_ALLOC_SIZE(pony_msg_t);
#endif

      pony_assert(ponyint_is_cycle(actor));
      DTRACE3(ACTOR_MSG_RUN, (uintptr_t)ctx->scheduler, (uintptr_t)actor, msg->id);
      actor->type->dispatch(ctx, actor, msg);
      return false;
    }

    default:
    {
#ifdef USE_RUNTIMESTATS_MESSAGES
      ctx->schedulerstats.mem_used_inflight_messages -= POOL_SIZE(msg->index);
      ctx->schedulerstats.mem_allocated_inflight_messages -= POOL_SIZE(msg->index);
#endif

      pony_assert(!ponyint_is_cycle(actor));
      if(has_internal_flag(actor, FLAG_BLOCKED_SENT))
      {
        // send unblock if we've sent a block
        send_unblock(actor);
      }

      DTRACE3(ACTOR_MSG_RUN, (uintptr_t)ctx->scheduler, (uintptr_t)actor, msg->id);
      actor->type->dispatch(ctx, actor, msg);
      return true;
    }
  }
}

// return true if mute occurs
static bool maybe_should_mute(pony_actor_t* actor)
{
  // if we become muted as a result of handling a message, bail out now.
  // we aren't set to "muted" at this point. setting to muted during a
  // a behavior can lead to race conditions that might result in a
  // deadlock.
  // Given that actor's are not run when they are muted, then when we
  // started out batch, actor->muted would have been 0. If any of our
  // message sends would result in the actor being muted, that value will
  // have changed to greater than 0.
  //
  // We will then set the actor to "muted". Once set, any actor sending
  // a message to it will be also be muted unless said sender is marked
  // as overloaded.
  //
  // The key points here is that:
  //   1. We can't set the actor to "muted" until after its finished running
  //   a behavior.
  //   2. We should bail out from running the actor and return false so that
  //   it won't be rescheduled.
  if(actor->muted > 0)
  {
    mute_actor(actor);
    return true;
  }

  return false;
}

static bool batch_limit_reached(pony_actor_t* actor, bool polling)
{
  if(!has_sync_flag(actor, SYNC_FLAG_OVERLOADED) && !polling)
  {
    // If we hit our batch size, consider this actor to be overloaded
    // only if we're not polling from C code.
    // Overloaded actors are allowed to send to other overloaded actors
    // and to muted actors without being muted themselves.
    actor_setoverloaded(actor);
  }

  return true;
}

bool ponyint_actor_run(pony_ctx_t* ctx, pony_actor_t* actor, bool polling)
{
  pony_assert(!has_sync_flag(actor, SYNC_FLAG_MUTED));
  ctx->current = actor;
  size_t batch = PONY_SCHED_BATCH;

  pony_msg_t* msg;
  size_t app = 0;


  // check to see at the start of a run, and most importantly, the first time
  // we run if our GC is over 0. If it is 0 the first time we run, then the
  // actor starts as an orphan with no references to it. There is no way that
  // the cycle detector will find out about an orphan actor unless it either
  // contacts the cycle detector itself (which we don't do) or the orphan
  // contacts another and needs to participate in the cycle detection protocol.
  // if an actors rc never goes above 0, it will be able to safely delete itself
  // even in the presence of the cycle detector.
  if (!actor_noblock && actor->gc.rc > 0)
    set_internal_flag(actor, FLAG_RC_OVER_ZERO_SEEN);

  // If we have been scheduled, the head will not be marked as empty.
  pony_msg_t* head = atomic_load_explicit(&actor->q.head, memory_order_acquire);

  while((msg = ponyint_actor_messageq_pop(&actor->q
#ifdef USE_DYNAMIC_TRACE
    , ctx->scheduler, ctx->current
#endif
    )) != NULL)
  {
#ifdef USE_RUNTIMESTATS
    uint64_t used_cpu = ponyint_sched_cpu_used(ctx);
    ctx->schedulerstats.misc_cpu += used_cpu;
#endif

    bool app_msg = handle_message(ctx, actor, msg);

    // if an actors rc never goes above 0, it will be able to safely delete
    // itself even in the presence of the cycle detector. This is one of two
    // checks to see if that invariant is in place for a given actor.
    if (!actor_noblock && actor->gc.rc > 0)
      set_internal_flag(actor, FLAG_RC_OVER_ZERO_SEEN);

#ifdef USE_RUNTIMESTATS
    used_cpu = ponyint_sched_cpu_used(ctx);
#endif

    if(app_msg)
    {
#ifdef USE_RUNTIMESTATS
      ctx->schedulerstats.actor_app_cpu += used_cpu;
      actor->actorstats.app_cpu += used_cpu;
      actor->actorstats.app_messages_processed_counter++;
#endif

      // If we handle an application message, try to gc.
      app++;
      try_gc(ctx, actor);

      // maybe mute actor; returns true if mute occurs
      if(maybe_should_mute(actor))
        return false;

      // if we've reached our batch limit
      // or if we're polling where we want to stop after one app message
      if(app == batch || polling)
        return batch_limit_reached(actor, polling);
    }
    else
    {
#ifdef USE_RUNTIMESTATS
      ctx->schedulerstats.actor_system_cpu += used_cpu;
      actor->actorstats.system_cpu += used_cpu;
      actor->actorstats.system_messages_processed_counter++;
#endif
    }

    // Stop handling a batch if we reach the head we found when we were
    // scheduled.
    if(msg == head)
      break;
  }

  // We didn't hit our app message batch limit. We now believe our queue to be
  // empty, but we may have received further messages.
  pony_assert(app < batch);
  pony_assert(!has_sync_flag(actor, SYNC_FLAG_MUTED));

  if(has_sync_flag(actor, SYNC_FLAG_OVERLOADED))
  {
    // if we were overloaded and didn't process a full batch, set ourselves as
    // no longer overloaded. Once this is done:
    // 1- sending to this actor is no longer grounds for an actor being muted
    // 2- this actor can no longer send to other actors free from muting should
    //    the receiver be overloaded or muted
    actor_unsetoverloaded(actor);
  }

  try_gc(ctx, actor);

  // If we have processed any application level messages, defer blocking.
  if(app > 0)
    return true;

  // note that we're logically blocked
  if(!has_internal_flag(actor, FLAG_BLOCKED | FLAG_SYSTEM | FLAG_BLOCKED_SENT))
  {
    set_internal_flag(actor, FLAG_BLOCKED);
  }

  if (has_internal_flag(actor, FLAG_BLOCKED))
  {
    if (actor->gc.rc == 0)
    {
      // Here, we is what we know to be true:
      //
      // - the actor is blocked
      // - the actor likely has no messages in its queue
      // - there's no references to this actor
      //

      if (actor_noblock || !has_internal_flag(actor, FLAG_RC_OVER_ZERO_SEEN))
      {
        // When 'actor_noblock` is true, the cycle detector isn't running.
        // this means actors won't be garbage collected unless we take special
        // action. Therefore if `noblock` is on, we should garbage collect the
        // actor
        //
        // When the cycle detector is running, it is still safe to locally
        // delete if our RC has never been above 0 because the cycle detector
        // can't possibly know about the actor's existence so, if it's message
        // queue is empty, doing a local delete is safe.
        if(ponyint_messageq_isempty(&actor->q))
        {
          // The actors queue is empty which means this actor is a zombie
          // and can be reaped.

          // mark the queue as empty or else destroy will hang
          bool empty = ponyint_messageq_markempty(&actor->q);

          // make sure the queue is actually empty as expected
          pony_assert(empty);

          // "Locally delete" the actor.
          ponyint_actor_setpendingdestroy(actor);
          ponyint_actor_final(ctx, actor);
          ponyint_actor_sendrelease(ctx, actor);
          ponyint_actor_destroy(actor);

          // make sure the scheduler will not reschedule this actor
          return !empty;
        }
      } else {
        // Tell cycle detector that this actor is a zombie and will not get
        // any more messages/work and can be reaped.
        // Mark the actor as FLAG_BLOCKED_SENT and send a BLOCKED message
        // to speed up reaping otherwise waiting for the cycle detector
        // to get around to asking if we're blocked could result in
        // unnecessary memory growth.
        //
        // We're blocked, send block message telling the cycle detector
        // to reap this actor (because its `rc == 0`).
        // This is concurrency safe because, only the cycle detector might
        // have a reference to this actor (rc is 0) so another actor can not
        // send it an application message that results this actor becoming
        // unblocked (which would create a race condition).
        send_block(ctx, actor);

        // We have to ensure that this actor will not be rescheduled if the
        // cycle detector happens to send it a message.
        //
        // intentionally don't mark the queue as empty because we don't want
        // this actor to be rescheduled if the cycle detector sends it a message
        //
        // make sure the scheduler will not reschedule this actor
        return false;
      }
    } else {
      // gc is greater than 0
      if (!actor_noblock && !has_internal_flag(actor, FLAG_CD_CONTACTED))
      {
        // The cycle detector is running and we've never contacted it ourselves,
        // so let it know we exist in case it is unaware.
        send_block(ctx, actor);
      }
    }
  }

  // If we mark the queue as empty, then it is no longer safe to do any
  // operations on this actor that aren't concurrency safe so make
  // `ponyint_messageq_markempty` the last thing we do.
  // Return true (i.e. reschedule immediately) if our queue isn't empty.
  return !ponyint_messageq_markempty(&actor->q);
}

void ponyint_actor_destroy(pony_actor_t* actor)
{
  pony_assert(has_sync_flag(actor, SYNC_FLAG_PENDINGDESTROY));

  // Make sure the actor being destroyed has finished marking its queue
  // as empty. Otherwise, it may spuriously see that tail and head are not
  // the same and fail to mark the queue as empty, resulting in it getting
  // rescheduled.
  pony_msg_t* head = NULL;
  do
  {
    head = atomic_load_explicit(&actor->q.head, memory_order_relaxed);
  } while(((uintptr_t)head & (uintptr_t)1) != (uintptr_t)1);

  atomic_thread_fence(memory_order_acquire);
#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_AFTER(&actor->q.head);
#endif

  ponyint_messageq_destroy(&actor->q, false);
  ponyint_gc_destroy(&actor->gc);
  ponyint_heap_destroy(&actor->heap);

#ifdef USE_RUNTIMESTATS
  pony_ctx_t* ctx = pony_ctx();
  ctx->schedulerstats.mem_used_actors -= actor->type->size;
  ctx->schedulerstats.mem_allocated_actors -= ponyint_pool_used_size(actor->type->size);
  ctx->schedulerstats.destroyed_actors_counter++;
  if (ponyint_sched_print_stats())
    print_actor_stats(actor);
#endif

  // Free variable sized actors correctly.
  ponyint_pool_free_size(actor->type->size, actor);
}

gc_t* ponyint_actor_gc(pony_actor_t* actor)
{
  return &actor->gc;
}

heap_t* ponyint_actor_heap(pony_actor_t* actor)
{
  return &actor->heap;
}

bool ponyint_actor_pendingdestroy(pony_actor_t* actor)
{
  return has_sync_flag(actor, SYNC_FLAG_PENDINGDESTROY);
}

void ponyint_actor_setpendingdestroy(pony_actor_t* actor)
{
  // This is thread-safe, even though the flag is set from the cycle detector.
  // The function is only called after the cycle detector has detected a true
  // cycle and an actor won't change its flags if it is part of a true cycle.
  // The synchronisation is done through the ACK message sent by the actor to
  // the cycle detector.
  set_sync_flag(actor, SYNC_FLAG_PENDINGDESTROY);
}

void ponyint_actor_final(pony_ctx_t* ctx, pony_actor_t* actor)
{
  // This gets run while the cycle detector is handling a message. Set the
  // current actor before running anything.
  pony_actor_t* prev = ctx->current;
  ctx->current = actor;

  // Run the actor finaliser if it has one.
  if(actor->type->final != NULL)
    actor->type->final(actor);

  // Run all outstanding object finalisers.
  ponyint_heap_final(&actor->heap);

  // Restore the current actor.
  ctx->current = prev;
}

void ponyint_actor_sendrelease(pony_ctx_t* ctx, pony_actor_t* actor)
{
  ponyint_gc_sendrelease(ctx, &actor->gc);
}

void ponyint_actor_setsystem(pony_actor_t* actor)
{
  set_internal_flag(actor, FLAG_SYSTEM);
}

void ponyint_actor_setnoblock(bool state)
{
  actor_noblock = state;
}

bool ponyint_actor_getnoblock()
{
  return actor_noblock;
}

PONY_API pony_actor_t* pony_create(pony_ctx_t* ctx, pony_type_t* type,
  bool orphaned)
{
  pony_assert(type != NULL);

  // allocate variable sized actors correctly
  pony_actor_t* actor = (pony_actor_t*)ponyint_pool_alloc_size(type->size);
  memset(actor, 0, type->size);
  actor->type = type;

#ifdef USE_RUNTIMESTATS
  ctx->schedulerstats.mem_used_actors += type->size;
  ctx->schedulerstats.mem_allocated_actors += ponyint_pool_used_size(type->size);
  ctx->schedulerstats.created_actors_counter++;
#endif

  ponyint_messageq_init(&actor->q);
  ponyint_heap_init(&actor->heap);
  ponyint_gc_done(&actor->gc);

  if(ctx->current != NULL && !orphaned)
  {
    // Do not set an rc if the actor is orphaned. The compiler determined that
    // there are no references to this actor. By not setting a non-zero RC, we
    // will GC the actor sooner and lower overall memory usage.
    actor->gc.rc = GC_INC_MORE;
    ponyint_gc_createactor(ctx->current, actor);
  } else {
    // no creator, so the actor isn't referenced by anything
    actor->gc.rc = 0;
  }

  DTRACE2(ACTOR_ALLOC, (uintptr_t)ctx->scheduler, (uintptr_t)actor);
  return actor;
}

// this is currently only used for two purposes:
// * to destroy the cycle detector
// * to destroy the temporary actor created to run primitive finalisers
//   that happens after the schedulers have exited and the cycle detector
//   has been destroyed
//
// as a result, this does not need to be concurrency safe
// or tell the cycle detector of what it is doing
PONY_API void ponyint_destroy(pony_actor_t* actor)
{
  // This destroys an actor immediately.
  // The finaliser is not called.

  ponyint_actor_setpendingdestroy(actor);
  ponyint_actor_destroy(actor);
}

PONY_API pony_msg_t* pony_alloc_msg(uint32_t index, uint32_t id)
{
#ifdef USE_RUNTIMESTATS
  pony_ctx_t* ctx = pony_ctx();
  if(ctx->current)
    ctx->current->actorstats.messages_sent_counter++;
#endif
#ifdef USE_RUNTIMESTATS_MESSAGES
  ctx->schedulerstats.mem_used_inflight_messages += POOL_SIZE(index);
  ctx->schedulerstats.mem_allocated_inflight_messages += POOL_SIZE(index);
  ctx->schedulerstats.num_inflight_messages++;
#endif

  pony_msg_t* msg = (pony_msg_t*)ponyint_pool_alloc(index);
  msg->index = index;
  msg->id = id;
#ifndef PONY_NDEBUG
  atomic_store_explicit(&msg->next, NULL, memory_order_relaxed);
#endif

  return msg;
}

PONY_API pony_msg_t* pony_alloc_msg_size(size_t size, uint32_t id)
{
  return pony_alloc_msg((uint32_t)ponyint_pool_index(size), id);
}

PONY_API void pony_sendv(pony_ctx_t* ctx, pony_actor_t* to, pony_msg_t* first,
  pony_msg_t* last, bool has_app_msg)
{
  // The function takes a prebuilt chain instead of varargs because the latter
  // is expensive and very hard to optimise.

  pony_assert(well_formed_msg_chain(first, last));

  // Make sure we're not trying to send a message to an actor that is about
  // to be destroyed.
  pony_assert(!ponyint_actor_pendingdestroy(to));

  if(DTRACE_ENABLED(ACTOR_MSG_SEND))
  {
    pony_msg_t* m = first;

    while(m != last)
    {
      DTRACE4(ACTOR_MSG_SEND, (uintptr_t)ctx->scheduler, m->id,
        (uintptr_t)ctx->current, (uintptr_t)to);
      m = atomic_load_explicit(&m->next, memory_order_relaxed);
    }

    DTRACE4(ACTOR_MSG_SEND, (uintptr_t)ctx->scheduler, last->id,
        (uintptr_t)ctx->current, (uintptr_t)to);
  }

  if(has_app_msg)
    maybe_mark_should_mute(ctx, to);

  if(ponyint_actor_messageq_push(&to->q, first, last
#ifdef USE_DYNAMIC_TRACE
    , ctx->scheduler, ctx->current, to
#endif
    ))
  {
    if(!has_sync_flag(to, SYNC_FLAG_MUTED))
    {
      ponyint_sched_add(ctx, to);
    }
  }
}

PONY_API void pony_sendv_single(pony_ctx_t* ctx, pony_actor_t* to,
  pony_msg_t* first, pony_msg_t* last, bool has_app_msg)
{
  // The function takes a prebuilt chain instead of varargs because the latter
  // is expensive and very hard to optimise.

  pony_assert(well_formed_msg_chain(first, last));

  // make sure we're not trying to send a message to an actor
  // that is about to be destroyed
  pony_assert(!ponyint_actor_pendingdestroy(to));

  if(DTRACE_ENABLED(ACTOR_MSG_SEND))
  {
    pony_msg_t* m = first;

    while(m != last)
    {
      DTRACE4(ACTOR_MSG_SEND, (uintptr_t)ctx->scheduler, m->id,
        (uintptr_t)ctx->current, (uintptr_t)to);
      m = atomic_load_explicit(&m->next, memory_order_relaxed);
    }

    DTRACE4(ACTOR_MSG_SEND, (uintptr_t)ctx->scheduler, last->id,
        (uintptr_t)ctx->current, (uintptr_t)to);
  }

  if(has_app_msg)
    maybe_mark_should_mute(ctx, to);

  if(ponyint_actor_messageq_push_single(&to->q, first, last
#ifdef USE_DYNAMIC_TRACE
    , ctx->scheduler, ctx->current, to
#endif
    ))
  {
    if(!has_sync_flag(to, SYNC_FLAG_MUTED))
    {
      // if the receiving actor is currently not unscheduled AND it's not
      // muted, schedule it.
      ponyint_sched_add(ctx, to);
    }
  }
}

PONY_API void pony_chain(pony_msg_t* prev, pony_msg_t* next)
{
  pony_assert(atomic_load_explicit(&prev->next, memory_order_relaxed) == NULL);
  atomic_store_explicit(&prev->next, next, memory_order_relaxed);
}

PONY_API void pony_send(pony_ctx_t* ctx, pony_actor_t* to, uint32_t id)
{
#ifdef USE_RUNTIMESTATS_MESSAGES
  ctx->schedulerstats.mem_used_inflight_messages += sizeof(pony_msg_t);
  ctx->schedulerstats.mem_used_inflight_messages -= POOL_ALLOC_SIZE(pony_msg_t);
#endif

  pony_msg_t* m = pony_alloc_msg(POOL_INDEX(sizeof(pony_msg_t)), id);
  pony_sendv(ctx, to, m, m, id <= ACTORMSG_APPLICATION_START);
}

PONY_API void pony_sendp(pony_ctx_t* ctx, pony_actor_t* to, uint32_t id,
  void* p)
{
#ifdef USE_RUNTIMESTATS_MESSAGES
  ctx->schedulerstats.mem_used_inflight_messages += sizeof(pony_msgp_t);
  ctx->schedulerstats.mem_used_inflight_messages -= POOL_ALLOC_SIZE(pony_msgp_t);
#endif

  pony_msgp_t* m = (pony_msgp_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(pony_msgp_t)), id);
  m->p = p;

  pony_sendv(ctx, to, &m->msg, &m->msg, id <= ACTORMSG_APPLICATION_START);
}

PONY_API void pony_sendi(pony_ctx_t* ctx, pony_actor_t* to, uint32_t id,
  intptr_t i)
{
#ifdef USE_RUNTIMESTATS_MESSAGES
  ctx->schedulerstats.mem_used_inflight_messages += sizeof(pony_msgi_t);
  ctx->schedulerstats.mem_used_inflight_messages -= POOL_ALLOC_SIZE(pony_msgi_t);
#endif

  pony_msgi_t* m = (pony_msgi_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(pony_msgi_t)), id);
  m->i = i;

  pony_sendv(ctx, to, &m->msg, &m->msg, id <= ACTORMSG_APPLICATION_START);
}

void ponyint_sendv_inject(pony_actor_t* to, pony_msg_t* msg)
{
  // Make sure we're not trying to send a message to an actor that is about
  // to be destroyed.
  pony_assert(!ponyint_actor_pendingdestroy(to));

  pony_ctx_t* ctx = pony_ctx();

  if(DTRACE_ENABLED(ACTOR_MSG_SEND))
  {
    DTRACE4(ACTOR_MSG_SEND, (uintptr_t)ctx->scheduler, msg->id,
      (uintptr_t)ctx->current, (uintptr_t)to);
  }

  if(ponyint_actor_messageq_push(&to->q, msg, msg
#ifdef USE_DYNAMIC_TRACE
    , ctx->scheduler, ctx->current, to
#endif
    ))
  {
    if(!has_sync_flag(to, SYNC_FLAG_MUTED))
    {
      ponyint_sched_add_inject(to);
    }
  }

  (void)ctx;
}

void ponyint_send_inject(pony_actor_t* to, uint32_t id)
{
  pony_msg_t* m = pony_alloc_msg(POOL_INDEX(sizeof(pony_msg_t)), id);
  ponyint_sendv_inject(to, m);
}

void ponyint_sendp_inject(pony_actor_t* to, uint32_t id, void* p)
{
  pony_msgp_t* m = (pony_msgp_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(pony_msgp_t)), id);
  m->p = p;

  ponyint_sendv_inject(to, &m->msg);
}

void ponyint_sendi_inject(pony_actor_t* to, uint32_t id, intptr_t i)
{
  pony_msgi_t* m = (pony_msgi_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(pony_msgi_t)), id);
  m->i = i;

  ponyint_sendv_inject(to, &m->msg);
}

PONY_API void* pony_alloc(pony_ctx_t* ctx, size_t size)
{
  pony_assert(ctx->current != NULL);
  DTRACE3(HEAP_ALLOC, (uintptr_t)ctx->scheduler, (uintptr_t)ctx->current, size);

  return ponyint_heap_alloc(ctx->current, &ctx->current->heap, size,
    TRACK_NO_FINALISERS);
}

PONY_API void* pony_alloc_small(pony_ctx_t* ctx, uint32_t sizeclass)
{
  pony_assert(ctx->current != NULL);
  DTRACE3(HEAP_ALLOC, (uintptr_t)ctx->scheduler, (uintptr_t)ctx->current, HEAP_MIN << sizeclass);

  return ponyint_heap_alloc_small(ctx->current, &ctx->current->heap, sizeclass,
    TRACK_NO_FINALISERS);
}

PONY_API void* pony_alloc_large(pony_ctx_t* ctx, size_t size)
{
  pony_assert(ctx->current != NULL);
  DTRACE3(HEAP_ALLOC, (uintptr_t)ctx->scheduler, (uintptr_t)ctx->current, size);

  return ponyint_heap_alloc_large(ctx->current, &ctx->current->heap, size,
    TRACK_NO_FINALISERS);
}

PONY_API void* pony_realloc(pony_ctx_t* ctx, void* p, size_t size, size_t copy)
{
  pony_assert(ctx->current != NULL);
  DTRACE3(HEAP_ALLOC, (uintptr_t)ctx->scheduler, (uintptr_t)ctx->current, size);

  return ponyint_heap_realloc(ctx->current, &ctx->current->heap, p, size, copy);
}

PONY_API void* pony_alloc_final(pony_ctx_t* ctx, size_t size)
{
  pony_assert(ctx->current != NULL);
  DTRACE3(HEAP_ALLOC, (uintptr_t)ctx->scheduler, (uintptr_t)ctx->current, size);

  return ponyint_heap_alloc(ctx->current, &ctx->current->heap, size,
    TRACK_ALL_FINALISERS);
}

void* pony_alloc_small_final(pony_ctx_t* ctx, uint32_t sizeclass)
{
  pony_assert(ctx->current != NULL);
  DTRACE3(HEAP_ALLOC, (uintptr_t)ctx->scheduler, (uintptr_t)ctx->current, HEAP_MIN << sizeclass);

  return ponyint_heap_alloc_small(ctx->current, &ctx->current->heap,
    sizeclass, TRACK_ALL_FINALISERS);
}

void* pony_alloc_large_final(pony_ctx_t* ctx, size_t size)
{
  pony_assert(ctx->current != NULL);
  DTRACE3(HEAP_ALLOC, (uintptr_t)ctx->scheduler, (uintptr_t)ctx->current, size);

  return ponyint_heap_alloc_large(ctx->current, &ctx->current->heap,
    size, TRACK_ALL_FINALISERS);
}

PONY_API void pony_triggergc(pony_ctx_t* ctx)
{
  pony_assert(ctx->current != NULL);
  ctx->current->heap.next_gc = 0;
}

bool ponyint_actor_is_pinned(pony_actor_t* actor)
{
  return has_internal_flag(actor, FLAG_PINNED);
}

PONY_API void pony_actor_set_pinned()
{
  pony_ctx_t* ctx = pony_ctx();
  set_internal_flag(ctx->current, FLAG_PINNED);
}

PONY_API void pony_actor_unset_pinned()
{
  pony_ctx_t* ctx = pony_ctx();
  unset_internal_flag(ctx->current, FLAG_PINNED);
}

void ponyint_become(pony_ctx_t* ctx, pony_actor_t* actor)
{
  ctx->current = actor;
}

PONY_API void pony_poll(pony_ctx_t* ctx)
{
  // TODO: this seems like it could allow muted actors to get `ponyint_actor_run`
  // which shouldn't be allowed. Fixing might require API changes.
  pony_assert(ctx->current != NULL);
  ponyint_actor_run(ctx, ctx->current, true);
}

PONY_API void pony_apply_backpressure()
{
  pony_ctx_t* ctx = pony_ctx();
  set_sync_flag(ctx->current, SYNC_FLAG_UNDER_PRESSURE);
  DTRACE1(ACTOR_UNDER_PRESSURE, (uintptr_t)ctx->current);
}

PONY_API void pony_release_backpressure()
{
  pony_ctx_t* ctx = pony_ctx();
  unset_sync_flag(ctx->current, SYNC_FLAG_UNDER_PRESSURE);
  DTRACE1(ACTOR_PRESSURE_RELEASED, (uintptr_t)ctx->current);
  if (!has_sync_flag(ctx->current, SYNC_FLAG_OVERLOADED))
    ponyint_sched_start_global_unmute(ctx->scheduler->index, ctx->current);
}

PONY_API actorstats_t* pony_actor_stats()
{
#ifdef USE_RUNTIMESTATS
  pony_ctx_t* ctx = pony_ctx();
  return &ctx->current->actorstats;
#else
  return NULL;
#endif
}

#ifdef USE_RUNTIMESTATS
size_t ponyint_actor_mem_size(pony_actor_t* actor)
{
  return actor->type->size;
}

size_t ponyint_actor_alloc_size(pony_actor_t* actor)
{
  return ponyint_pool_used_size(actor->type->size);
}

size_t ponyint_actor_total_mem_size(pony_actor_t* actor)
{
  // memeory categories:
  //   used - memory allocated that is actively being used by the runtime
  return
      // actor struct size (maybe this shouldn't be counted to avoid double
      // counting since it is counted as part of the scheduler thread mem used?)
      actor->type->size
      // cycle detector memory used (or 0 if not cycle detector)
    + ( ponyint_is_cycle(actor) ? ponyint_cycle_mem_size() : 0)
      // actor heap memory used
    + ponyint_heap_mem_size(actor)
      // actor gc total memory used
    + ponyint_gc_total_mem_size(actor, &actor->gc)
      // size of stub message when message_q is initialized
    + sizeof(pony_msg_t);
}

size_t ponyint_actor_total_alloc_size(pony_actor_t* actor)
{
  // memeory categories:
  //   alloc - memory allocated whether it is actively being used or not
  return
      // allocation for actor struct size (maybe this shouldn't be counted to
      // avoid double counting since it is counted as part of the scheduler
      // thread mem allocated?)
      ponyint_pool_used_size(actor->type->size)
      // cycle detector memory allocated (or 0 if not cycle detector)
    + ( ponyint_is_cycle(actor) ? ponyint_cycle_alloc_size() : 0)
      // actor heap memory allocated
    + ponyint_heap_alloc_size(actor)
      // actor gc total memory allocated
    + ponyint_gc_total_alloc_size(actor, &actor->gc)
      // allocation of stub message when message_q is initialized
    + POOL_ALLOC_SIZE(pony_msg_t);
}
#endif

PONY_EXTERN_C_END
