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
#include <stdio.h>

#ifdef USE_VALGRIND
#include <valgrind/helgrind.h>
#endif

// Ignore padding at the end of the type.
pony_static_assert((offsetof(pony_actor_t, gc) + sizeof(gc_t)) ==
   sizeof(pony_actor_pad_t), "Wrong actor pad size!");

enum
{
  FLAG_BLOCKED = 1 << 0,
  FLAG_RC_CHANGED = 1 << 1,
  FLAG_SYSTEM = 1 << 2,
  FLAG_UNSCHEDULED = 1 << 3,
  FLAG_PENDINGDESTROY = 1 << 4,
  FLAG_OVERLOADED = 1 << 5,
  FLAG_UNDER_PRESSURE = 1 << 6,
  FLAG_MUTED = 1 << 7,
};

static bool actor_noblock = false;

static bool has_flag(pony_actor_t* actor, uint8_t flag)
{
  uint8_t flags = atomic_load_explicit(&actor->flags, memory_order_relaxed);
  return (flags & flag) != 0;
}

// The flags of a given actor cannot be mutated from more than one actor at
// once, so these operations need not be atomic RMW.

static void set_flag(pony_actor_t* actor, uint8_t flag)
{
  uint8_t flags = atomic_load_explicit(&actor->flags, memory_order_relaxed);
  atomic_store_explicit(&actor->flags, flags | flag, memory_order_relaxed);
}

static void unset_flag(pony_actor_t* actor, uint8_t flag)
{
  uint8_t flags = atomic_load_explicit(&actor->flags, memory_order_relaxed);
  atomic_store_explicit(&actor->flags, flags & (uint8_t)~flag,
    memory_order_relaxed);
}

#ifndef NDEBUG
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

static bool handle_message(pony_ctx_t* ctx, pony_actor_t* actor,
  pony_msg_t* msg)
{
  switch(msg->id)
  {
    case ACTORMSG_ACQUIRE:
    {
      pony_msgp_t* m = (pony_msgp_t*)msg;

      if(ponyint_gc_acquire(&actor->gc, (actorref_t*)m->p) &&
        has_flag(actor, FLAG_BLOCKED))
      {
        // If our rc changes, we have to tell the cycle detector before sending
        // any CONF messages.
        set_flag(actor, FLAG_RC_CHANGED);
      }

      return false;
    }

    case ACTORMSG_RELEASE:
    {
      pony_msgp_t* m = (pony_msgp_t*)msg;

      if(ponyint_gc_release(&actor->gc, (actorref_t*)m->p) &&
        has_flag(actor, FLAG_BLOCKED))
      {
        // If our rc changes, we have to tell the cycle detector before sending
        // any CONF messages.
        set_flag(actor, FLAG_RC_CHANGED);
      }

      return false;
    }

    case ACTORMSG_CONF:
    {
      if(has_flag(actor, FLAG_BLOCKED) && !has_flag(actor, FLAG_RC_CHANGED))
      {
        // We're blocked and our RC hasn't changed since our last block
        // message, send confirm.
        pony_msgi_t* m = (pony_msgi_t*)msg;
        ponyint_cycle_ack(ctx, m->i);
      }

      return false;
    }

    case ACTORMSG_BLOCK:
    {
      DTRACE3(ACTOR_MSG_RUN, (uintptr_t)ctx->scheduler, (uintptr_t)actor, msg->id);
      actor->type->dispatch(ctx, actor, msg);
      return false;
    }

    case ACTORMSG_UNBLOCK:
    {
      DTRACE3(ACTOR_MSG_RUN, (uintptr_t)ctx->scheduler, (uintptr_t)actor, msg->id);
      actor->type->dispatch(ctx, actor, msg);
      return false;
    }

    default:
    {
      if(has_flag(actor, FLAG_BLOCKED))
      {
        // Send unblock before continuing. We no longer need to send any
        // pending rc change to the cycle detector.
        unset_flag(actor, FLAG_BLOCKED | FLAG_RC_CHANGED);
        ponyint_cycle_unblock(ctx, actor);
      }

      DTRACE3(ACTOR_MSG_RUN, (uintptr_t)ctx->scheduler, (uintptr_t)actor, msg->id);
      actor->type->dispatch(ctx, actor, msg);
      return true;
    }
  }
}

