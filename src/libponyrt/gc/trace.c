#include "trace.h"
#include "gc.h"
#include "../actor/actor.h"

typedef gcstack_t* (*trace_object_fn)(gcstack_t*, pony_actor_t* current,
  heap_t* heap, gc_t* gc, void* p, pony_trace_fn f);

typedef void (*trace_actor_fn)(pony_actor_t* current, heap_t* heap, gc_t* gc,
  pony_actor_t* actor);

static __pony_thread_local trace_object_fn trace_object;
static __pony_thread_local trace_actor_fn trace_actor;

void pony_gc_send()
{
  trace_object = gc_sendobject;
  trace_actor = gc_sendactor;
}

void pony_gc_recv()
{
  trace_object = gc_recvobject;
  trace_actor = gc_recvactor;
}

void pony_gc_mark()
{
  trace_object = gc_markobject;
  trace_actor = gc_markactor;
}

void pony_send_done(void* stack, pony_actor_t* current)
{
  gc_handlestack((gcstack_t*)stack, current);
  gc_sendacquire();
  gc_done(actor_gc(current));
}

void pony_recv_done(void* stack, pony_actor_t* current)
{
  gc_handlestack((gcstack_t*)stack, current);
  gc_done(actor_gc(current));
}

void pony_trace(pony_actor_t* current, void* p)
{
  trace_object(NULL, current, actor_heap(current), actor_gc(current), p, NULL);
}

void pony_traceactor(pony_actor_t* current, pony_actor_t* p)
{
  trace_actor(current, actor_heap(current), actor_gc(current), p);
}

void* pony_traceobject(void* stack, pony_actor_t* current, void* p,
  pony_trace_fn f)
{
  return trace_object((gcstack_t*)stack, current, actor_heap(current),
    actor_gc(current), p, f);
}

void* pony_traceunknown(void* stack, pony_actor_t* current, void* p)
{
  pony_type_t* type = *(pony_type_t**)p;

  if(type->dispatch != NULL)
  {
    trace_actor(current, actor_heap(current), actor_gc(current),
      (pony_actor_t*)p);

    return stack;
  }

  return trace_object((gcstack_t*)stack, current, actor_heap(current),
    actor_gc(current), p, type->trace);
}

void pony_trace_tag_or_actor(pony_actor_t* current, void* p)
{
  pony_type_t* type = *(pony_type_t**)p;

  if(type->dispatch != NULL)
  {
    trace_actor(current, actor_heap(current), actor_gc(current),
      (pony_actor_t*)p);
  } else {
    trace_object(NULL, current, actor_heap(current), actor_gc(current),
      p, NULL);
  }
}
