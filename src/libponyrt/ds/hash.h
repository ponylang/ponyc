#ifndef ds_hash_h
#define ds_hash_h

#include "fun.h"
#include <pony.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HASHMAP_BEGIN ((size_t)-1)

/** Definition of a quadratic probing hash map.
 *
 *  Do not access the fields of this type.
 */
typedef struct hashmap_t
{
  size_t count;   /* number of elements in the map */
  size_t size;    /* size of the buckets array */
  void** buckets;
} hashmap_t;

/** Initializes a new hash map.
 *
 *  This is a quadratic probing hash map.
 */
void hashmap_init(hashmap_t* map, size_t size, alloc_fn alloc);

/** Destroys a hash map.
 *
 */
void hashmap_destroy(hashmap_t* map, free_size_fn fr, free_fn free_elem);

/** Compact a hash map.
 *
 *  Automatic compaction is not provided.
 */
hashmap_t* hashmap_compact(hashmap_t* map, hash_fn hash, cmp_fn cmp,
  alloc_fn alloc, free_size_fn fr);

/** Retrieve an element from a hash map.
 *
 *  Returns a pointer to the element, or NULL.
 */
void* hashmap_get(hashmap_t* map, void* key, hash_fn hash, cmp_fn cmp);

/** Put a new element in a hash map.
 *
 *  If the element (according to cmp_fn) is already in the hash map, the old
 *  element is overwritten and returned to the caller.
 */
void* hashmap_put(hashmap_t* map, void* entry, hash_fn hash, cmp_fn cmp,
  alloc_fn alloc, free_size_fn fr);

/** Removes a given entry from a hash map.
 *
 *  Returns the element removed (if any).
 */
void* hashmap_remove(hashmap_t* map, void* entry, hash_fn hash, cmp_fn cmp);

/** Removes a given entry from a hash map by index.
 *
 *  Returns the element removed (if any).
 */
void* hashmap_removeindex(hashmap_t* map, size_t index);

/** Get the number of elements in the map.
 */
size_t hashmap_size(hashmap_t* map);

/** Hashmap iterator.
 *
 *  Set i to HASHMAP_BEGIN, then call until this returns NULL.
 */
void* hashmap_next(hashmap_t* map, size_t* i);

/** GC trace hashmap.
 *
 */
void hashmap_trace(hashmap_t* map, pony_trace_fn t);

#define DECLARE_HASHMAP(name, type) \
  typedef struct name##_t { hashmap_t contents; } name##_t; \
  void name##_init(name##_t* map, size_t size); \
  void name##_destroy(name##_t* map); \
  name##_t* name##_compact(name##_t* map); \
  type* name##_get(name##_t* map, type* key); \
  type* name##_put(name##_t* map, type* entry); \
  type* name##_remove(name##_t* map, type* entry); \
  type* name##_removeindex(name##_t* map, size_t index); \
  size_t name##_size(name##_t* map); \
  type* name##_next(name##_t* map, size_t* i); \
  void name##_trace(void* map); \

#define DEFINE_HASHMAP(name, type, hash, cmp, alloc, fr, free_elem, trace)\
  typedef struct name##_t name##_t; \
  typedef uint64_t (*name##_hash_fn)(type* m); \
  typedef bool (*name##_cmp_fn)(type* a, type* b); \
  typedef void (*name##_free_fn)(type* a); \
  typedef void (*name##_trace_fn)(void* a); \
  \
  void name##_init(name##_t* map, size_t size) \
  { \
    alloc_fn allocf = alloc; \
    hashmap_init((hashmap_t*)map, size, allocf); \
  } \
  void name##_destroy(name##_t* map) \
  { \
    name##_free_fn freef = free_elem; \
    hashmap_destroy((hashmap_t*)map, fr, (free_fn)freef); \
  } \
  name##_t* name##_compact(name##_t* map) \
  { \
    name##_hash_fn hashf = hash; \
    name##_cmp_fn cmpf = cmp; \
    return (name##_t*)hashmap_compact((hashmap_t*)map, (hash_fn)hashf, \
      (cmp_fn)cmpf, alloc, fr); \
  } \
  type* name##_get(name##_t* map, type* key) \
  { \
    name##_hash_fn hashf = hash; \
    name##_cmp_fn cmpf = cmp; \
    return (type*)hashmap_get((hashmap_t*)map, (void*)key, (hash_fn)hashf, \
      (cmp_fn)cmpf); \
  } \
  type* name##_put(name##_t* map, type* entry) \
  { \
    name##_hash_fn hashf = hash; \
    name##_cmp_fn cmpf = cmp; \
    return (type*)hashmap_put((hashmap_t*)map, (void*)entry, (hash_fn)hashf, \
      (cmp_fn)cmpf, alloc, fr); \
  } \
  type* name##_remove(name##_t* map, type* entry) \
  { \
    name##_hash_fn hashf = hash; \
    name##_cmp_fn cmpf = cmp; \
    return (type*)hashmap_remove((hashmap_t*) map, (void*)entry, \
      (hash_fn)hashf, (cmp_fn)cmpf); \
  } \
  type* name##_removeindex(name##_t* map, size_t index) \
  { \
    return (type*)hashmap_removeindex((hashmap_t*) map, index); \
  } \
  size_t name##_size(name##_t* map) \
  { \
    return hashmap_size((hashmap_t*)map); \
  } \
  type* name##_next(name##_t* map, size_t* i) \
  { \
    return (type*)hashmap_next((hashmap_t*)map, i); \
  } \
  void name##_trace(void* map) \
  { \
    name##_trace_fn tracef = trace; \
    hashmap_trace((hashmap_t*)map, (pony_trace_fn)tracef); \
  } \

#ifdef __cplusplus
}
#endif

#endif
