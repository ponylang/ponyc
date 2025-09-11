#include "trace.h"
#include "gc.h"
#include "../sched/scheduler.h"
#include "../sched/cpu.h"
#include "../actor/actor.h"
#include "../tracing/tracing.h"
#include "ponyassert.h"
#include <dtrace.h>

PONY_API void pony_gc_send(pony_ctx_t* ctx)
{
  pony_assert(ctx->current != NULL);
  pony_assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_gc_sendobject;
  ctx->trace_actor = ponyint_gc_sendactor;

  DTRACE2(GC_SEND_START, (uintptr_t)ctx->scheduler, (uintptr_t)ctx->current);
}

PONY_API void pony_gc_recv(pony_ctx_t* ctx)
{
  pony_assert(ctx->current != NULL);
  pony_assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_gc_recvobject;
  ctx->trace_actor = ponyint_gc_recvactor;

  DTRACE2(GC_RECV_START, (uintptr_t)ctx->scheduler, (uintptr_t)ctx->current);
}

void ponyint_gc_mark(pony_ctx_t* ctx)
{
  pony_assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_gc_markobject;
  ctx->trace_actor = ponyint_gc_markactor;
}

PONY_API void pony_gc_acquire(pony_ctx_t* ctx)
{
  pony_assert(ctx->current != NULL);
  pony_assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_gc_acquireobject;
  ctx->trace_actor = ponyint_gc_acquireactor;
}

PONY_API void pony_gc_release(pony_ctx_t* ctx)
{
  pony_assert(ctx->current != NULL);
  pony_assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_gc_releaseobject;
  ctx->trace_actor = ponyint_gc_releaseactor;
}

PONY_API void pony_send_done(pony_ctx_t* ctx)
{
  pony_assert(ctx->current != NULL);
  ponyint_gc_handlestack(ctx);
  ponyint_gc_sendacquire(ctx);
  ponyint_gc_done(ponyint_actor_gc(ctx->current));

  DTRACE2(GC_SEND_END, (uintptr_t)ctx->scheduler, (uintptr_t)ctx->current);
}

PONY_API void pony_recv_done(pony_ctx_t* ctx)
{
  pony_assert(ctx->current != NULL);
  ponyint_gc_handlestack(ctx);
  ponyint_gc_done(ponyint_actor_gc(ctx->current));

  DTRACE2(GC_RECV_END, (uintptr_t)ctx->scheduler, (uintptr_t)ctx->current);
}

void ponyint_mark_done(pony_ctx_t* ctx)
{
  ponyint_gc_markimmutable(ctx, ponyint_actor_gc(ctx->current));
  ponyint_gc_handlestack(ctx);

#ifdef USE_RUNTIMESTATS
  uint64_t used_cpu = ponyint_sched_cpu_used(ctx);
  ctx->schedulerstats.actor_gc_mark_cpu += used_cpu;
  ctx->current->actorstats.gc_mark_cpu += used_cpu;
#endif

  TRACING_ACTOR_GC_MARK_END(ctx->current);
  ponyint_gc_sendacquire(ctx);
  TRACING_ACTOR_GC_SWEEP_START(ctx->current);
  ponyint_gc_sweep(ctx, ponyint_actor_gc(ctx->current));
  ponyint_gc_done(ponyint_actor_gc(ctx->current));
}

PONY_API void pony_acquire_done(pony_ctx_t* ctx)
{
  pony_assert(ctx->current != NULL);
  ponyint_gc_handlestack(ctx);
  ponyint_gc_sendacquire(ctx);
  ponyint_gc_done(ponyint_actor_gc(ctx->current));
}

PONY_API void pony_release_done(pony_ctx_t* ctx)
{
  pony_assert(ctx->current != NULL);
  ponyint_gc_handlestack(ctx);
  ponyint_gc_sendrelease_manual(ctx);
  ponyint_gc_done(ponyint_actor_gc(ctx->current));
}

PONY_API void pony_send_next(pony_ctx_t* ctx)
{
  pony_assert(ctx->current != NULL);
  ponyint_gc_handlestack(ctx);
  ponyint_gc_done(ponyint_actor_gc(ctx->current));
}

PONY_API void pony_trace(pony_ctx_t* ctx, void* p)
{
  ctx->trace_object(ctx, p, NULL, PONY_TRACE_OPAQUE);
}

PONY_API void pony_traceknown(pony_ctx_t* ctx, void* p, pony_type_t* t, int m)
{
  if(t->dispatch != NULL)
  {
    ctx->trace_actor(ctx, (pony_actor_t*)p);
  } else {
    ctx->trace_object(ctx, p, t, m);
  }
}

PONY_API void pony_traceunknown(pony_ctx_t* ctx, void* p, int m)
{
  pony_type_t* t = *(pony_type_t**)p;

  if(t->dispatch != NULL)
  {
    ctx->trace_actor(ctx, (pony_actor_t*)p);
  } else {
    ctx->trace_object(ctx, p, t, m);
  }
}
