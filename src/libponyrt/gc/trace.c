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

void pony_trace(pony_ctx_t* ctx, void* p)
{
  ctx->trace_object(ctx, p, NULL, false);
}

void pony_traceactor(pony_ctx_t* ctx, pony_actor_t* p)
{
  ctx->trace_actor(ctx, p);
}

void pony_traceobject(pony_ctx_t* ctx, void* p, pony_trace_fn f, int immutable)
{
  ctx->trace_object(ctx, p, f, immutable != 0);
}

void pony_traceunknown(pony_ctx_t* ctx, void* p, int immutable)
{
  pony_type_t* type = *(pony_type_t**)p;

  if(type->dispatch != NULL)
  {
    ctx->trace_actor(ctx, (pony_actor_t*)p);
  } else {
    ctx->trace_object(ctx, p, type->trace, immutable != 0);
  }
}

void pony_trace_tag_or_actor(pony_ctx_t* ctx, void* p)
{
  pony_type_t* type = *(pony_type_t**)p;

  if(type->dispatch != NULL)
  {
    ctx->trace_actor(ctx, (pony_actor_t*)p);
  } else {
    ctx->trace_object(ctx, p, NULL, false);
  }
}
