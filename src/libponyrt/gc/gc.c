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
  aref->rc += GC_INC_MORE;
}

static void acquire_object(pony_ctx_t* ctx, pony_actor_t* actor, void* address,
  bool immutable)
{
  actorref_t* aref = actormap_getorput(&ctx->acquire, actor, 0);
  object_t* obj = actorref_getorput(aref, address, 0);

  obj->rc += GC_INC_MORE;
  obj->immutable = immutable;
}

static void recurse(pony_ctx_t* ctx, void* p, void* f)
{
  if(f != NULL)
  {
    ctx->stack = gcstack_push(ctx->stack, p);
    ctx->stack = gcstack_push(ctx->stack, f);
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

  gc->delta = deltamap_update(gc->delta, aref->actor, aref->rc);
}

static void recv_remote_actor(pony_ctx_t* ctx, gc_t* gc, actorref_t* aref)
{
  if(aref->mark == gc->mark)
    return;

  aref->mark = gc->mark;
  aref->rc++;
  gc->delta = deltamap_update(gc->delta, aref->actor, aref->rc);
  heap_used(actor_heap(ctx->current), GC_ACTOR_HEAP_EQUIV);
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
    gc->delta = deltamap_update(gc->delta, aref->actor, aref->rc);
  }

  heap_used(actor_heap(ctx->current), GC_ACTOR_HEAP_EQUIV);
}

static void send_local_object(pony_ctx_t* ctx, void* p, pony_trace_fn f,
  bool immutable)
{
  gc_t* gc = actor_gc(ctx->current);
  object_t* obj = objectmap_getorput(&gc->local, p, gc->mark);

  if(obj->mark == gc->mark)
    return;

  // Implicitly send the owner.
  send_local_actor(gc);

  // Inc, mark, and recurse if not immutable.
  obj->rc++;
  obj->mark = gc->mark;

  if(immutable)
    obj->immutable = true;

  if(!obj->immutable)
    recurse(ctx, p, f);
}

static void recv_local_object(pony_ctx_t* ctx, void* p, pony_trace_fn f,
  bool immutable)
{
  // get the object
  gc_t* gc = actor_gc(ctx->current);
  object_t* obj = objectmap_getobject(&gc->local, p);
  assert(obj != NULL);

  if(obj->mark == gc->mark)
    return;

  // Implicitly receive the owner.
  recv_local_actor(gc);

  // Dec, mark and recurse. Mark as reachable from the actor, since a
  // release message for this object could arrive before a gc pass.
  obj->rc--;
  obj->mark = gc->mark;
  obj->reachable = true;

  if(immutable)
    obj->immutable = true;

  if(!obj->immutable)
    recurse(ctx, p, f);
}

static void mark_local_object(pony_ctx_t* ctx, chunk_t* chunk, void* p,
  pony_trace_fn f)
{
  if(f != NULL)
  {
    // Mark in our heap and recurse if it wasn't already marked.
    if(!heap_mark(chunk, p))
      recurse(ctx, p, f);
  } else {
    // No recurse function, so do a shallow mark. If the same address is
    // later marked with a recurse function, it will recurse.
    heap_mark_shallow(chunk, p);
  }
}

static void send_remote_object(pony_ctx_t* ctx, pony_actor_t* actor,
  void* p, pony_trace_fn f, bool immutable)
{
  gc_t* gc = actor_gc(ctx->current);
  actorref_t* aref = actormap_getorput(&gc->foreign, actor, gc->mark);
  object_t* obj = actorref_getorput(aref, p, gc->mark);

  if(obj->mark == gc->mark)
    return;

  // Implicitly send the owner.
  send_remote_actor(ctx, gc, aref);

  // Mark the object.
  obj->mark = gc->mark;

  if(immutable && !obj->immutable && (obj->rc > 0))
  {
    // If we received the object as not immutable (it's not marked as immutable
    // and it has an RC > 0), but we are now sending it as immutable, we need
    // to acquire it in order to inform the owner it is now immutable. But we
    // also need to continue tracing, to protect the contents until the owner
    // has received the acquire messages.
    obj->rc += (GC_INC_MORE - 1);
    obj->immutable = true;
    acquire_object(ctx, actor, p, true);

    immutable = false;
  } else if(obj->rc <= 1) {
    // Add to the acquire message and decrement.
    obj->rc += (GC_INC_MORE - 1);
    acquire_object(ctx, actor, p, obj->immutable);
  } else {
    // Decrement.
    obj->rc--;
  }

  if(!immutable)
    recurse(ctx, p, f);
}

static void recv_remote_object(pony_ctx_t* ctx, pony_actor_t* actor,
  void* p, pony_trace_fn f, bool immutable, chunk_t* chunk)
{
  gc_t* gc = actor_gc(ctx->current);
  actorref_t* aref = actormap_getorput(&gc->foreign, actor, gc->mark);
  object_t* obj = actorref_getorput(aref, p, gc->mark);

  if(obj->mark == gc->mark)
    return;

  // Implicitly receive the owner.
  recv_remote_actor(ctx, gc, aref);

  // If this is our first reference, add to our heap used size.
  if(obj->rc == 0)
    heap_used(actor_heap(ctx->current), heap_size(chunk));

  // Inc, mark and recurse.
  obj->rc++;
  obj->mark = gc->mark;

  if(immutable)
    obj->immutable = true;

  if(!obj->immutable)
    recurse(ctx, p, f);
}

