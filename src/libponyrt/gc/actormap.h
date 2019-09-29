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

actorref_t* ponyint_actormap_getactor(actormap_t* map, pony_actor_t* actor, size_t* index);

actorref_t* ponyint_actormap_getorput(actormap_t* map, pony_actor_t* actor,
  uint32_t mark);

deltamap_t* ponyint_actormap_sweep(pony_ctx_t* ctx, actormap_t* map,
#ifdef USE_MEMTRACK
  uint32_t mark, deltamap_t* delta, bool actor_noblock, size_t* mem_used_freed,
  size_t* mem_allocated_freed);
#else
  uint32_t mark, deltamap_t* delta, bool actor_noblock);
#endif

#ifdef USE_MEMTRACK
size_t ponyint_actormap_partial_mem_size(actormap_t* map);

size_t ponyint_actormap_partial_alloc_size(actormap_t* map);

size_t ponyint_actormap_total_mem_size(actormap_t* map);

size_t ponyint_actormap_total_alloc_size(actormap_t* map);
#endif

PONY_EXTERN_C_END

#endif
