#ifndef gc_delta_h
#define gc_delta_h

#include "../ds/hash.h"
#include <pony.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct delta_t delta_t;

pony_actor_t* delta_actor(delta_t* delta);

size_t delta_rc(delta_t* delta);

DECLARE_HASHMAP(deltamap, delta_t);

deltamap_t* deltamap_update(deltamap_t* map, pony_actor_t* actor, size_t rc);

void deltamap_free(deltamap_t* map);

PONY_EXTERN_C_END

#endif
