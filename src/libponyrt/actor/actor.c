#include "actor.h"
#include "messageq.h"
#include "../sched/scheduler.h"
#include "../mem/pool.h"
#include "../gc/cycle.h"
#include "../gc/trace.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

enum
{
  FLAG_BLOCKED = 1 << 0,
  FLAG_SYSTEM = 1 << 1,
  FLAG_UNSCHEDULED = 1 << 2,
  FLAG_PENDINGDESTROY = 1 << 3,
};

typedef struct pony_actor_t
{
  pony_type_t* type;
  messageq_t q;
  pony_msg_t* continuation;

  // keep things accessed by other actors on a separate cache line
  __pony_spec_align__(heap_t heap, 64);
  gc_t gc;
  struct pony_actor_t* next;
  uint8_t flags;
} pony_actor_t;

static __pony_thread_local pony_actor_t* this_actor;

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

static bool handle_message(pony_actor_t* actor, pony_msg_t* msg, bool* notify)
{
  switch(msg->id)
  {
    case ACTORMSG_ACQUIRE:
    {
      pony_msgp_t* m = (pony_msgp_t*)msg;

      if(gc_acquire(&actor->gc, (actorref_t*)m->p) &&
        has_flag(actor, FLAG_BLOCKED))
      {
        *notify = true;
      }

      return false;
    }

    case ACTORMSG_RELEASE:
    {
      pony_msgp_t* m = (pony_msgp_t*)msg;

      if(gc_release(&actor->gc, (actorref_t*)m->p) &&
        has_flag(actor, FLAG_BLOCKED))
      {
        *notify = true;
      }

      return false;
    }

    case ACTORMSG_CONF:
    {
      pony_msgi_t* m = (pony_msgi_t*)msg;
      cycle_ack(m->i);
      return false;
    }

    default:
    {
      if(has_flag(actor, FLAG_BLOCKED))
      {
        *notify = false;
        cycle_unblock(actor);
        unset_flag(actor, FLAG_BLOCKED);
      }

      actor->type->dispatch(actor, msg);
      return true;
    }
  }
}

static void try_gc(pony_actor_t* actor)
{
  if(!heap_startgc(&actor->heap))
    return;

  pony_gc_mark();

  if(actor->type->trace != NULL)
    actor->type->trace(actor);

  gc_handlestack();
  gc_sweep(&actor->gc);
  gc_done(&actor->gc);
  heap_endgc(&actor->heap);
}

bool actor_run(pony_actor_t* actor)
{
  this_actor = actor;

  pony_msg_t* msg;
  bool notify = false;

  if(actor->continuation != NULL)
  {
    msg = actor->continuation;
    actor->continuation = NULL;
    bool ret = handle_message(actor, msg, &notify);
    pool_free(msg->size, msg);

    if(ret)
    {
      // If we handle an application message, try to gc and then return.
      try_gc(actor);
      return !has_flag(actor, FLAG_UNSCHEDULED);
    }
  }

  while((msg = messageq_pop(&actor->q)) != NULL)
  {
    if(handle_message(actor, msg, &notify))
    {
      // If we handle an application message, try to gc and then return.
      try_gc(actor);
      return !has_flag(actor, FLAG_UNSCHEDULED);
    }
  }

  // No application messages were in the queue.
  try_gc(actor);

  if(has_flag(actor, FLAG_UNSCHEDULED))
  {
    // When unscheduling, don't mark the queue as empty, since we don't want
    // to get rescheduled if we receive a message.
    return false;
  }

  // If we are just now blocking, or we received an acquire or a release
  // message, tell the cycle detector.
  if(notify || !has_flag(actor, FLAG_BLOCKED | FLAG_SYSTEM))
  {
    cycle_block(actor, &actor->gc);
    set_flag(actor, FLAG_BLOCKED);
  }

  return !messageq_markempty(&actor->q);
}

void actor_destroy(pony_actor_t* actor)
{
  assert(has_flag(actor, FLAG_PENDINGDESTROY));

  messageq_destroy(&actor->q);
  gc_destroy(&actor->gc);
  heap_destroy(&actor->heap);

  // Free variable sized actors correctly.
  pool_free_size(actor->type->size, actor);
}

