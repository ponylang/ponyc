#include "gc.h"
#include "../actor/actor.h"
#include "../sched/scheduler.h"
#include "../mem/pagemap.h"
#include "../tracing/tracing.h"
#include "ponyassert.h"
#include <string.h>

#define GC_ACTOR_HEAP_EQUIV 1024
#define GC_IMMUT_HEAP_EQUIV 1024

DEFINE_STACK(ponyint_gcstack, gcstack_t, void);

static bool might_reference_actor(pony_type_t* t)
{
  return t != NULL && t->might_reference_actor;
}

static void acquire_actor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  actorref_t* aref = ponyint_actormap_getorput(&ctx->acquire, actor, 0);
  aref->rc += GC_INC_MORE;
}

static void acquire_object(pony_ctx_t* ctx, pony_actor_t* actor, void* address,
  bool immutable)
{
  actorref_t* aref = ponyint_actormap_getorput(&ctx->acquire, actor, 0);
  pony_type_t* type = *(pony_type_t**)address;
  object_t* obj = ponyint_actorref_getorput(aref, address, type, 0);

  obj->rc += GC_INC_MORE;
  obj->immutable = immutable;
}

static void recurse(pony_ctx_t* ctx, void* p, void* f)
{
  if(f != NULL)
  {
    ctx->stack = ponyint_gcstack_push(ctx->stack, p);
    ctx->stack = ponyint_gcstack_push(ctx->stack, f);
  }
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

static void acquire_local_actor(gc_t* gc)
{
  gc->rc++;
}

static void release_local_actor(gc_t* gc)
{
  pony_assert(gc->rc > 0);
  gc->rc--;
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

static void acq_or_rel_remote_actor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  actorref_t* aref = ponyint_actormap_getorput(&ctx->acquire, actor, 0);
  aref->rc += 1;
}

static void send_local_object(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  gc_t* gc = ponyint_actor_gc(ctx->current);
  object_t* obj = ponyint_objectmap_getorput(&gc->local, p, t, gc->mark);

  if(obj->mark == gc->mark)
    return;

  // Implicitly send the owner.
  send_local_actor(gc);

  // Inc, mark, and recurse if not immutable.
  obj->rc++;
  obj->mark = gc->mark;

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

  if(obj->mark == gc->mark)
    return;

  // Implicitly receive the owner.
  recv_local_actor(gc);

  // Dec, mark and recurse.
  obj->rc--;
  obj->mark = gc->mark;

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

static void acquire_local_object(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  gc_t* gc = ponyint_actor_gc(ctx->current);
  object_t* obj = ponyint_objectmap_getorput(&gc->local, p, t, gc->mark);

  if(obj->mark == gc->mark)
    return;

  // Implicitly acquire the owner.
  acquire_local_actor(gc);

  obj->rc++;
  obj->mark = gc->mark;

  if(mutability == PONY_TRACE_OPAQUE)
    return;

  if(mutability == PONY_TRACE_IMMUTABLE)
    obj->immutable = true;

  if(!obj->immutable || might_reference_actor(t))
    recurse(ctx, p, t->trace);
}

static void release_local_object(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  size_t index = HASHMAP_UNKNOWN;
  gc_t* gc = ponyint_actor_gc(ctx->current);
  object_t* obj = ponyint_objectmap_getobject(&gc->local, p, &index);
  pony_assert(obj != NULL);

  if(obj->mark == gc->mark)
    return;

  // Implicitly release the owner.
  release_local_actor(gc);

  obj->rc--;
  obj->mark = gc->mark;

  if(mutability == PONY_TRACE_OPAQUE)
    return;

  if(mutability == PONY_TRACE_IMMUTABLE)
    obj->immutable = true;

  if(!obj->immutable || might_reference_actor(t))
    recurse(ctx, p, t->trace);
}

static void send_remote_object(pony_ctx_t* ctx, pony_actor_t* actor,
  void* p, pony_type_t* t, int mutability)
{
  gc_t* gc = ponyint_actor_gc(ctx->current);
  actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor, gc->mark);
#ifdef USE_RUNTIMESTATS
  size_t mem_used_before = ponyint_objectmap_total_mem_size(&aref->map);
  size_t mem_allocated_before = ponyint_objectmap_total_alloc_size(&aref->map);
#endif

  object_t* obj = ponyint_actorref_getorput(aref, p, t, gc->mark);

#ifdef USE_RUNTIMESTATS
  size_t mem_used_after = ponyint_objectmap_total_mem_size(&aref->map);
  size_t mem_allocated_after = ponyint_objectmap_total_alloc_size(&aref->map);
  ctx->current->actorstats.foreign_actormap_objectmap_mem_used +=
    (mem_used_after - mem_used_before);
  ctx->current->actorstats.foreign_actormap_objectmap_mem_allocated +=
    (mem_allocated_after - mem_allocated_before);
#endif

  if(obj->mark == gc->mark)
    return;

  // Implicitly send the owner.
  send_remote_actor(ctx, gc, aref);

  // Mark the object.
  obj->mark = gc->mark;

  if((mutability == PONY_TRACE_IMMUTABLE) && !obj->immutable && (obj->rc > 0))
  {
    // If we received the object as not immutable (it's not marked as immutable
    // and it has an RC > 0), but we are now sending it as immutable, we need
    // to acquire it in order to inform the owner it is now immutable. But we
    // also need to continue tracing, to protect the contents until the owner
    // has received the acquire messages.
    obj->rc += (GC_INC_MORE - 1);
    obj->immutable = true;
    acquire_object(ctx, actor, p, true);

    // Set the to PONY_TRACE_MUTABLE to force recursion.
    mutability = PONY_TRACE_MUTABLE;
  } else if(obj->rc <= 1) {
    // If we haven't seen this object, it's an object that is reached from
    // another immutable object we received, or it's a pointer to an embedded
    // field received in an iso. Invent some references to this object and
    // acquire it.
    if(mutability == PONY_TRACE_IMMUTABLE)
      obj->immutable = true;

    // Add to the acquire message and decrement.
    obj->rc += (GC_INC_MORE - 1);
    acquire_object(ctx, actor, p, obj->immutable);
  } else {
    // Decrement.
    obj->rc--;
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
  actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor, gc->mark);
#ifdef USE_RUNTIMESTATS
  size_t mem_used_before = ponyint_objectmap_total_mem_size(&aref->map);
  size_t mem_allocated_before = ponyint_objectmap_total_alloc_size(&aref->map);
#endif

  object_t* obj = ponyint_actorref_getorput(aref, p, t, gc->mark);

#ifdef USE_RUNTIMESTATS
  size_t mem_used_after = ponyint_objectmap_total_mem_size(&aref->map);
  size_t mem_allocated_after = ponyint_objectmap_total_alloc_size(&aref->map);
  ctx->current->actorstats.foreign_actormap_objectmap_mem_used +=
    (mem_used_after - mem_used_before);
  ctx->current->actorstats.foreign_actormap_objectmap_mem_allocated +=
    (mem_allocated_after - mem_allocated_before);
#endif

  if(obj->mark == gc->mark)
    return;

  // Implicitly receive the owner.
  recv_remote_actor(ctx, gc, aref);

  if(obj->rc == 0)
  {
    // Increase apparent used memory to provoke GC.
    ponyint_heap_used(ponyint_actor_heap(ctx->current),
      ponyint_heap_size(chunk));

    // Increase apparent used memory further if the object is immutable, to
    // account for memory that is reachable but not traced by this actor.
    if(mutability == PONY_TRACE_IMMUTABLE && !might_reference_actor(t))
      ponyint_heap_used(ponyint_actor_heap(ctx->current), GC_IMMUT_HEAP_EQUIV);
  }

  // Inc, mark and recurse.
  obj->rc++;
  obj->mark = gc->mark;

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
  actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor, gc->mark);
#ifdef USE_RUNTIMESTATS
  size_t mem_used_before = ponyint_objectmap_total_mem_size(&aref->map);
  size_t mem_allocated_before = ponyint_objectmap_total_alloc_size(&aref->map);
#endif

  object_t* obj = ponyint_actorref_getorput(aref, p, t, gc->mark);

#ifdef USE_RUNTIMESTATS
  size_t mem_used_after = ponyint_objectmap_total_mem_size(&aref->map);
  size_t mem_allocated_after = ponyint_objectmap_total_alloc_size(&aref->map);
  ctx->current->actorstats.foreign_actormap_objectmap_mem_used +=
    (mem_used_after - mem_used_before);
  ctx->current->actorstats.foreign_actormap_objectmap_mem_allocated +=
    (mem_allocated_after - mem_allocated_before);
#endif

  if(obj->mark == gc->mark)
    return;

  // Increase apparent used memory to provoke GC.
  ponyint_heap_used(ponyint_actor_heap(ctx->current),
    ponyint_heap_size(chunk));

  // Increase apparent used memory further if the object is immutable, to
  // account for memory that is reachable but not traced by this actor.
  if(mutability == PONY_TRACE_IMMUTABLE && !might_reference_actor(t))
    ponyint_heap_used(ponyint_actor_heap(ctx->current), GC_IMMUT_HEAP_EQUIV);

  // Implicitly mark the owner.
  mark_remote_actor(ctx, gc, aref);

  // Mark the object.
  obj->mark = gc->mark;

  if((mutability == PONY_TRACE_IMMUTABLE) && !obj->immutable && (obj->rc > 0))
  {
    // If we received the object as not immutable (it's not marked as immutable
    // and it has an RC > 0), but we are now marking it as immutable, we need
    // to acquire it in order to inform the owner it is now immutable. But we
    // also need to continue tracing, to protect the contents until the owner
    // has received the acquire messages.
    obj->rc += GC_INC_MORE;
    obj->immutable = true;
    acquire_object(ctx, actor, p, true);

    // Set the to PONY_TRACE_MUTABLE to force recursion.
    mutability = PONY_TRACE_MUTABLE;
  } else if(obj->rc == 0) {
    // If we haven't seen this object, it's an object that is reached from
    // another immutable object we received, or it's a pointer to an embedded
    // field received in an iso. Invent some references to this object and
    // acquire it.
    if(mutability == PONY_TRACE_IMMUTABLE)
      obj->immutable = true;

    obj->rc += GC_INC_MORE;
    acquire_object(ctx, actor, p, obj->immutable);
  }

  if(mutability == PONY_TRACE_OPAQUE)
    return;

  if(mutability == PONY_TRACE_MUTABLE || might_reference_actor(t))
    recurse(ctx, p, t->trace);
}

