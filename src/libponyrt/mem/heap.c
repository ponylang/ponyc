#include "heap.h"
#include "pool.h"
#include "pagemap.h"
#include "../ds/fun.h"
#include <string.h>
#include <assert.h>

#include <platform/platform.h>

#define HEAP_INITIALGC (1 << 14)
#define HEAP_MIN (1 << HEAP_MINBITS)
#define HEAP_MAX (1 << HEAP_MAXBITS)

struct chunk_t
{
  // immutable
  pony_actor_t* actor;
  char* m;
  uint64_t size;

  // mutable
  uint32_t slots;
  uint32_t shallow;

  struct chunk_t* next;
};

typedef char block_t[HEAP_MAX];
typedef void (*chunk_fn)(chunk_t* chunk);

static const uint32_t sizeclass_size[HEAP_SIZECLASSES] =
{
  HEAP_MIN << 0,
  HEAP_MIN << 1,
  HEAP_MIN << 2,
  HEAP_MIN << 3,
  HEAP_MIN << 4,
  HEAP_MIN << 5
};

static const uint32_t sizeclass_empty[HEAP_SIZECLASSES] =
{
  0xFFFFFFFF,
  0x55555555,
  0x11111111,
  0x01010101,
  0x00010001,
  0x00000001
};

static const uint8_t sizeclass_table[HEAP_MAX / HEAP_MIN] =
{
  0, 1, 2, 2, 3, 3, 3, 3,
  4, 4, 4, 4, 4, 4, 4, 4,
  5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5,
};

static void clear_small(chunk_t* chunk)
{
  chunk->slots = sizeclass_empty[chunk->size];
  chunk->shallow = chunk->slots;
}

static void clear_large(chunk_t* chunk)
{
  chunk->slots = 1;
  chunk->shallow = 1;
}

static void destroy_small(chunk_t* chunk)
{
  POOL_FREE(block_t, chunk->m);
  POOL_FREE(chunk_t, chunk);
}

static void destroy_large(chunk_t* chunk)
{
  pool_free_size(chunk->size, chunk->m);
  POOL_FREE(chunk_t, chunk);
}

static size_t sweep_small(chunk_t* chunk, chunk_t** avail, chunk_t** full,
  uint32_t empty)
{
  size_t used = 0;
  chunk_t* next;

  while(chunk != NULL)
  {
    next = chunk->next;
    chunk->slots &= chunk->shallow;

    if(chunk->slots == 0)
    {
      used += sizeof(block_t);
      chunk->next = *full;
      *full = chunk;
    } else if(chunk->slots == empty) {
      destroy_small(chunk);
    } else {
      used += sizeof(block_t) -
        (__pony_popcount(chunk->slots) * sizeclass_size[chunk->size]);
      chunk->next = *avail;
      *avail = chunk;
    }

    chunk = next;
  }

  return used;
}

static chunk_t* sweep_large(chunk_t* chunk, size_t* used)
{
  chunk_t* list = NULL;
  chunk_t* next;

  while(chunk != NULL)
  {
    next = chunk->next;
    chunk->slots &= chunk->shallow;

    if(chunk->slots == 0)
    {
      chunk->next = list;
      list = chunk;
      *used += chunk->size;
    } else {
      destroy_large(chunk);
    }

    chunk = next;
  }

  return list;
}

static void chunk_list(chunk_fn f, chunk_t* current)
{
  chunk_t* next;

  while(current != NULL)
  {
    next = current->next;
    f(current);
    current = next;
  }
}

static void* small_malloc(pony_actor_t* actor, heap_t* heap, size_t size)
{
  // size is in range 1..HEAP_MAX
  // change to 0..((HEAP_MAX / HEAP_MIN) - 1) and look up in table
  uint32_t sizeclass = sizeclass_table[(size - 1) >> HEAP_MINBITS];
  chunk_t* chunk = heap->small_free[sizeclass];

  // if there are none in this size class, get a new one
  if(chunk == NULL)
  {
    chunk_t* n = (chunk_t*) POOL_ALLOC(chunk_t);
    n->actor = actor;
    n->m = (char*) POOL_ALLOC(block_t);
    n->size = sizeclass;
    n->slots = sizeclass_empty[sizeclass];
    n->next = chunk;

    pagemap_set(n->m, n);

    for(int i = 0; i < HEAP_MAX; i += 64)
    {
      assert(pagemap_get(n->m + i) == n);
    }

    heap->small_free[sizeclass] = n;
    chunk = n;
  }

  // get the first available slot and clear it
  uint32_t bit = __pony_ffs(chunk->slots) - 1;
  chunk->slots &= ~(1 << bit);
  void* m = chunk->m + (bit << HEAP_MINBITS);

  // if we're full, move us to the full list
  if(chunk->slots == 0)
  {
    heap->small_free[sizeclass] = chunk->next;
    chunk->next = heap->small_full[sizeclass];
    heap->small_full[sizeclass] = chunk;
  }

  heap->used += sizeclass_size[sizeclass];
  return m;
}

static void* large_malloc(pony_actor_t* actor, heap_t* heap, size_t size)
{
  chunk_t* chunk = (chunk_t*) POOL_ALLOC(chunk_t);
  chunk->actor = actor;
  chunk->size = size;
  chunk->m = (char*) pool_alloc_size(size);
  chunk->slots = 0;

  char* p = chunk->m;
  char* end = chunk->m + chunk->size;

  while(p < end)
  {
    pagemap_set(p, chunk);
    p += HEAP_MAX;
  }

  chunk->next = heap->large;
  heap->large = chunk;
  heap->used += chunk->size;

  return chunk->m;
}

