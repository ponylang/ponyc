#include "serialise.h"
#include "../sched/scheduler.h"
#include "../actor/actor.h"
#include <assert.h>

struct serialise_t
{
  void* p;
  pony_type_t* t;
  size_t offset;
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
    // TODO: actor, MaybePointer[A] or Pointer[A], raise an error?
    return;
  }

  serialise_t k;
  k.p = p;
  serialise_t* s = ponyint_serialise_get(&ctx->serialise, &k);

  if(s != NULL)
    return;

  s = POOL_ALLOC(serialise_t);
  s->p = p;
  s->t = t;
  s->offset = ctx->serialise_size;
  ponyint_serialise_put(&ctx->serialise, s);

  ctx->serialise_size += t->size;

  if(mutability != PONY_TRACE_OPAQUE)
    recurse(ctx, p, t->trace);
}

void ponyint_serialise_actor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  // TODO: raise an error?
  // when writing to the buffer, write the global id
  //   global id might not fit
  //   write an invalid position that maps to a global id
  // if it's not a pony internal thing for distribution, record an error?
  // can't serialise the actor: can't look at its state
  (void)ctx;
  (void)actor;
}

void* pony_serialise(pony_ctx_t* ctx, void* p)
{
  assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_serialise_object;
  ctx->trace_actor = ponyint_serialise_actor;

  pony_traceunknown(ctx, p, PONY_TRACE_MUTABLE);

  ponyint_gc_handlestack(ctx);
  ponyint_gc_done(ponyint_actor_gc(ctx->current));

  void* out = ponyint_pool_alloc_size(ctx->serialise_size);

  size_t i = HASHMAP_BEGIN;
  serialise_t* s;

  while((s = ponyint_serialise_next(&ctx->serialise, &i)) != NULL)
  {
    // TODO:
    s->t->serialise(ctx, s->p, out + s->offset);
  }

  ctx->serialise_size = 0;
  ponyint_serialise_destroy(&ctx->serialise);
  return out;
}
