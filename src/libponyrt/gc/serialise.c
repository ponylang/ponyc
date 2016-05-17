#include "serialise.h"
#include "../sched/scheduler.h"
#include "../lang/lang.h"
#include <string.h>
#include <assert.h>

#define HIGH_BIT ((size_t)1 << ((sizeof(size_t) * 8) - 1))

extern size_t __DescTableSize;
extern pony_type_t* __DescTable;

typedef struct
{
  pony_type_t* t;
  size_t size;
  size_t alloc;
  char* ptr;
} ponyint_array_t;

struct serialise_t
{
  uintptr_t key;
  uintptr_t value;
  pony_type_t* t;
  int mutability;
};

static size_t serialise_hash(serialise_t* p)
{
  return ponyint_hash_size(p->key);
}

static bool serialise_cmp(serialise_t* a, serialise_t* b)
{
  return a->key == b->key;
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
  k.key = (uintptr_t)p;
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
    s->key = (uintptr_t)p;
    s->value = ctx->serialise_size;
    s->t = t;

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

  ponyint_array_t* r = (ponyint_array_t*)out;
  r->size = ctx->serialise_size;
  r->alloc = r->size;
  r->ptr = (char*)ponyint_pool_alloc_size(r->size);

  size_t i = HASHMAP_BEGIN;
  serialise_t* s;

  while((s = ponyint_serialise_next(&ctx->serialise, &i)) != NULL)
    s->t->serialise(ctx, (void*)s->key, r->ptr + s->value, s->mutability);

  ctx->serialise_size = 0;
  ponyint_serialise_destroy(&ctx->serialise);
}

size_t pony_serialise_offset(pony_ctx_t* ctx, void* p)
{
  serialise_t k;
  k.key = (uintptr_t)p;
  serialise_t* s = ponyint_serialise_get(&ctx->serialise, &k);

  // If we are in the map, return the offset.
  if(s != NULL)
    return s->value;

  // If we are not in the map, we are an untraced primitive. Return the type id
  // with the high bit set.
  pony_type_t* t = *(pony_type_t**)p;
  return (size_t)t->id | HIGH_BIT;
}

void* pony_deserialise_offset(pony_ctx_t* ctx, pony_type_t* t,
  uintptr_t offset)
{
  // If the high bit of the offset is set, it is either an unserialised
  // primitive, or an unserialised field in an opaque object.
  if((offset & HIGH_BIT) != 0)
  {
    offset &= ~HIGH_BIT;

    if(offset > __DescTableSize)
      return NULL;

    // Return the global instance, if there is one. It's ok to return null if
    // there is no global instance, as this will then be an unserisalised
    // field in an opaque object.
    t = (&__DescTable)[offset];
    return t->instance;
  }

  // Lookup the offset, return the associated object if there is one.
  serialise_t k;
  k.key = offset;
  serialise_t* s = ponyint_serialise_get(&ctx->serialise, &k);

  if(s != NULL)
    return (void*)s->value;

  // If we haven't been passed a type descriptor, read one.
  if(t == NULL)
  {
    // Make sure we have space to read a type id.
    if((offset + sizeof(uintptr_t)) > ctx->serialise_size)
      pony_throw();

    // Turn the type id into a descriptor pointer.
    uintptr_t id = *(uintptr_t*)((uintptr_t)ctx->serialise_buffer + offset);
    t = (&__DescTable)[id];
  }

  // If it's a primitive, return the global instance.
  if(t->instance != NULL)
    return t->instance;

  // Make sure we have space to read the object.
  if((offset + t->size) > ctx->serialise_size)
    pony_throw();

  // Allocate the object, memcpy to it.
  void* object = pony_alloc(ctx, t->size);
  memcpy(object, (void*)((uintptr_t)ctx->serialise_buffer + offset), t->size);

  // Store a mapping of offset to object.
  s = POOL_ALLOC(serialise_t);
  s->key = offset;
  s->value = (uintptr_t)object;
  ponyint_serialise_put(&ctx->serialise, s);

  recurse(ctx, object, t->deserialise);
  return object;
}

void* pony_deserialise(pony_ctx_t* ctx, void* in)
{
  // This can raise an error.
  ponyint_array_t* r = (ponyint_array_t*)in;
  ctx->serialise_buffer = r->ptr;
  ctx->serialise_size = r->size;

  void* object = pony_deserialise_offset(ctx, NULL, 0);
  ponyint_gc_handlestack(ctx);

  ctx->serialise_size = 0;
  ponyint_serialise_destroy(&ctx->serialise);
  return object;
}
