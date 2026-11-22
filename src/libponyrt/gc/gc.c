#include "gc.h"
#include "../actor/actor.h"
#include "../sched/scheduler.h"
#include "../mem/pagemap.h"
#include "../mem/pool.h"
#include "../tracing/tracing.h"
#include "ponyassert.h"
#include <string.h>
#ifdef USE_SYSTEMATIC_TESTING
#include <stdlib.h>
#endif

#define GC_ACTOR_HEAP_EQUIV 1024
#define GC_IMMUT_HEAP_EQUIV 1024

DEFINE_STACK(ponyint_gcstack, gcstack_t, void);

static bool might_reference_actor(pony_type_t* t)
{
  return t != NULL && t->might_reference_actor;
}

static void acquire_actor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  actorref_t* aref = ponyint_actormap_getorput(&ctx->acquire, actor);
  aref->rc += GC_INC_MORE;
}

static void acquire_object(pony_ctx_t* ctx, pony_actor_t* actor,
  const object_t* source)
{
  actorref_t* aref = ponyint_actormap_getorput(&ctx->acquire, actor);
  object_t* obj = ponyint_actorref_getorput(aref, source->address,
    source->type);

  obj->rc += GC_INC_MORE;
  obj->immutable = source->immutable;
}

static void recurse(pony_ctx_t* ctx, void* p, void* f)
{
  if(f != NULL)
  {
    ctx->stack = ponyint_gcstack_push(ctx->stack, p);
    ctx->stack = ponyint_gcstack_push(ctx->stack, f);
  }
}

typedef enum
{
  MARK_HANDLED,
  MARK_OPAQUE,
  MARK_BORROWED_WITHOUT_REFERENCE,
  MARK_BORROWED_WITH_REFERENCE,
  MARK_COUNT
} mark_offset_t;

// On a repeat visit the stored offset determines what remains to be done.
typedef enum
{
  VISIT_SKIP = MARK_HANDLED,
  VISIT_AFTER_OPAQUE = MARK_OPAQUE,
  VISIT_AFTER_FIRST_BORROW = MARK_BORROWED_WITHOUT_REFERENCE,
  VISIT_AFTER_REBORROW = MARK_BORROWED_WITH_REFERENCE,
  VISIT_FIRST = MARK_COUNT
} visit_t;

enum { GC_EPOCH_STRIDE = MARK_COUNT };
pony_static_assert((GC_EPOCH_STRIDE & (GC_EPOCH_STRIDE - 1)) == 0,
  "The epoch stride must be a power of two to wrap at zero");

static visit_t begin_visit(gc_t* gc, object_t* obj, int mutability)
{
  pony_assert(gc->mark != 0);
  if(!ponyint_object_marked(obj, gc->mark))
  {
    obj->mark = gc->mark + ((mutability == PONY_TRACE_OPAQUE) ? MARK_OPAQUE :
      MARK_HANDLED);
    return VISIT_FIRST;
  }

  if((mutability == PONY_TRACE_OPAQUE) || (obj->mark == gc->mark))
    return VISIT_SKIP;

  visit_t visit = (visit_t)(obj->mark - gc->mark);
  obj->mark = gc->mark;
  return visit;
}

// An opaque borrow has a pending acquire for this object until the epoch ends.
static void record_opaque_borrow(gc_t* gc, object_t* obj)
{
  pony_assert(obj->rc <= 1);
  obj->mark = gc->mark + ((obj->rc == 0) ? MARK_BORROWED_WITHOUT_REFERENCE :
    MARK_BORROWED_WITH_REFERENCE);
}

// True when the caller must trace the fields until the owner receives the
// immutable acquire.
static bool acquire_immutable(pony_ctx_t* ctx, pony_actor_t* actor,
  object_t* obj, visit_t visit)
{
  obj->immutable = true;
  if((visit == VISIT_AFTER_FIRST_BORROW) || (visit == VISIT_AFTER_REBORROW))
  {
    size_t index = HASHMAP_UNKNOWN;
    actorref_t* aref = ponyint_actormap_getactor(&ctx->acquire, actor, &index);
    pony_assert(aref != NULL);
    object_t* pending = ponyint_actorref_getobject(aref, obj->address);
    pony_assert(pending != NULL);
    pending->type = obj->type;
    pending->immutable = true;
    return visit == VISIT_AFTER_REBORROW;
  }

  obj->rc += GC_INC_MORE;
  acquire_object(ctx, actor, obj);
  return true;
}

