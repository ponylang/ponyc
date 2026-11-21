#include "marklocal.h"
#include "gc.h"
#include "frames.h"
#include "../actor/actor.h"
#include "../mem/pagemap.h"
#include "../mem/pool.h"
#include "../sched/scheduler.h"
#include "../tracing/tracing.h"
#include <errno.h>
#include <string.h>
#ifdef PLATFORM_IS_WINDOWS
#  include <winsock2.h>
#endif

PONY_EXTERN_C_BEGIN

typedef struct local_ref_t
{
  void* address;
  chunk_t* chunk;
  pony_type_t* type;
} local_ref_t;

typedef struct local_chunk_t
{
  chunk_t* chunk;
  uint32_t slots;
  uint32_t deep;
} local_chunk_t;

typedef struct local_mark_t
{
  pony_ctx_t ctx;
  hashmap_t refs;
  local_chunk_t* chunks;
  size_t count;
  size_t capacity;
  size_t foreign_charge;
  bool refused;
  int saved_errno;
#ifdef USE_RUNTIMESTATS
  pony_ctx_t* scheduler_ctx;
#endif
#ifdef PLATFORM_IS_WINDOWS
  DWORD saved_error;
#endif
} local_mark_t;

static bool same_address(void* a, void* b)
{
  local_ref_t* left = (local_ref_t*)a;
  local_ref_t* right = (local_ref_t*)b;
  return left->address == right->address && left->type == right->type;
}

static void free_ref(void* ref)
{
  POOL_FREE(local_ref_t, ref);
}

static bool mark_chunk(local_mark_t* mark, chunk_t* chunk, uint32_t* index,
  uint32_t slot, bool readable)
{
  local_chunk_t* ref;
  if(*index < mark->count && mark->chunks[*index].chunk == chunk)
  {
    ref = &mark->chunks[*index];
    if(((readable ? ref->deep : ref->slots) & slot) != 0)
      return false;
  } else {
    if(mark->count == mark->capacity)
    {
      if(mark->capacity > SIZE_MAX / 2 / sizeof(*ref) ||
        mark->count >= UINT32_MAX)
      {
        mark->refused = true;
        return false;
      }
      size_t next = mark->capacity == 0 ? 32 : mark->capacity * 2;
      mark->chunks = (local_chunk_t*)ponyint_pool_realloc_size(
        mark->capacity * sizeof(*ref), next * sizeof(*ref), mark->chunks);
      mark->capacity = next;
    }
    ref = &mark->chunks[mark->count];
    ref->chunk = chunk;
    ref->slots = ref->deep = 0;
    *index = (uint32_t)mark->count++;
  }
  ref->slots |= slot;
  if(readable)
    ref->deep |= slot;
  return true;
}

static void trace_object(pony_ctx_t* ctx, void* p, pony_type_t* type, int mode)
{
  local_mark_t* mark = (local_mark_t*)ctx;

  if(p == NULL || mark->refused)
    return;

  pony_actor_t* owner = NULL;
  chunk_t* chunk = ponyint_pagemap_get(p, &owner);
  if(chunk != NULL && owner != ctx->current)
  {
    mark->refused = true;
    return;
  }

  bool recurse = mode != PONY_TRACE_OPAQUE && type != NULL &&
    type->trace != NULL;
  if(chunk == NULL && !recurse)
    return;

  bool readable = mode != PONY_TRACE_OPAQUE && type != NULL;
  uint32_t slot = chunk == NULL ? 0 : ponyint_heap_base_slot(chunk, p);
  // Only the object's own descriptor can share the deep bit. Keep other
  // typed views, including embedded fields, distinct during traversal.
  if(slot != 0 && readable &&
    (type->size < sizeof(type) || memcmp(p, &type, sizeof(type)) != 0))
    slot = 0;
  uint32_t* index = slot == 0 ? NULL : ponyint_heap_local_index(chunk);
  if(index != NULL)
  {
    if(!mark_chunk(mark, chunk, index, slot, readable))
      return;
  } else {
    local_ref_t key = {p, chunk, readable ? type : NULL};
    size_t hash = ponyint_hash_ptr(p);
    size_t bucket = HASHMAP_UNKNOWN;
    if(ponyint_hashmap_get(&mark->refs, &key, hash, same_address, &bucket) != NULL)
      return;
    local_ref_t* ref = POOL_ALLOC(local_ref_t);
    *ref = key;
    ponyint_hashmap_putindex(&mark->refs, ref, hash, same_address, bucket);
  }

  if(recurse)
  {
    ctx->stack = ponyint_gcstack_push(ctx->stack, p);
    ctx->stack = ponyint_gcstack_push(ctx->stack, (void*)type->trace);
  }
}

