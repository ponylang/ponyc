#include "gc.h"
#include "../actor/actor.h"
#include "../sched/scheduler.h"
#include "../mem/pagemap.h"
#include <string.h>
#include <assert.h>

#define GC_ACTOR_HEAP_EQUIV 1024

DEFINE_STACK(ponyint_gcstack, gcstack_t, void);

static void acquire_actor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  actorref_t* aref = ponyint_actormap_getorput(&ctx->acquire, actor, 0);
  aref->rc += GC_INC_MORE;
}

static void acquire_object(pony_ctx_t* ctx, pony_actor_t* actor, void* address,
  bool immutable)
{
  actorref_t* aref = ponyint_actormap_getorput(&ctx->acquire, actor, 0);
  object_t* obj = ponyint_actorref_getorput(aref, address, 0);

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
    assert(gc->rc > 0);
    gc->rc--;
  }
}

static void acquire_local_actor(gc_t* gc)
{
  gc->rc++;
}

static void release_local_actor(gc_t* gc)
{
  assert(gc->rc > 0);
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

  gc->delta = ponyint_deltamap_update(gc->delta, aref->actor, aref->rc);
}

static void recv_remote_actor(pony_ctx_t* ctx, gc_t* gc, actorref_t* aref)
{
  if(aref->mark == gc->mark)
    return;

  aref->mark = gc->mark;
  aref->rc++;
  gc->delta = ponyint_deltamap_update(gc->delta, aref->actor, aref->rc);
  ponyint_heap_used(ponyint_actor_heap(ctx->current), GC_ACTOR_HEAP_EQUIV);
}

static void mark_remote_actor(pony_ctx_t* ctx, gc_t* gc, actorref_t* aref)
{
  if(aref->mark == gc->mark)
    return;

  aref->mark = gc->mark;

  if(aref->rc == 0)
  {
    // If we haven't seen this actor, it's the owner of an immutable object
    // that is reached from another immutable object we received. Invent
    // some references to this actor and acquire it.
    aref->rc += GC_INC_MORE;
    acquire_actor(ctx, aref->actor);
    gc->delta = ponyint_deltamap_update(gc->delta, aref->actor, aref->rc);
  }

  ponyint_heap_used(ponyint_actor_heap(ctx->current), GC_ACTOR_HEAP_EQUIV);
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
  object_t* obj = ponyint_objectmap_getorput(&gc->local, p, gc->mark);

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

  if(!obj->immutable)
    recurse(ctx, p, t->trace);
}

static void recv_local_object(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  // get the object
  gc_t* gc = ponyint_actor_gc(ctx->current);
  object_t* obj = ponyint_objectmap_getobject(&gc->local, p);
  assert(obj != NULL);

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

  if(!obj->immutable)
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
  object_t* obj = ponyint_objectmap_getorput(&gc->local, p, gc->mark);

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

  if(!obj->immutable)
    recurse(ctx, p, t->trace);
}

static void release_local_object(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  gc_t* gc = ponyint_actor_gc(ctx->current);
  object_t* obj = ponyint_objectmap_getobject(&gc->local, p);
  assert(obj != NULL);

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

  if(!obj->immutable)
    recurse(ctx, p, t->trace);
}

static void send_remote_object(pony_ctx_t* ctx, pony_actor_t* actor,
  void* p, pony_type_t* t, int mutability)
{
  gc_t* gc = ponyint_actor_gc(ctx->current);
  actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor, gc->mark);
  object_t* obj = ponyint_actorref_getorput(aref, p, gc->mark);

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
  } else if(obj->rc <= 1) {
    // If we haven't seen this object, it's an object that is reached from
    // another immutable object we received. Invent some references to this
    // object and acquire it. This object should either be immutable or a tag.
    assert((obj->rc > 0) || (mutability != PONY_TRACE_MUTABLE));

    // Add to the acquire message and decrement.
    if(mutability == PONY_TRACE_IMMUTABLE)
      obj->immutable = true;

    obj->rc += (GC_INC_MORE - 1);
    acquire_object(ctx, actor, p, obj->immutable);
  } else {
    // Decrement.
    obj->rc--;
  }

  if(mutability == PONY_TRACE_MUTABLE)
    recurse(ctx, p, t->trace);
}