pony_actor_t* actor_current()
{
  return this_actor;
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

void actor_final(pony_actor_t* actor)
{
  // This gets run while the cycle detector is handling a message. Set the
  // current actor before running anything.
  pony_actor_t* prev = this_actor;
  this_actor = actor;

  // Run the actor finaliser if it has one.
  if(actor->type->final != NULL)
    actor->type->final(actor);

  // Run all outstanding object finalisers.
  gc_final(&actor->gc);

  // Restore the current actor.
  this_actor = prev;
}

void actor_sendrelease(pony_actor_t* actor)
{
  gc_sendrelease(&actor->gc);
}

void actor_setsystem(pony_actor_t* actor)
{
  set_flag(actor, FLAG_SYSTEM);
}

pony_actor_t* actor_next(pony_actor_t* actor)
{
  return actor->next;
}

void actor_setnext(pony_actor_t* actor, pony_actor_t* next)
{
  actor->next = next;
}

pony_actor_t* pony_create(pony_type_t* type)
{
  assert(type != NULL);
  pony_actor_t* current = this_actor;

  // allocate variable sized actors correctly
  pony_actor_t* actor = (pony_actor_t*)pool_alloc_size(type->size);
  memset(actor, 0, type->size);
  actor->type = type;

  messageq_init(&actor->q);
  heap_init(&actor->heap);
  gc_done(&actor->gc);

  if(this_actor != NULL)
  {
    // actors begin unblocked and referenced by the creating actor
    actor->gc.rc = GC_INC_MORE;
    gc_createactor(&current->heap, &current->gc, actor);
  } else {
    // no creator, so the actor isn't referenced by anything
    actor->gc.rc = 0;
  }

  return actor;
}

void pony_destroy(pony_actor_t* actor)
{
  // This destroy an actor immediately. If any other actor has a reference to
  // this actor, the program will likely crash.
  actor_setpendingdestroy(actor);
  actor_final(actor);
  actor_destroy(actor);
}

pony_msg_t* pony_alloc_msg(uint32_t size, uint32_t id)
{
  pony_msg_t* msg = (pony_msg_t*)pool_alloc(size);
  msg->size = size;
  msg->id = id;

  return msg;
}

void pony_sendv(pony_actor_t* to, pony_msg_t* m)
{
  if(messageq_push(&to->q, m))
  {
    if(!has_flag(to, FLAG_UNSCHEDULED))
      scheduler_add(to);
  }
}

void pony_send(pony_actor_t* to, uint32_t id)
{
  pony_msg_t* m = pony_alloc_msg(POOL_INDEX(sizeof(pony_msg_t)), id);
  pony_sendv(to, m);
}

void pony_sendp(pony_actor_t* to, uint32_t id, void* p)
{
  pony_msgp_t* m = (pony_msgp_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(pony_msgp_t)), id);
  m->p = p;

  pony_sendv(to, &m->msg);
}

void pony_sendi(pony_actor_t* to, uint32_t id, intptr_t i)
{
  pony_msgi_t* m = (pony_msgi_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(pony_msgi_t)), id);
  m->i = i;

  pony_sendv(to, &m->msg);
}

void pony_continuation(pony_actor_t* to, pony_msg_t* m)
{
  assert(to->continuation == NULL);
  m->next = NULL;
  to->continuation = m;
}

void* pony_alloc(size_t size)
{
  return heap_alloc(this_actor, &this_actor->heap, size);
}

void* pony_realloc(void* p, size_t size)
{
  return heap_realloc(this_actor, &this_actor->heap, p, size);
}

void* pony_alloc_final(size_t size, pony_final_fn final)
{
  void* p = heap_alloc(this_actor, &this_actor->heap, size);
  gc_register_final(&this_actor->gc, p, final);
  return p;
}

void pony_triggergc()
{
  this_actor->heap.next_gc = 0;
}

void pony_schedule(pony_actor_t* actor)
{
  if(!has_flag(actor, FLAG_UNSCHEDULED))
    return;

  unset_flag(actor, FLAG_UNSCHEDULED);
  scheduler_add(actor);
}

void pony_unschedule()
{
  if(has_flag(this_actor, FLAG_BLOCKED))
  {
    cycle_unblock(this_actor);
    unset_flag(this_actor, FLAG_BLOCKED);
  }

  set_flag(this_actor, FLAG_UNSCHEDULED);
}

void pony_become(pony_actor_t* actor)
{
  this_actor = actor;
}

bool pony_poll(pony_actor_t* actor)
{
  return actor_run(actor);
}