static void acq_or_rel_remote_object(pony_ctx_t* ctx, pony_actor_t* actor,
  void* p, pony_type_t* t, int mutability)
{
  gc_t* gc = ponyint_actor_gc(ctx->current);
  actorref_t* aref = ponyint_actormap_getorput(&ctx->acquire, actor, 0);
  object_t* obj = ponyint_actorref_getorput(aref, p, t, gc->mark);

  if(obj->mark == gc->mark)
    return;

  // Implicitly acquire/release the owner.
  acq_or_rel_remote_actor(ctx, actor);

  obj->rc++;
  obj->mark = gc->mark;

  if(mutability == PONY_TRACE_OPAQUE)
    return;

  if(mutability == PONY_TRACE_IMMUTABLE)
    obj->immutable = true;

  if(!obj->immutable || might_reference_actor(t))
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

void ponyint_gc_acquireobject(pony_ctx_t* ctx, void* p, pony_type_t* t,
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
    acquire_local_object(ctx, p, t, mutability);
  else
    acq_or_rel_remote_object(ctx, actor, p, t, mutability);
}

void ponyint_gc_releaseobject(pony_ctx_t* ctx, void* p, pony_type_t* t,
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
    release_local_object(ctx, p, t, mutability);
  else
    acq_or_rel_remote_object(ctx, actor, p, t, mutability);

}

void ponyint_gc_sendactor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  gc_t* gc = ponyint_actor_gc(ctx->current);

  if(actor == ctx->current)
  {
    send_local_actor(gc);
  } else {
    actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor,
      gc->mark);
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
    actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor,
      gc->mark);
    recv_remote_actor(ctx, gc, aref);
  }
}

