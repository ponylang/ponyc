#include "delta.h"
#include "../mem/pool.h"

typedef struct delta_t
{
  pony_actor_t* actor;
  size_t rc;
} delta_t;

static uint64_t delta_hash(delta_t* delta)
{
  return hash_ptr(delta->actor);
}

static bool delta_cmp(delta_t* a, delta_t* b)
{
  return a->actor == b->actor;
}

static void delta_free(delta_t* delta)
{
  POOL_FREE(delta_t, delta);
}

pony_actor_t* delta_actor(delta_t* delta)
{
  return delta->actor;
}

size_t delta_rc(delta_t* delta)
{
  return delta->rc;
}

DEFINE_HASHMAP(deltamap, delta_t, delta_hash, delta_cmp,
  pool_alloc_size, pool_free_size, delta_free, NULL);

deltamap_t* deltamap_update(deltamap_t* map, pony_actor_t* actor, size_t rc)
{
  if(map == NULL)
  {
    // allocate a new map with space for at least one element
    map = (deltamap_t*)POOL_ALLOC(deltamap_t);
    deltamap_init(map, 1);
  }

  delta_t key;
  key.actor = actor;

  delta_t* delta = deltamap_get(map, &key);

  if(delta != NULL)
  {
    delta->rc = rc;
  } else {
    delta = (delta_t*)POOL_ALLOC(delta_t);
    delta->actor = actor;
    delta->rc = rc;

    deltamap_put(map, delta);
  }

  return map;
}

void deltamap_free(deltamap_t* map)
{
  deltamap_destroy(map);
  POOL_FREE(deltamap_t, map);
}
