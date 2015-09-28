#include "trace.h"
#include "gc.h"
#include "../sched/scheduler.h"
#include "../actor/actor.h"
#include <assert.h>

void pony_gc_send(pony_ctx_t* ctx)
{
  assert(ctx->stack == NULL);
  ctx->trace_object = gc_sendobject;
  ctx->trace_actor = gc_sendactor;
}

void pony_gc_recv(pony_ctx_t* ctx)
{
  assert(ctx->stack == NULL);
  ctx->trace_object = gc_recvobject;
  ctx->trace_actor = gc_recvactor;
}

void pony_gc_mark(pony_ctx_t* ctx)
{
  assert(ctx->stack == NULL);
  ctx->trace_object = gc_markobject;
  ctx->trace_actor = gc_markactor;
}

void pony_send_done(pony_ctx_t* ctx)
{
  gc_handlestack(ctx);
  gc_sendacquire();
  gc_done(actor_gc(ctx->current));
}

void pony_recv_done(pony_ctx_t* ctx)
{
  gc_handlestack(ctx);
  gc_done(actor_gc(ctx->current));
}

void pony_trace(pony_ctx_t* ctx, void* p)
{
  ctx->trace_object(NULL, ctx->current, actor_heap(ctx->current),
    actor_gc(ctx->current), p, NULL);
}

void pony_traceactor(pony_ctx_t* ctx, pony_actor_t* p)
{
  ctx->trace_actor(ctx->current, actor_heap(ctx->current),
    actor_gc(ctx->current), p);
}

void pony_traceobject(pony_ctx_t* ctx, void* p, pony_trace_fn f)
{
  ctx->stack = ctx->trace_object(ctx->stack, ctx->current,
    actor_heap(ctx->current), actor_gc(ctx->current), p, f);
}

void pony_traceunknown(pony_ctx_t* ctx, void* p)
{
  pony_type_t* type = *(pony_type_t**)p;

  if(type->dispatch != NULL)
  {
    ctx->trace_actor(ctx->current, actor_heap(ctx->current),
      actor_gc(ctx->current), (pony_actor_t*)p);
  } else {
    ctx->stack = ctx->trace_object(ctx->stack, ctx->current,
      actor_heap(ctx->current), actor_gc(ctx->current), p, type->trace);
  }
}

void pony_trace_tag_or_actor(pony_ctx_t* ctx, void* p)
{
  pony_type_t* type = *(pony_type_t**)p;

  if(type->dispatch != NULL)
  {
    ctx->trace_actor(ctx->current, actor_heap(ctx->current),
      actor_gc(ctx->current), (pony_actor_t*)p);
  } else {
    ctx->trace_object(NULL, ctx->current, actor_heap(ctx->current),
      actor_gc(ctx->current), p, NULL);
  }
}
