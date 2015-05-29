#include "objectmap.h"
#include "gc.h"
#include "../ds/hash.h"
#include "../ds/fun.h"
#include "../mem/pool.h"
#include "../mem/pagemap.h"
#include <assert.h>

typedef struct object_t
{
  void* address;
  pony_final_fn final;
  size_t rc;
  uint32_t mark;
} object_t;

static uint64_t object_hash(object_t* obj)
{
  return hash_ptr(obj->address);
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

  // a new object is unmarked
  obj->mark = mark - 1;
  return obj;
}

static void object_free(object_t* obj)
{
  POOL_FREE(object_t, obj);
}

void* object_address(object_t* obj)
{
  return obj->address;
}

size_t object_rc(object_t* obj)
{
  return obj->rc;
}

bool object_marked(object_t* obj, uint32_t mark)
{
  return obj->mark == mark;
}

void object_mark(object_t* obj, uint32_t mark)
{
  obj->mark = mark;
}

void object_inc(object_t* obj)
{
  obj->rc++;
  assert(obj->rc > 0);
}

void object_inc_more(object_t* obj)
{
  assert(obj->rc == 0);
  obj->rc = GC_INC_MORE;
}

void object_inc_some(object_t* obj, size_t rc)
{
  obj->rc += rc;
}

bool object_dec(object_t* obj)
{
  assert(obj->rc > 0);
  obj->rc--;
  return obj->rc > 0;
}

void object_dec_some(object_t* obj, size_t rc)
{
  assert(obj->rc >= rc);
  obj->rc -= rc;
}

DEFINE_HASHMAP(objectmap, object_t, object_hash, object_cmp, pool_alloc_size,
  pool_free_size, object_free);

object_t* objectmap_getobject(objectmap_t* map, void* address)
{
  object_t obj;
  obj.address = address;

  return objectmap_get(map, &obj);
}

object_t* objectmap_getorput(objectmap_t* map, void* address, uint32_t mark)
{
  object_t* obj = objectmap_getobject(map, address);

  if(obj != NULL)
    return obj;

  obj = object_alloc(address, mark);
  objectmap_put(map, obj);
  return obj;
}

object_t* objectmap_register_final(objectmap_t* map, void* address,
  pony_final_fn final, uint32_t mark)
{
  object_t* obj = objectmap_getorput(map, address, mark);
  obj->final = final;
  return obj;
}

void objectmap_final(objectmap_t* map)
{
  size_t i = HASHMAP_BEGIN;
  object_t* obj;

  while((obj = objectmap_next(map, &i)) != NULL)
  {
    if(obj->final != NULL)
      obj->final(obj->address);
  }
}

size_t objectmap_sweep(objectmap_t* map)
{
  size_t count = 0;
  size_t i = HASHMAP_BEGIN;
  object_t* obj;

  while((obj = objectmap_next(map, &i)) != NULL)
  {
    if(obj->rc > 0)
    {
      pony_trace(obj->address);
    } else {
      if(obj->final != NULL)
      {
        // If we are not free in the heap, don't run the finaliser and don't
        // remove this entry from the object map.
        chunk_t* chunk = (chunk_t*)pagemap_get(obj->address);

        if(heap_ismarked(chunk, obj->address))
          continue;

        obj->final(obj->address);
        count++;
      }

      objectmap_removeindex(map, i);
      object_free(obj);
    }
  }

  return count;
}
