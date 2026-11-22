#ifndef gc_objectmap_h
#define gc_objectmap_h

#include "../ds/hash.h"
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct object_t
{
  void* address;
  size_t rc;
  uint32_t mark; // A nonzero mark holds the epoch plus the visit offset.
  // Approximate number of queued self-sends that reference this object.
  // Losing a pin can cause another acquire. The message itself keeps the
  // object alive. The 16-bit count fits in padding on LP64.
  uint16_t self_send_pins;
  bool immutable;
  pony_type_t* type;
} object_t;

// An epoch reserves consecutive marks for the different ways an object has
// been visited. Older marks are smaller, including zero for an unvisited
// object. All marks are cleared before epoch numbers are reused.
static inline bool ponyint_object_marked(const object_t* obj, uint32_t epoch)
{
  return obj->mark >= epoch;
}

DECLARE_HASHMAP(ponyint_objectmap, objectmap_t, object_t);

object_t* ponyint_objectmap_getobject(objectmap_t* map, void* address, size_t* index);

// A typed visit supplies the descriptor if the first visit was untyped.
object_t* ponyint_objectmap_getorput(objectmap_t* map, void* address,
  pony_type_t* type);

void ponyint_objectmap_sweep(objectmap_t* map);

#ifdef USE_RUNTIMESTATS
size_t ponyint_objectmap_total_mem_size(objectmap_t* map);

size_t ponyint_objectmap_total_alloc_size(objectmap_t* map);
#endif

PONY_EXTERN_C_END

#endif
