#include <string.h>
#include "mutemap.h"
#include "ds/fun.h"
#include "mem/pool.h"
#include "ponyassert.h"

static size_t muteset_hash(pony_actor_t* actor)
{
  return ponyint_hash_ptr(actor);
}

static bool muteset_cmp(pony_actor_t* a, pony_actor_t* b)
{
  return a == b;
}

DEFINE_HASHMAP(ponyint_muteset, muteset_t, pony_actor_t, muteset_hash,
  muteset_cmp, NULL);

static size_t muteref_hash(muteref_t* mref)
{
  return ponyint_hash_ptr(mref->key);
}

static bool muteref_cmp(muteref_t* a, muteref_t* b)
{
  return a->key == b->key;
}

muteref_t* ponyint_muteref_alloc(pony_actor_t* key)
{
  muteref_t* mref = (muteref_t*)POOL_ALLOC(muteref_t);
  memset(mref, 0, sizeof(muteref_t));
  mref->key = key;

  return mref;
}

void ponyint_muteref_free(muteref_t* mref)
{
  ponyint_muteset_destroy(&mref->value);
  POOL_FREE(muteref_t, mref);
}

DEFINE_HASHMAP(ponyint_mutemap, mutemap_t, muteref_t, muteref_hash,
  muteref_cmp, ponyint_muteref_free);

#ifdef USE_MEMTRACK
size_t ponyint_mutemap_total_mem_size(mutemap_t* map)
{
  size_t t = 0;
  t += ponyint_mutemap_mem_size(map);
  size_t i = HASHMAP_UNKNOWN;
  muteref_t* mref = NULL;

  while((mref = ponyint_mutemap_next(map, &i)) != NULL)
  {
    t += ponyint_muteset_mem_size(&mref->value);
    t += sizeof(muteref_t);
  }

  return t;
}

size_t ponyint_mutemap_total_alloc_size(mutemap_t* map)
{
  size_t t = 0;
  t += ponyint_mutemap_alloc_size(map);
  size_t i = HASHMAP_UNKNOWN;
  muteref_t* mref = NULL;

  while((mref = ponyint_mutemap_next(map, &i)) != NULL)
  {
    t += ponyint_muteset_alloc_size(&mref->value);
    t += POOL_ALLOC_SIZE(muteref_t);
  }

  return t;
}
#endif
