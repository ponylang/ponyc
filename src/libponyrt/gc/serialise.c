#include "serialise.h"
#include "../sched/scheduler.h"

struct serialise_t
{
  void* p;
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

void ponyint_serialise_object_size(pony_ctx_t* ctx, void* p, pony_trace_fn f,
  bool immutable)
{
  (void)immutable;

  serialise_t k;
  k.p = p;
  serialise_t* s = ponyint_serialise_get(&ctx->serialise, &k);

  if(s != NULL)
    return;

  s = POOL_ALLOC(serialise_t);
  s->p = p;
  s->offset = ctx->serialise_size;
  ponyint_serialise_put(&ctx->serialise, s);

  pony_type_t* type = *(pony_type_t**)p;
  ctx->serialise_size += type->size;
  recurse(ctx, p, f);

  // do embedded objects need to show up?

  // TODO: writing to the buffer
  // allocate a single buffer that's large enough
  // for each object in the map
  // write the object to its position in the buffer
  // * write the type ID instead of the descriptor
  // * for each field
  //   * if it's a machine word, write it
  //   * if it's an embedded object, write it
  //   * if it's an object, write its buffer position
  //     * if it's not in the buffer, write an invalid position
  //   * if it's a tuple, write each element
}

void ponyint_serialise_actor_size(pony_ctx_t* ctx, pony_actor_t* actor)
{
  // TODO: do nothing
  // when writing to the buffer, write the global id
  //   global id might not fit
  //   write an invalid position that maps to a global id
  // if it's not a pony internal thing for distribution, record an error?
  // can't serialise the actor: can't look at its state
  (void)ctx;
  (void)actor;
}
