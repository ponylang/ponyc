#include "trace.h"
#include "gc.h"
#include "../sched/scheduler.h"
#include "../sched/cpu.h"
#include "../actor/actor.h"
#include "../tracing/tracing.h"
#include "ponyassert.h"
#include <dtrace.h>

PONY_API void pony_gc_send(pony_ctx_t* ctx, pony_actor_t* to)
{
  pony_assert(ctx->current != NULL);
  pony_assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_gc_sendobject;
  ctx->trace_actor = ponyint_gc_sendactor;

  // Record the destination of the message being traced so the send-trace can
  // recognise a self-send and pin its objects against the local sweep. Cleared
  // by pony_send_done; updated per message by pony_send_next when several
  // sends are traced in one round (see pony_send_next). Carried as an argument
  // (not a separate call) precisely so it survives the message-merge optimiser:
  // that pass rewrites a later message's pony_gc_send into pony_send_next via
  // setCalledFunction, which keeps the destination operand.
  ctx->msg_target = to;

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

PONY_API void pony_send_done(pony_ctx_t* ctx)
{
  pony_assert(ctx->current != NULL);
  ponyint_gc_handlestack(ctx);
  ponyint_gc_sendacquire(ctx);
  ponyint_gc_done(ponyint_actor_gc(ctx->current));

  // Clear the self-send destination now the send round is finished so it never
  // lingers past the trace. Defensive: every pony_gc_send sets it afresh (the
  // ASIO and bootstrap paths pass NULL), so nothing reads a stale value.
  ctx->msg_target = NULL;

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

PONY_API void pony_send_next(pony_ctx_t* ctx, pony_actor_t* to)
{
  pony_assert(ctx->current != NULL);
  // Finish tracing the previous message FIRST: handlestack drains that message's
  // pushed references while msg_target still names its destination. Only then
  // switch msg_target to this message's destination, so each message's objects
  // (top-level and recursively-reached) are classified against the right
  // destination even when the message-merge optimiser folds several sends into
  // one trace round.
  ponyint_gc_handlestack(ctx);
  ctx->msg_target = to;
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
