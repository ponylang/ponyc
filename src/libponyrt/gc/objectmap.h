#ifndef gc_objectmap_h
#define gc_objectmap_h

#include "../ds/hash.h"
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct object_t
{
  void* address;
  size_t rc;
  uint32_t mark;
  // Number of in-flight self-sent messages that reference this object. While
  // non-zero the object is sitting in the owning actor's own queue (it was sent
  // to itself) but is not reachable from its fields; the local sweep must not
  // release it to its owner, or the owner could collect it before the actor
  // receives it back. Incremented by the send-trace on a self-send, decremented
  // by the recv-trace, and checked by the sweep (move_unmarked_objects). Sized
  // and placed to fit in existing padding so object_t does not grow; a count
  // this wide cannot realistically overflow, and if it did the only cost would
  // be lost acquire amortisation, never incorrect collection (the reference
  // counting is unchanged).
  uint16_t self_send_pins;
  bool immutable;
  pony_type_t* type;
} object_t;

DECLARE_HASHMAP(ponyint_objectmap, objectmap_t, object_t);

object_t* ponyint_objectmap_getobject(objectmap_t* map, void* address, size_t* index);

object_t* ponyint_objectmap_getorput(objectmap_t* map, void* address,
  pony_type_t* type, uint32_t mark);

void ponyint_objectmap_sweep(objectmap_t* map);

#ifdef USE_RUNTIMESTATS
size_t ponyint_objectmap_total_mem_size(objectmap_t* map);

size_t ponyint_objectmap_total_alloc_size(objectmap_t* map);
#endif

PONY_EXTERN_C_END

#endif
