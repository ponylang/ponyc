#ifndef gc_gc_h
#define gc_gc_h

#include "objectmap.h"
#include "actormap.h"
#include "delta.h"
#include "../mem/heap.h"
#include "../ds/stack.h"
#include <pony.h>
#include <platform.h>

#define GC_INC_MORE 256

PONY_EXTERN_C_BEGIN

typedef struct gc_t
{
  uint32_t mark;
  uint32_t rc_mark;
  size_t rc;
  size_t finalisers;
  objectmap_t local;
  actormap_t foreign;
  deltamap_t* delta;
} gc_t;

DECLARE_STACK(gcstack, void);

void gc_sendobject(pony_ctx_t* ctx, void* p, pony_trace_fn f);

void gc_recvobject(pony_ctx_t* ctx, void* p, pony_trace_fn f);

void gc_markobject(pony_ctx_t* ctx, void* p, pony_trace_fn f);

void gc_sendactor(pony_ctx_t* ctx, pony_actor_t* actor);

void gc_recvactor(pony_ctx_t* ctx, pony_actor_t* actor);

void gc_markactor(pony_ctx_t* ctx, pony_actor_t* actor);

void gc_createactor(pony_actor_t* current, pony_actor_t* actor);

void gc_handlestack(pony_ctx_t* ctx);

void gc_sweep(pony_ctx_t* ctx, gc_t* gc);

void gc_sendacquire(pony_ctx_t* ctx);

void gc_sendrelease(pony_ctx_t* ctx, gc_t* gc);

bool gc_acquire(gc_t* gc, actorref_t* aref);

bool gc_release(gc_t* gc, actorref_t* aref);

size_t gc_rc(gc_t* gc);

deltamap_t* gc_delta(gc_t* gc);

void gc_register_final(pony_ctx_t* ctx, void* p, pony_final_fn final);

void gc_final(pony_ctx_t* ctx, gc_t* gc);

void gc_done(gc_t* gc);

void gc_destroy(gc_t* gc);

PONY_EXTERN_C_END

#endif