static void send_local_actor(gc_t* gc)
{
  // Increment the current actor rc if it hasn't been yet.
  if(gc->rc_mark != gc->mark)
  {
    gc->rc_mark = gc->mark;
    gc->rc++;
  }
}

static void recv_local_actor(gc_t* gc)
{
  // Decrement the current actor rc if it hasn't been yet.
  if(gc->rc_mark != gc->mark)
  {
    gc->rc_mark = gc->mark;
    pony_assert(gc->rc > 0);
    gc->rc--;
  }
}

static void send_remote_actor(pony_ctx_t* ctx, gc_t* gc, actorref_t* aref)
{
  if(aref->mark == gc->mark)
    return;

  aref->mark = gc->mark;

  // Dec. If we can't, add to the acquire message.
  if(aref->rc <= 1)
  {
    aref->rc += (GC_INC_MORE - 1);
    acquire_actor(ctx, aref->actor);
  } else {
    aref->rc--;
  }

  // only update if cycle detector is enabled
  if(!ponyint_actor_getnoblock())
    gc->delta = ponyint_deltamap_update(gc->delta, aref->actor, aref->rc);
}

static void recv_remote_actor(pony_ctx_t* ctx, gc_t* gc, actorref_t* aref)
{
  if(aref->mark == gc->mark)
    return;

  if(aref->rc == 0)
  {
    // Increase apparent used memory to provoke GC.
    ponyint_heap_used(ponyint_actor_heap(ctx->current), GC_ACTOR_HEAP_EQUIV);
  }

  aref->mark = gc->mark;
  aref->rc++;

  // only update if cycle detector is enabled
  if(!ponyint_actor_getnoblock())
    gc->delta = ponyint_deltamap_update(gc->delta, aref->actor, aref->rc);
}

static void mark_remote_actor(pony_ctx_t* ctx, gc_t* gc, actorref_t* aref)
{
  if(aref->mark == gc->mark)
    return;

  // Increase apparent used memory to provoke GC.
  ponyint_heap_used(ponyint_actor_heap(ctx->current), GC_ACTOR_HEAP_EQUIV);

  aref->mark = gc->mark;

  if(aref->rc == 0)
  {
    // Invent some references to this actor and acquire it.
    aref->rc += GC_INC_MORE;
    acquire_actor(ctx, aref->actor);

    // only update if cycle detector is enabled
    if(!ponyint_actor_getnoblock())
      gc->delta = ponyint_deltamap_update(gc->delta, aref->actor, aref->rc);
  }
}

static void send_local_object(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  gc_t* gc = ponyint_actor_gc(ctx->current);
  object_t* obj = ponyint_objectmap_getorput(&gc->local, p, t);
  visit_t visit = begin_visit(gc, obj, mutability);

  if(visit == VISIT_SKIP)
    return;

  if(visit == VISIT_FIRST)
  {
    // Implicitly send the owner.
    send_local_actor(gc);
    obj->rc++;
  }

  if(mutability == PONY_TRACE_OPAQUE)
    return;

  if(mutability == PONY_TRACE_IMMUTABLE)
    obj->immutable = true;

  if(!obj->immutable || might_reference_actor(t))
    recurse(ctx, p, t->trace);
}

static void recv_local_object(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  // get the object
  size_t index = HASHMAP_UNKNOWN;
  gc_t* gc = ponyint_actor_gc(ctx->current);
  object_t* obj = ponyint_objectmap_getobject(&gc->local, p, &index);
  pony_assert(obj != NULL);
  if(obj->type == NULL)
    obj->type = t;
  visit_t visit = begin_visit(gc, obj, mutability);

  if(visit == VISIT_SKIP)
    return;

  if(visit == VISIT_FIRST)
  {
    // Implicitly receive the owner.
    recv_local_actor(gc);
    obj->rc--;
  }

  if(mutability == PONY_TRACE_OPAQUE)
    return;

  if(mutability == PONY_TRACE_IMMUTABLE)
    obj->immutable = true;

  if(!obj->immutable || might_reference_actor(t))
    recurse(ctx, p, t->trace);
}