static void try_gc(pony_ctx_t* ctx, pony_actor_t* actor)
{
  if(!ponyint_heap_startgc(&actor->heap))
    return;

  DTRACE1(GC_START, (uintptr_t)ctx->scheduler);

  ponyint_gc_mark(ctx);

  if(actor->type->trace != NULL)
    actor->type->trace(ctx, actor);

  ponyint_mark_done(ctx);
  ponyint_heap_endgc(&actor->heap);

  DTRACE1(GC_END, (uintptr_t)ctx->scheduler);
}

bool ponyint_actor_run(pony_ctx_t* ctx, pony_actor_t* actor, size_t batch)
{
  /*if (ponyint_is_muted(actor))
  {
    printf("%p is muted at start of run in %p\n", actor, ctx->scheduler);
  }*/

  pony_assert(!ponyint_is_muted(actor));
  ctx->current = actor;

  pony_msg_t* msg;
  size_t app = 0;

#ifdef USE_ACTOR_CONTINUATIONS
  while(actor->continuation != NULL)
  {
    msg = actor->continuation;
    actor->continuation = atomic_load_explicit(&msg->next,
      memory_order_relaxed);
    bool ret = handle_message(ctx, actor, msg);
    ponyint_pool_free(msg->index, msg);

    if(ret)
    {
      // If we handle an application message, try to gc.
      app++;
      try_gc(ctx, actor);

      if(app == batch)
        return !has_flag(actor, FLAG_UNSCHEDULED);
    }
  }
#endif

  // If we have been scheduled, the head will not be marked as empty.
  pony_msg_t* head = atomic_load_explicit(&actor->q.head, memory_order_relaxed);

  while((msg = ponyint_messageq_pop(&actor->q)) != NULL)
  {
    if(handle_message(ctx, actor, msg))
    {
      // If we handle an application message, try to gc.
      app++;
      try_gc(ctx, actor);

      // if we become muted as a result of handling a message, bail out now.
      if(atomic_load_explicit(&actor->muted, memory_order_relaxed) > 0)
      {
        //printf("%p is bailing out due to mute in %p\n", actor, ctx->scheduler);
        ponyint_mute_actor(actor);
        return false;
      }

      if(app == batch)
      {
        if(!has_flag(actor, FLAG_OVERLOADED))
        {
          // if we hit our batch size, consider this actor to be overloaded
          ponyint_actor_setoverloaded(actor);
        }

        return !has_flag(actor, FLAG_UNSCHEDULED);
      }
    }

    // Stop handling a batch if we reach the head we found when we were
    // scheduled.
    if(msg == head)
      break;
  }

  // We didn't hit our app message batch limit. We now believe our queue to be
  // empty, but we may have received further messages.
  pony_assert(app < batch);
  pony_assert(!ponyint_is_muted(actor));

  if(has_flag(actor, FLAG_OVERLOADED))
  {
    // if we were overloaded and didn't process a full batch, set ourselves as no
    // longer overloaded.
    ponyint_actor_unsetoverloaded(actor);
  }

  try_gc(ctx, actor);

  if(has_flag(actor, FLAG_UNSCHEDULED))
  {
    // When unscheduling, don't mark the queue as empty, since we don't want
    // to get rescheduled if we receive a message.
    return false;
  }

  // If we have processed any application level messages, defer blocking.
  if(app > 0)
    return true;

  // Tell the cycle detector we are blocking. We may not actually block if a
  // message is received between now and when we try to mark our queue as
  // empty, but that's ok, we have still logically blocked.
  if(!has_flag(actor, FLAG_BLOCKED | FLAG_SYSTEM) ||
    has_flag(actor, FLAG_RC_CHANGED))
  {
    set_flag(actor, FLAG_BLOCKED);
    unset_flag(actor, FLAG_RC_CHANGED);
    ponyint_cycle_block(ctx, actor, &actor->gc);
  }

  bool empty = ponyint_messageq_markempty(&actor->q);
  if (empty && actor_noblock && (actor->gc.rc == 0))
  {
    // when 'actor_noblock` is true, the cycle detector isn't running.
    // this means actors won't be garbage collected unless we take special
    // action. Here, we know that:
    // - the actor has no messages in its queue
    // - there's no references to this actor
    // therefore if `noblock` is on, we should garbage collect the actor.
    ponyint_actor_setpendingdestroy(actor);
    ponyint_actor_final(ctx, actor);
    ponyint_actor_destroy(actor);
  }

  // Return true (i.e. reschedule immediately) if our queue isn't empty.
  return !empty;
}

