#include "trace.h"
#include "gc.h"
#include "../sched/scheduler.h"
#include "../sched/cpu.h"
#include "../actor/actor.h"
#include <assert.h>

void pony_gc_send(pony_ctx_t* ctx)
{
  assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_gc_sendobject;
  ctx->trace_actor = ponyint_gc_sendactor;

#ifdef USE_TELEMETRY
  ctx->tsc = ponyint_cpu_tick();
#endif
}

void pony_gc_recv(pony_ctx_t* ctx)
{
  assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_gc_recvobject;
  ctx->trace_actor = ponyint_gc_recvactor;

#ifdef USE_TELEMETRY
  ctx->tsc = ponyint_cpu_tick();
#endif
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

#ifdef USE_TELEMETRY
  ctx->time_in_send_scan += (ponyint_cpu_tick() - ctx->tsc);
#endif
}

void pony_recv_done(pony_ctx_t* ctx)
{
  ponyint_gc_handlestack(ctx);
  ponyint_gc_done(ponyint_actor_gc(ctx->current));

#ifdef USE_TELEMETRY
  ctx->time_in_recv_scan += (ponyint_cpu_tick() - ctx->tsc);
#endif
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

void ponyint_serialise_size_begin(pony_ctx_t* ctx)
{
  assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_serialise_object_size;
  ctx->trace_actor = ponyint_serialise_actor_size;
}

void ponyint_serialise_size_done(pony_ctx_t* ctx)
{
  ponyint_gc_handlestack(ctx);
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
