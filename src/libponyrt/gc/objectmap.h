#ifndef gc_objectmap_h
#define gc_objectmap_h

#include "../ds/hash.h"
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct object_t object_t;

void* object_address(object_t* obj);

size_t object_rc(object_t* obj);

bool object_marked(object_t* obj, size_t mark);

void object_mark(object_t* obj, size_t mark);

void object_inc(object_t* obj);

void object_inc_more(object_t* obj);

void object_inc_some(object_t* obj, size_t rc);

bool object_dec(object_t* obj);

void object_dec_some(object_t* obj, size_t rc);

DECLARE_HASHMAP(objectmap, object_t);

object_t* objectmap_getobject(objectmap_t* map, void* address);

object_t* objectmap_getorput(objectmap_t* map, void* address, size_t mark);

void objectmap_mark(objectmap_t* map);

PONY_EXTERN_C_END

#endif