void ponyint_actor_destroy(pony_actor_t* actor)
{
  pony_assert(has_flag(actor, FLAG_PENDINGDESTROY));

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

  ponyint_messageq_destroy(&actor->q);
  ponyint_gc_destroy(&actor->gc);
  ponyint_heap_destroy(&actor->heap);

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
  return has_flag(actor, FLAG_PENDINGDESTROY);
}

void ponyint_actor_setpendingdestroy(pony_actor_t* actor)
{
  // This is thread-safe, even though the flag is set from the cycle detector.
  // The function is only called after the cycle detector has detected a true
  // cycle and an actor won't change its flags if it is part of a true cycle.
  // The synchronisation is done through the ACK message sent by the actor to
  // the cycle detector.
  set_flag(actor, FLAG_PENDINGDESTROY);
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
  set_flag(actor, FLAG_SYSTEM);
}

void ponyint_actor_setnoblock(bool state)
{
  actor_noblock = state;
}

PONY_API pony_actor_t* pony_create(pony_ctx_t* ctx, pony_type_t* type)
{
  pony_assert(type != NULL);

  // allocate variable sized actors correctly
  pony_actor_t* actor = (pony_actor_t*)ponyint_pool_alloc_size(type->size);
  memset(actor, 0, type->size);
  actor->type = type;

  ponyint_messageq_init(&actor->q);
  ponyint_heap_init(&actor->heap);
  ponyint_gc_done(&actor->gc);

  if(actor_noblock)
    ponyint_actor_setsystem(actor);

  if(ctx->current != NULL)
  {
    // actors begin unblocked and referenced by the creating actor
    actor->gc.rc = GC_INC_MORE;
    ponyint_gc_createactor(ctx->current, actor);
  } else {
    // no creator, so the actor isn't referenced by anything
    actor->gc.rc = 0;
  }

  DTRACE2(ACTOR_ALLOC, (uintptr_t)ctx->scheduler, (uintptr_t)actor);
  return actor;
}

PONY_API void ponyint_destroy(pony_actor_t* actor)
{
  // This destroys an actor immediately. If any other actor has a reference to
  // this actor, the program will likely crash. The finaliser is not called.
  ponyint_actor_setpendingdestroy(actor);
  ponyint_actor_destroy(actor);
}

