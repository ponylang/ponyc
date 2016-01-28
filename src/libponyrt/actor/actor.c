#include "actor.h"
#include "../sched/scheduler.h"
#include "../sched/cpu.h"
#include "../mem/pool.h"
#include "../gc/cycle.h"
#include "../gc/trace.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

enum
{
  FLAG_BLOCKED = 1 << 0,
  FLAG_RC_CHANGED = 1 << 1,
  FLAG_SYSTEM = 1 << 2,
  FLAG_UNSCHEDULED = 1 << 3,
  FLAG_PENDINGDESTROY = 1 << 4,
};

static bool has_flag(pony_actor_t* actor, uint8_t flag)
{
  return (actor->flags & flag) != 0;
}

static void set_flag(pony_actor_t* actor, uint8_t flag)
{
  actor->flags |= flag;
}

static void unset_flag(pony_actor_t* actor, uint8_t flag)
{
  actor->flags &= (uint8_t)~flag;
}

static bool handle_message(pony_ctx_t* ctx, pony_actor_t* actor,
  pony_msg_t* msg)
{
  switch(msg->id)
  {
    case ACTORMSG_ACQUIRE:
    {
      pony_msgp_t* m = (pony_msgp_t*)msg;

      if(gc_acquire(&actor->gc, (actorref_t*)m->p) &&
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

      if(gc_release(&actor->gc, (actorref_t*)m->p) &&
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
      // If we aren't blocked, there's no need to respond.
      if(!has_flag(actor, FLAG_BLOCKED))
        return false;

      if(has_flag(actor, FLAG_RC_CHANGED))
      {
        // Send the pending rc change instead of confirming.
        unset_flag(actor, FLAG_RC_CHANGED);
        cycle_block(ctx, actor, &actor->gc);
      } else {
        // Nothing has changed since our last block message, send confirm.
        pony_msgi_t* m = (pony_msgi_t*)msg;
        cycle_ack(ctx, m->i);
      }

      return false;
    }

    default:
    {
      if(has_flag(actor, FLAG_BLOCKED))
      {
        // Send unblock before continuing. We no longer need to send any
        // pending rc change to the cycle detector.
        unset_flag(actor, FLAG_BLOCKED | FLAG_RC_CHANGED);
        cycle_unblock(ctx, actor);
      }

      actor->type->dispatch(ctx, actor, msg);
      return true;
    }
  }
}

static void try_gc(pony_ctx_t* ctx, pony_actor_t* actor)
{
  if(!heap_startgc(&actor->heap))
    return;

#ifdef USE_TELEMETRY
  ctx->count_gc_passes++;
  size_t tsc = cpu_tick();
#endif

  pony_gc_mark(ctx);

  if(actor->type->trace != NULL)
    actor->type->trace(ctx, actor);

  pony_mark_done(ctx);
  heap_endgc(&actor->heap);

#ifdef USE_TELEMETRY
  ctx->time_in_gc += (cpu_tick() - tsc);
#endif
}

bool actor_run(pony_ctx_t* ctx, pony_actor_t* actor, size_t batch)
{
  ctx->current = actor;

  pony_msg_t* msg;
  size_t app = 0;

  while(actor->continuation != NULL)
  {
    msg = actor->continuation;
    actor->continuation = NULL;
    bool ret = handle_message(ctx, actor, msg);
    pool_free(msg->size, msg);

    if(ret)
    {
      // If we handle an application message, try to gc.
      app++;
      try_gc(ctx, actor);

      if(app == batch)
        return !has_flag(actor, FLAG_UNSCHEDULED);
    }
  }

  while((msg = messageq_pop(&actor->q)) != NULL)
  {
    if(handle_message(ctx, actor, msg))
    {
      // If we handle an application message, try to gc.
      app++;
      try_gc(ctx, actor);

      if(app == batch)
        return !has_flag(actor, FLAG_UNSCHEDULED);
    }
  }

  // We didn't hit our app message batch limit. We now believe our queue to be
  // empty, but we may have received further messages.
  assert(app < batch);
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
  if(!has_flag(actor, FLAG_BLOCKED | FLAG_SYSTEM))
  {
    set_flag(actor, FLAG_BLOCKED);
    unset_flag(actor, FLAG_RC_CHANGED);
    cycle_block(ctx, actor, &actor->gc);
  }

  // Return true (i.e. reschedule immediately) if our queue isn't empty.
  return !messageq_markempty(&actor->q);
}

void actor_destroy(pony_actor_t* actor)
{
  assert(has_flag(actor, FLAG_PENDINGDESTROY));

  // Make sure the actor being destroyed has finished marking its queue
  // as empty. Otherwise, it may spuriously see that tail and head are not
  // the same and fail to mark the queue as empty, resulting in it getting
  // rescheduled.
  pony_msg_t* head = _atomic_load(&actor->q.head);

  while(((uintptr_t)head & (uintptr_t)1) != (uintptr_t)1)
    head = _atomic_load(&actor->q.head);

  messageq_destroy(&actor->q);
  gc_destroy(&actor->gc);
  heap_destroy(&actor->heap);

  // Free variable sized actors correctly.
  pool_free_size(actor->type->size, actor);
}

gc_t* actor_gc(pony_actor_t* actor)
{
  return &actor->gc;
}

heap_t* actor_heap(pony_actor_t* actor)
{
  return &actor->heap;
}

bool actor_pendingdestroy(pony_actor_t* actor)
{
  return has_flag(actor, FLAG_PENDINGDESTROY);
}

void actor_setpendingdestroy(pony_actor_t* actor)
{
  set_flag(actor, FLAG_PENDINGDESTROY);
}

void actor_final(pony_ctx_t* ctx, pony_actor_t* actor)
{
  // This gets run while the cycle detector is handling a message. Set the
  // current actor before running anything.
  pony_actor_t* prev = ctx->current;
  ctx->current = actor;

  // Run the actor finaliser if it has one.
  if(actor->type->final != NULL)
    actor->type->final(actor);

  // Run all outstanding object finalisers.
  gc_final(ctx, &actor->gc);

  // Restore the current actor.
  ctx->current = prev;
}

void actor_sendrelease(pony_ctx_t* ctx, pony_actor_t* actor)
{
  gc_sendrelease(ctx, &actor->gc);
}

void actor_setsystem(pony_actor_t* actor)
{
  set_flag(actor, FLAG_SYSTEM);
}

pony_actor_t* pony_create(pony_ctx_t* ctx, pony_type_t* type)
{
  assert(type != NULL);

#ifdef USE_TELEMETRY
  ctx->count_alloc_actors++;
#endif

  // allocate variable sized actors correctly
  pony_actor_t* actor = (pony_actor_t*)pool_alloc_size(type->size);
  memset(actor, 0, type->size);
  actor->type = type;

  messageq_init(&actor->q);
  heap_init(&actor->heap);
  gc_done(&actor->gc);

  if(ctx->current != NULL)
  {
    // actors begin unblocked and referenced by the creating actor
    actor->gc.rc = GC_INC_MORE;
    gc_createactor(ctx->current, actor);
  } else {
    // no creator, so the actor isn't referenced by anything
    actor->gc.rc = 0;
  }

  return actor;
}

void pony_destroy(pony_actor_t* actor)
{
  // This destroy an actor immediately. If any other actor has a reference to
  // this actor, the program will likely crash. The finaliser is not called.
  actor_setpendingdestroy(actor);
  actor_destroy(actor);
}

pony_msg_t* pony_alloc_msg(uint32_t size, uint32_t id)
{
  pony_msg_t* msg = (pony_msg_t*)pool_alloc(size);
  msg->size = size;
  msg->id = id;

  return msg;
}

void pony_sendv(pony_ctx_t* ctx, pony_actor_t* to, pony_msg_t* m)
{
#ifdef USE_TELEMETRY
  switch(m->id)
  {
    case ACTORMSG_BLOCK: ctx->count_msg_block++; break;
    case ACTORMSG_UNBLOCK: ctx->count_msg_unblock++; break;
    case ACTORMSG_ACQUIRE: ctx->count_msg_acquire++; break;
    case ACTORMSG_RELEASE: ctx->count_msg_release++; break;
    case ACTORMSG_CONF: ctx->count_msg_conf++; break;
    case ACTORMSG_ACK: ctx->count_msg_ack++; break;
    default: ctx->count_msg_app++;
  }
#endif

  if(messageq_push(&to->q, m))
  {
    if(!has_flag(to, FLAG_UNSCHEDULED))
      scheduler_add(ctx, to);
  }
}

void pony_send(pony_ctx_t* ctx, pony_actor_t* to, uint32_t id)
{
  pony_msg_t* m = pony_alloc_msg(POOL_INDEX(sizeof(pony_msg_t)), id);
  pony_sendv(ctx, to, m);
}

void pony_sendp(pony_ctx_t* ctx, pony_actor_t* to, uint32_t id, void* p)
{
  pony_msgp_t* m = (pony_msgp_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(pony_msgp_t)), id);
  m->p = p;

  pony_sendv(ctx, to, &m->msg);
}

void pony_sendi(pony_ctx_t* ctx, pony_actor_t* to, uint32_t id, intptr_t i)
{
  pony_msgi_t* m = (pony_msgi_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(pony_msgi_t)), id);
  m->i = i;

  pony_sendv(ctx, to, &m->msg);
}

