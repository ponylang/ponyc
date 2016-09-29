#include "trace.h"
#include "gc.h"
#include "../sched/scheduler.h"
#include "../sched/cpu.h"
#include "../actor/actor.h"
#include <assert.h>
#include <dtrace.h>

void pony_gc_send(pony_ctx_t* ctx)
{
  assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_gc_sendobject;
  ctx->trace_actor = ponyint_gc_sendactor;

  DTRACE1(GC_SEND_START, (uintptr_t)ctx->scheduler);
}

void pony_gc_recv(pony_ctx_t* ctx)
{
  assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_gc_recvobject;
  ctx->trace_actor = ponyint_gc_recvactor;

  DTRACE1(GC_RECV_START, (uintptr_t)ctx->scheduler);
}

void ponyint_gc_mark(pony_ctx_t* ctx)
{
  assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_gc_markobject;
  ctx->trace_actor = ponyint_gc_markactor;
}

void pony_gc_acquire(pony_ctx_t* ctx)
{
  assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_gc_acquireobject;
  ctx->trace_actor = ponyint_gc_acquireactor;
}

void pony_gc_release(pony_ctx_t* ctx)
{
  assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_gc_releaseobject;
  ctx->trace_actor = ponyint_gc_releaseactor;
}

void pony_send_done(pony_ctx_t* ctx)
{
  ponyint_gc_handlestack(ctx);
  ponyint_gc_sendacquire(ctx);
  ponyint_gc_done(ponyint_actor_gc(ctx->current));

  DTRACE1(GC_SEND_END, (uintptr_t)ctx->scheduler);
}

void pony_recv_done(pony_ctx_t* ctx)
{
  ponyint_gc_handlestack(ctx);
  ponyint_gc_done(ponyint_actor_gc(ctx->current));

  DTRACE1(GC_RECV_END, (uintptr_t)ctx->scheduler);
}

void ponyint_mark_done(pony_ctx_t* ctx)
{
  ponyint_gc_markimmutable(ctx, ponyint_actor_gc(ctx->current));
  ponyint_gc_handlestack(ctx);
  ponyint_gc_sendacquire(ctx);
  ponyint_gc_sweep(ctx, ponyint_actor_gc(ctx->current));
  ponyint_gc_done(ponyint_actor_gc(ctx->current));
}

void pony_acquire_done(pony_ctx_t* ctx)
{
  ponyint_gc_handlestack(ctx);
  ponyint_gc_sendacquire(ctx);
  ponyint_gc_done(ponyint_actor_gc(ctx->current));
}

void pony_release_done(pony_ctx_t* ctx)
{
  ponyint_gc_handlestack(ctx);
  ponyint_gc_sendrelease_manual(ctx);
  ponyint_gc_done(ponyint_actor_gc(ctx->current));
}

void pony_trace(pony_ctx_t* ctx, void* p)
{
  ctx->trace_object(ctx, p, NULL, PONY_TRACE_OPAQUE);
}

void pony_traceknown(pony_ctx_t* ctx, void* p, pony_type_t* t, int m)
{
  if(t->dispatch != NULL)
  {
    ctx->trace_actor(ctx, (pony_actor_t*)p);
  } else {
    ctx->trace_object(ctx, p, t, m);
  }
}

void pony_traceunknown(pony_ctx_t* ctx, void* p, int m)
{
  pony_type_t* t = *(pony_type_t**)p;

  if(t->dispatch != NULL)
  {
    ctx->trace_actor(ctx, (pony_actor_t*)p);
  } else {
    ctx->trace_object(ctx, p, t, m);
  }
}
