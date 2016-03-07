#include "objectmap.h"
#include "gc.h"
#include "../ds/hash.h"
#include "../ds/fun.h"
#include "../mem/pool.h"
#include "../mem/pagemap.h"
#include <assert.h>

static size_t object_hash(object_t* obj)
{
  return ponyint_hash_ptr(obj->address);
}

static bool object_cmp(object_t* a, object_t* b)
{
  return a->address == b->address;
}

static object_t* object_alloc(void* address, uint32_t mark)
{
  object_t* obj = (object_t*)POOL_ALLOC(object_t);
  obj->address = address;
  obj->final = NULL;
  obj->rc = 0;
  obj->reachable = true;
  obj->immutable = false;

  // a new object is unmarked
  obj->mark = mark - 1;
  return obj;
}

static void object_free(object_t* obj)
{
  POOL_FREE(object_t, obj);
}

DEFINE_HASHMAP(ponyint_objectmap, objectmap_t, object_t, object_hash,
  object_cmp, ponyint_pool_alloc_size, ponyint_pool_free_size, object_free);

object_t* ponyint_objectmap_getobject(objectmap_t* map, void* address)
{
  object_t obj;
  obj.address = address;

  return ponyint_objectmap_get(map, &obj);
}

object_t* ponyint_objectmap_getorput(objectmap_t* map, void* address,
  uint32_t mark)
{
  object_t* obj = ponyint_objectmap_getobject(map, address);

  if(obj != NULL)
    return obj;

  obj = object_alloc(address, mark);
  ponyint_objectmap_put(map, obj);
  return obj;
}

object_t* ponyint_objectmap_register_final(objectmap_t* map, void* address,
  pony_final_fn final, uint32_t mark)
{
  object_t* obj = ponyint_objectmap_getorput(map, address, mark);
  obj->final = final;
  return obj;
}

void ponyint_objectmap_final(objectmap_t* map)
{
  size_t i = HASHMAP_BEGIN;
  object_t* obj;

  while((obj = ponyint_objectmap_next(map, &i)) != NULL)
  {
    if(obj->final != NULL)
      obj->final(obj->address);
  }
}

size_t ponyint_objectmap_sweep(objectmap_t* map)
{
  size_t count = 0;
  size_t i = HASHMAP_BEGIN;
  object_t* obj;

  while((obj = ponyint_objectmap_next(map, &i)) != NULL)
  {
    void* p = obj->address;

    if(obj->rc > 0)
    {
      // Keep track of whether or not the object was reachable.
      chunk_t* chunk = (chunk_t*)ponyint_pagemap_get(p);
      obj->reachable = ponyint_heap_ismarked(chunk, p);

      if(!obj->reachable)
        ponyint_heap_mark_shallow(chunk, p);
    } else {
      if(obj->final != NULL)
      {
        // If we are not free in the heap, don't run the finaliser and don't
        // remove this entry from the object map.
        chunk_t* chunk = (chunk_t*)ponyint_pagemap_get(p);

        if(ponyint_heap_ismarked(chunk, p))
          continue;

        obj->final(p);
        count++;
      }

      ponyint_objectmap_removeindex(map, i);
      object_free(obj);
    }
  }

  return count;
}