void pony_continuation(pony_actor_t* to, pony_msg_t* m)
{
  assert(to->continuation == NULL);
  m->next = NULL;
  to->continuation = m;
}

void* pony_alloc(pony_ctx_t* ctx, size_t size)
{
#ifdef USE_TELEMETRY
  ctx->count_alloc++;
  ctx->count_alloc_size += size;
#endif

  return heap_alloc(ctx->current, &ctx->current->heap, size);
}

void* pony_alloc_small(pony_ctx_t* ctx, uint32_t sizeclass)
{
#ifdef USE_TELEMETRY
  ctx->count_alloc++;
  ctx->count_alloc_size += HEAP_MIN << sizeclass;
#endif

  return heap_alloc_small(ctx->current, &ctx->current->heap, sizeclass);
}

void* pony_alloc_large(pony_ctx_t* ctx, size_t size)
{
#ifdef USE_TELEMETRY
  ctx->count_alloc++;
  ctx->count_alloc_size += size;
#endif

  return heap_alloc_large(ctx->current, &ctx->current->heap, size);
}

void* pony_realloc(pony_ctx_t* ctx, void* p, size_t size)
{
#ifdef USE_TELEMETRY
  ctx->count_alloc++;
  ctx->count_alloc_size += size;
#endif

  return heap_realloc(ctx->current, &ctx->current->heap, p, size);
}