static void trace_actor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  (void)ctx;
  (void)actor;
}

static bool can_mark(pony_ctx_t* ctx, pony_actor_t* actor, size_t* foreign_charge)
{
  if(ctx->trace_object != NULL ||
    ctx->stack != NULL || ctx->msg_target != NULL ||
    ponyint_actormap_size(&ctx->acquire) != 0)
    return false;

  size_t foreign = HASHMAP_BEGIN;
  actorref_t* aref;
  while((aref = ponyint_actormap_next(&actor->gc.foreign, &foreign)) != NULL)
  {
    if(ponyint_objectmap_size(&aref->map) != 0)
    {
      actor->gc_wait_boundary = true;
      return false;
    }
    if(aref->rc != 0)
      *foreign_charge += GC_ACTOR_HEAP_EQUIV;
  }

  size_t i = HASHMAP_BEGIN;
  object_t* obj;
  while((obj = ponyint_objectmap_next(&actor->gc.local, &i)) != NULL)
  {
    if(obj->rc != 0 || obj->self_send_pins != 0)
    {
      actor->gc_wait_boundary = true;
      return false;
    }
  }

  return true;
}

static PONY_NOINLINE pony_ctx_t* begin_mark(pony_actor_t* actor, gc_frame_t* frame)
{
  pony_ctx_t* ctx = pony_ctx();
  if(actor != ctx->current)
    return NULL;

  int saved_errno = errno;
#ifdef PLATFORM_IS_WINDOWS
  DWORD saved_error = GetLastError();
#endif
#ifdef USE_RUNTIMESTATS
  uint64_t elapsed = ponyint_sched_cpu_used(ctx);
  actor->actorstats.app_cpu += elapsed;
  ctx->schedulerstats.actor_app_cpu += elapsed;
#endif
  TRACING_ACTOR_GC_START(actor);
  TRACING_ACTOR_GC_MARK_START(actor);
  size_t foreign_charge = 0;
  if(!can_mark(ctx, actor, &foreign_charge))
  {
    TRACING_ACTOR_GC_MARK_END(actor);
    TRACING_ACTOR_GC_END(actor);
#ifdef USE_RUNTIMESTATS
    elapsed = ponyint_sched_cpu_used(ctx);
    actor->actorstats.gc_mark_cpu += elapsed;
    ctx->schedulerstats.actor_gc_mark_cpu += elapsed;
#endif
    errno = saved_errno;
#ifdef PLATFORM_IS_WINDOWS
    SetLastError(saved_error);
#endif
    return NULL;
  }
  local_mark_t* mark = POOL_ALLOC(local_mark_t);
  memset(mark, 0, sizeof(*mark));
#ifdef USE_RUNTIMESTATS
  mark->scheduler_ctx = ctx;
#endif
  mark->foreign_charge = foreign_charge;
  mark->saved_errno = saved_errno;
#ifdef PLATFORM_IS_WINDOWS
  mark->saved_error = saved_error;
#endif
  mark->ctx.current = actor;
  mark->ctx.trace_actor = trace_actor;
  mark->ctx.trace_object = trace_object;

  size_t i = HASHMAP_BEGIN;
  object_t* obj;
  while((obj = ponyint_objectmap_next(&actor->gc.local, &i)) != NULL)
    trace_object(&mark->ctx, obj->address, NULL, PONY_TRACE_OPAQUE);

  if(actor->type->trace != NULL)
    actor->type->trace(&mark->ctx, actor);
  if(!mark->refused && !ponyint_gc_trace_frames(&mark->ctx, frame))
    mark->refused = true;
  return &mark->ctx;
}

// The LTO pass introduces calls to this predicate after runtime inlining.
#ifdef __clang__
__attribute__((used))
#endif
PONY_API bool ponyint_gc_has_small_room(pony_actor_t* actor)
{
  return actor != NULL && actor->heap.used <= actor->heap.next_gc &&
    HEAP_MAX <= actor->heap.next_gc - actor->heap.used;
}

PONY_API pony_ctx_t* ponyint_gc_poll(pony_actor_t* actor, gc_frame_t* frame)
{
  if(actor == NULL || actor->heap.used <= actor->heap.next_gc ||
    actor->gc_native_alias || actor->gc_wait_boundary ||
    actor->gc_wait_finaliser)
    return NULL;
  return begin_mark(actor, frame);
}