static void recv_remote_object(pony_ctx_t* ctx, pony_actor_t* actor,
  void* p, pony_type_t* t, int mutability, chunk_t* chunk)
{
  gc_t* gc = ponyint_actor_gc(ctx->current);
  actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor, gc->mark);
  object_t* obj = ponyint_actorref_getorput(aref, p, gc->mark);

  if(obj->mark == gc->mark)
    return;

  // Implicitly receive the owner.
  recv_remote_actor(ctx, gc, aref);

  // If this is our first reference, add to our heap used size.
  if(obj->rc == 0)
  {
    ponyint_heap_used(ponyint_actor_heap(ctx->current),
      ponyint_heap_size(chunk));
  }

  // Inc, mark and recurse.
  obj->rc++;
  obj->mark = gc->mark;

  if(mutability == PONY_TRACE_OPAQUE)
    return;

  if(mutability == PONY_TRACE_IMMUTABLE)
    obj->immutable = true;

  if(!obj->immutable)
    recurse(ctx, p, t->trace);
}

static void mark_remote_object(pony_ctx_t* ctx, pony_actor_t* actor,
  void* p, pony_type_t* t, int mutability, chunk_t* chunk)
{
  gc_t* gc = ponyint_actor_gc(ctx->current);
  actorref_t* aref = ponyint_actormap_getorput(&gc->foreign, actor, gc->mark);
  object_t* obj = ponyint_actorref_getorput(aref, p, gc->mark);

  if(obj->mark == gc->mark)
    return;

  // Implicitly mark the owner.
  mark_remote_actor(ctx, gc, aref);

  // Mark the object.
  obj->mark = gc->mark;

  // Add to heap used size.
  ponyint_heap_used(ponyint_actor_heap(ctx->current),
    ponyint_heap_size(chunk));

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
  } else if(obj->rc == 0) {
    // If we haven't seen this object, it's an object that is reached from
    // another immutable object we received. Invent some references to this
    // object and acquire it. This object should either be immutable or a tag.
    assert(mutability != PONY_TRACE_MUTABLE);

    if(mutability == PONY_TRACE_IMMUTABLE)
      obj->immutable = true;

    obj->rc += GC_INC_MORE;
    acquire_object(ctx, actor, p, obj->immutable);
  }

  if(mutability == PONY_TRACE_MUTABLE)
    recurse(ctx, p, t->trace);
}

static void acq_or_rel_remote_object(pony_ctx_t* ctx, pony_actor_t* actor,
  void* p, pony_type_t* t, int mutability)
{
  gc_t* gc = ponyint_actor_gc(ctx->current);
  actorref_t* aref = ponyint_actormap_getorput(&ctx->acquire, actor, 0);
  object_t* obj = ponyint_actorref_getorput(aref, p, gc->mark);

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

  if(!obj->immutable)
    recurse(ctx, p, t->trace);
}

void ponyint_gc_sendobject(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  chunk_t* chunk = (chunk_t*)ponyint_pagemap_get(p);

  // Don't gc memory that wasn't pony_allocated, but do recurse.
  if(chunk == NULL)
  {
    if(mutability != PONY_TRACE_OPAQUE)
      recurse(ctx, p, t->trace);
    return;
  }

  pony_actor_t* actor = ponyint_heap_owner(chunk);

  if(actor == ctx->current)
    send_local_object(ctx, p, t, mutability);
  else
    send_remote_object(ctx, actor, p, t, mutability);
}

void ponyint_gc_recvobject(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  chunk_t* chunk = (chunk_t*)ponyint_pagemap_get(p);

  // Don't gc memory that wasn't pony_allocated, but do recurse.
  if(chunk == NULL)
  {
    if(mutability != PONY_TRACE_OPAQUE)
      recurse(ctx, p, t->trace);
    return;
  }

  pony_actor_t* actor = ponyint_heap_owner(chunk);

  if(actor == ctx->current)
    recv_local_object(ctx, p, t, mutability);
  else
    recv_remote_object(ctx, actor, p, t, mutability, chunk);
}

void ponyint_gc_markobject(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  chunk_t* chunk = (chunk_t*)ponyint_pagemap_get(p);

  // Don't gc memory that wasn't pony_allocated, but do recurse.
  if(chunk == NULL)
  {
    if(mutability != PONY_TRACE_OPAQUE)
      recurse(ctx, p, t->trace);
    return;
  }

  pony_actor_t* actor = ponyint_heap_owner(chunk);

  if(actor == ctx->current)
    mark_local_object(ctx, chunk, p, t, mutability);
  else
    mark_remote_object(ctx, actor, p, t, mutability, chunk);
}

void ponyint_gc_acquireobject(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  chunk_t* chunk = (chunk_t*)ponyint_pagemap_get(p);

  // Don't gc memory that wasn't pony_allocated, but do recurse.
  if(chunk == NULL)
  {
    if(mutability != PONY_TRACE_OPAQUE)
      recurse(ctx, p, t->trace);
    return;
  }

  pony_actor_t* actor = ponyint_heap_owner(chunk);

  if(actor == ctx->current)
    acquire_local_object(ctx, p, t, mutability);
  else
    acq_or_rel_remote_object(ctx, actor, p, t, mutability);
}

