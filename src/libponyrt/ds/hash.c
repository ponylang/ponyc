#include "hash.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define DELETED ((void*)1)

static bool valid(void* entry)
{
  return ((uintptr_t)entry) > ((uintptr_t)DELETED);
}

static void* search(hashmap_t* map, size_t* pos, void* key, hash_fn hash,
  cmp_fn cmp)
{
  size_t index_del = map->size;
  size_t mask = index_del - 1;

  size_t h = hash(key);
  size_t index = h & mask;
  void* elem;

  for(uint64_t i = 0; i <= mask; i++)
  {
    elem = map->buckets[index];

    if(elem == NULL)
    {
      if(index_del <= mask)
        *pos = index_del;
      else
        *pos = index;

      return NULL;
    } else if(elem == DELETED) {
      /* some element was here, remember the first deleted slot */
      if(index_del > mask)
        index_del = index;
    } else if(cmp(key, elem)) {
      *pos = index;
      return elem;
    }

    index = (h + ((i + (i * i)) >> 1)) & mask;
  }

  *pos = index_del;
  return NULL;
}

static void resize(hashmap_t* map, hash_fn hash, cmp_fn cmp, alloc_fn alloc,
  free_size_fn fr)
{
  size_t s = map->size;
  void** b = map->buckets;
  void* curr = NULL;

  map->count = 0;
  map->size = (s < 8) ? 8 : s << 3;
  map->buckets = (void**)alloc(map->size * sizeof(void*));
  memset(map->buckets, 0, map->size * sizeof(void*));

  for(size_t i = 0; i < s; i++)
  {
    curr = b[i];

    if(valid(curr))
      hashmap_put(map, curr, hash, cmp, alloc, fr);
  }

  if((fr != NULL) && (b != NULL))
    fr(s * sizeof(void*), b);
}

void hashmap_init(hashmap_t* map, size_t size, alloc_fn alloc)
{
  if(size > 0)
  {
    // make sure we have room for this many elements without resizing
    size <<= 1;

    if(size < 8)
      size = 8;
    else
      size = next_pow2(size);
  }

  map->count = 0;
  map->size = size;

  if(size > 0)
  {
    map->buckets = (void**)alloc(size * sizeof(void*));
    memset(map->buckets, 0, size * sizeof(void*));
  } else {
    map->buckets = NULL;
  }
}

void hashmap_destroy(hashmap_t* map, free_size_fn fr, free_fn free_elem)
{
  if(free_elem != NULL)
  {
    void* curr = NULL;

    for(size_t i = 0; i < map->size; i++)
    {
      curr = map->buckets[i];

      if(valid(curr))
        free_elem(curr);
    }
  }

  if((fr != NULL) && (map->size > 0))
    fr(map->size * sizeof(void*), map->buckets);
}

void* hashmap_get(hashmap_t* map, void* key, hash_fn hash, cmp_fn cmp)
{
  if(map->count == 0)
    return NULL;

  size_t pos;
  return search(map, &pos, key, hash, cmp);
}

void* hashmap_put(hashmap_t* map, void* entry, hash_fn hash, cmp_fn cmp,
  alloc_fn alloc, free_size_fn fr)
{
  if(map->size == 0)
    hashmap_init(map, 4, alloc);

  size_t pos;
  void* elem = search(map, &pos, entry, hash, cmp);

  map->buckets[pos] = entry;

  if(elem == NULL)
  {
    map->count++;

    if((map->count << 1) > map->size)
      resize(map, hash, cmp, alloc, fr);
  }

  return elem;
}

void* hashmap_remove(hashmap_t* map, void* entry, hash_fn hash, cmp_fn cmp)
{
  if(map->count == 0)
    return NULL;

  size_t pos;
  void* elem = search(map, &pos, entry, hash, cmp);

  if(elem != NULL)
  {
    map->buckets[pos] = DELETED;
    map->count--;
  }

  return elem;
}

void* hashmap_removeindex(hashmap_t* map, size_t index)
{
  if(map->size <= index)
    return NULL;

  void* elem = map->buckets[index];

  if(!valid(elem))
    return NULL;

  map->buckets[index] = DELETED;
  map->count--;
  return elem;
}

void* hashmap_next(hashmap_t* map, size_t* i)
{
  if(map->count == 0)
    return NULL;

  void* elem = NULL;
  size_t index = *i + 1;

  while(index < map->size)
  {
    elem = map->buckets[index];

    if(valid(elem))
    {
      *i = index;
      return elem;
    }

    index++;
  }

  *i = index;
  return NULL;
}

size_t hashmap_size(hashmap_t* map)
{
  return map->count;
}

void hashmap_trace(hashmap_t* map, pony_trace_fn t)
{
  void* elem = NULL;
  size_t i = HASHMAP_BEGIN;

  pony_trace(map->buckets);

  while((elem = hashmap_next(map, &i)) != NULL)
    pony_traceobject(elem, t);
}
