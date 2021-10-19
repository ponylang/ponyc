#include <string.h>
#include <platform.h>

#include <actor/actor.h>
#include <gc/objectmap.h>
#include <mem/pool.h>

#ifdef _MSC_VER
#  define EXPORT_SYMBOL __declspec(dllexport)
#else
#  define EXPORT_SYMBOL
#endif

extern "C"
{

EXPORT_SYMBOL void* raw_cast(void* p)
{
  return p;
}

static void objectmap_copy(objectmap_t* src, objectmap_t* dst)
{
  size_t i = HASHMAP_BEGIN;
  object_t* obj;
  while((obj = ponyint_objectmap_next(src, &i)) != NULL)
  {
    object_t* obj_copy = POOL_ALLOC(object_t);
    memcpy(obj_copy, obj, sizeof(object_t));
    ponyint_objectmap_put(dst, obj_copy);
  }
}

EXPORT_SYMBOL size_t objectmap_size(objectmap_t* map)
{
  return ponyint_objectmap_size(map);
}

EXPORT_SYMBOL objectmap_t* gc_local(pony_actor_t* actor)
{
  return &actor->gc.local;
}

EXPORT_SYMBOL objectmap_t* gc_local_snapshot(pony_actor_t* actor)
{
  objectmap_t* local = &actor->gc.local;
  objectmap_t* copy = POOL_ALLOC(objectmap_t);
  memset(copy, 0, sizeof(objectmap_t));
  objectmap_copy(local, copy);
  return copy;
}

EXPORT_SYMBOL void gc_local_snapshot_destroy(objectmap_t* map)
{
  ponyint_objectmap_destroy(map);
  POOL_FREE(objectmap_t, map);
}

EXPORT_SYMBOL bool objectmap_has_object(objectmap_t* map, void* address)
{
  size_t i = HASHMAP_BEGIN;
  object_t* obj;
  while((obj = ponyint_objectmap_next(map, &i)) != NULL)
  {
    if(obj->address == address)
      return true;
  }

  return false;
}

EXPORT_SYMBOL bool objectmap_has_object_rc(objectmap_t* map, void* address,
  size_t rc)
{
  size_t i = HASHMAP_BEGIN;
  object_t* obj;
  while((obj = ponyint_objectmap_next(map, &i)) != NULL)
  {
    if(obj->address == address)
      return obj->rc == rc;
  }

  return false;
}

}