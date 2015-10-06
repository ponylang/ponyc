#include "gc.h"
#include "../actor/actor.h"
#include "../sched/scheduler.h"
#include "../mem/pagemap.h"
#include <string.h>
#include <assert.h>

#define GC_ACTOR_HEAP_EQUIV 1024

DEFINE_STACK(gcstack, void);

static void acquire_actor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  actorref_t* aref = actormap_getorput(&ctx->acquire, actor, 0);
  actorref_inc_more(aref);
}

static void acquire_object(pony_ctx_t* ctx, pony_actor_t* actor, void* address)
{
  actorref_t* aref = actormap_getorput(&ctx->acquire, actor, 0);
  object_t* obj = actorref_getorput(aref, address, 0);
  object_inc_more(obj);
}

static void current_actor_inc(gc_t* gc)
{
  if(gc->rc_mark != gc->mark)
  {
    gc->rc_mark = gc->mark;
    gc->rc++;
  }
}

static void current_actor_dec(gc_t* gc)
{
  if(gc->rc_mark != gc->mark)
  {
    gc->rc_mark = gc->mark;
    assert(gc->rc > 0);
    gc->rc--;
  }
}

void gc_sendobject(pony_ctx_t* ctx, void* p, pony_trace_fn f)
{
  chunk_t* chunk = (chunk_t*)pagemap_get(p);

  // Don't gc memory that wasn't pony_allocated, but do recurse.
  if(chunk == NULL)
  {
    if(f != NULL)
    {
      ctx->stack = gcstack_push(ctx->stack, p);
      ctx->stack = gcstack_push(ctx->stack, f);
    }

    return;
  }

  pony_actor_t* actor = heap_owner(chunk);
  gc_t* gc = actor_gc(ctx->current);

  if(actor == ctx->current)
  {
    current_actor_inc(gc);

    // get the object
    object_t* obj = objectmap_getorput(&gc->local, p, gc->mark);

    if(!object_marked(obj, gc->mark))
    {
      // inc, mark and recurse
      object_inc(obj);
      object_mark(obj, gc->mark);

      if(f != NULL)
      {
        ctx->stack = gcstack_push(ctx->stack, p);
        ctx->stack = gcstack_push(ctx->stack, f);
      }
    }

    return;
  }

  // get the actor
  actorref_t* aref = actormap_getactor(&gc->foreign, actor);

  // we've reached this by tracing a tag through a union
  if(aref == NULL)
    return;

  // get the object
  object_t* obj = actorref_getobject(aref, p);

  // we've reached this by tracing a tag through a union
  if(obj == NULL)
    return;

  if(!actorref_marked(aref, gc->mark))
  {
    // dec. if we can't, we need to build an acquire message
    if(!actorref_dec(aref))
    {
      actorref_inc_more(aref);
      acquire_actor(ctx, actor);
    }

    actorref_mark(aref, gc->mark);
    gc->delta = deltamap_update(gc->delta,
      actorref_actor(aref), actorref_rc(aref));
  }

  if(!object_marked(obj, gc->mark))
  {
    // dec. if we can't, we need to build an acquire message
    if(!object_dec(obj))
    {
      object_inc_more(obj);
      acquire_object(ctx, actor, p);
    }

    object_mark(obj, gc->mark);

    if(f != NULL)
    {
      ctx->stack = gcstack_push(ctx->stack, p);
      ctx->stack = gcstack_push(ctx->stack, f);
    }
  }
}

void gc_recvobject(pony_ctx_t* ctx, void* p, pony_trace_fn f)
{
  chunk_t* chunk = (chunk_t*)pagemap_get(p);

  // Don't gc memory that wasn't pony_allocated, but do recurse.
  if(chunk == NULL)
  {
    if(f != NULL)
    {
      ctx->stack = gcstack_push(ctx->stack, p);
      ctx->stack = gcstack_push(ctx->stack, f);
    }

    return;
  }

  pony_actor_t* actor = heap_owner(chunk);
  gc_t* gc = actor_gc(ctx->current);

  if(actor == ctx->current)
  {
    current_actor_dec(gc);

    // get the object
    object_t* obj = objectmap_getobject(&gc->local, p);
    assert(obj != NULL);

    if(!object_marked(obj, gc->mark))
    {
      // Dec, mark and recurse. Mark as reachable from the actor, since a
      // release message for this object could arrive before a gc pass.
      object_dec(obj);
      object_mark(obj, gc->mark);
      object_markreachable(obj);

      if(f != NULL)
      {
        ctx->stack = gcstack_push(ctx->stack, p);
        ctx->stack = gcstack_push(ctx->stack, f);
      }
    }

    return;
  }

  // get the actor
  actorref_t* aref = actormap_getorput(&gc->foreign, actor, gc->mark);

  if(!actorref_marked(aref, gc->mark))
  {
    // inc and mark
    actorref_inc(aref);
    actorref_mark(aref, gc->mark);
    gc->delta = deltamap_update(gc->delta,
      actorref_actor(aref), actorref_rc(aref));
  }

  // get the object
  object_t* obj = actorref_getorput(aref, p, gc->mark);

  if(!object_marked(obj, gc->mark))
  {
    // if this is our first reference, add to our heap used size
    if(object_rc(obj) == 0)
      heap_used(actor_heap(ctx->current), heap_size(chunk));

    // inc, mark and recurse
    object_inc(obj);
    object_mark(obj, gc->mark);

    if(f != NULL)
    {
      ctx->stack = gcstack_push(ctx->stack, p);
      ctx->stack = gcstack_push(ctx->stack, f);
    }
  }
}