PONY_API pony_msg_t* pony_alloc_msg(uint32_t index, uint32_t id)
{
  pony_msg_t* msg = (pony_msg_t*)ponyint_pool_alloc(index);
  msg->index = index;
  msg->id = id;
#ifndef NDEBUG
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
    ponyint_maybe_mute(ctx, to);

  if(ponyint_messageq_push(&to->q, first, last))
  {
    if(!has_flag(to, FLAG_UNSCHEDULED) && !ponyint_is_muted(to))
    {
      //printf("SCHEDULING %p on %p because of empty queue sendv\n", to, ctx->scheduler);

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
    ponyint_maybe_mute(ctx, to);

  if(ponyint_messageq_push_single(&to->q, first, last))
  {
    if(!has_flag(to, FLAG_UNSCHEDULED) && !ponyint_is_muted(to))
    {
      // if the receiving actor is currently not unscheduled AND it's not
      // muted, schedule it.
      //printf("SCHEDULING %p on %p because of empty queue\n", to, ctx->scheduler);
      ponyint_sched_add(ctx, to);
    }
  }
}

void ponyint_maybe_mute(pony_ctx_t* ctx, pony_actor_t* to)
{
  if(ctx->current != NULL)
  {
    // only mute a sender IF:
    // 1. the receiver is overloaded/under pressure/muted
    // AND
    // 2. the sender isn't overloaded
    // AND
    // 3. we are sending to another actor (as compared to sending to self)
    if(ponyint_triggers_muting(to) &&
       !has_flag(ctx->current, FLAG_OVERLOADED) &&
       ctx->current != to)
    {
      ponyint_sched_mute(ctx, ctx->current, to);
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
  pony_msg_t* m = pony_alloc_msg(POOL_INDEX(sizeof(pony_msg_t)), id);
  pony_sendv(ctx, to, m, m, id <= ACTORMSG_APPLICATION_START);
}

PONY_API void pony_sendp(pony_ctx_t* ctx, pony_actor_t* to, uint32_t id,
  void* p)
{
  pony_msgp_t* m = (pony_msgp_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(pony_msgp_t)), id);
  m->p = p;

  pony_sendv(ctx, to, &m->msg, &m->msg, id <= ACTORMSG_APPLICATION_START);
}

PONY_API void pony_sendi(pony_ctx_t* ctx, pony_actor_t* to, uint32_t id,
  intptr_t i)
{
  pony_msgi_t* m = (pony_msgi_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(pony_msgi_t)), id);
  m->i = i;

  pony_sendv(ctx, to, &m->msg, &m->msg, id <= ACTORMSG_APPLICATION_START);
}

#ifdef USE_ACTOR_CONTINUATIONS
PONY_API void pony_continuation(pony_ctx_t* ctx, pony_msg_t* m)
{
  pony_assert(ctx->current != NULL);
  pony_actor_t* self = ctx->current;
  atomic_store_explicit(&m->next, self->continuation, memory_order_relaxed);
  self->continuation = m;
}
#endif

PONY_API void* pony_alloc(pony_ctx_t* ctx, size_t size)
{
  pony_assert(ctx->current != NULL);
  DTRACE2(HEAP_ALLOC, (uintptr_t)ctx->scheduler, size);

  return ponyint_heap_alloc(ctx->current, &ctx->current->heap, size);
}

PONY_API void* pony_alloc_small(pony_ctx_t* ctx, uint32_t sizeclass)
{
  pony_assert(ctx->current != NULL);
  DTRACE2(HEAP_ALLOC, (uintptr_t)ctx->scheduler, HEAP_MIN << sizeclass);

  return ponyint_heap_alloc_small(ctx->current, &ctx->current->heap, sizeclass);
}

PONY_API void* pony_alloc_large(pony_ctx_t* ctx, size_t size)
{
  pony_assert(ctx->current != NULL);
  DTRACE2(HEAP_ALLOC, (uintptr_t)ctx->scheduler, size);

  return ponyint_heap_alloc_large(ctx->current, &ctx->current->heap, size);
}

PONY_API void* pony_realloc(pony_ctx_t* ctx, void* p, size_t size)
{
  pony_assert(ctx->current != NULL);
  DTRACE2(HEAP_ALLOC, (uintptr_t)ctx->scheduler, size);

  return ponyint_heap_realloc(ctx->current, &ctx->current->heap, p, size);
}

PONY_API void* pony_alloc_final(pony_ctx_t* ctx, size_t size)
{
  pony_assert(ctx->current != NULL);
  DTRACE2(HEAP_ALLOC, (uintptr_t)ctx->scheduler, size);

  return ponyint_heap_alloc_final(ctx->current, &ctx->current->heap, size);
}

void* pony_alloc_small_final(pony_ctx_t* ctx, uint32_t sizeclass)
{
  pony_assert(ctx->current != NULL);
  DTRACE2(HEAP_ALLOC, (uintptr_t)ctx->scheduler, HEAP_MIN << sizeclass);

  return ponyint_heap_alloc_small_final(ctx->current, &ctx->current->heap,
    sizeclass);
}

void* pony_alloc_large_final(pony_ctx_t* ctx, size_t size)
{
  pony_assert(ctx->current != NULL);
  DTRACE2(HEAP_ALLOC, (uintptr_t)ctx->scheduler, size);

  return ponyint_heap_alloc_large_final(ctx->current, &ctx->current->heap,
    size);
}

PONY_API void pony_triggergc(pony_ctx_t* ctx)
{
  pony_assert(ctx->current != NULL);
  ctx->current->heap.next_gc = 0;
}

PONY_API void pony_schedule(pony_ctx_t* ctx, pony_actor_t* actor)
{
  if(!has_flag(actor, FLAG_UNSCHEDULED) || ponyint_is_muted(actor))
    return;

  unset_flag(actor, FLAG_UNSCHEDULED);
  ponyint_sched_add(ctx, actor);
}

PONY_API void pony_unschedule(pony_ctx_t* ctx, pony_actor_t* actor)
{
  if(has_flag(actor, FLAG_BLOCKED))
  {
    ponyint_cycle_unblock(ctx, actor);
    unset_flag(actor, FLAG_BLOCKED | FLAG_RC_CHANGED);
  }

  set_flag(actor, FLAG_UNSCHEDULED);
}

PONY_API void pony_become(pony_ctx_t* ctx, pony_actor_t* actor)
{
  ctx->current = actor;
}

PONY_API void pony_poll(pony_ctx_t* ctx)
{
  pony_assert(ctx->current != NULL);
  ponyint_actor_run(ctx, ctx->current, 1);
}

void ponyint_actor_setoverloaded(pony_actor_t* actor)
{
  pony_assert(!ponyint_is_cycle(actor));
  set_flag(actor, FLAG_OVERLOADED);
  DTRACE1(ACTOR_OVERLOADED, (uintptr_t)actor);
}

bool ponyint_actor_overloaded(pony_actor_t* actor)
{
  return has_flag(actor, FLAG_OVERLOADED);
}

void ponyint_actor_unsetoverloaded(pony_actor_t* actor)
{
  unset_flag(actor, FLAG_OVERLOADED);
  DTRACE1(ACTOR_OVERLOADED_CLEARED, (uintptr_t)actor);
  if (!has_flag(actor, FLAG_UNDER_PRESSURE)) {
    //printf("OVERLOAD OF %p cleared\n", actor);
    ponyint_sched_start_global_unmute(actor);
  }
}

PONY_API void pony_apply_backpressure()
{
  pony_ctx_t* ctx = pony_ctx();
  set_flag(ctx->current, FLAG_UNDER_PRESSURE);
  DTRACE1(ACTOR_UNDER_PRESSURE, (uintptr_t)ctx->current);
}

PONY_API void pony_release_backpressure()
{
  pony_ctx_t* ctx = pony_ctx();
  unset_flag(ctx->current, FLAG_UNDER_PRESSURE);
  DTRACE1(ACTOR_PRESSURE_RELEASED, (uintptr_t)ctx->current);
  if (!has_flag(ctx->current, FLAG_OVERLOADED))
      ponyint_sched_start_global_unmute(ctx->current);
}

bool ponyint_triggers_muting(pony_actor_t* actor)
{
  return has_flag(actor, FLAG_OVERLOADED) ||
    has_flag(actor, FLAG_UNDER_PRESSURE) ||
    ponyint_is_muted(actor);
}

bool ponyint_is_muted(pony_actor_t* actor)
{
  return (atomic_fetch_add_explicit(&actor->is_muted, 0, memory_order_relaxed) > 0);
}

void ponyint_mute_actor(pony_actor_t* actor)
{
  atomic_fetch_add_explicit(&actor->is_muted, 1, memory_order_relaxed);

//    uint64_t muted = atomic_load_explicit(&sender->muted, memory_order_relaxed);
  // muted++;
   // atomic_store_explicit(&sender->muted, muted, memory_order_relaxed);

}

void ponyint_unmute_actor(pony_actor_t* actor)
{
  atomic_fetch_sub_explicit(&actor->is_muted, 1, memory_order_relaxed);
}
