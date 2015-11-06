#ifndef gc_actormap_h
#define gc_actormap_h

#include <platform.h>

#include "objectmap.h"
#include "delta.h"
#include "../ds/hash.h"

PONY_EXTERN_C_BEGIN

typedef struct actorref_t actorref_t;

pony_actor_t* actorref_actor(actorref_t* aref);

size_t actorref_rc(actorref_t* aref);

objectmap_t* actorref_map(actorref_t* aref);

bool actorref_marked(actorref_t* aref, uint32_t mark);

void actorref_mark(actorref_t* aref, uint32_t mark);

void actorref_inc(actorref_t* aref);

void actorref_inc_more(actorref_t* aref);

bool actorref_dec(actorref_t* aref);

object_t* actorref_getobject(actorref_t* aref, void* address);

object_t* actorref_getorput(actorref_t* aref, void* address, uint32_t mark);

void actorref_free(actorref_t* aref);

DECLARE_HASHMAP(actormap, actorref_t);

actorref_t* actormap_getactor(actormap_t* map, pony_actor_t* actor);

actorref_t* actormap_getorput(actormap_t* map, pony_actor_t* actor,
  uint32_t mark);

deltamap_t* actormap_sweep(pony_ctx_t* ctx, actormap_t* map, uint32_t mark,
  deltamap_t* delta);

PONY_EXTERN_C_END

#endif
