#include "actormap.h"
#include "objectmap.h"
#include "gc.h"
#include "../actor/actor.h"
#include "../ds/hash.h"
#include "../ds/fun.h"
#include "../mem/pool.h"
#include "ponyassert.h"
#include <string.h>

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
  size_t index = HASHMAP_UNKNOWN;
  return ponyint_objectmap_getobject(&aref->map, address, &index);
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
  actorref_cmp, ponyint_actorref_free);

static actorref_t* move_unmarked_objects(actorref_t* from, uint32_t mark)
{
  size_t size = ponyint_objectmap_size(&from->map);

  if(size == 0)
    return NULL;

  actorref_t* to = NULL;
  size_t i = HASHMAP_BEGIN;
  object_t* obj;
  bool needs_optimize = false;

  while((obj = ponyint_objectmap_next(&from->map, &i)) != NULL)
  {
    if(obj->mark == mark)
      continue;

    ponyint_objectmap_clearindex(&from->map, i);
    needs_optimize = true;

    if(to == NULL)
    {
      // Guarantee we don't resize during insertion.
      to = actorref_alloc(from->actor, mark);
      ponyint_objectmap_init(&to->map, size);
    }

    ponyint_objectmap_put(&to->map, obj);
  }

  if(needs_optimize)
    ponyint_objectmap_optimize(&from->map);

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

actorref_t* ponyint_actormap_getactor(actormap_t* map, pony_actor_t* actor, size_t* index)
{
  actorref_t key;
  key.actor = actor;

  return ponyint_actormap_get(map, &key, index);
}

actorref_t* ponyint_actormap_getorput(actormap_t* map, pony_actor_t* actor,
  uint32_t mark)
{
  size_t index = HASHMAP_UNKNOWN;
  actorref_t* aref = ponyint_actormap_getactor(map, actor, &index);

  if(aref != NULL)
    return aref;

  aref = actorref_alloc(actor, mark);
  ponyint_actormap_putindex(map, aref, index);
  return aref;
}

deltamap_t* ponyint_actormap_sweep(pony_ctx_t* ctx, actormap_t* map,
#ifdef USE_MEMTRACK
  uint32_t mark, deltamap_t* delta, bool actor_noblock, size_t* mem_used_freed,
  size_t* mem_allocated_freed)
#else
  uint32_t mark, deltamap_t* delta, bool actor_noblock)
#endif
{
  size_t i = HASHMAP_BEGIN;
  actorref_t* aref;
  bool needs_optimize = false;

#ifdef USE_MEMTRACK
  size_t objectmap_mem_used_freed = 0;
  size_t objectmap_mem_allocated_freed = 0;
#endif

  while((aref = ponyint_actormap_next(map, &i)) != NULL)
  {
#ifdef USE_MEMTRACK
    objectmap_mem_used_freed += ponyint_objectmap_total_mem_size(&aref->map);
    objectmap_mem_allocated_freed +=
      ponyint_objectmap_total_alloc_size(&aref->map);
#endif

    if(aref->mark == mark)
    {
#ifdef USE_MEMTRACK
      actorref_t* old_aref = aref;
#endif
      aref = move_unmarked_objects(aref, mark);
#ifdef USE_MEMTRACK
      // captures difference in # of entries removed from objectmap and their
      // object_t sizes freed although technically not freed yet; will be freed
      // by receiving scheduler thread during gc_release
      objectmap_mem_used_freed -=
        ponyint_objectmap_total_mem_size(&old_aref->map);
      objectmap_mem_allocated_freed -=
        ponyint_objectmap_total_alloc_size(&old_aref->map);
#endif
    } else {
      ponyint_actormap_clearindex(map, i);

      // only update if cycle detector is enabled
      if(!actor_noblock)
        delta = ponyint_deltamap_update(delta, aref->actor, 0);

      needs_optimize = true;
    }

#ifdef USE_MEMTRACK
    if(aref != NULL)
    {
      ctx->mem_used_actors += (sizeof(actorref_t)
        + ponyint_objectmap_total_mem_size(&aref->map));
      ctx->mem_allocated_actors += (POOL_ALLOC_SIZE(actorref_t)
        + ponyint_objectmap_total_alloc_size(&aref->map));
    }
#endif

    send_release(ctx, aref);
  }

#ifdef USE_MEMTRACK
  *mem_used_freed = objectmap_mem_used_freed;
  *mem_allocated_freed = objectmap_mem_allocated_freed;
#endif

  if(needs_optimize)
    ponyint_actormap_optimize(map);

  return delta;
}

#ifdef USE_MEMTRACK
size_t ponyint_actormap_partial_mem_size(actormap_t* map)
{
  return ponyint_actormap_mem_size(map)
    + (ponyint_actormap_size(map) * sizeof(actorref_t));
}

size_t ponyint_actormap_partial_alloc_size(actormap_t* map)
{
  return ponyint_actormap_alloc_size(map)
    + (ponyint_actormap_size(map) * POOL_ALLOC_SIZE(actorref_t));
}

size_t ponyint_actormap_total_mem_size(actormap_t* map)
{
  size_t t = 0;

  size_t i = HASHMAP_UNKNOWN;
  actorref_t* aref = NULL;

  while((aref = ponyint_actormap_next(map, &i)) != NULL)
  {
    t += ponyint_objectmap_total_mem_size(&aref->map);
  }

  return ponyint_actormap_mem_size(map)
    + (ponyint_actormap_size(map) * sizeof(actorref_t))
    + t;
}

size_t ponyint_actormap_total_alloc_size(actormap_t* map)
{
  size_t t = 0;

  size_t i = HASHMAP_UNKNOWN;
  actorref_t* aref = NULL;

  while((aref = ponyint_actormap_next(map, &i)) != NULL)
  {
    t += ponyint_objectmap_total_alloc_size(&aref->map);
  }

  return ponyint_actormap_alloc_size(map)
    + (ponyint_actormap_size(map) * POOL_ALLOC_SIZE(actorref_t))
    + t;
}
#endif
