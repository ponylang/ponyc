#ifdef __linux__
#define _GNU_SOURCE
#endif

#include "serialise.h"
#include "../sched/scheduler.h"
#include "ponyassert.h"
#include <string.h>

#if defined(PLATFORM_IS_POSIX_BASED)
#include <dlfcn.h>
#else
#include <Windows.h>
#endif

#define HIGH_BIT ((size_t)1 << ((sizeof(size_t) * 8) - 1))
#define ALL_BITS ((size_t)(~0))

PONY_EXTERN_C_BEGIN

static size_t desc_table_size = 0;
static pony_type_t** desc_table = NULL;

PONY_EXTERN_C_END

struct serialise_t
{
  uintptr_t key;
  uintptr_t value;
  pony_type_t* t;
  int mutability;
  bool block;
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
  serialise_t, serialise_hash, serialise_cmp, serialise_free);

static void recurse(pony_ctx_t* ctx, void* p, void* f)
{
  if(f != NULL)
  {
    ctx->stack = ponyint_gcstack_push(ctx->stack, p);
    ctx->stack = ponyint_gcstack_push(ctx->stack, f);
  }
}

static void serialise_cleanup(pony_ctx_t* ctx)
{
  ponyint_gc_discardstack(ctx);
  ctx->serialise_size = 0;
  ponyint_serialise_destroy(&ctx->serialise);
}

static void custom_deserialise(pony_ctx_t* ctx)
{
  size_t i = HASHMAP_BEGIN;
  serialise_t* s;

  while((s = ponyint_serialise_next(&ctx->serialise, &i)) != NULL)
  {
    if (s->t != NULL && s->t->custom_deserialise != NULL)
    {
      s->t->custom_deserialise((void *)(s->value),
        (void*)((uintptr_t)ctx->serialise_buffer + s->key + s->t->size));
    }
  }
}

bool ponyint_serialise_setup(pony_type_t** table, size_t table_size)
{
#ifndef PONY_NDEBUG
  for(uint32_t i = 0; i < table_size; i++)
  {
    if(table[i] != NULL)
      pony_assert(table[i]->id == i);
  }
#endif

  desc_table = table;
  desc_table_size = table_size;

  return true;
}

void ponyint_serialise_object(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability)
{
  serialise_t k;
  k.key = (uintptr_t)p;
  size_t index = HASHMAP_UNKNOWN;
  serialise_t* s = ponyint_serialise_get(&ctx->serialise, &k, &index);

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
    s->block = false;

    // didn't find it in the map but index is where we can put the
    // new one without another search
    ponyint_serialise_putindex(&ctx->serialise, s, index);
    ctx->serialise_size += t->size;

    if(t->custom_serialise_space)
    {
      ctx->serialise_size += t->custom_serialise_space(p);
    }
  }
  // Set (or update) mutability.
  s->mutability = mutability;

  if(mutability != PONY_TRACE_OPAQUE)
    recurse(ctx, p, t->serialise_trace);
}

void ponyint_serialise_actor(pony_ctx_t* ctx, pony_actor_t* actor)
{
  (void)ctx;
  (void)actor;
  serialise_cleanup(ctx);
  ctx->serialise_throw();
  abort();
}

PONY_API void pony_serialise_reserve(pony_ctx_t* ctx, void* p, size_t size)
{
  // Reserve a block of memory to serialise into. This is only needed for
  // String and Array[A].
  serialise_t k;
  k.key = (uintptr_t)p;
  size_t index = HASHMAP_UNKNOWN;

  serialise_t* s = ponyint_serialise_get(&ctx->serialise, &k, &index);

  if(s != NULL)
    return;

  // Put an entry in the map and reserve space.
  s = POOL_ALLOC(serialise_t);
  s->key = (uintptr_t)p;
  s->value = ctx->serialise_size;
  s->t = NULL;
  s->mutability = PONY_TRACE_OPAQUE;
  s->block = true;

  // didn't find it in the map but index is where we can put the
  // new one without another search
  ponyint_serialise_putindex(&ctx->serialise, s, index);
  ctx->serialise_size += size;
}

PONY_API size_t pony_serialise_offset(pony_ctx_t* ctx, void* p)
{
  if(p == NULL)
    return ALL_BITS;

  serialise_t k;
  k.key = (uintptr_t)p;
  size_t index = HASHMAP_UNKNOWN;
  serialise_t* s = ponyint_serialise_get(&ctx->serialise, &k, &index);

  // If we are in the map, return the offset.
  if(s != NULL)
  {
    if(s->block || (s->t != NULL && s->t->serialise != NULL))
      return s->value;
    else
      return ALL_BITS;
  }

  // If we are not in the map, we are an untraced primitive. Return the type id
  // with the high bit set.
  pony_type_t* t = *(pony_type_t**)p;
  return (size_t)t->id | HIGH_BIT;
}

PONY_API void pony_serialise(pony_ctx_t* ctx, void* p, pony_type_t* t,
  ponyint_array_t* out, serialise_alloc_fn alloc_fn,
  serialise_throw_fn throw_fn)
{
  // This can raise an error.
  pony_assert(ctx->stack == NULL);
  ctx->trace_object = ponyint_serialise_object;
  ctx->trace_actor = ponyint_serialise_actor;
  ctx->serialise_size = 0;
  ctx->serialise_alloc = alloc_fn;
  ctx->serialise_throw = throw_fn;

  if(t != NULL)
    pony_traceknown(ctx, p, t, PONY_TRACE_MUTABLE);
  else
    pony_traceunknown(ctx, p, PONY_TRACE_MUTABLE);

  ponyint_gc_handlestack(ctx);

  out->size = ctx->serialise_size;
  out->alloc = out->size;
  out->ptr = (char*)alloc_fn(ctx, out->size);
  memset(out->ptr, 0, out->size);

  size_t i = HASHMAP_BEGIN;
  serialise_t* s;

  while((s = ponyint_serialise_next(&ctx->serialise, &i)) != NULL)
  {
    if(!(s->block) && s->t != NULL && s->t->serialise != NULL)
      s->t->serialise(ctx, (void*)s->key, out->ptr, s->value, s->mutability);
  }

  serialise_cleanup(ctx);
}

