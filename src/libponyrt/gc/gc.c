#include "gc.h"
#include "../actor/actor.h"
#include "../ds/stack.h"
#include "../mem/pagemap.h"
#include <string.h>
#include <assert.h>

DECLARE_STACK(gcstack, void);
DEFINE_STACK(gcstack, void);

static __pony_thread_local actormap_t acquire;
static __pony_thread_local gcstack_t* stack;

static void acquire_actor(pony_actor_t* actor)
{
  actorref_t* aref = actormap_getorput(&acquire, actor, 0);
  actorref_inc_more(aref);
}

static void acquire_object(pony_actor_t* actor, void* address)
{
  actorref_t* aref = actormap_getorput(&acquire, actor, 0);
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

void gc_sendobject(pony_actor_t* current, heap_t* heap, gc_t* gc,
  void* p, pony_trace_fn f)
{
  chunk_t* chunk = (chunk_t*)pagemap_get(p);

  // don't gc memory that wasn't pony_allocated
  if(chunk == NULL)
    return;

  pony_actor_t* actor = heap_owner(chunk);
  heap_base(chunk, &p);

  if(actor == current)
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
        stack = gcstack_push(stack, p);
        stack = gcstack_push(stack, f);
      }
    }
  } else {
    // get the actor
    actorref_t* aref = actormap_getactor(&gc->foreign, actor);
    assert(aref != NULL);

    if(!actorref_marked(aref, gc->mark))
    {
      // dec. if we can't, we need to build an acquire message
      if(!actorref_dec(aref))
      {
        actorref_inc_more(aref);
        acquire_actor(actor);
      }

      actorref_mark(aref, gc->mark);
      gc->delta = deltamap_update(gc->delta,
        actorref_actor(aref), actorref_rc(aref));
    }

    // get the object
    object_t* obj = actorref_getobject(aref, p);
    assert(obj != NULL);

    if(!object_marked(obj, gc->mark))
    {
      // dec. if we can't, we need to build an acquire message
      if(!object_dec(obj))
      {
        object_inc_more(obj);
        acquire_object(actor, p);
      }

      object_mark(obj, gc->mark);

      if(f != NULL)
      {
        stack = gcstack_push(stack, p);
        stack = gcstack_push(stack, f);
      }
    }
  }
}

void gc_recvobject(pony_actor_t* current, heap_t* heap, gc_t* gc,
  void* p, pony_trace_fn f)
{
  chunk_t* chunk = (chunk_t*)pagemap_get(p);

  // don't gc memory that wasn't pony_allocated
  if(chunk == NULL)
    return;

  pony_actor_t* actor = heap_owner(chunk);
  size_t objsize = heap_base(chunk, &p);

  if(actor == current)
  {
    current_actor_dec(gc);

    // get the object
    object_t* obj = objectmap_getobject(&gc->local, p);
    assert(obj != NULL);

    if(!object_marked(obj, gc->mark))
    {
      // dec, mark and recurse
      object_dec(obj);
      object_mark(obj, gc->mark);

      if(f != NULL)
      {
        stack = gcstack_push(stack, p);
        stack = gcstack_push(stack, f);
      }
    }
  } else {
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
        heap_used(heap, objsize);

      // inc, mark and recurse
      object_inc(obj);
      object_mark(obj, gc->mark);

      if(f != NULL)
      {
        stack = gcstack_push(stack, p);
        stack = gcstack_push(stack, f);
      }
    }
  }
}

void gc_markobject(pony_actor_t* current, heap_t* heap, gc_t* gc,
  void* p, pony_trace_fn f)
{
  chunk_t* chunk = (chunk_t*)pagemap_get(p);

  // don't gc memory that wasn't pony_allocated
  if(chunk == NULL)
    return;

  pony_actor_t* actor = heap_owner(chunk);
  size_t objsize = heap_base(chunk, &p);

  if(actor == current)
  {
    if(f != NULL)
    {
      // mark in our heap and recurse if it wasn't already marked
      if(!heap_mark(chunk, p))
      {
        stack = gcstack_push(stack, p);
        stack = gcstack_push(stack, f);
      }
    } else {
      // no recurse function, so do a shallow mark. if the same address is
      // later marked with a recurse function, it will recurse.
      heap_mark_shallow(chunk, p);
    }
  } else {
    // mark the owner
    actorref_t* aref = actormap_getactor(&gc->foreign, actor);
    actorref_mark(aref, gc->mark);

    // get the object
    object_t* obj = actorref_getobject(aref, p);

    if(!object_marked(obj, gc->mark))
    {
      // add to heap used size
      heap_used(heap, objsize);

      // mark and recurse
      object_mark(obj, gc->mark);

      if(f != NULL)
      {
        stack = gcstack_push(stack, p);
        stack = gcstack_push(stack, f);
      }
    }
  }
}

void gc_sendactor(pony_actor_t* current, gc_t* gc, pony_actor_t* actor)
{
  if(actor == current)
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
        acquire_actor(actor);
      }

      actorref_mark(aref, gc->mark);
      gc->delta = deltamap_update(gc->delta,
        actorref_actor(aref), actorref_rc(aref));
    }
  }
}

void gc_recvactor(pony_actor_t* current, gc_t* gc, pony_actor_t* actor)
{
  if(actor == current)
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
    }
  }
}

void gc_markactor(pony_actor_t* current, gc_t* gc, pony_actor_t* actor)
{
  if(actor == current)
    return;

  actorref_t* aref = actormap_getactor(&gc->foreign, actor);
  actorref_mark(aref, gc->mark);
}

void gc_createactor(gc_t* gc, pony_actor_t* actor)
{
  actorref_t* aref = actormap_getorput(&gc->foreign, actor, gc->mark);
  actorref_inc_more(aref);
  gc->delta = deltamap_update(gc->delta,
    actorref_actor(aref), actorref_rc(aref));
}

void gc_handlestack()
{
  pony_trace_fn f;
  void *p;

  while(stack != NULL)
  {
    stack = gcstack_pop(stack, (void**)&f);
    stack = gcstack_pop(stack, &p);
    f(p);
  }
}

void gc_mark(gc_t* gc)
{
  objectmap_mark(&gc->local);
}

void gc_sweep(gc_t* gc)
{
  gc->delta = actormap_sweep(&gc->foreign, gc->mark, gc->delta);
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
    object_t* obj_local = objectmap_getobject(&gc->local, object_address(obj));
    object_dec_some(obj_local, object_rc(obj));
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

void gc_sendacquire()
{
  size_t i = HASHMAP_BEGIN;
  actorref_t* aref;

  while((aref = actormap_next(&acquire, &i)) != NULL)
  {
    actormap_removeindex(&acquire, i);
    pony_sendp(actorref_actor(aref), ACTORMSG_ACQUIRE, aref);
  }

  actormap_destroy(&acquire);
  memset(&acquire, 0, sizeof(actormap_t));
}

void gc_done(gc_t* gc)
{
  gc->mark++;
}
