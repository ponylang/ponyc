#include "objectmap.h"
#include "gc.h"
#include "../ds/hash.h"
#include "../ds/fun.h"
#include "../mem/pool.h"
#include <assert.h>

typedef struct object_t
{
  void* address;
  size_t rc;
  size_t mark;
} object_t;

static uint64_t object_hash(object_t* obj)
{
  return hash_ptr(obj->address);
}

static bool object_cmp(object_t* a, object_t* b)
{
  return a->address == b->address;
}

static object_t* object_alloc(void* address, size_t mark)
{
  object_t* obj = (object_t*)POOL_ALLOC(object_t);
  obj->address = address;
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

bool object_marked(object_t* obj, size_t mark)
{
  return obj->mark == mark;
}

void object_mark(object_t* obj, size_t mark)
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

DEFINE_HASHMAP(objectmap, object_t, object_hash, object_cmp,
  pool_alloc_size, pool_free_size, object_free, NULL);

object_t* objectmap_getobject(objectmap_t* map, void* address)
{
  object_t obj;
  obj.address = address;

  return objectmap_get(map, &obj);
}

object_t* objectmap_getorput(objectmap_t* map, void* address, size_t mark)
{
  object_t* obj = objectmap_getobject(map, address);

  if(obj != NULL)
    return obj;

  obj = object_alloc(address, mark);
  objectmap_put(map, obj);
  return obj;
}

void objectmap_mark(objectmap_t* map)
{
  size_t i = HASHMAP_BEGIN;
  object_t* obj;

  while((obj = objectmap_next(map, &i)) != NULL)
  {
    if(obj->rc > 0)
    {
      pony_trace(obj->address);
    } else {
      objectmap_removeindex(map, i);
      object_free(obj);
    }
  }
}