PONY_API void* pony_deserialise_offset(pony_ctx_t* ctx, pony_type_t* t,
  uintptr_t offset)
{
  // if all the bits of the offset are set, it is either a Pointer[A] or a
  // NullablePointer[A].
  if(offset == ALL_BITS)
    return NULL;

  // If the high bit of the offset is set, it is either an unserialised
  // primitive, or an unserialised field in an opaque object.
  if((offset & HIGH_BIT) != 0)
  {
    offset &= ~HIGH_BIT;

    if(offset > desc_table_size)
      return NULL;

    // Return the global instance, if there is one. It's ok to return null if
    // there is no global instance, as this will then be an unserialised
    // field in an opaque object.
    t = desc_table[offset];
    return t->instance;
  }

  // Lookup the offset, return the associated object if there is one.
  serialise_t k;
  k.key = offset;
  size_t index = HASHMAP_UNKNOWN;
  serialise_t* s = ponyint_serialise_get(&ctx->serialise, &k, &index);

  if(s != NULL)
    return (void*)s->value;

  // If we haven't been passed a type descriptor, read one.
  if(t == NULL)
  {
    // Make sure we have space to read a type id.
    if((offset + sizeof(uintptr_t)) > ctx->serialise_size)
    {
      serialise_cleanup(ctx);
      ctx->serialise_throw();
      abort();
    }

    // Turn the type id into a descriptor pointer.
    uintptr_t id = *(uintptr_t*)((uintptr_t)ctx->serialise_buffer + offset);
    t = desc_table[id];
  }

  // If it's a primitive, return the global instance.
  if(t->instance != NULL)
    return t->instance;

  // Make sure we have space to read the object.
  if((offset + t->size) > ctx->serialise_size)
  {
    serialise_cleanup(ctx);
    ctx->serialise_throw();
    abort();
  }

  // Allocate the object, memcpy to it.
  void* object;
  if(t->final == NULL)
    object = ctx->serialise_alloc(ctx, t->size);
  else
    object = ctx->serialise_alloc_final(ctx, t->size);

  memcpy(object, (void*)((uintptr_t)ctx->serialise_buffer + offset), t->size);

  // Store a mapping of offset to object.
  s = POOL_ALLOC(serialise_t);
  s->key = offset;
  s->value = (uintptr_t)object;
  s->t = t;

  // didn't find it in the map but index is where we can put the
  // new one without another search
  ponyint_serialise_putindex(&ctx->serialise, s, index);

  recurse(ctx, object, t->deserialise);
  return object;
}

PONY_API void* pony_deserialise_block(pony_ctx_t* ctx, uintptr_t offset,
  size_t size)
{
  if(offset == ALL_BITS)
    return NULL;

  // Allocate the block, memcpy to it.
  if((offset + size) > ctx->serialise_size)
  {
    serialise_cleanup(ctx);
    ctx->serialise_throw();
    abort();
  }

  void* block = ctx->serialise_alloc(ctx, size);
  memcpy(block, (void*)((uintptr_t)ctx->serialise_buffer + offset), size);
  return block;
}

PONY_API void* pony_deserialise_raw(pony_ctx_t* ctx, uintptr_t offset,
  deserialise_raw_fn ds_fn)
{
  if(offset == ALL_BITS)
    return NULL;

  // Lookup the offset, return the associated object if there is one.
  serialise_t k;
  k.key = offset;
  size_t index = HASHMAP_UNKNOWN;
  serialise_t* s = ponyint_serialise_get(&ctx->serialise, &k, &index);

  if(s != NULL)
    return (void*)s->value;

  void* object = ds_fn((void*)((uintptr_t)ctx->serialise_buffer + offset),
    ctx->serialise_size - offset);

  if(object == NULL)
  {
    serialise_cleanup(ctx);
    ctx->serialise_throw();
    abort();
  }

  // Store a mapping of offset to object.
  s = POOL_ALLOC(serialise_t);
  s->key = offset;
  s->value = (uintptr_t)object;
  s->t = NULL;

  // didn't find it in the map but index is where we can put the
  // new one without another search
  ponyint_serialise_putindex(&ctx->serialise, s, index);
  return object;
}

PONY_API void* pony_deserialise(pony_ctx_t* ctx, pony_type_t* t,
  ponyint_array_t* in, serialise_alloc_fn alloc_fn,
  serialise_alloc_fn alloc_final_fn, serialise_throw_fn throw_fn)
{
  // This can raise an error.
  ctx->serialise_buffer = in->ptr;
  ctx->serialise_size = in->size;
  ctx->serialise_alloc = alloc_fn;
  ctx->serialise_alloc_final = alloc_final_fn;
  ctx->serialise_throw = throw_fn;

  void* object = pony_deserialise_offset(ctx, t, 0);
  ponyint_gc_handlestack(ctx);

  custom_deserialise(ctx);

  serialise_cleanup(ctx);
  return object;
}