void ponyint_gc_releaseobject(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  chunk_t* chunk = (chunk_t*)ponyint_pagemap_get(p);

  // Don't gc memory that wasn't pony_allocated, but do recurse.
  if(chunk == NULL)
  {
    if(mutability != PONY_TRACE_OPAQUE)
      recurse(ctx, p, t->trace);
    return;
  }

  pony_actor_t* actor = ponyint_heap_owner(chunk);

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
  gc->delta = ponyint_deltamap_update(gc->delta, actor, aref->rc);
  ponyint_heap_used(ponyint_actor_heap(current), GC_ACTOR_HEAP_EQUIV);
}

void ponyint_gc_markimmutable(pony_ctx_t* ctx, gc_t* gc)
{
  objectmap_t* map = &gc->local;
  size_t i = HASHMAP_BEGIN;
  object_t* obj;

  while((obj = ponyint_objectmap_next(map, &i)) != NULL)
  {
    if(obj->immutable && (obj->rc > 0))
    {
      // Mark in our heap and recurse if it wasn't already marked.
      void* p = obj->address;
      chunk_t* chunk = (chunk_t*)ponyint_pagemap_get(p);
      pony_type_t* type = *(pony_type_t**)p;
      mark_local_object(ctx, chunk, p, type, PONY_TRACE_IMMUTABLE);
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

void ponyint_gc_sweep(pony_ctx_t* ctx, gc_t* gc)
{
  gc->finalisers -= ponyint_objectmap_sweep(&gc->local);
  gc->delta = ponyint_actormap_sweep(ctx, &gc->foreign, gc->mark, gc->delta);
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
      gc->mark);
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
  assert(gc->rc >= rc);
  gc->rc -= rc;

  objectmap_t* map = &aref->map;
  size_t i = HASHMAP_BEGIN;
  object_t* obj;

  while((obj = ponyint_objectmap_next(map, &i)) != NULL)
  {
    void* p = obj->address;
    object_t* obj_local = ponyint_objectmap_getobject(&gc->local, p);

    assert(obj_local->rc >= obj->rc);
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
    ponyint_actormap_removeindex(&ctx->acquire, i);
    pony_sendp(ctx, aref->actor, ACTORMSG_ACQUIRE, aref);
  }

  ponyint_actormap_destroy(&ctx->acquire);
  memset(&ctx->acquire, 0, sizeof(actormap_t));
}

void ponyint_gc_sendrelease(pony_ctx_t* ctx, gc_t* gc)
{
  gc->delta = ponyint_actormap_sweep(ctx, &gc->foreign, gc->mark, gc->delta);
}

void ponyint_gc_sendrelease_manual(pony_ctx_t* ctx)
{
  size_t i = HASHMAP_BEGIN;
  actorref_t* aref;

  while((aref = ponyint_actormap_next(&ctx->acquire, &i)) != NULL)
  {
    ponyint_actormap_removeindex(&ctx->acquire, i);
    pony_sendp(ctx, aref->actor, ACTORMSG_RELEASE, aref);
  }

  ponyint_actormap_destroy(&ctx->acquire);
  memset(&ctx->acquire, 0, sizeof(actormap_t));
}

void ponyint_gc_register_final(pony_ctx_t* ctx, void* p, pony_final_fn final)
{
  if(!ctx->finalising)
  {
    // If we aren't finalising an actor, register the finaliser.
    gc_t* gc = ponyint_actor_gc(ctx->current);
    ponyint_objectmap_register_final(&gc->local, p, final, gc->mark);
    gc->finalisers++;
  } else {
    // Otherwise, put the finaliser on the gc stack.
    recurse(ctx, p, final);
  }
}

void ponyint_gc_final(pony_ctx_t* ctx, gc_t* gc)
{
  if(gc->finalisers == 0)
    return;

  // Set the finalising flag.
  ctx->finalising = true;

  // Run all finalisers in the object map.
  ponyint_objectmap_final(&gc->local);

  // Finalise any objects that were created during finalisation.
  pony_final_fn f;
  void *p;

  while(ctx->stack != NULL)
  {
    ctx->stack = ponyint_gcstack_pop(ctx->stack, (void**)&f);
    ctx->stack = ponyint_gcstack_pop(ctx->stack, &p);
    f(p);
  }

  ctx->finalising = false;
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