static void mark_local_object(pony_ctx_t* ctx, chunk_t* chunk, void* p,
  pony_type_t* t, int mutability)
{
  if(mutability != PONY_TRACE_OPAQUE)
  {
    // Mark in our heap and recurse if it wasn't already marked.
    if(!ponyint_heap_mark(chunk, p))
      recurse(ctx, p, t->trace);
  } else {
    // Do a shallow mark. If the same address is later marked as something that
    // is not opaque, it will recurse.
    ponyint_heap_mark_shallow(chunk, p);
  }
}

static void send_remote_object(pony_ctx_t* ctx, pony_actor_t* actor,
  void* p, pony_type_t* t, int mutability)
{
  gc_t* gc = ponyint_actor_gc(ctx->current);
  actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor);
#ifdef USE_RUNTIMESTATS
  size_t mem_used_before = ponyint_objectmap_total_mem_size(&aref->map);
  size_t mem_allocated_before = ponyint_objectmap_total_alloc_size(&aref->map);
#endif

  object_t* obj = ponyint_actorref_getorput(aref, p, t);

#ifdef USE_RUNTIMESTATS
  size_t mem_used_after = ponyint_objectmap_total_mem_size(&aref->map);
  size_t mem_allocated_after = ponyint_objectmap_total_alloc_size(&aref->map);
  ctx->current->actorstats.foreign_actormap_objectmap_mem_used +=
    (mem_used_after - mem_used_before);
  ctx->current->actorstats.foreign_actormap_objectmap_mem_allocated +=
    (mem_allocated_after - mem_allocated_before);
#endif

  visit_t visit = begin_visit(gc, obj, mutability);

  if(visit == VISIT_SKIP)
    return;

  if(visit == VISIT_FIRST)
  {
    // Implicitly send the owner.
    send_remote_actor(ctx, gc, aref);

    bool self_send = (ctx->msg_target == ctx->current);
    if(self_send)
      obj->self_send_pins++;

    if((mutability == PONY_TRACE_IMMUTABLE) && !obj->immutable && (obj->rc > 0))
    {
      // One of the borrowed references goes to the message.
      obj->rc += (GC_INC_MORE - 1);
      obj->immutable = true;
      acquire_object(ctx, actor, obj);

      mutability = PONY_TRACE_MUTABLE;
    } else if(obj->rc <= 1) {
      // Borrow enough references to send one and retain a reference here.
      if(mutability == PONY_TRACE_IMMUTABLE)
        obj->immutable = true;

      if(mutability == PONY_TRACE_OPAQUE)
        record_opaque_borrow(gc, obj);

      obj->rc += (GC_INC_MORE - 1);
      acquire_object(ctx, actor, obj);
    } else {
      obj->rc--;
    }
  } else if((mutability == PONY_TRACE_IMMUTABLE) && !obj->immutable) {
    if(acquire_immutable(ctx, actor, obj, visit))
      mutability = PONY_TRACE_MUTABLE;
  }

  if(mutability == PONY_TRACE_OPAQUE)
    return;

  if(mutability == PONY_TRACE_MUTABLE || might_reference_actor(t))
    recurse(ctx, p, t->trace);
}

static void recv_remote_object(pony_ctx_t* ctx, pony_actor_t* actor,
  void* p, pony_type_t* t, int mutability, chunk_t* chunk)
{
  gc_t* gc = ponyint_actor_gc(ctx->current);
  actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor);
#ifdef USE_RUNTIMESTATS
  size_t mem_used_before = ponyint_objectmap_total_mem_size(&aref->map);
  size_t mem_allocated_before = ponyint_objectmap_total_alloc_size(&aref->map);
