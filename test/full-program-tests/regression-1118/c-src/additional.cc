#include <platform.h>

#include <actor/actor.h>
#include <gc/gc.h>
#include <gc/actormap.h>

#ifdef _MSC_VER
#  define EXPORT_SYMBOL __declspec(dllexport)
#else
#  define EXPORT_SYMBOL
#endif

extern "C"
{

// Return a pointer to the given actor's foreign actor-reference map.
EXPORT_SYMBOL actormap_t* gc_foreign(pony_actor_t* actor)
{
  return &actor->gc.foreign;
}

// Does the foreign actormap contain a reference to `target`?
EXPORT_SYMBOL bool actormap_has_actor(actormap_t* map, pony_actor_t* target)
{
  size_t i = HASHMAP_BEGIN;
  actorref_t* aref;
  while((aref = ponyint_actormap_next(map, &i)) != NULL)
  {
    if(aref->actor == target)
      return true;
  }

  return false;
}

// The reference count the foreign actormap records for `target`, or
// (size_t)-1 if `target` is absent.
EXPORT_SYMBOL size_t actormap_actor_rc(actormap_t* map, pony_actor_t* target)
{
  size_t i = HASHMAP_BEGIN;
  actorref_t* aref;
  while((aref = ponyint_actormap_next(map, &i)) != NULL)
  {
    if(aref->actor == target)
      return aref->rc;
  }

  return (size_t)-1;
}

}
