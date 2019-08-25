#include "delta.h"
#include "../mem/pool.h"

typedef struct delta_t
{
  pony_actor_t* actor;
  size_t rc;
} delta_t;

static size_t delta_hash(delta_t* delta)
{
  return ponyint_hash_ptr(delta->actor);
}

static bool delta_cmp(delta_t* a, delta_t* b)
{
  return a->actor == b->actor;
}

static void delta_free(delta_t* delta)
{
  POOL_FREE(delta_t, delta);
}

pony_actor_t* ponyint_delta_actor(delta_t* delta)
{
  return delta->actor;
}

size_t ponyint_delta_rc(delta_t* delta)
{
  return delta->rc;
}

DEFINE_HASHMAP(ponyint_deltamap, deltamap_t, delta_t, delta_hash, delta_cmp,
  delta_free);

deltamap_t* ponyint_deltamap_update(deltamap_t* map, pony_actor_t* actor,
  size_t rc)
{
  size_t index = HASHMAP_UNKNOWN;

  if(map == NULL)
  {
    // allocate a new map with space for at least one element
    map = (deltamap_t*)POOL_ALLOC(deltamap_t);
    ponyint_deltamap_init(map, 1);
  } else {
    delta_t key;
    key.actor = actor;
    delta_t* delta = ponyint_deltamap_get(map, &key, &index);

    if(delta != NULL)
    {
      delta->rc = rc;
      return map;
    }
  }

  delta_t* delta = (delta_t*)POOL_ALLOC(delta_t);
  delta->actor = actor;
  delta->rc = rc;

  if(index == HASHMAP_UNKNOWN)
  {
    // new map
    ponyint_deltamap_put(map, delta);
  } else {
    // didn't find it in the map but index is where we can put the
    // new one without another search
    ponyint_deltamap_putindex(map, delta, index);
  }

  return map;
}

void ponyint_deltamap_free(deltamap_t* map)
{
  ponyint_deltamap_destroy(map);
  POOL_FREE(deltamap_t, map);
}

#ifdef USE_MEMTRACK
size_t ponyint_deltamap_total_mem_size(deltamap_t* map)
{
  return ponyint_deltamap_mem_size(map)
    + (ponyint_deltamap_size(map) * sizeof(delta_t));
}

size_t ponyint_deltamap_total_alloc_size(deltamap_t* map)
{
  return ponyint_deltamap_alloc_size(map)
    + (ponyint_deltamap_size(map) * POOL_ALLOC_SIZE(delta_t));
}
#endif