static void mark_remote_object(pony_ctx_t* ctx, pony_actor_t* actor,
  void* p, pony_trace_fn f, bool immutable, chunk_t* chunk)
{
  gc_t* gc = actor_gc(ctx->current);
  actorref_t* aref = actormap_getorput(&gc->foreign, actor, gc->mark);
  object_t* obj = actorref_getorput(aref, p, gc->mark);

  if(obj->mark == gc->mark)
    return;

  // Implicitly mark the owner.
  mark_remote_actor(ctx, gc, aref);

  // Mark the object.
  obj->mark = gc->mark;

  // Add to heap used size.
  heap_used(actor_heap(ctx->current), heap_size(chunk));

  if(immutable && !obj->immutable && (obj->rc > 0))
  {
    // If we received the object as not immutable (it's not marked as immutable
    // and it has an RC > 0), but we are now marking it as immutable, we need
    // to acquire it in order to inform the owner it is now immutable. But we
    // also need to continue tracing, to protect the contents until the owner
    // has received the acquire messages.
    obj->rc += (GC_INC_MORE - 1);
    obj->immutable = true;
    acquire_object(ctx, actor, p, true);

    immutable = false;
  } else if(obj->rc == 0) {
    // If we haven't seen this object, it's an object that is reached from
    // another immutable object we received. Invent some references to this
    // object and acquire it.
    if(immutable)
      obj->immutable = true;

    obj->rc += GC_INC_MORE;
    acquire_object(ctx, actor, p, obj->immutable);
  } else {
    // TODO: ?
  }

  if(!immutable)
    recurse(ctx, p, f);




  obj->mark = gc->mark;

  if(obj->rc == 0)
  {
    obj->rc += GC_INC_MORE;
    acquire_object(ctx, actor, p, );
    gc->delta = deltamap_update(gc->delta, aref->actor, aref->rc);
  }

  // TODO: if we weren't received as immutable, still have to trace
  // and tell the owner its immutable now, so the receiver's partial trace
  // doesn't cause errors
  immutable = object_immutable(obj, immutable);

  if(!immutable)
    recurse(ctx, p, f);
}

void gc_sendobject(pony_ctx_t* ctx, void* p, pony_trace_fn f, bool immutable)
{
  chunk_t* chunk = (chunk_t*)pagemap_get(p);

  // Don't gc memory that wasn't pony_allocated, but do recurse.
  if(chunk == NULL)
  {
    recurse(ctx, p, f);
    return;
  }

  pony_actor_t* actor = heap_owner(chunk);

  if(actor == ctx->current)
    send_local_object(ctx, p, f, immutable);
  else
    send_remote_object(ctx, actor, p, f, immutable);
}

void gc_recvobject(pony_ctx_t* ctx, void* p, pony_trace_fn f, bool immutable)
{
  chunk_t* chunk = (chunk_t*)pagemap_get(p);

  // Don't gc memory that wasn't pony_allocated, but do recurse.
  if(chunk == NULL)
  {
    recurse(ctx, p, f);
    return;
  }

  pony_actor_t* actor = heap_owner(chunk);

  if(actor == ctx->current)
    recv_local_object(ctx, p, f, immutable);
  else
    recv_remote_object(ctx, actor, p, f, immutable, chunk);
}

void gc_markobject(pony_ctx_t* ctx, void* p, pony_trace_fn f, bool immutable)
{
  chunk_t* chunk = (chunk_t*)pagemap_get(p);

  // Don't gc memory that wasn't pony_allocated, but do recurse.
  if(chunk == NULL)
  {
    recurse(ctx, p, f);
    return;
  }

  pony_actor_t* actor = heap_owner(chunk);

  if(actor == ctx->current)
    mark_local_object(ctx, chunk, p, f);
  else
    mark_remote_object(ctx, actor, p, f, immutable, chunk);
}

void gc_sendactor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  gc_t* gc = actor_gc(ctx->current);

  if(actor == ctx->current)
  {
    send_local_actor(gc);
  } else {
    actorref_t* aref = actormap_getorput(&gc->foreign, actor, gc->mark);
    send_remote_actor(ctx, gc, aref);
  }
}

void gc_recvactor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  gc_t* gc = actor_gc(ctx->current);

  if(actor == ctx->current)
  {
    recv_local_actor(gc);
  } else {
    actorref_t* aref = actormap_getorput(&gc->foreign, actor, gc->mark);
    recv_remote_actor(ctx, gc, aref);
  }
}

void gc_markactor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  if(actor == ctx->current)
    return;

  gc_t* gc = actor_gc(ctx->current);
  actorref_t* aref = actormap_getactor(&gc->foreign, actor);
  mark_remote_actor(ctx, gc, aref);
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
    recurse(ctx, p, final);
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
