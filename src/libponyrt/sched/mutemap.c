#include <string.h>
#include "mutemap.h"
#include "ds/fun.h"
#include "mem/pool.h"

static size_t muteset_hash(pony_actor_t* actor)
{
  return ponyint_hash_ptr(actor);
}

static bool muteset_cmp(pony_actor_t* a, pony_actor_t* b)
{
  return a == b;
}

DEFINE_HASHMAP(ponyint_muteset, muteset_t, pony_actor_t, muteset_hash,
  muteset_cmp, ponyint_pool_alloc_size, ponyint_pool_free_size,
  NULL);

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

static void muteref_free(muteref_t* mref)
{
  ponyint_muteset_destroy(&mref->value);
  POOL_FREE(muteref_t, mref);
}

DEFINE_HASHMAP(ponyint_mutemap, mutemap_t, muteref_t, muteref_hash,
  muteref_cmp, ponyint_pool_alloc_size, ponyint_pool_free_size,
  muteref_free);
