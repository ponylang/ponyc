#ifndef ds_hash_h
#define ds_hash_h

#include "fun.h"
#include "../pony.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

#define HASHMAP_BEGIN ((size_t)-1)
#define HASHMAP_UNKNOWN ((size_t)-1)

#ifdef PLATFORM_IS_ILP32
  typedef uint32_t bitmap_t;
  #define HASHMAP_BITMAP_TYPE_BITS 5 // 2^5 = 32
#else
  typedef uint64_t bitmap_t;
  #define HASHMAP_BITMAP_TYPE_BITS 6 // 2^6 = 64
#endif

#define HASHMAP_BITMAP_TYPE_SIZE (1 << HASHMAP_BITMAP_TYPE_BITS)
#define HASHMAP_BITMAP_TYPE_MASK (HASHMAP_BITMAP_TYPE_SIZE - 1)

/** Definition of a hash map entry for uintptr_t.
 */
typedef struct hashmap_entry_t
{
  void* ptr;         /* pointer */
  size_t hash;       /* hash */
} hashmap_entry_t;

/** Definition of a linear probing robin hood hash map
 *  with backward shift deletion.
 *
 *  Do not access the fields of this type.
 */
typedef struct hashmap_t
{
  size_t count;   /* number of elements in the map */
  size_t size;    /* size of the buckets array */
  bitmap_t* item_bitmap;   /* Item bitarray to keep track items for optimized scanning */
  hashmap_entry_t* buckets;
} hashmap_t;

/** Initializes a new hash map.
 *
 *  This is a quadratic probing hash map.
 */
void ponyint_hashmap_init(hashmap_t* map, size_t size, alloc_fn alloc);

/** Destroys a hash map.
 *
 */
void ponyint_hashmap_destroy(hashmap_t* map, free_size_fn fr,
  free_fn free_elem);

/** Optimize hashmap by iterating hashmap and calling optimize_item for each entry.
 */
void ponyint_hashmap_optimize(hashmap_t* map, alloc_fn alloc,
  free_size_fn fr, cmp_fn cmp);

/** Retrieve an element from a hash map.
 *
 *  Returns a pointer to the element, or NULL.
 */
void* ponyint_hashmap_get(hashmap_t* map, void* key, size_t hash, cmp_fn cmp, size_t* index);

/** Put a new element in a hash map.
 *
 *  If the element (according to cmp_fn) is already in the hash map, the old
 *  element is overwritten and returned to the caller.
 */
void* ponyint_hashmap_put(hashmap_t* map, void* entry, size_t hash, cmp_fn cmp,
  alloc_fn alloc, free_size_fn fr);


/** Put a new element in a hash map at a specific index (shifting other elements
 *  if necessary).
 */
void ponyint_hashmap_putindex(hashmap_t* map, void* entry, size_t hash, cmp_fn cmp,
  alloc_fn alloc, free_size_fn fr, size_t index);

/** Removes a given entry from a hash map.
 *
 *  Returns the element removed (if any).
 */
void* ponyint_hashmap_remove(hashmap_t* map, void* entry, size_t hash,
  cmp_fn cmp);

/** Removes a given entry from a hash map by index.
 *
 *  Returns the element removed (if any).
 */
void ponyint_hashmap_removeindex(hashmap_t* map, size_t index);

/** Clears a given entry from a hash map by index.
 *
 *  Returns the element clear (if any).
 */
void ponyint_hashmap_clearindex(hashmap_t* map, size_t index);

/** Get the number of elements in the map.
 */
size_t ponyint_hashmap_size(hashmap_t* map);

/** Hashmap iterator.
 *
 *  Set i to HASHMAP_BEGIN, then call until this returns NULL.
 */
void* ponyint_hashmap_next(size_t* i, size_t count, bitmap_t* item_bitmap,
  size_t size, hashmap_entry_t* buckets);

#define DECLARE_HASHMAP(name, name_t, type) \
  typedef struct name_t { hashmap_t contents; } name_t; \
  void name##_init(name_t* map, size_t size); \
  void name##_destroy(name_t* map); \
  void name##_optimize(name_t* map); \
  type* name##_get(name_t* map, type* key, size_t* index); \
  type* name##_put(name_t* map, type* entry); \
  void name##_putindex(name_t* map, type* entry, size_t index); \
  type* name##_remove(name_t* map, type* entry); \
  void name##_removeindex(name_t* map, size_t index); \
  void name##_clearindex(name_t* map, size_t index); \
  size_t name##_size(name_t* map); \
  type* name##_next(name_t* map, size_t* i); \
  void name##_trace(void* map); \

#define DEFINE_HASHMAP(name, name_t, type, hash, cmp, alloc, fr, free_elem) \
  typedef struct name_t name_t; \
  typedef bool (*name##_cmp_fn)(type* a, type* b); \
  typedef void (*name##_free_fn)(type* a); \
  typedef void (*name_trace_fn)(void* a); \
  \
  void name##_init(name_t* map, size_t size) \
  { \
    alloc_fn allocf = alloc; \
    ponyint_hashmap_init((hashmap_t*)map, size, allocf); \
  } \
  void name##_destroy(name_t* map) \
  { \
    name##_free_fn freef = free_elem; \
    ponyint_hashmap_destroy((hashmap_t*)map, fr, (free_fn)freef); \
  } \
  void name##_optimize(name_t* map) \
  { \
    name##_cmp_fn cmpf = cmp; \
    return ponyint_hashmap_optimize((hashmap_t*)map, \
      alloc, fr, (cmp_fn)cmpf); \
  } \
  type* name##_get(name_t* map, type* key, size_t* index) \
  { \
    name##_cmp_fn cmpf = cmp; \
    return (type*)ponyint_hashmap_get((hashmap_t*)map, (void*)key, \
      hash(key), (cmp_fn)cmpf, index); \
  } \
  type* name##_put(name_t* map, type* entry) \
  { \
    name##_cmp_fn cmpf = cmp; \
    return (type*)ponyint_hashmap_put((hashmap_t*)map, (void*)entry, \
      hash(entry), (cmp_fn)cmpf, alloc, fr); \
  } \
  void name##_putindex(name_t* map, type* entry, size_t index) \
  { \
    name##_cmp_fn cmpf = cmp; \
    ponyint_hashmap_putindex((hashmap_t*)map, (void*)entry, \
      hash(entry), (cmp_fn)cmpf, alloc, fr, index); \
  } \
  type* name##_remove(name_t* map, type* entry) \
  { \
    name##_cmp_fn cmpf = cmp; \
    return (type*)ponyint_hashmap_remove((hashmap_t*) map, (void*)entry, \
      hash(entry), (cmp_fn)cmpf); \
  } \
  void name##_removeindex(name_t* map, size_t index) \
  { \
    ponyint_hashmap_removeindex((hashmap_t*) map, index); \
  } \
  void name##_clearindex(name_t* map, size_t index) \
  { \
    ponyint_hashmap_clearindex((hashmap_t*) map, index); \
  } \
  size_t name##_size(name_t* map) \
  { \
    return ponyint_hashmap_size((hashmap_t*)map); \
  } \
  type* name##_next(name_t* map, size_t* i) \
  { \
    hashmap_t* h = (hashmap_t*)map; \
    return (type*)ponyint_hashmap_next(i, h->count, h->item_bitmap, \
      h->size, h->buckets); \
  } \

#define HASHMAP_INIT {0, 0, NULL}

PONY_EXTERN_C_END

#endif
