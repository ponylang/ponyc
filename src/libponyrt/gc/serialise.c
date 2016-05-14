#include "serialise.h"
#include "../sched/scheduler.h"
#include "../actor/actor.h"
#include "../lang/lang.h"
#include <assert.h>

typedef struct
{
  pony_type_t* t;
  size_t size;
  size_t alloc;
  char* ptr;
} ponyint_array_t;

struct serialise_t
{
  void* p;
  pony_type_t* t;
  size_t offset;
  int mutability;
};

static size_t serialise_hash(serialise_t* p)
{
  return ponyint_hash_ptr(p->p);
}

static bool serialise_cmp(serialise_t* a, serialise_t* b)
{
  return a->p == b->p;
}

static void serialise_free(serialise_t* p)
{
  POOL_FREE(serialise_t, p);
}

DEFINE_HASHMAP(ponyint_serialise, ponyint_serialise_t,
  serialise_t, serialise_hash, serialise_cmp,
  ponyint_pool_alloc_size, ponyint_pool_free_size, serialise_free);

static void recurse(pony_ctx_t* ctx, void* p, void* f)
{
  if(f != NULL)
  {
    ctx->stack = ponyint_gcstack_push(ctx->stack, p);
    ctx->stack = ponyint_gcstack_push(ctx->stack, f);
  }
}

void ponyint_serialise_object(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  if(t->serialise == NULL)
  {
    // A type without a serialisation function raises an error.
    // This applies to Pointer[A] and MaybePointer[A].
    pony_throw();
    return;
  }

  serialise_t k;
  k.p = p;
  serialise_t* s = ponyint_serialise_get(&ctx->serialise, &k);

  if(s != NULL)
  {
    // If it was traced as opaque, and now is not opaque, change it and trace.
    if((s->mutability != PONY_TRACE_OPAQUE) ||
      (mutability == PONY_TRACE_OPAQUE))
      return;
  } else {
    // Put an entry in the map and reserve space.
    s = POOL_ALLOC(serialise_t);
    s->p = p;
    s->t = t;
    s->offset = ctx->serialise_size;

    ponyint_serialise_put(&ctx->serialise, s);
    ctx->serialise_size += t->size;
  }

  // Set (or update) mutability.
  s->mutability = mutability;

  if(mutability != PONY_TRACE_OPAQUE)
  {
    // These will only be called once per object.
    if(t->serialise_trace == t->trace)
    {
      recurse(ctx, p, t->serialise_trace);
    } else {
      // This is a String or an Array[A]. The serialise_trace function must be
      // called immediately so that the extra size it requests in the buffer
      // is associated with this object.
      t->serialise_trace(ctx, p);
    }
  }
}

void ponyint_serialise_actor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  (void)ctx;
  (void)actor;
  pony_throw();
}

void pony_serialise_size(pony_ctx_t* ctx, size_t size)
{
  ctx->serialise_size += size;
}

void pony_serialise(pony_ctx_t* ctx, void* p, void* out)
{
  // This can raise an error.
  assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_serialise_object;
  ctx->trace_actor = ponyint_serialise_actor;
  ctx->serialise_size = 0;

  pony_traceunknown(ctx, p, PONY_TRACE_MUTABLE);

  ponyint_gc_handlestack(ctx);
  ponyint_gc_done(ponyint_actor_gc(ctx->current));

  ponyint_array_t* r = (ponyint_array_t*)out;
  r->size = ctx->serialise_size;
  r->alloc = r->size;
  r->ptr = (char*)ponyint_pool_alloc_size(r->size);

  size_t i = HASHMAP_BEGIN;
  serialise_t* s;

  while((s = ponyint_serialise_next(&ctx->serialise, &i)) != NULL)
    s->t->serialise(ctx, s->p, r->ptr + s->offset, s->mutability);

  ctx->serialise_size = 0;
  ponyint_serialise_destroy(&ctx->serialise);
}

size_t pony_serialise_offset(pony_ctx_t* ctx, void* p)
{
  serialise_t k;
  k.p = p;
  serialise_t* s = ponyint_serialise_get(&ctx->serialise, &k);

  // If we are in the map, return the offset.
  if(s != NULL)
    return s->offset;

  // If we are not in the map, we are an untraced primitive. Return the type id
  // with the high bit set.
  pony_type_t* t = *(pony_type_t**)p;
  return (size_t)t->id | ((size_t)1 << ((sizeof(size_t) * 8) - 1));
}