#endif

  object_t* obj = ponyint_actorref_getorput(aref, p, t);

#ifdef USE_RUNTIMESTATS
  size_t mem_used_after = ponyint_objectmap_total_mem_size(&aref->map);
  size_t mem_allocated_after = ponyint_objectmap_total_alloc_size(&aref->map);
  ctx->current->actorstats.foreign_actormap_objectmap_mem_used +=
    (mem_used_after - mem_used_before);
  ctx->current->actorstats.foreign_actormap_objectmap_mem_allocated +=
    (mem_allocated_after - mem_allocated_before);
#endif

  // If this object was self-sent, this receive consumes one of the in-flight
  // self-sent references that pinned it against the local sweep. Drop the pin
  // before the dedup early-return below, so a receive in the same GC epoch as
  // the send still consumes it and the pin can never get stuck non-zero (which
  // would leak the object by pinning it forever). The pin is a best-effort
  // optimisation hint, not refcount state: because this runs before the dedup
  // and the send-side increment runs after it, an object reached via several
  // references in one message can be decremented more than the matching send
  // incremented, unpinning it a round early. That only forgoes some acquire
  // amortisation (reverting to the pre-fix release/reacquire churn) -- it can
  // never collect a still-referenced object, since the reference counting that
  // governs collection is untouched.
  if(obj->self_send_pins > 0)
    obj->self_send_pins--;

  visit_t visit = begin_visit(gc, obj, mutability);

  if(visit == VISIT_SKIP)
    return;

  if(visit == VISIT_FIRST)
  {
    // Implicitly receive the owner.
    recv_remote_actor(ctx, gc, aref);

    if(obj->rc == 0)
    {
      // Increase apparent used memory to provoke GC.
      ponyint_heap_used(ponyint_actor_heap(ctx->current),
        ponyint_heap_size(chunk));

      if(mutability == PONY_TRACE_IMMUTABLE && !might_reference_actor(t))
        ponyint_heap_used(ponyint_actor_heap(ctx->current),
          GC_IMMUT_HEAP_EQUIV);
    }

    obj->rc++;
  } else if((obj->rc == 1) && (mutability == PONY_TRACE_IMMUTABLE) &&
    !might_reference_actor(t)) {
    // The earlier tag visit supplied this actor's only reference. Account
    // for the memory reachable through fields this actor won't trace.
    ponyint_heap_used(ponyint_actor_heap(ctx->current), GC_IMMUT_HEAP_EQUIV);
  }

  if(mutability == PONY_TRACE_OPAQUE)
    return;

  if(mutability == PONY_TRACE_IMMUTABLE)
    obj->immutable = true;

  if(!obj->immutable || might_reference_actor(t))
    recurse(ctx, p, t->trace);
}

static void mark_remote_object(pony_ctx_t* ctx, pony_actor_t* actor,
  void* p, pony_type_t* t, int mutability, chunk_t* chunk)
{
  gc_t* gc = ponyint_actor_gc(ctx->current);
  actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor);
#ifdef USE_RUNTIMESTATS
  size_t mem_used_before = ponyint_objectmap_total_mem_size(&aref->map);
  size_t mem_allocated_before = ponyint_objectmap_total_alloc_size(&aref->map);
#endif

  object_t* obj = ponyint_actorref_getorput(aref, p, t);

#ifdef USE_RUNTIMESTATS
  size_t mem_used_after = ponyint_objectmap_total_mem_size(&aref->map);
  size_t mem_allocated_after = ponyint_objectmap_total_alloc_size(&aref->map);
  ctx->current->actorstats.foreign_actormap_objectmap_mem_used +=
    (mem_used_after - mem_used_before);
  ctx->current->actorstats.foreign_actormap_objectmap_mem_allocated +=
    (mem_allocated_after - mem_allocated_before);