void* pony_alloc_final(pony_ctx_t* ctx, size_t size, pony_final_fn final)
{
#ifdef USE_TELEMETRY
  ctx->count_alloc++;
  ctx->count_alloc_size += size;
#endif

  void* p = heap_alloc(ctx->current, &ctx->current->heap, size);
  gc_register_final(ctx, p, final);
  return p;
}

void pony_triggergc(pony_actor_t* actor)
{
  actor->heap.next_gc = 0;
}

void pony_schedule(pony_ctx_t* ctx, pony_actor_t* actor)
{
  if(!has_flag(actor, FLAG_UNSCHEDULED))
    return;

  unset_flag(actor, FLAG_UNSCHEDULED);
  scheduler_add(ctx, actor);
}

void pony_unschedule(pony_ctx_t* ctx, pony_actor_t* actor)
{
  if(has_flag(actor, FLAG_BLOCKED))
  {
    cycle_unblock(ctx, actor);
    unset_flag(actor, FLAG_BLOCKED | FLAG_RC_CHANGED);
  }

  set_flag(actor, FLAG_UNSCHEDULED);
}

void pony_become(pony_ctx_t* ctx, pony_actor_t* actor)
{
  ctx->current = actor;
}

void pony_poll(pony_ctx_t* ctx)
{
  assert(ctx->current != NULL);
  actor_run(ctx, ctx->current, 1);
}
