#include "objectmap.h"
#include "gc.h"
#include "../ds/hash.h"
#include "../ds/fun.h"
#include "../mem/pool.h"
#include "../mem/pagemap.h"
#include "ponyassert.h"

static size_t object_hash(object_t* obj)
{
  return ponyint_hash_ptr(obj->address);
}

static bool object_cmp(object_t* a, object_t* b)
{
  return a->address == b->address;
}

static object_t* object_alloc(void* address, pony_type_t* type, uint32_t mark)
{
  object_t* obj = (object_t*)POOL_ALLOC(object_t);
  obj->address = address;
  obj->rc = 0;
  obj->immutable = false;
  obj->type = type;

  // a new object is unmarked
  obj->mark = mark - 1;
  return obj;
}

static void object_free(object_t* obj)
{
  POOL_FREE(object_t, obj);
}

DEFINE_HASHMAP(ponyint_objectmap, objectmap_t, object_t, object_hash,
  object_cmp, object_free);

object_t* ponyint_objectmap_getobject(objectmap_t* map, void* address, size_t* index)
{
  object_t obj;
  obj.address = address;

  return ponyint_objectmap_get(map, &obj, index);
}

object_t* ponyint_objectmap_getorput(objectmap_t* map, void* address,
  pony_type_t* type, uint32_t mark)
{
  size_t index = HASHMAP_UNKNOWN;
  object_t* obj = ponyint_objectmap_getobject(map, address, &index);

  if(obj != NULL)
    return obj;

  obj = object_alloc(address, type, mark);
  ponyint_objectmap_putindex(map, obj, index);
  return obj;
}

void ponyint_objectmap_sweep(objectmap_t* map)
{
  size_t i = HASHMAP_BEGIN;
  object_t* obj;
  bool needs_optimize = false;

  while((obj = ponyint_objectmap_next(map, &i)) != NULL)
  {
    void* p = obj->address;

    if(obj->rc > 0)
    {
      chunk_t* chunk = ponyint_pagemap_get_chunk(p);
      ponyint_heap_mark_shallow(chunk, p);
    } else {
      ponyint_objectmap_clearindex(map, i);
      needs_optimize = true;

      object_free(obj);
    }
  }

  if(needs_optimize)
    ponyint_objectmap_optimize(map);
}

#ifdef USE_RUNTIMESTATS
size_t ponyint_objectmap_total_mem_size(objectmap_t* map)
{
  return ponyint_objectmap_mem_size(map)
    + (ponyint_objectmap_size(map) * sizeof(object_t));
}

size_t ponyint_objectmap_total_alloc_size(objectmap_t* map)
{
  return ponyint_objectmap_alloc_size(map)
    + (ponyint_objectmap_size(map) * POOL_ALLOC_SIZE(object_t));
}
#endif
