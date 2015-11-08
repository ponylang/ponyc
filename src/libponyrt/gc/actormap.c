#include "actormap.h"
#include "objectmap.h"
#include "gc.h"
#include "../actor/actor.h"
#include "../ds/hash.h"
#include "../ds/fun.h"
#include "../mem/pool.h"
#include <string.h>
#include <assert.h>

typedef struct actorref_t
{
  pony_actor_t* actor;
  size_t rc;
  uint32_t mark;
  objectmap_t map;
} actorref_t;

static uint64_t actorref_hash(actorref_t* aref)
{
  return hash_ptr(aref->actor);
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

pony_actor_t* actorref_actor(actorref_t* aref)
{
  return aref->actor;
}

size_t actorref_rc(actorref_t* aref)
{
  return aref->rc;
}

objectmap_t* actorref_map(actorref_t* aref)
{
  return &aref->map;
}

bool actorref_marked(actorref_t* aref, uint32_t mark)
{
  return aref->mark == mark;
}

void actorref_mark(actorref_t* aref, uint32_t mark)
{
  aref->mark = mark;
}

void actorref_inc(actorref_t* aref)
{
  aref->rc++;
  assert(aref->rc > 0);
}

void actorref_inc_more(actorref_t* aref)
{
  assert(aref->rc == 0);
  aref->rc = GC_INC_MORE;
}

bool actorref_dec(actorref_t* aref)
{
  assert(aref->rc > 0);
  aref->rc--;
  return aref->rc > 0;
}

object_t* actorref_getobject(actorref_t* aref, void* address)
{
  return objectmap_getobject(&aref->map, address);
}

object_t* actorref_getorput(actorref_t* aref, void* address, uint32_t mark)
{
  return objectmap_getorput(&aref->map, address, mark);
}

void actorref_free(actorref_t* aref)
{
  objectmap_destroy(&aref->map);
  POOL_FREE(actorref_t, aref);
}

DEFINE_HASHMAP(actormap, actorref_t, actorref_hash, actorref_cmp,
  pool_alloc_size, pool_free_size, actorref_free);

static actorref_t* move_unmarked_objects(actorref_t* from, uint32_t mark)
{
  size_t size = objectmap_size(&from->map);

  if(size == 0)
    return NULL;

  actorref_t* to = NULL;
  size_t i = HASHMAP_BEGIN;
  object_t* obj;

  while((obj = objectmap_next(&from->map, &i)) != NULL)
  {
    if(object_marked(obj, mark))
      continue;

    objectmap_removeindex(&from->map, i);

    if(to == NULL)
    {
      // Guarantee we don't resize during insertion.
      to = actorref_alloc(from->actor, mark);
      objectmap_init(&to->map, size);
    }

    objectmap_put(&to->map, obj);
  }

  return to;
}

static void send_release(pony_ctx_t* ctx, actorref_t* aref)
{
  if(aref == NULL)
    return;

  if(actor_pendingdestroy(aref->actor) ||
    ((aref->rc == 0) && (objectmap_size(&aref->map) == 0))
    )
  {
    actorref_free(aref);
    return;
  }

  pony_sendp(ctx, aref->actor, ACTORMSG_RELEASE, aref);
}

actorref_t* actormap_getactor(actormap_t* map, pony_actor_t* actor)
{
  actorref_t key;
  key.actor = actor;

  return actormap_get(map, &key);
}

actorref_t* actormap_getorput(actormap_t* map, pony_actor_t* actor,
  uint32_t mark)
{
  actorref_t* aref = actormap_getactor(map, actor);

  if(aref != NULL)
    return aref;

  aref = actorref_alloc(actor, mark);
  actormap_put(map, aref);
  return aref;
}

deltamap_t* actormap_sweep(pony_ctx_t* ctx, actormap_t* map, uint32_t mark,
  deltamap_t* delta)
{
  size_t i = HASHMAP_BEGIN;
  actorref_t* aref;

  while((aref = actormap_next(map, &i)) != NULL)
  {
    if(aref->mark == mark)
    {
      aref = move_unmarked_objects(aref, mark);
    } else {
      actormap_removeindex(map, i);
      delta = deltamap_update(delta, aref->actor, 0);
    }

    send_release(ctx, aref);
  }

  return delta;
}
