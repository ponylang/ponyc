#ifndef gc_delta_h
#define gc_delta_h

#include "../ds/hash.h"
#include "../pony.h"
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct delta_t delta_t;

pony_actor_t* ponyint_delta_actor(delta_t* delta);

size_t ponyint_delta_rc(delta_t* delta);

DECLARE_HASHMAP(ponyint_deltamap, deltamap_t, delta_t);

deltamap_t* ponyint_deltamap_update(deltamap_t* map, pony_actor_t* actor,
  size_t rc);

void ponyint_deltamap_free(deltamap_t* map);

#ifdef USE_MEMTRACK
size_t ponyint_deltamap_total_mem_size(deltamap_t* map);

size_t ponyint_deltamap_total_alloc_size(deltamap_t* map);
#endif

PONY_EXTERN_C_END

#endif
