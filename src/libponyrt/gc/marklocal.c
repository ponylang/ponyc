#include "marklocal.h"
#include "gc.h"
#include "image.h"
#include "frames.h"
#include "../actor/actor.h"
#include "../mem/pagemap.h"
#include "../mem/pool.h"
#include "../sched/scheduler.h"
#include "../tracing/tracing.h"
#include "ponyassert.h"
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
  bool refused;
  bool private_finalisers;
  int saved_errno;
  pony_ctx_t* scheduler_ctx;
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

// The chunk index that records a mark of p and its slot there, or NULL when
// the mark is recorded in the hash map instead. Only the object's own
// descriptor can share the deep bit. Other typed views, including embedded
// fields, stay distinct during traversal.
static uint32_t* mark_record(chunk_t* chunk, void* p, pony_type_t* type,
  bool readable, uint32_t* slot)
{
  *slot = chunk == NULL ? 0 : ponyint_heap_base_slot(chunk, p);
  if(*slot != 0 && readable &&
    (type->size < sizeof(type) || memcmp(p, &type, sizeof(type)) != 0))
    *slot = 0;
  return *slot == 0 ? NULL : ponyint_heap_local_index(chunk);
}

static bool queue_object(pony_ctx_t* ctx, void* p, pony_type_t* type, int mode)
{
  local_mark_t* mark = (local_mark_t*)ctx;

  if(p == NULL || mark->refused)
    return false;

  pony_actor_t* owner = NULL;
  chunk_t* chunk = ponyint_pagemap_get(p, &owner);
  if(owner != ctx->current)
    chunk = NULL;

  // Constants in the program image aren't traced (see image.h).
  if((chunk == NULL) && ponyint_image_constant(p))
    return false;

  bool recurse = mode != PONY_TRACE_OPAQUE && type != NULL &&
    type->trace != NULL;
  if(chunk == NULL && !recurse)
    return false;

  bool readable = mode != PONY_TRACE_OPAQUE && type != NULL;
  uint32_t slot;
  uint32_t* index = mark_record(chunk, p, type, readable, &slot);
  if(index != NULL)
  {
    if(!mark_chunk(mark, chunk, index, slot, readable))
      return false;
  } else {
    local_ref_t key = {p, chunk, readable ? type : NULL};
    size_t hash = ponyint_hash_ptr(p);
    size_t bucket = HASHMAP_UNKNOWN;
    if(ponyint_hashmap_get(&mark->refs, &key, hash, same_address, &bucket) !=
      NULL)
      return false;
    local_ref_t* ref = POOL_ALLOC(local_ref_t);
    *ref = key;
    ponyint_hashmap_putindex(&mark->refs, ref, hash, same_address, bucket);
  }

  if(recurse)
  {
    ctx->stack = ponyint_gcstack_push(ctx->stack, p);
    ctx->stack = ponyint_gcstack_push(ctx->stack, (void*)type->trace);
  }
  return recurse;
}

static void trace_object(pony_ctx_t* ctx, void* p, pony_type_t* type, int mode)
{
  queue_object(ctx, p, type, mode);
}

static void trace_actor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  (void)ctx;
  (void)actor;
}

static bool has_shared_objects(gc_t* gc)
{
  size_t i = HASHMAP_BEGIN;
  object_t* obj;
  while((obj = ponyint_objectmap_next(&gc->local, &i)) != NULL)
  {
    if(obj->rc > 0)
      return true;
  }

  i = HASHMAP_BEGIN;
  actorref_t* aref;
  while((aref = ponyint_actormap_next(&gc->foreign, &i)) != NULL)
  {
    if(ponyint_objectmap_size(&aref->map) != 0)
      return true;
  }
  return false;
}

static PONY_NOINLINE pony_ctx_t* begin_mark(pony_actor_t* actor,
  gc_frame_t* frame)
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
  if(ctx->trace_object != NULL || ctx->stack != NULL ||
    ctx->msg_target != NULL || ponyint_actormap_size(&ctx->acquire) != 0)
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
  mark->scheduler_ctx = ctx;
  mark->saved_errno = saved_errno;
#ifdef PLATFORM_IS_WINDOWS
  mark->saved_error = saved_error;