void heap_init(heap_t* heap)
{
  memset(heap, 0, sizeof(heap_t));
  heap->next_gc = HEAP_INITIALGC;
}

void heap_destroy(heap_t* heap)
{
  chunk_list(destroy_large, heap->large);

  for(int i = 0; i < HEAP_SIZECLASSES; i++)
  {
    chunk_list(destroy_small, heap->small_free[i]);
    chunk_list(destroy_small, heap->small_full[i]);
  }
}

void* heap_alloc(pony_actor_t* actor, heap_t* heap, size_t size)
{
  if(size == 0)
  {
    return NULL;
  } else if(size <= sizeof(block_t)) {
    return small_malloc(actor, heap, size);
  } else {
    return large_malloc(actor, heap, size);
  }
}

void* heap_realloc(pony_actor_t* actor, heap_t* heap, void* p, size_t size)
{
  if(p == NULL)
    return heap_alloc(actor, heap, size);

  chunk_t* chunk = (chunk_t*)pagemap_get(p);

  if(chunk->size < HEAP_SIZECLASSES)
  {
    // Previous allocation was a small_malloc.
    if(size <= sizeof(block_t))
    {
      uint32_t sizeclass = sizeclass_table[(size - 1) >> HEAP_MINBITS];

      // If the new allocation is the same size or smaller, return the old one.
      if(sizeclass <= chunk->size)
        return p;
    }

    // Get new memory and copy from the old memory.
    void* q = heap_alloc(actor, heap, size);
    memcpy(q, p, 1 << (chunk->size + HEAP_MINBITS));
    return q;
  }

  // Previous allocation was a large_malloc.
  if(size <= chunk->size)
    return p;

  // Reallocate the large_malloc.
  chunk->m = (char*)pool_realloc_size(p, chunk->size, size);
  chunk->size = size;

  return chunk->m;
}

void heap_used(heap_t* heap, size_t size)
{
  heap->used += size;
}

bool heap_startgc(heap_t* heap)
{
  if(heap->used < heap->next_gc)
    return false;

  for(int i = 0; i < HEAP_SIZECLASSES; i++)
  {
    chunk_list(clear_small, heap->small_free[i]);
    chunk_list(clear_small, heap->small_full[i]);
  }

  chunk_list(clear_large, heap->large);

  // reset used to zero
  heap->used = 0;
  return true;
}

bool heap_mark(chunk_t* chunk, void* p)
{
  bool marked;

  if(chunk->size >= HEAP_SIZECLASSES)
  {
    marked = chunk->slots == 0;
    chunk->slots = 0;
  } else {
    // shift to account for smallest allocation size
    uint32_t slot = 1 << ((uintptr_t)((char*)p - chunk->m) >> HEAP_MINBITS);

    // check if it was already marked
    marked = (chunk->slots & slot) == 0;

    // a clear bit is in-use, a set bit is available
    chunk->slots &= ~slot;
  }

  return marked;
}

void heap_mark_shallow(chunk_t* chunk, void* p)
{
  if(chunk->size >= HEAP_SIZECLASSES)
  {
    chunk->shallow = 0;
  } else {
    // shift to account for smallest allocation size
    uint32_t slot = 1 << ((uintptr_t)((char*)p - chunk->m) >> HEAP_MINBITS);

    // a clear bit is in-use, a set bit is available
    chunk->shallow &= ~slot;
  }
}

void heap_endgc(heap_t* heap)
{
  size_t used = 0;

  for(int i = 0; i < HEAP_SIZECLASSES; i++)
  {
    chunk_t* list1 = heap->small_free[i];
    chunk_t* list2 = heap->small_full[i];

    heap->small_free[i] = NULL;
    heap->small_full[i] = NULL;

    chunk_t** avail = &heap->small_free[i];
    chunk_t** full = &heap->small_full[i];

    used += sweep_small(list1, avail, full, sizeclass_empty[i]);
    used += sweep_small(list2, avail, full, sizeclass_empty[i]);
  }

  heap->large = sweep_large(heap->large, &used);

  // foreign object sizes will have been added to heap->used already. here we
  // add local object sizes as well and set the next gc point for when memory
  // usage has doubled.
  heap->used += used;
  heap->next_gc = heap->used << 1;

  if(heap->next_gc < HEAP_INITIALGC)
    heap->next_gc = HEAP_INITIALGC;
}

pony_actor_t* heap_owner(chunk_t* chunk)
{
  // FIX: false sharing
  // reading from something that will never be written
  // but is on a cache line that will often be written
  // called during tracing
  // actual chunk only needed for GC tracing
  // all other tracing only needs the owner
  // so the owner needs the chunk and everyone else just needs the owner
  return chunk->actor;
}

size_t heap_base(chunk_t* chunk, void** p)
{
  if(chunk->size >= HEAP_SIZECLASSES)
  {
    *p = chunk->m;
    return chunk->size;
  }

  size_t size = sizeclass_size[chunk->size];
  *p = (void*)((uintptr_t)(*p) & ~(size - 1));
  return size;
}