static bool retain_finaliser(void* arg, void* p, bool traceable)
{
  local_mark_t* mark = (local_mark_t*)arg;
  if(!traceable)
  {
    mark->ctx.current->gc_wait_finaliser = true;
    return false;
  }
  trace_object(&mark->ctx, p, *(pony_type_t**)p, PONY_TRACE_MUTABLE);
  return !mark->refused;
}

PONY_API bool ponyint_gc_finish(pony_ctx_t* trace_ctx)
{
  local_mark_t* mark = (local_mark_t*)trace_ctx;
  pony_actor_t* actor = trace_ctx->current;
  if(!mark->refused && !ponyint_heap_visit_finalisers(&actor->heap,
    retain_finaliser, mark))
    mark->refused = true;
  if(mark->refused)
    ponyint_gc_discardstack(trace_ctx);
  else
    ponyint_gc_handlestack(trace_ctx);
  TRACING_ACTOR_GC_MARK_END(actor);
#ifdef USE_RUNTIMESTATS
  pony_ctx_t* scheduler_ctx = mark->scheduler_ctx;
  uint64_t elapsed = ponyint_sched_cpu_used(scheduler_ctx);
  actor->actorstats.gc_mark_cpu += elapsed;
  scheduler_ctx->schedulerstats.actor_gc_mark_cpu += elapsed;
#endif

  if(!mark->refused)
  {
    TRACING_ACTOR_GC_SWEEP_START(actor);
    TRACING_ACTOR_GC_HEAP_SWEEP_START(actor);
    actor->gc_after_local = true;
    ponyint_heap_startgc(&actor->heap
#ifdef USE_RUNTIMESTATS
      , actor
#endif
      );
    ponyint_heap_used(&actor->heap, mark->foreign_charge);
    for(size_t i = 0; i < mark->count; i++)
    {
      local_chunk_t* ref = &mark->chunks[i];
      if(ref->deep != 0)
        ponyint_heap_mark_slots(ref->chunk, ref->deep, false);
      uint32_t shallow = ref->slots & ~ref->deep;
      if(shallow != 0)
        ponyint_heap_mark_slots(ref->chunk, shallow, true);
    }
    size_t n = HASHMAP_BEGIN;
    local_ref_t* ref;
    while((ref = (local_ref_t*)ponyint_hashmap_next(&n, mark->refs.count,
      mark->refs.item_bitmap, mark->refs.size, mark->refs.buckets)) != NULL)
    {
      if(ref->chunk == NULL)
        continue;
      if(ref->type != NULL)
        ponyint_heap_mark(ref->chunk, ref->address);
      else
        ponyint_heap_mark_shallow(ref->chunk, ref->address);
    }
    bool held = ponyint_heap_endgc(&actor->heap, 0
#ifdef USE_RUNTIMESTATS
      , actor
#endif
      );
    TRACING_ACTOR_GC_THRESHOLD_CHANGED(actor, held);
    TRACING_ACTOR_GC_HEAP_SWEEP_END(actor);
    TRACING_ACTOR_GC_SWEEP_END(actor);
  }
  bool collected = !mark->refused;
  int saved_errno = mark->saved_errno;
#ifdef PLATFORM_IS_WINDOWS
  DWORD saved_error = mark->saved_error;
#endif
  if(mark->chunks != NULL)
    ponyint_pool_free_size(mark->capacity * sizeof(local_chunk_t), mark->chunks);
  ponyint_hashmap_destroy(&mark->refs, free_ref);
  POOL_FREE(local_mark_t, mark);
  TRACING_ACTOR_GC_END(actor);
#ifdef USE_RUNTIMESTATS
  elapsed = ponyint_sched_cpu_used(scheduler_ctx);
  if(collected)
  {
    actor->actorstats.gc_sweep_cpu += elapsed;
    scheduler_ctx->schedulerstats.actor_gc_sweep_cpu += elapsed;
  } else {
    actor->actorstats.gc_mark_cpu += elapsed;
    scheduler_ctx->schedulerstats.actor_gc_mark_cpu += elapsed;
  }
#endif
  errno = saved_errno;
#ifdef PLATFORM_IS_WINDOWS
  SetLastError(saved_error);
#endif
  return collected;
}

PONY_EXTERN_C_END
