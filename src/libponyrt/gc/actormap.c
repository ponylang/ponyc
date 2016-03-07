#include "actormap.h"
#include "objectmap.h"
#include "gc.h"
#include "../actor/actor.h"
#include "../ds/hash.h"
#include "../ds/fun.h"
#include "../mem/pool.h"
#include <string.h>
#include <assert.h>

static size_t actorref_hash(actorref_t* aref)
{
  return ponyint_hash_ptr(aref->actor);
}

static bool actorref_cmp(actorref_t* a, actorref_t* b)
{
  return a->actor == b->actor;
}

static actorref_t* actorref_alloc(pony_actor_t* actor, uint32_t mark)
{
  actorref_t* aref = (actorref_t*)POOL_ALLOC(actorref_t);
  memset(aref, 0, sizeof(actorref_t));
  aref->actor = actor;

  // a new actorref is unmarked
  aref->mark = mark - 1;
  return aref;
}

object_t* ponyint_actorref_getobject(actorref_t* aref, void* address)
{
  return ponyint_objectmap_getobject(&aref->map, address);
}

object_t* ponyint_actorref_getorput(actorref_t* aref, void* address,
  uint32_t mark)
{
  return ponyint_objectmap_getorput(&aref->map, address, mark);
}

void ponyint_actorref_free(actorref_t* aref)
{
  ponyint_objectmap_destroy(&aref->map);
  POOL_FREE(actorref_t, aref);
}

DEFINE_HASHMAP(ponyint_actormap, actormap_t, actorref_t, actorref_hash,
  actorref_cmp, ponyint_pool_alloc_size, ponyint_pool_free_size,
  ponyint_actorref_free);

static actorref_t* move_unmarked_objects(actorref_t* from, uint32_t mark)
{
  size_t size = ponyint_objectmap_size(&from->map);

  if(size == 0)
    return NULL;

  actorref_t* to = NULL;
  size_t i = HASHMAP_BEGIN;
  object_t* obj;

  while((obj = ponyint_objectmap_next(&from->map, &i)) != NULL)
  {
    if(obj->mark == mark)
      continue;

    ponyint_objectmap_removeindex(&from->map, i);

    if(to == NULL)
    {
      // Guarantee we don't resize during insertion.
      to = actorref_alloc(from->actor, mark);
      ponyint_objectmap_init(&to->map, size);
    }

    ponyint_objectmap_put(&to->map, obj);
  }

  return to;
}

static void send_release(pony_ctx_t* ctx, actorref_t* aref)
{
  if(aref == NULL)
    return;

  if(ponyint_actor_pendingdestroy(aref->actor) ||
    ((aref->rc == 0) && (ponyint_objectmap_size(&aref->map) == 0))
    )
  {
    ponyint_actorref_free(aref);
    return;
  }

  pony_sendp(ctx, aref->actor, ACTORMSG_RELEASE, aref);
}

actorref_t* ponyint_actormap_getactor(actormap_t* map, pony_actor_t* actor)
{
  actorref_t key;
  key.actor = actor;

  return ponyint_actormap_get(map, &key);
}

actorref_t* ponyint_actormap_getorput(actormap_t* map, pony_actor_t* actor,
  uint32_t mark)
{
  actorref_t* aref = ponyint_actormap_getactor(map, actor);

  if(aref != NULL)
    return aref;

  aref = actorref_alloc(actor, mark);
  ponyint_actormap_put(map, aref);
  return aref;
}

deltamap_t* ponyint_actormap_sweep(pony_ctx_t* ctx, actormap_t* map,
  uint32_t mark, deltamap_t* delta)
{
  size_t i = HASHMAP_BEGIN;
  actorref_t* aref;

  while((aref = ponyint_actormap_next(map, &i)) != NULL)
  {
    if(aref->mark == mark)
    {
      aref = move_unmarked_objects(aref, mark);
    } else {
      ponyint_actormap_removeindex(map, i);
      delta = ponyint_deltamap_update(delta, aref->actor, 0);
    }

    send_release(ctx, aref);
  }

  return delta;
}
