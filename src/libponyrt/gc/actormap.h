#ifndef gc_actormap_h
#define gc_actormap_h

#include <platform.h>

#include "objectmap.h"
#include "delta.h"
#include "../ds/hash.h"

PONY_EXTERN_C_BEGIN

typedef struct actorref_t
{
  pony_actor_t* actor;
  size_t rc;
  uint32_t mark;
  objectmap_t map;
} actorref_t;

object_t* ponyint_actorref_getobject(actorref_t* aref, void* address);

object_t* ponyint_actorref_getorput(actorref_t* aref, void* address,
  uint32_t mark);

void ponyint_actorref_free(actorref_t* aref);

DECLARE_HASHMAP(ponyint_actormap, actormap_t, actorref_t);

actorref_t* ponyint_actormap_getactor(actormap_t* map, pony_actor_t* actor);

actorref_t* ponyint_actormap_getorput(actormap_t* map, pony_actor_t* actor,
  uint32_t mark);

deltamap_t* ponyint_actormap_sweep(pony_ctx_t* ctx, actormap_t* map,
  uint32_t mark, deltamap_t* delta);

PONY_EXTERN_C_END

#endif