void gc_markobject(pony_ctx_t* ctx, void* p, pony_trace_fn f)
{
  chunk_t* chunk = (chunk_t*)pagemap_get(p);

  // Don't gc memory that wasn't pony_allocated, but do recurse.
  if(chunk == NULL)
  {
    if(f != NULL)
    {
      ctx->stack = gcstack_push(ctx->stack, p);
      ctx->stack = gcstack_push(ctx->stack, f);
    }

    return;
  }

  pony_actor_t* actor = heap_owner(chunk);

  if(actor == ctx->current)
  {
    if(f != NULL)
    {
      // mark in our heap and recurse if it wasn't already marked
      if(!heap_mark(chunk, p))
      {
        ctx->stack = gcstack_push(ctx->stack, p);
        ctx->stack = gcstack_push(ctx->stack, f);
      }
    } else {
      // no recurse function, so do a shallow mark. if the same address is
      // later marked with a recurse function, it will recurse.
      heap_mark_shallow(chunk, p);
    }

    return;
  }

  // mark the owner
  gc_t* gc = actor_gc(ctx->current);
  actorref_t* aref = actormap_getactor(&gc->foreign, actor);

  // we've reached this by tracing a tag through a union
  if(aref == NULL)
    return;

  // get the object
  object_t* obj = actorref_getobject(aref, p);

  // we've reached this by tracing a tag through a union
  if(obj == NULL)
    return;

  actorref_mark(aref, gc->mark);

  if(!object_marked(obj, gc->mark))
  {
    // add to heap used size
    heap_used(actor_heap(ctx->current), heap_size(chunk));

    // mark and recurse
    object_mark(obj, gc->mark);

    if(f != NULL)
    {
      ctx->stack = gcstack_push(ctx->stack, p);
      ctx->stack = gcstack_push(ctx->stack, f);
    }
  }
}

void gc_sendactor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  gc_t* gc = actor_gc(ctx->current);

  if(actor == ctx->current)
  {
    current_actor_inc(gc);
  } else {
    actorref_t* aref = actormap_getorput(&gc->foreign, actor, gc->mark);

    if(!actorref_marked(aref, gc->mark))
    {
      // dec. if we can't, we need to build an acquire message
      if(!actorref_dec(aref))
      {
        actorref_inc_more(aref);
        acquire_actor(ctx, actor);
      }

      actorref_mark(aref, gc->mark);
      gc->delta = deltamap_update(gc->delta,
        actorref_actor(aref), actorref_rc(aref));
    }
  }
}

void gc_recvactor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  gc_t* gc = actor_gc(ctx->current);

  if(actor == ctx->current)
  {
    current_actor_dec(gc);
  } else {
    actorref_t* aref = actormap_getorput(&gc->foreign, actor, gc->mark);

    if(!actorref_marked(aref, gc->mark))
    {
      actorref_inc(aref);
      actorref_mark(aref, gc->mark);
      gc->delta = deltamap_update(gc->delta,
        actorref_actor(aref), actorref_rc(aref));
      heap_used(actor_heap(ctx->current), GC_ACTOR_HEAP_EQUIV);
    }
  }
}

void gc_markactor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  if(actor == ctx->current)
    return;

  gc_t* gc = actor_gc(ctx->current);
  actorref_t* aref = actormap_getactor(&gc->foreign, actor);

  // we've reached this by tracing a tag through a union
  if(aref == NULL)
    return;

  if(actorref_marked(aref, gc->mark))
    return;

  actorref_mark(aref, gc->mark);
  heap_used(actor_heap(ctx->current), GC_ACTOR_HEAP_EQUIV);
}

void gc_createactor(pony_actor_t* current, pony_actor_t* actor)
{
  gc_t* gc = actor_gc(current);
  actorref_t* aref = actormap_getorput(&gc->foreign, actor, gc->mark);
  actorref_inc_more(aref);
  gc->delta = deltamap_update(gc->delta,
    actorref_actor(aref), actorref_rc(aref));
  heap_used(actor_heap(current), GC_ACTOR_HEAP_EQUIV);
}