void ponyint_gc_markactor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  if(actor == ctx->current)
    return;

  gc_t* gc = ponyint_actor_gc(ctx->current);
  actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor, gc->mark);
  mark_remote_actor(ctx, gc, aref);
}

void ponyint_gc_acquireactor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  if(actor == ctx->current)
    acquire_local_actor(ponyint_actor_gc(ctx->current));
  else
    acq_or_rel_remote_actor(ctx, actor);
}

void ponyint_gc_releaseactor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  if(actor == ctx->current)
    release_local_actor(ponyint_actor_gc(ctx->current));
  else
    acq_or_rel_remote_actor(ctx, actor);
}

void ponyint_gc_createactor(pony_actor_t* current, pony_actor_t* actor)
{
  gc_t* gc = ponyint_actor_gc(current);
  actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor, gc->mark);
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
      obj->type, gc->mark);
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

void ponyint_gc_sendacquire(pony_ctx_t* ctx)
{
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
    pony_sendp(ctx, aref->actor, ACTORMSG_ACQUIRE, aref);
  }

  pony_assert(ponyint_actormap_size(&ctx->acquire) == 0);
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

void ponyint_gc_sendrelease_manual(pony_ctx_t* ctx)
{
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
    pony_sendp(ctx, aref->actor, ACTORMSG_RELEASE, aref);
  }

  pony_assert(ponyint_actormap_size(&ctx->acquire) == 0);
}

void ponyint_gc_done(gc_t* gc)
{
  gc->mark++;
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