#endif

  visit_t visit = begin_visit(gc, obj, mutability);

  if(visit == VISIT_SKIP)
    return;

  if(visit == VISIT_FIRST)
  {
    // Increase apparent used memory to provoke GC.
    ponyint_heap_used(ponyint_actor_heap(ctx->current),
      ponyint_heap_size(chunk));

    // Implicitly mark the owner.
    mark_remote_actor(ctx, gc, aref);
  }

  // Charge for fields traced by the owner even when the first immutable
  // acquire also requires this actor to trace them.
  if(mutability == PONY_TRACE_IMMUTABLE && !might_reference_actor(t))
    ponyint_heap_used(ponyint_actor_heap(ctx->current), GC_IMMUT_HEAP_EQUIV);

  if((mutability == PONY_TRACE_IMMUTABLE) && !obj->immutable && (obj->rc > 0))
  {
    if(acquire_immutable(ctx, actor, obj, visit))
      mutability = PONY_TRACE_MUTABLE;
  } else if(obj->rc == 0) {
    // If we haven't seen this object, it's an object that is reached from
    // another immutable object we received, or it's a pointer to an embedded
    // field received in an iso. Invent some references to this object and
    // acquire it.
    if(mutability == PONY_TRACE_IMMUTABLE)
      obj->immutable = true;

    if(mutability == PONY_TRACE_OPAQUE)
      record_opaque_borrow(gc, obj);

    obj->rc += GC_INC_MORE;
    acquire_object(ctx, actor, obj);
  }

  if(mutability == PONY_TRACE_OPAQUE)
    return;

  if(mutability == PONY_TRACE_MUTABLE || might_reference_actor(t))
    recurse(ctx, p, t->trace);
}

void ponyint_gc_sendobject(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  pony_actor_t* actor = NULL;
  chunk_t* chunk = ponyint_pagemap_get(p, &actor);

  // Don't gc memory that wasn't pony_allocated, but do recurse.
  if(chunk == NULL)
  {
    if(mutability != PONY_TRACE_OPAQUE)
      recurse(ctx, p, t->trace);
    return;
  }

  if(actor == ctx->current)
    send_local_object(ctx, p, t, mutability);
  else
    send_remote_object(ctx, actor, p, t, mutability);
}

void ponyint_gc_recvobject(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  pony_actor_t* actor = NULL;
  chunk_t* chunk = ponyint_pagemap_get(p, &actor);

  // Don't gc memory that wasn't pony_allocated, but do recurse.
  if(chunk == NULL)
  {
    if(mutability != PONY_TRACE_OPAQUE)
      recurse(ctx, p, t->trace);
    return;
  }

  if(actor == ctx->current)
    recv_local_object(ctx, p, t, mutability);
  else
    recv_remote_object(ctx, actor, p, t, mutability, chunk);
}

void ponyint_gc_markobject(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  pony_actor_t* actor = NULL;
  chunk_t* chunk = ponyint_pagemap_get(p, &actor);

  // Don't gc memory that wasn't pony_allocated, but do recurse.
  if(chunk == NULL)
  {
    if(mutability != PONY_TRACE_OPAQUE)
      recurse(ctx, p, t->trace);
    return;
  }

  if(actor == ctx->current)
    mark_local_object(ctx, chunk, p, t, mutability);
  else
    mark_remote_object(ctx, actor, p, t, mutability, chunk);
}

void ponyint_gc_sendactor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  gc_t* gc = ponyint_actor_gc(ctx->current);

  if(actor == ctx->current)
  {
    send_local_actor(gc);
  } else {
    actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor);
    send_remote_actor(ctx, gc, aref);
  }
}

void ponyint_gc_recvactor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  gc_t* gc = ponyint_actor_gc(ctx->current);

  if(actor == ctx->current)
  {
    recv_local_actor(gc);
  } else {
    actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor);
    recv_remote_actor(ctx, gc, aref);
  }
}

void ponyint_gc_markactor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  if(actor == ctx->current)
    return;

  gc_t* gc = ponyint_actor_gc(ctx->current);
  actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor);
  mark_remote_actor(ctx, gc, aref);
}

