#ifndef gc_serialise_h
#define gc_serialise_h

#include "gc.h"

PONY_EXTERN_C_BEGIN

typedef struct
{
  pony_type_t* t;
  size_t size;
  size_t alloc;
  char* ptr;
} ponyint_array_t;

typedef void* (*serialise_alloc_fn)(pony_ctx_t* ctx, size_t size);

typedef void (*serialise_throw_fn)();

typedef void* (*deserialise_raw_fn)(void* buf, size_t remaining_size);

typedef struct serialise_t serialise_t;

DECLARE_HASHMAP(ponyint_serialise, ponyint_serialise_t, serialise_t);

bool ponyint_serialise_setup(pony_type_t** table, size_t table_size);

void ponyint_serialise_object(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability);

void ponyint_serialise_actor(pony_ctx_t* ctx, pony_actor_t* actor);

PONY_API void pony_serialise(pony_ctx_t* ctx, void* p, pony_type_t* t,
  ponyint_array_t* out, serialise_alloc_fn alloc_fn,
  serialise_throw_fn throw_fn);
PONY_API size_t pony_serialise_offset(pony_ctx_t* ctx, void* p);
PONY_API void pony_serialise_reserve(pony_ctx_t* ctx, void* p, size_t size);

PONY_API void* pony_deserialise(pony_ctx_t* ctx, pony_type_t* t,
  ponyint_array_t* in, serialise_alloc_fn alloc_fn,
  serialise_alloc_fn alloc_final_fn, serialise_throw_fn throw_fn);
PONY_API void* pony_deserialise_block(pony_ctx_t* ctx, uintptr_t offset,
  size_t size);
PONY_API void* pony_deserialise_offset(pony_ctx_t* ctx, pony_type_t* t,
  uintptr_t offset);
PONY_API void* pony_deserialise_raw(pony_ctx_t* ctx, uintptr_t offset,
  deserialise_raw_fn ds_fn);

PONY_EXTERN_C_END

#endif
