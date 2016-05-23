#ifndef gc_objectmap_h
#define gc_objectmap_h

#include "../ds/hash.h"
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct object_t
{
  void* address;
  pony_final_fn final;
  size_t rc;
  uint32_t mark;
  bool immutable;
} object_t;

DECLARE_HASHMAP(ponyint_objectmap, objectmap_t, object_t);

object_t* ponyint_objectmap_getobject(objectmap_t* map, void* address);

object_t* ponyint_objectmap_getorput(objectmap_t* map, void* address,
  uint32_t mark);

object_t* ponyint_objectmap_register_final(objectmap_t* map, void* address,
  pony_final_fn final, uint32_t mark);

void ponyint_objectmap_final(objectmap_t* map);

size_t ponyint_objectmap_sweep(objectmap_t* map);

PONY_EXTERN_C_END

#endif