void ponyint_gc_createactor(pony_actor_t* current, pony_actor_t* actor)
{
  gc_t* gc = ponyint_actor_gc(current);
  actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor);
  aref->rc = GC_INC_MORE;

  // only update if cycle detector is enabled
  if(!ponyint_actor_getnoblock())
    gc->delta = ponyint_deltamap_update(gc->delta, actor, aref->rc);

  // Increase apparent used memory to provoke GC.
  ponyint_heap_used(ponyint_actor_heap(current), GC_ACTOR_HEAP_EQUIV);
}

void ponyint_gc_markimmutable(pony_ctx_t* ctx, gc_t* gc)
{
  objectmap_t* map = &gc->local;
  size_t i = HASHMAP_BEGIN;
  object_t* obj;

  while((obj = ponyint_objectmap_next(map, &i)) != NULL)
  {
    if(obj->immutable && (obj->rc > 0) && !might_reference_actor(obj->type))
    {
      pony_assert(obj->type != NULL);
      // Mark in our heap and recurse if it wasn't already marked.
      void* p = obj->address;
      chunk_t* chunk = ponyint_pagemap_get_chunk(p);
      mark_local_object(ctx, chunk, p, obj->type, PONY_TRACE_IMMUTABLE);
    }
  }
}

void ponyint_gc_handlestack(pony_ctx_t* ctx)
{
  pony_trace_fn f;
  void *p;

  while(ctx->stack != NULL)
  {
    ctx->stack = ponyint_gcstack_pop(ctx->stack, (void**)&f);
    ctx->stack = ponyint_gcstack_pop(ctx->stack, &p);
    f(ctx, p);
  }
}

void ponyint_gc_discardstack(pony_ctx_t* ctx)
{
  pony_trace_fn f;
  void *p;

  while(ctx->stack != NULL)
  {
    ctx->stack = ponyint_gcstack_pop(ctx->stack, (void**)&f);
    ctx->stack = ponyint_gcstack_pop(ctx->stack, &p);
  }
}

void ponyint_gc_sweep(pony_ctx_t* ctx, gc_t* gc)
{
  TRACING_ACTOR_GC_OBJECTMAP_SWEEP_START(ctx->current);
  ponyint_objectmap_sweep(&gc->local);
  TRACING_ACTOR_GC_OBJECTMAP_SWEEP_END(ctx->current);

#ifdef USE_RUNTIMESTATS
  size_t objectmap_mem_used_freed = 0;
  size_t objectmap_mem_allocated_freed = 0;
#endif

  TRACING_ACTOR_GC_ACTORMAP_SWEEP_START(ctx->current);

  gc->delta = ponyint_actormap_sweep(ctx, &gc->foreign, gc->mark, gc->delta,
#ifdef USE_RUNTIMESTATS
    ponyint_actor_getnoblock(), &objectmap_mem_used_freed,
    &objectmap_mem_allocated_freed);
#else
    ponyint_actor_getnoblock());
#endif

#ifdef USE_RUNTIMESTATS
  ctx->current->actorstats.foreign_actormap_objectmap_mem_used -= objectmap_mem_used_freed;
  ctx->current->actorstats.foreign_actormap_objectmap_mem_allocated -= objectmap_mem_allocated_freed;

  pony_assert((ponyint_actormap_partial_mem_size(&gc->foreign)
    + ctx->current->actorstats.foreign_actormap_objectmap_mem_used)
    == ponyint_actormap_total_mem_size(&gc->foreign));
  pony_assert((ponyint_actormap_partial_alloc_size(&gc->foreign)
    + ctx->current->actorstats.foreign_actormap_objectmap_mem_allocated)
    == ponyint_actormap_total_alloc_size(&gc->foreign));
#endif

  TRACING_ACTOR_GC_ACTORMAP_SWEEP_END(ctx->current);
}

bool ponyint_gc_acquire(gc_t* gc, actorref_t* aref)
{
  size_t rc = aref->rc;
  gc->rc += rc;

  objectmap_t* map = &aref->map;
  size_t i = HASHMAP_BEGIN;
  object_t* obj;

  while((obj = ponyint_objectmap_next(map, &i)) != NULL)
  {
    // Add to our RC. The object may not be in our object map, if it was
    // reached through another immutable reference.
    object_t* obj_local = ponyint_objectmap_getorput(&gc->local, obj->address,
      obj->type);
    obj_local->rc += obj->rc;

    // Mark as immutable if necessary.
    if(obj->immutable)
      obj_local->immutable = true;
  }

  ponyint_actorref_free(aref);
  return rc > 0;
}