void gc_handlestack(pony_ctx_t* ctx)
{
  pony_trace_fn f;
  void *p;

  while(ctx->stack != NULL)
  {
    ctx->stack = gcstack_pop(ctx->stack, (void**)&f);
    ctx->stack = gcstack_pop(ctx->stack, &p);
    f(ctx, p);
  }
}

void gc_sweep(pony_ctx_t* ctx, gc_t* gc)
{
  gc->finalisers -= objectmap_sweep(&gc->local);
  gc->delta = actormap_sweep(ctx, &gc->foreign, gc->mark, gc->delta);
}

bool gc_acquire(gc_t* gc, actorref_t* aref)
{
  size_t rc = actorref_rc(aref);
  gc->rc += rc;

  objectmap_t* map = actorref_map(aref);
  size_t i = HASHMAP_BEGIN;
  object_t* obj;

  while((obj = objectmap_next(map, &i)) != NULL)
  {
    object_t* obj_local = objectmap_getobject(&gc->local, object_address(obj));
    object_inc_some(obj_local, object_rc(obj));
  }

  actorref_free(aref);
  return rc > 0;
}

bool gc_release(gc_t* gc, actorref_t* aref)
{
  size_t rc = actorref_rc(aref);
  assert(gc->rc >= rc);
  gc->rc -= rc;

  objectmap_t* map = actorref_map(aref);
  size_t i = HASHMAP_BEGIN;
  object_t* obj;

  while((obj = objectmap_next(map, &i)) != NULL)
  {
    void* p = object_address(obj);
    object_t* obj_local = objectmap_getobject(&gc->local, p);

    if(object_dec_some(obj_local, object_rc(obj)))
    {
      // The local rc for this object has dropped to zero. We keep track of
      // whether or not the object was reachable. If we go to 0 rc and it
      // wasn't reachable, we free it. If we receive the object in a message,
      // mark it as reachable again.
      if(!object_reachable(obj_local))
      {
        chunk_t* chunk = (chunk_t*)pagemap_get(p);
        heap_free(chunk, p);
      }
    }
  }

  actorref_free(aref);
  return rc > 0;
}

size_t gc_rc(gc_t* gc)
{
  return gc->rc;
}

deltamap_t* gc_delta(gc_t* gc)
{
  deltamap_t* delta = gc->delta;
  gc->delta = NULL;
  return delta;
}

void gc_sendacquire(pony_ctx_t* ctx)
{
  size_t i = HASHMAP_BEGIN;
  actorref_t* aref;

  while((aref = actormap_next(&ctx->acquire, &i)) != NULL)
  {
    actormap_removeindex(&ctx->acquire, i);
    pony_sendp(ctx, actorref_actor(aref), ACTORMSG_ACQUIRE, aref);
  }

  actormap_destroy(&ctx->acquire);
  memset(&ctx->acquire, 0, sizeof(actormap_t));
}

void gc_sendrelease(pony_ctx_t* ctx, gc_t* gc)
{
  gc->delta = actormap_sweep(ctx, &gc->foreign, gc->mark, gc->delta);
}

void gc_register_final(pony_ctx_t* ctx, void* p, pony_final_fn final)
{
  if(!ctx->finalising)
  {
    // If we aren't finalising an actor, register the finaliser.
    gc_t* gc = actor_gc(ctx->current);
    objectmap_register_final(&gc->local, p, final, gc->mark);
    gc->finalisers++;
  } else {
    // Otherwise, put the finaliser on the gc stack.
    ctx->stack = gcstack_push(ctx->stack, p);
    ctx->stack = gcstack_push(ctx->stack, final);
  }
}

void gc_final(pony_ctx_t* ctx, gc_t* gc)
{
  if(gc->finalisers == 0)
    return;

  // Set the finalising flag.
  ctx->finalising = true;

  // Run all finalisers in the object map.
  objectmap_final(&gc->local);

  // Finalise any objects that were created during finalisation.
  pony_final_fn f;
  void *p;

  while(ctx->stack != NULL)
  {
    ctx->stack = gcstack_pop(ctx->stack, (void**)&f);
    ctx->stack = gcstack_pop(ctx->stack, &p);
    f(p);
  }

  ctx->finalising = false;
}

void gc_done(gc_t* gc)
{
  gc->mark++;
}

void gc_destroy(gc_t* gc)
{
  objectmap_destroy(&gc->local);
  actormap_destroy(&gc->foreign);

  if(gc->delta != NULL)
  {
    deltamap_free(gc->delta);
    gc->delta = NULL;
  }
}