#endif
  mark->ctx.current = actor;
  mark->ctx.trace_actor = trace_actor;
  mark->ctx.trace_object = trace_object;
  // The local mark records each visit by address and type, so a routed type
  // needs no separate function.
  mark->ctx.trace_routed = trace_object;

  if(!ponyint_gc_trace_frames(&mark->ctx, frame))
  {
    mark->refused = true;
    return &mark->ctx;
  }
  size_t i = HASHMAP_BEGIN;
  object_t* obj;
  while((obj = ponyint_gc_next_immutable_root(&actor->gc, &i)) != NULL)
    trace_object(&mark->ctx, obj->address, obj->type, PONY_TRACE_IMMUTABLE);
  if(actor->type->trace != NULL)
    actor->type->trace(&mark->ctx, actor);
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
    actor->gc_native_alias || actor->gc_wait_finaliser)
    return NULL;
  return begin_mark(actor, frame);
}

// True when this mark reached p through a readable reference to the object
// itself and recorded it in p's chunk. False never means the object is
// unsafe to keep, only that this mark didn't record it that way.
static bool reached_readable(local_mark_t* mark, void* p)
{
  chunk_t* chunk = ponyint_pagemap_get_chunk(p);
  uint32_t slot;
  uint32_t* index = mark_record(chunk, p, *(pony_type_t**)p, true, &slot);
  return index != NULL && *index < mark->count &&
    mark->chunks[*index].chunk == chunk &&
    (mark->chunks[*index].deep & slot) != 0;
}

static bool retain_finaliser(void* arg, void* p, bool traceable)
{
  local_mark_t* mark = (local_mark_t*)arg;
  if(!traceable)
  {
    // A readable reference to the object itself can't exist until every
    // field is defined, so a finaliser this mark reached that way is already
    // kept and traced.
    if(reached_readable(mark, p))
      return !mark->refused;
    // An object whose type has no fields to trace is kept without reading any
    // of them.
    pony_type_t* type = *(pony_type_t**)p;
    if(type->trace == NULL)
    {
      queue_object(&mark->ctx, p, type, PONY_TRACE_OPAQUE);
      return !mark->refused;
    }
    // No poll runs while a class constructor is running, so no constructor
    // finishing later in this behavior makes this finaliser traceable.
    mark->ctx.current->gc_wait_finaliser = GC_WAIT_FOR_BOUNDARY;
    return false;
  }
  // Keeping an object for its finaliser does not give this actor permission
  // to read fields that another actor may be changing.
  if(queue_object(&mark->ctx, p, *(pony_type_t**)p, PONY_TRACE_MUTABLE) &&
    !mark->private_finalisers)
  {
    if(has_shared_objects(&mark->ctx.current->gc))
    {
      mark->ctx.current->gc_wait_finaliser = GC_WAIT_FOR_BOUNDARY;
      return false;
    }
    mark->private_finalisers = true;
  }
  return !mark->refused;
}

PONY_API bool ponyint_gc_finish(pony_ctx_t* trace_ctx)
{
  local_mark_t* mark = (local_mark_t*)trace_ctx;
  pony_actor_t* actor = trace_ctx->current;
  if(!mark->refused)
    ponyint_gc_handlestack(trace_ctx);

  size_t local_used;
  bool has_foreign = ponyint_actormap_size(&actor->gc.foreign) != 0;
  if(!mark->refused && !ponyint_heap_visit_finalisers(&actor->heap,
    retain_finaliser, mark, has_foreign ? &local_used : NULL))
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
    mark->scheduler_ctx->collected_locally = true;
    // ORCA charges foreign references to heap.used. Only local storage
    // is reclaimed by this sweep.
    pony_assert(!has_foreign || actor->heap.used >= local_used);
    size_t foreign_used = has_foreign ? actor->heap.used - local_used : 0;
    ponyint_heap_startgc(&actor->heap
#ifdef USE_RUNTIMESTATS
      , actor
#endif
      );
    ponyint_heap_used(&actor->heap, foreign_used);
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
    TRACING_ACTOR_GC_OBJECTMAP_SWEEP_START(actor);
    ponyint_objectmap_sweep(&actor->gc.local);
    TRACING_ACTOR_GC_OBJECTMAP_SWEEP_END(actor);
    TRACING_ACTOR_GC_HEAP_SWEEP_START(actor);
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
    ponyint_pool_free_size(mark->capacity * sizeof(local_chunk_t),
      mark->chunks);
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