bool ponyint_gc_release(gc_t* gc, actorref_t* aref)
{
  size_t rc = aref->rc;
  pony_assert(gc->rc >= rc);
  gc->rc -= rc;

  objectmap_t* map = &aref->map;
  size_t i = HASHMAP_BEGIN;
  object_t* obj;
  size_t index = HASHMAP_UNKNOWN;

  while((obj = ponyint_objectmap_next(map, &i)) != NULL)
  {
    void* p = obj->address;
    object_t* obj_local = ponyint_objectmap_getobject(&gc->local, p, &index);

    pony_assert(obj_local->rc >= obj->rc);
    obj_local->rc -= obj->rc;
  }

  ponyint_actorref_free(aref);
  return rc > 0;
}

size_t ponyint_gc_rc(gc_t* gc)
{
  return gc->rc;
}

deltamap_t* ponyint_gc_delta(gc_t* gc)
{
  deltamap_t* delta = gc->delta;
  gc->delta = NULL;
  return delta;
}

#ifdef USE_SYSTEMATIC_TESTING
// Order two actorrefs by their target actor's stable creation-order id. ids are
// unique per actor, so there are no ties and the order is total and
// layout-independent.
static int gc_actorref_systematic_testing_id_cmp(const void* a, const void* b)
{
  uint64_t ia = (*(actorref_t* const*)a)->actor->systematic_testing_id;
  uint64_t ib = (*(actorref_t* const*)b)->actor->systematic_testing_id;
  return (ia > ib) - (ia < ib);
}

// Drain ctx->acquire and send each referenced actor an ACTORMSG_ACQUIRE,
// ordered by the target actor's stable creation-order id rather than by the
// actormap's pointer-hash iteration order. Each send schedules its recipient,
// so sending in a layout-independent order is what keeps a fixed seed's
// interleaving reproducible under ASLR. See pony_actor_t.systematic_testing_id.
static void gc_drain_acquire_ordered(pony_ctx_t* ctx)
{
  size_t n = ponyint_actormap_size(&ctx->acquire);

  if(n > 0)
  {
    actorref_t** refs =
      (actorref_t**)ponyint_pool_alloc_size(n * sizeof(actorref_t*));
    size_t i = HASHMAP_BEGIN;
    size_t k = 0;
    actorref_t* aref;

    while((aref = ponyint_actormap_next(&ctx->acquire, &i)) != NULL)
    {
      ponyint_actormap_clearindex(&ctx->acquire, i);
      refs[k++] = aref;
    }

    qsort(refs, n, sizeof(actorref_t*),
      gc_actorref_systematic_testing_id_cmp);

    for(k = 0; k < n; k++)
    {
      aref = refs[k];
#ifdef USE_RUNTIMESTATS
      ctx->schedulerstats.mem_used_actors += (sizeof(actorref_t)
        + ponyint_objectmap_total_mem_size(&aref->map));
      ctx->schedulerstats.mem_allocated_actors += (POOL_ALLOC_SIZE(actorref_t)
        + ponyint_objectmap_total_alloc_size(&aref->map));
#endif
      ponyint_sendp(ctx, aref->actor, ACTORMSG_ACQUIRE, aref);
    }

    ponyint_pool_free_size(n * sizeof(actorref_t*), refs);
  }

  pony_assert(ponyint_actormap_size(&ctx->acquire) == 0);
}
#endif

