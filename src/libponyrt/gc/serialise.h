#ifndef gc_serialise_h
#define gc_serialise_h

#include "gc.h"

PONY_EXTERN_C_BEGIN

typedef struct serialise_t serialise_t;

DECLARE_HASHMAP(ponyint_serialise, ponyint_serialise_t, serialise_t);

void ponyint_serialise_object(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability);

void ponyint_serialise_actor(pony_ctx_t* ctx, pony_actor_t* actor);

void pony_serialise(pony_ctx_t* ctx, void* p, void* out);

PONY_EXTERN_C_END

#endif