void ponyint_gc_sendacquire(pony_ctx_t* ctx)
{
#ifdef USE_SYSTEMATIC_TESTING
  gc_drain_acquire_ordered(ctx);
#else
  size_t i = HASHMAP_BEGIN;
  actorref_t* aref;

  while((aref = ponyint_actormap_next(&ctx->acquire, &i)) != NULL)
  {
#ifdef USE_RUNTIMESTATS
    ctx->schedulerstats.mem_used_actors += (sizeof(actorref_t)
      + ponyint_objectmap_total_mem_size(&aref->map));
    ctx->schedulerstats.mem_allocated_actors += (POOL_ALLOC_SIZE(actorref_t)
      + ponyint_objectmap_total_alloc_size(&aref->map));
#endif

    ponyint_actormap_clearindex(&ctx->acquire, i);
    ponyint_sendp(ctx, aref->actor, ACTORMSG_ACQUIRE, aref);
  }

  pony_assert(ponyint_actormap_size(&ctx->acquire) == 0);
#endif
}

void ponyint_gc_sendrelease(pony_ctx_t* ctx, gc_t* gc)
{
#ifdef USE_RUNTIMESTATS
  size_t objectmap_mem_used_freed = 0;
  size_t objectmap_mem_allocated_freed = 0;
#endif

  gc->delta = ponyint_actormap_sweep(ctx, &gc->foreign, gc->mark, gc->delta,
#ifdef USE_RUNTIMESTATS
    ponyint_actor_getnoblock(), &objectmap_mem_used_freed,
    &objectmap_mem_allocated_freed);
#else
    ponyint_actor_getnoblock());
#endif

#ifdef USE_RUNTIMESTATS
  ctx->current->actorstats.foreign_actormap_objectmap_mem_used -= objectmap_mem_used_freed;
  ctx->current->actorstats.foreign_actormap_objectmap_mem_allocated -= objectmap_mem_allocated_freed;
#endif
}

static void clear_object_marks(objectmap_t* map)
{
  size_t i = HASHMAP_BEGIN;
  object_t* obj;
  while((obj = ponyint_objectmap_next(map, &i)) != NULL)
    obj->mark = 0;
}

static PONY_NOINLINE void reset_marks(gc_t* gc)
{
  // Zero is reserved for an unvisited entry. Forget all visits before epoch
  // numbers are reused, including borrows whose acquire has already been sent.
  gc->mark = GC_EPOCH_STRIDE;
  gc->rc_mark = 0;
  clear_object_marks(&gc->local);

  size_t i = HASHMAP_BEGIN;
  actorref_t* aref;
  while((aref = ponyint_actormap_next(&gc->foreign, &i)) != NULL)
  {
    aref->mark = 0;
    clear_object_marks(&aref->map);
  }
}

void ponyint_gc_done(gc_t* gc)
{
  gc->mark += GC_EPOCH_STRIDE;
  if(gc->mark == 0)
    reset_marks(gc);
}

void ponyint_gc_destroy(gc_t* gc)
{
  ponyint_objectmap_destroy(&gc->local);
  ponyint_actormap_destroy(&gc->foreign);

  if(gc->delta != NULL)
  {
    ponyint_deltamap_free(gc->delta);
    gc->delta = NULL;
  }
}

#ifdef USE_RUNTIMESTATS
size_t ponyint_gc_total_mem_size(pony_actor_t* actor, gc_t* gc)
{
  // gc total mem size
  return
    // objectmap size
      ponyint_objectmap_total_mem_size(&gc->local)
    // foreign actormap size (without objectmaps inside actormaps)
    + ponyint_actormap_partial_mem_size(&gc->foreign)
    + actor->actorstats.foreign_actormap_objectmap_mem_used
    // deltamap size
    + (gc->delta != NULL ? ponyint_deltamap_total_mem_size(gc->delta) : 0);
}

size_t ponyint_gc_total_alloc_size(pony_actor_t* actor, gc_t* gc)
{
  // gc total mem allocated
  return
    // objectmap allocated
      ponyint_objectmap_total_alloc_size(&gc->local)
    // foreign actormap allocated (without objectmaps inside actormaps)
    + ponyint_actormap_partial_alloc_size(&gc->foreign)
    + actor->actorstats.foreign_actormap_objectmap_mem_allocated
    // deltamap allocated
    + (gc->delta != NULL ? ponyint_deltamap_total_alloc_size(gc->delta) : 0);
}
#endif
