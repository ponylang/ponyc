#include "heap.h"
#include "pagemap.h"
#include "../ds/fun.h"
#include "../actor/actor.h"
#include "ponyassert.h"
#include <string.h>

#include <platform.h>
#include <dtrace.h>

typedef struct chunk_t
{
  // used for pointer tagging
  // bit 0 (lowest bit) for keeping track of chunk type (1 = small; 0 = large)
  // bit 1 for keeping track of chunks to be cleared
  char* m;
} chunk_t;

typedef struct large_chunk_t
{
  // stores slot/shallow/finaliser info as lowest bits 2 - 4 of chunk->m
  chunk_t base;

  // immutable
  size_t size;

  // mutable
  struct large_chunk_t* next;
} large_chunk_t;

typedef struct small_chunk_t
{
  // stores sizeclass info as lowest bits 2 - 4 of chunk->m
  chunk_t base;

  // mutable
  uint32_t slots;
  uint32_t shallow;
  uint32_t finalisers;
  struct small_chunk_t* next;
} small_chunk_t;


#define CHUNK_TYPE_BITMASK (uintptr_t)0x1
#define CHUNK_NEEDS_TO_BE_CLEARED_BITMASK (uintptr_t)0x2
#define SMALL_CHUNK_SIZECLASS_BITMASK (uintptr_t)0x1C
// TODO: figure out how to calculate the `2` from `SMALL_CHUNK_SIZECLASS_BITMASK` at compile time
#define SMALL_CHUNK_SIZECLASS_SHIFT (uintptr_t)2
#define LARGE_CHUNK_SLOT_BITMASK (uintptr_t)0x4
#define LARGE_CHUNK_SHALLOW_BITMASK (uintptr_t)0x8
#define LARGE_CHUNK_FINALISER_BITMASK (uintptr_t)0x10
#define CHUNK_M_BITMASK ~(CHUNK_TYPE_BITMASK | CHUNK_NEEDS_TO_BE_CLEARED_BITMASK | SMALL_CHUNK_SIZECLASS_BITMASK | LARGE_CHUNK_SLOT_BITMASK | LARGE_CHUNK_SHALLOW_BITMASK | LARGE_CHUNK_FINALISER_BITMASK)

enum
{
  FORCE_NO_FINALISERS = 0,
  FORCE_ALL_FINALISERS = 0xFFFFFFFF,
};

typedef char block_t[POOL_ALIGN];
typedef void (*large_chunk_fn)(large_chunk_t* chunk, uint32_t mark);
typedef void (*small_chunk_fn)(small_chunk_t* chunk, uint32_t mark);

#define SIZECLASS_SIZE(sizeclass) (HEAP_MIN << (sizeclass))
#define SIZECLASS_MASK(sizeclass) (~(SIZECLASS_SIZE(sizeclass) - 1))

#define EXTERNAL_PTR(p, sizeclass) \
  ((void*)((uintptr_t)p & SIZECLASS_MASK(sizeclass)))

#define FIND_SLOT(ext, base) \
  ((uint32_t)1 << ((uintptr_t)((char*)(ext) - (char*)(base)) >> HEAP_MINBITS))

static const uint32_t sizeclass_empty[HEAP_SIZECLASSES] =
{
  0xFFFFFFFF,
  0x55555555,
  0x11111111,
  0x01010101,
  0x00010001
};

static const uint32_t sizeclass_init[HEAP_SIZECLASSES] =
{
  0xFFFFFFFE,
  0x55555554,
  0x11111110,
  0x01010100,
  0x00010000
};

#ifdef USE_RUNTIMESTATS
static const uint32_t sizeclass_slot_count[HEAP_SIZECLASSES] =
{
  32, 16, 8, 4, 2
};
#endif

static const uint8_t sizeclass_table[HEAP_MAX / HEAP_MIN] =
{
  0, 1, 2, 2, 3, 3, 3, 3,
  4, 4, 4, 4, 4, 4, 4, 4
};

static size_t heap_initialgc = 1 << 14;
static double heap_nextgc_factor = 2.0;

#ifdef USE_RUNTIMESTATS
/** Get the total number of allocations on the heap.
 */
size_t ponyint_heap_alloc_counter(pony_actor_t* actor)
{
  return actor->actorstats.heap_alloc_counter;
}

/** Get the total number of heap allocations freed.
 */
size_t ponyint_heap_free_counter(pony_actor_t* actor)
{
  return actor->actorstats.heap_free_counter;
}

/** Get the total number of GC iterations run.
 */
size_t ponyint_heap_gc_counter(pony_actor_t* actor)
{
  return actor->actorstats.heap_gc_counter;
}

/** Get the memory used by the heap.
 */
size_t ponyint_heap_mem_size(pony_actor_t* actor)
{
  // include memory that is in use by the heap but not counted as part of
  // `used` like `chunk_t`. also, don't include "fake used" for purposes of
  // triggering GC.
  return actor->actorstats.heap_mem_used;
}

/** Get the memory allocated by the heap.
 */
size_t ponyint_heap_alloc_size(pony_actor_t* actor)
{
  return actor->actorstats.heap_mem_allocated;
}

/** Get the memory overhead used by the heap.
 */
size_t ponyint_heap_overhead_size(pony_actor_t* actor)
{
  return actor->actorstats.heap_mem_allocated - actor->actorstats.heap_mem_used;
}

#endif

static bool get_chunk_is_small_chunk(chunk_t* chunk)
{
  return (((uintptr_t)chunk->m & CHUNK_TYPE_BITMASK) == CHUNK_TYPE_BITMASK);
}

static void set_chunk_is_small_chunk(chunk_t* chunk)
{
  chunk->m = (char*)((uintptr_t)chunk->m | CHUNK_TYPE_BITMASK);
}

static uint32_t get_large_chunk_slot(large_chunk_t* chunk)
{
  // `!!` to normalize to 1 or 0
  return !!(((uintptr_t)((chunk_t*)chunk)->m & LARGE_CHUNK_SLOT_BITMASK) == LARGE_CHUNK_SLOT_BITMASK);
}

static void set_large_chunk_slot(large_chunk_t* chunk, uint32_t slot)
{
  // `!!` to normalize to 1 or 0
  slot = !!slot;
  ((chunk_t*)chunk)->m = (char*)(((uintptr_t)((chunk_t*)chunk)->m & ~LARGE_CHUNK_SLOT_BITMASK) | (slot == 1 ? LARGE_CHUNK_SLOT_BITMASK : 0));
}

static uint32_t get_large_chunk_shallow(large_chunk_t* chunk)
{
  // `!!` to normalize to 1 or 0
  return (((uintptr_t)((chunk_t*)chunk)->m & LARGE_CHUNK_SHALLOW_BITMASK) == LARGE_CHUNK_SHALLOW_BITMASK);
}

static void set_large_chunk_shallow(large_chunk_t* chunk, uint32_t shallow)
{
  // `!!` to normalize to 1 or 0
  shallow = !!shallow;
  ((chunk_t*)chunk)->m = (char*)(((uintptr_t)((chunk_t*)chunk)->m & ~LARGE_CHUNK_SHALLOW_BITMASK) | (shallow == 1 ? LARGE_CHUNK_SHALLOW_BITMASK : 0));
}

static uint32_t get_large_chunk_finaliser(large_chunk_t* chunk)
{
  // `!!` to normalize to 1 or 0
  return !!(((uintptr_t)((chunk_t*)chunk)->m & LARGE_CHUNK_FINALISER_BITMASK) == LARGE_CHUNK_FINALISER_BITMASK);
}

static void set_large_chunk_finaliser(large_chunk_t* chunk, uint32_t finaliser)
{
  // `!!` to normalize to 1 or 0
  finaliser = !!finaliser;
  ((chunk_t*)chunk)->m = (char*)(((uintptr_t)((chunk_t*)chunk)->m & ~LARGE_CHUNK_FINALISER_BITMASK) | (finaliser == 1 ? LARGE_CHUNK_FINALISER_BITMASK : 0));
}

static size_t get_small_chunk_size(small_chunk_t* chunk)
{
  return ((uintptr_t)((chunk_t*)chunk)->m & SMALL_CHUNK_SIZECLASS_BITMASK) >> SMALL_CHUNK_SIZECLASS_SHIFT;
}

static void set_small_chunk_size(small_chunk_t* chunk, size_t size)
{
  pony_assert(size < HEAP_SIZECLASSES);

  // left shift size to get bits in the right spot for OR'ing into `chunk->m`
  size = size << SMALL_CHUNK_SIZECLASS_SHIFT;
  ((chunk_t*)chunk)->m = (char*)((uintptr_t)((chunk_t*)chunk)->m | (size & SMALL_CHUNK_SIZECLASS_BITMASK));
}

static bool get_chunk_needs_clearing(chunk_t* chunk)
{
  return (((uintptr_t)chunk->m & CHUNK_NEEDS_TO_BE_CLEARED_BITMASK) == CHUNK_NEEDS_TO_BE_CLEARED_BITMASK);
}

static void set_chunk_needs_clearing(chunk_t* chunk)
{
  chunk->m = (char*)((uintptr_t)chunk->m | CHUNK_NEEDS_TO_BE_CLEARED_BITMASK);
}

static void unset_chunk_needs_clearing(chunk_t* chunk)
{
  chunk->m = (char*)((uintptr_t)chunk->m & ~CHUNK_NEEDS_TO_BE_CLEARED_BITMASK);
}

static char* get_m(chunk_t* chunk)
{
  return (char*)((uintptr_t)chunk->m & CHUNK_M_BITMASK);
}

static void large_pagemap(char* m, size_t size, chunk_t* chunk, pony_actor_t* actor)
{
  ponyint_pagemap_set_bulk(m, chunk, actor, size);
}

static void maybe_clear_chunk(chunk_t* chunk)
{
  // handle delayed clearing of the chunk since we don't do it
  // before starting GC anymore as an optimization
  if(get_chunk_needs_clearing(chunk))
  {
    if(get_chunk_is_small_chunk(chunk))
    {
      small_chunk_t* small_chunk = (small_chunk_t*)chunk;
      small_chunk->slots = sizeclass_empty[get_small_chunk_size(small_chunk)];
      small_chunk->shallow = sizeclass_empty[get_small_chunk_size(small_chunk)];
    }
    else
    {
      large_chunk_t* large_chunk = (large_chunk_t*)chunk;
      set_large_chunk_slot(large_chunk, 1);
      set_large_chunk_shallow(large_chunk, 1);
    }
    unset_chunk_needs_clearing(chunk);
  }
}

static void final_small(small_chunk_t* chunk, uint32_t force_finalisers_mask)
{
  // this function is only called in one of two scenarios.
  // 1. when the full chunk is being freed or destroyed. In this case
  //    `force_finalisers_mask` should always be `FORCE_ALL_FINALISERS` to make
  //    all finalisers get run no matter what.
  // 2. when part of the chunk is being freed. In this case
  //    `force_finalisers_mask` should always be `0` to make sure that
  //    only those finalisers get run as per the free `chunk->slots`
  pony_assert(force_finalisers_mask == FORCE_NO_FINALISERS ||
    force_finalisers_mask == FORCE_ALL_FINALISERS);
  uint32_t finalisers = chunk->finalisers & (chunk->slots | force_finalisers_mask);

  // exit early before any memory writes if no finalisers to run
  if(finalisers == 0)
    return;

  // unset all finalisers bits since we're going to run them all now
  chunk->finalisers = chunk->finalisers & ~(chunk->slots | force_finalisers_mask);

  // run any finalisers that need to be run
  void* p = NULL;

  uint64_t bit = 0;

  // if there's a finaliser to run for a used slot
  while(finalisers != 0)
  {
    bit = __pony_ctz(finalisers);
    p = get_m((chunk_t*)chunk) + (bit << HEAP_MINBITS);

    // run finaliser
    pony_assert((*(pony_type_t**)p)->final != NULL);
    (*(pony_type_t**)p)->final(p);

    // clear bit just found in our local finaliser map
    finalisers &= (finalisers - 1);
  }
}

static void final_large(large_chunk_t* chunk, uint32_t mark)
{
  if(get_large_chunk_finaliser(chunk) == 1)
  {
    // run finaliser
    char* m = get_m((chunk_t*)chunk);
    pony_assert((*(pony_type_t**)m)->final != NULL);
    (*(pony_type_t**)m)->final(m);
    set_large_chunk_finaliser(chunk, 0);
  }
  (void)mark;
}

static void destroy_small(small_chunk_t* chunk, uint32_t mark)
{
  (void)mark;

  // run any finalisers that need running
  final_small(chunk, FORCE_ALL_FINALISERS);

  char* m = get_m((chunk_t*)chunk);
  ponyint_pagemap_set(m, NULL, NULL);
#ifndef PONY_NDEBUG
  memset(m, 0, sizeof(block_t));
#endif
  POOL_FREE(block_t, m);
  POOL_FREE(small_chunk_t, chunk);
}

static void destroy_large(large_chunk_t* chunk, uint32_t mark)
{

  (void)mark;

  // run any finalisers that need running
  final_large(chunk, mark);

  char* m = get_m((chunk_t*)chunk);
  large_pagemap(m, chunk->size, NULL, NULL);

  if(m != NULL)
  {
#ifndef PONY_NDEBUG
    memset(m, 0, chunk->size);
#endif
    ponyint_pool_free_size(chunk->size, m);
  }

  POOL_FREE(large_chunk_t, chunk);
}

static size_t sweep_small(small_chunk_t* chunk, small_chunk_t** avail, small_chunk_t** full, small_chunk_t** to_destroy,
#ifdef USE_RUNTIMESTATS
  uint32_t empty, size_t size, size_t* mem_allocated, size_t* mem_used,
  size_t* num_allocated)
#else
  uint32_t empty, size_t size)
#endif
{
#ifdef USE_RUNTIMESTATS
  size_t num_slots_used = 0;
#endif

  size_t used = 0;
  small_chunk_t* next;

  while(chunk != NULL)
  {
    next = chunk->next;

    maybe_clear_chunk((chunk_t*)chunk);

    chunk->slots &= chunk->shallow;

    set_chunk_needs_clearing((chunk_t*)chunk);

    if(chunk->slots == 0)
    {
#ifdef USE_RUNTIMESTATS
      *mem_allocated += POOL_ALLOC_SIZE(small_chunk_t);
      *mem_allocated += POOL_ALLOC_SIZE(block_t);
      *mem_used += sizeof(small_chunk_t);
      *mem_used += sizeof(block_t);
      num_slots_used += sizeclass_slot_count[ponyint_heap_index(size)];
#endif
      used += sizeof(block_t);
      chunk->next = *full;
      *full = chunk;
    } else if(chunk->slots == empty) {
      // run finalisers for freed slots
      final_small(chunk, FORCE_ALL_FINALISERS);

      // cannot destroy chunk yet since the heap has not been fully swept yet
      // and another object may still reference this chunk and access it in a finaliser
      chunk->next = *to_destroy;
      *to_destroy = chunk;
    } else {
#ifdef USE_RUNTIMESTATS
      *mem_allocated += POOL_ALLOC_SIZE(small_chunk_t);
      *mem_allocated += POOL_ALLOC_SIZE(block_t);
      *mem_used += sizeof(small_chunk_t);
      *mem_used += (sizeof(block_t) - (__pony_popcount(chunk->slots) * size));
      num_slots_used += (sizeclass_slot_count[ponyint_heap_index(size)] - __pony_popcount(chunk->slots));
#endif
      used += (sizeof(block_t) -
        (__pony_popcount(chunk->slots) * size));

      // run finalisers for freed slots
      final_small(chunk, FORCE_NO_FINALISERS);

      chunk->next = *avail;
      *avail = chunk;
    }

    chunk = next;
  }

#ifdef USE_RUNTIMESTATS
  *num_allocated += num_slots_used;
#endif

  return used;
}

#ifdef USE_RUNTIMESTATS
static large_chunk_t* sweep_large(large_chunk_t* chunk, large_chunk_t** to_destroy, size_t* used, size_t* mem_allocated,
  size_t* mem_used, size_t* num_allocated)
#else
static large_chunk_t* sweep_large(large_chunk_t* chunk, large_chunk_t** to_destroy, size_t* used)
#endif
{
#ifdef USE_RUNTIMESTATS
  size_t num_slots_used = 0;
#endif

  large_chunk_t* list = NULL;
  large_chunk_t* next;

  while(chunk != NULL)
  {
    next = chunk->next;

    maybe_clear_chunk((chunk_t*)chunk);

    set_large_chunk_slot(chunk, get_large_chunk_slot(chunk) & get_large_chunk_shallow(chunk));

    set_chunk_needs_clearing((chunk_t*)chunk);

    if(get_large_chunk_slot(chunk) == 0)
    {
      chunk->next = list;
      list = chunk;
#ifdef USE_RUNTIMESTATS
      *mem_allocated += POOL_ALLOC_SIZE(large_chunk_t);
      *mem_allocated += ponyint_pool_used_size(chunk->size);
      *mem_used += sizeof(large_chunk_t);
      *mem_used += chunk->size;
      num_slots_used++;
#endif
      *used += chunk->size;
    } else {
      // run finaliser
      final_large(chunk, 0);

      // cannot destroy chunk yet since the heap has not been fully swept yet
      // and another object may still reference this chunk and access it in a finaliser
      chunk->next = *to_destroy;
      *to_destroy = chunk;
    }

    chunk = next;
  }

#ifdef USE_RUNTIMESTATS
  *num_allocated += num_slots_used;
#endif

  return list;
}

static void large_chunk_list(large_chunk_fn f, large_chunk_t* current, uint32_t mark)
{
  large_chunk_t* next;

  while(current != NULL)
  {
    next = current->next;
    f(current, mark);
    current = next;
  }
}

static void small_chunk_list(small_chunk_fn f, small_chunk_t* current, uint32_t mark)
{
  small_chunk_t* next;

  while(current != NULL)
  {
    next = current->next;
    f(current, mark);
    current = next;
  }
}

uint32_t ponyint_heap_index(size_t size)
{
  // size is in range 1..HEAP_MAX
  // change to 0..((HEAP_MAX / HEAP_MIN) - 1) and look up in table
  return sizeclass_table[(size - 1) >> HEAP_MINBITS];
}

void ponyint_heap_setinitialgc(size_t size)
{
  heap_initialgc = (size_t)1 << size;
}

void ponyint_heap_setnextgcfactor(double factor)
{
  if(factor < 1.0)
    factor = 1.0;

  DTRACE1(GC_THRESHOLD, factor);
  heap_nextgc_factor = factor;
}

void ponyint_heap_init(heap_t* heap)
{
  memset(heap, 0, sizeof(heap_t));
  heap->next_gc = heap_initialgc;
}

void ponyint_heap_destroy(heap_t* heap)
{
  large_chunk_list(destroy_large, heap->large, 0);

  for(int i = 0; i < HEAP_SIZECLASSES; i++)
  {
    small_chunk_list(destroy_small, heap->small_free[i], 0);
    small_chunk_list(destroy_small, heap->small_full[i], 0);
  }
}

void ponyint_heap_final(heap_t* heap)
{
  large_chunk_list(final_large, heap->large, 0);

  for(int i = 0; i < HEAP_SIZECLASSES; i++)
  {
    small_chunk_list(final_small, heap->small_free[i], FORCE_ALL_FINALISERS);
    small_chunk_list(final_small, heap->small_full[i], FORCE_ALL_FINALISERS);
  }
}

void* ponyint_heap_alloc(pony_actor_t* actor, heap_t* heap, size_t size,
  uint32_t track_finalisers_mask)
{
  // 1. when `track_finalisers_mask == TRACK_NO_FINALISERS` then finalisers are
  //    disabled
  // 2. when `track_finalisers_mask == TRACK_ALL_FINALISERS` then
  //    finalisers are enabled
  pony_assert(track_finalisers_mask == TRACK_NO_FINALISERS ||
    track_finalisers_mask == TRACK_ALL_FINALISERS);

  if(size == 0)
  {
    return NULL;
  } else if(size <= HEAP_MAX) {
    return ponyint_heap_alloc_small(actor, heap, ponyint_heap_index(size),
      track_finalisers_mask);
  } else {
    return ponyint_heap_alloc_large(actor, heap, size, track_finalisers_mask);
  }
}

void* ponyint_heap_alloc_small(pony_actor_t* actor, heap_t* heap,
  uint32_t sizeclass, uint32_t track_finalisers_mask)
{
  // 1. when `track_finalisers_mask == TRACK_NO_FINALISERS` then finalisers are
  //    disabled
  // 2. when `track_finalisers_mask == TRACK_ALL_FINALISERS` then
  //    finalisers are enabled
  pony_assert(track_finalisers_mask == TRACK_NO_FINALISERS ||
    track_finalisers_mask == TRACK_ALL_FINALISERS);

  small_chunk_t* chunk = heap->small_free[sizeclass];
  void* m;

  // If there are none in this size class, get a new one.
  if(chunk != NULL)
  {
    // Clear and use the first available slot.
    uint32_t slots = chunk->slots;
    uint32_t bit = __pony_ctz(slots);
    slots &= ~((uint32_t)1 << bit);

    m = get_m((chunk_t*)chunk) + (bit << HEAP_MINBITS);
    chunk->slots = slots;

    // note if a finaliser needs to run or not
    chunk->finalisers |= (track_finalisers_mask & ((uint32_t)1 << bit));

    if(slots == 0)
    {
      heap->small_free[sizeclass] = chunk->next;
      chunk->next = heap->small_full[sizeclass];
      heap->small_full[sizeclass] = chunk;
    }
  } else {
    small_chunk_t* n = (small_chunk_t*) POOL_ALLOC(small_chunk_t);
    n->base.m = (char*) POOL_ALLOC(block_t);
    set_small_chunk_size(n, sizeclass);
#ifdef USE_RUNTIMESTATS
    actor->actorstats.heap_mem_used += sizeof(small_chunk_t);
    actor->actorstats.heap_mem_allocated += POOL_ALLOC_SIZE(small_chunk_t);
    actor->actorstats.heap_mem_allocated += POOL_ALLOC_SIZE(block_t);
#endif

    // note if a finaliser needs to run or not
    n->finalisers = (track_finalisers_mask & 1);

    set_chunk_is_small_chunk((chunk_t*)n);

    // Clear the first bit.
    n->slots = sizeclass_init[sizeclass];
    n->next = NULL;

    set_chunk_needs_clearing((chunk_t*)n);

    ponyint_pagemap_set(get_m((chunk_t*)n), (chunk_t*)n, actor);

    heap->small_free[sizeclass] = n;
    chunk = n;

    // Use the first slot.
    m = get_m((chunk_t*)chunk);
  }

#ifdef USE_RUNTIMESTATS
  actor->actorstats.heap_alloc_counter++;
  actor->actorstats.heap_num_allocated++;
  actor->actorstats.heap_mem_used += SIZECLASS_SIZE(sizeclass);
#endif
  heap->used += SIZECLASS_SIZE(sizeclass);
  return m;
}

void* ponyint_heap_alloc_large(pony_actor_t* actor, heap_t* heap, size_t size,
  uint32_t track_finalisers_mask)
{
  // 1. when `track_finalisers_mask == TRACK_NO_FINALISERS` then finalisers are
  //    disabled
  // 2. when `track_finalisers_mask == TRACK_ALL_FINALISERS` then
  //    finalisers are enabled
  pony_assert(track_finalisers_mask == TRACK_NO_FINALISERS ||
    track_finalisers_mask == TRACK_ALL_FINALISERS);

  size = ponyint_pool_adjust_size(size);

  large_chunk_t* chunk = (large_chunk_t*) POOL_ALLOC(large_chunk_t);
  chunk->size = size;
  chunk->base.m = (char*) ponyint_pool_alloc_size(size);
#ifdef USE_RUNTIMESTATS
  actor->actorstats.heap_mem_used += sizeof(large_chunk_t);
  actor->actorstats.heap_mem_used += chunk->size;
  actor->actorstats.heap_mem_allocated += POOL_ALLOC_SIZE(large_chunk_t);
  actor->actorstats.heap_mem_allocated += ponyint_pool_used_size(size);
  actor->actorstats.heap_alloc_counter++;
  actor->actorstats.heap_num_allocated++;
#endif
  set_large_chunk_slot(chunk, 0);

  set_chunk_needs_clearing((chunk_t*)chunk);

  // note if a finaliser needs to run or not
  set_large_chunk_finaliser(chunk, (track_finalisers_mask & 1));

  large_pagemap(get_m((chunk_t*)chunk), size, (chunk_t*)chunk, actor);

  chunk->next = heap->large;
  heap->large = chunk;
  heap->used += chunk->size;

  return get_m((chunk_t*)chunk);
}

void* ponyint_heap_realloc(pony_actor_t* actor, heap_t* heap, void* p,
  size_t size, size_t copy)
{
  if(p == NULL)
    return ponyint_heap_alloc(actor, heap, size, TRACK_NO_FINALISERS);

#ifdef USE_RUNTIMESTATS
  actor->actorstats.heap_realloc_counter++;
#endif

  chunk_t* chunk = ponyint_pagemap_get_chunk(p);

  // We can't realloc memory that wasn't pony_alloc'ed since we can't know how
  // much to copy from the previous location.
  pony_assert(chunk != NULL);

  size_t oldsize;

  if(get_chunk_is_small_chunk(chunk))
  {
    size_t chunk_size = get_small_chunk_size((small_chunk_t*)chunk);
    // Previous allocation was a ponyint_heap_alloc_small.
    void* ext = EXTERNAL_PTR(p, chunk_size);

    oldsize = SIZECLASS_SIZE(chunk_size) - ((uintptr_t)p - (uintptr_t)ext);
  } else {
    // Previous allocation was a ponyint_heap_alloc_large.

    oldsize = ((large_chunk_t*)chunk)->size - ((uintptr_t)p - (uintptr_t)get_m(chunk));
  }

  pony_assert(copy <= size);
  // Determine how much memory to copy.
  if(copy > size)
    copy = size;

  if(oldsize > copy)
    oldsize = copy;

  // Get new memory and copy from the old memory.
  void* q = ponyint_heap_alloc(actor, heap, size, TRACK_NO_FINALISERS);
  memcpy(q, p, oldsize);
  return q;
}

void ponyint_heap_used(heap_t* heap, size_t size)
{
  heap->used += size;
}

bool ponyint_heap_startgc(heap_t* heap
#ifdef USE_RUNTIMESTATS
  , pony_actor_t* actor)
#else
  )
#endif
{
  if(heap->used <= heap->next_gc)
    return false;

  // reset used to zero
  heap->used = 0;
#ifdef USE_RUNTIMESTATS
  actor->actorstats.heap_mem_allocated = 0;
  actor->actorstats.heap_mem_used = 0;
#endif
  return true;
}

bool ponyint_heap_mark(chunk_t* chunk, void* p)
{
  // If it's an internal pointer, we shallow mark it instead. This will
  // preserve the external pointer, but allow us to mark and recurse the
  // external pointer in the same pass.
  bool marked;

  maybe_clear_chunk(chunk);

  if(get_chunk_is_small_chunk(chunk))
  {
    small_chunk_t* small_chunk = (small_chunk_t*)chunk;

    // Calculate the external pointer.
    void* ext = EXTERNAL_PTR(p, get_small_chunk_size(small_chunk));

    // Shift to account for smallest allocation size.
    uint32_t slot = FIND_SLOT(ext, get_m((chunk_t*)small_chunk));

    // Check if it was already marked.
    marked = (small_chunk->slots & slot) == 0;

    // A clear bit is in-use, a set bit is available.
    if(p == ext)
      small_chunk->slots &= ~slot;
    else
      small_chunk->shallow &= ~slot;
  } else {
    large_chunk_t* large_chunk = (large_chunk_t*)chunk;

    marked = get_large_chunk_slot(large_chunk) == 0;

    if(p == get_m(chunk))
      set_large_chunk_slot(large_chunk, 0);
    else
      set_large_chunk_shallow(large_chunk, 0);
  }

  return marked;
}

void ponyint_heap_mark_shallow(chunk_t* chunk, void* p)
{
  maybe_clear_chunk(chunk);

  if(get_chunk_is_small_chunk(chunk))
  {
    // Calculate the external pointer.
    void* ext = EXTERNAL_PTR(p, get_small_chunk_size((small_chunk_t*)chunk));

    // Shift to account for smallest allocation size.
    uint32_t slot = FIND_SLOT(ext, get_m(chunk));

    // A clear bit is in-use, a set bit is available.
    ((small_chunk_t*)chunk)->shallow &= ~slot;
  } else {
    set_large_chunk_shallow((large_chunk_t*)chunk, 0);
  }
}

void ponyint_heap_free(chunk_t* chunk, void* p)
{
  if(!get_chunk_is_small_chunk(chunk))
  {
    if(p == get_m(chunk))
    {
      // run finaliser if needed
      final_large((large_chunk_t*)chunk, 0);

      ponyint_pool_free_size(((large_chunk_t*)chunk)->size, get_m(chunk));
      chunk->m = NULL;
      set_large_chunk_slot((large_chunk_t*)chunk, 1);
    }
    return;
  }

  small_chunk_t* small_chunk = (small_chunk_t*)chunk;

  // Calculate the external pointer.
  void* ext = EXTERNAL_PTR(p, get_small_chunk_size(small_chunk));

  if(p == ext)
  {
    // Shift to account for smallest allocation size.
    uint32_t slot = FIND_SLOT(ext, get_m((chunk_t*)small_chunk));

    // check if there's a finaliser to run
    if((small_chunk->finalisers & slot) != 0)
    {
      // run finaliser
      (*(pony_type_t**)p)->final(p);

      // clear finaliser
      small_chunk->finalisers &= ~slot;
    }

    // free slot
    small_chunk->slots |= slot;
  }
}

void ponyint_heap_endgc(heap_t* heap
#ifdef USE_RUNTIMESTATS
  , pony_actor_t* actor)
#else
  )
#endif
{
  size_t used = 0;
#ifdef USE_RUNTIMESTATS
  size_t mem_allocated = 0;
  size_t mem_used = 0;
  size_t num_allocated = 0;
#endif

  // make a copy of all the heap list pointers into a temporary heap
  heap_t theap = {};
  heap_t* temp_heap = &theap;
  memcpy(temp_heap, heap, offsetof(heap_t, used));

  // set all the heap list pointers to NULL in the real heap to ensure that
  // any new allocations by finalisers during the GC process don't reuse memory
  // freed during the GC process
  memset(heap, 0, offsetof(heap_t, used));

  // lists of chunks to destroy
  small_chunk_t* to_destroy_small = NULL;
  large_chunk_t* to_destroy_large = NULL;

  // sweep all the chunks in the temporary heap while accumulating chunks to destroy
  // NOTE: the real heap will get new chunks allocated and added to it during this process
  //       if a finaliser allocates memory... we have to make sure that finalisers don't
  //       reuse memory being freed or we'll end up with a use-after-free bug due to
  //       potential cross objects references in finalisers after memory has been reused.
  for(int i = 0; i < HEAP_SIZECLASSES; i++)
  {
    small_chunk_t* list1 = temp_heap->small_free[i];
    small_chunk_t* list2 = temp_heap->small_full[i];

    temp_heap->small_free[i] = NULL;
    temp_heap->small_full[i] = NULL;

    small_chunk_t** avail = &temp_heap->small_free[i];
    small_chunk_t** full = &temp_heap->small_full[i];

    size_t size = SIZECLASS_SIZE(i);
    uint32_t empty = sizeclass_empty[i];

#ifdef USE_RUNTIMESTATS
    used += sweep_small(list1, avail, full, &to_destroy_small, empty, size,
      &mem_allocated, &mem_used, &num_allocated);
    used += sweep_small(list2, avail, full, &to_destroy_small, empty, size,
      &mem_allocated, &mem_used, &num_allocated);
#else
    used += sweep_small(list1, avail, full, &to_destroy_small, empty, size);
    used += sweep_small(list2, avail, full, &to_destroy_small, empty, size);
#endif
  }

#ifdef USE_RUNTIMESTATS
  temp_heap->large = sweep_large(temp_heap->large, &to_destroy_large, &used, &mem_allocated, &mem_used,
    &num_allocated);
#else
  temp_heap->large = sweep_large(temp_heap->large, &to_destroy_large, &used);
#endif

  // we need to combine the temporary heap lists with the real heap lists
  // because the real heap might not be empty due to allocations in finalisers
  // any new chunks with no finalisers to run can be destroyed immediately
  // the new allocations will get GC'd in the next GC cycle even though they
  // are definitely not reachable by the actor anymore
  for(int i = 0; i < HEAP_SIZECLASSES; i++)
  {
    // iterate to the end of the real heap list and append the temporary heap list
    // because the real heap list is likely to be smaller than the temporary heap list
    // and remove any chunks that have no finalisers to run immediately
    small_chunk_t** sc_temp = &heap->small_free[i];
    while (*sc_temp != NULL)
    {
      if ((*sc_temp)->finalisers == 0)
      {
        small_chunk_t* temp = *sc_temp;
        *sc_temp = temp->next;
        temp->next = to_destroy_small;
        to_destroy_small = temp;
      } else {
        sc_temp = &(*sc_temp)->next;
      }
    }
    *sc_temp = temp_heap->small_free[i];

    // iterate to the end of the real heap list and append the temporary heap list
    // because the real heap list is likely to be smaller than the temporary heap list
    // and remove any chunks that have no finalisers to run immediately
    sc_temp = &heap->small_full[i];
    while (*sc_temp != NULL)
    {
      if ((*sc_temp)->finalisers == 0)
      {
        small_chunk_t* temp = *sc_temp;
        *sc_temp = temp->next;
        temp->next = to_destroy_small;
        to_destroy_small = temp;
      } else {
        sc_temp = &(*sc_temp)->next;
      }
    }
    *sc_temp = temp_heap->small_full[i];
  }

  // iterate to the end of the real heap list and append the temporary heap list
  // because the real heap list is likely to be smaller than the temporary heap list
  // and remove any chunks that have no finalisers to run immediately
  large_chunk_t** lc_temp = &heap->large;
  while (*lc_temp != NULL)
  {
    if (get_large_chunk_finaliser(*lc_temp) == 0)
    {
      large_chunk_t* temp = *lc_temp;
      *lc_temp = temp->next;
      temp->next = to_destroy_large;
      to_destroy_large = temp;
    } else {
      lc_temp = &(*lc_temp)->next;
    }
  }
  *lc_temp = temp_heap->large;

  // destroy all the chunks that need to be destroyed
  small_chunk_list(destroy_small, to_destroy_small, 0);
  large_chunk_list(destroy_large, to_destroy_large, 0);


  // Foreign object sizes will have been added to heap->used already. Here we
  // add local object sizes as well and set the next gc point for when memory
  // usage has increased.
  heap->used += used;
#ifdef USE_RUNTIMESTATS
  actor->actorstats.heap_mem_allocated += mem_allocated;
  actor->actorstats.heap_mem_used += mem_used;
  actor->actorstats.heap_free_counter += (actor->actorstats.heap_num_allocated - num_allocated);
  actor->actorstats.heap_num_allocated = num_allocated;
  actor->actorstats.heap_gc_counter++;
#endif
  heap->next_gc = (size_t)((double)heap->used * heap_nextgc_factor);

  if(heap->next_gc < heap_initialgc)
    heap->next_gc = heap_initialgc;
}

size_t ponyint_heap_size(chunk_t* chunk)
{
  if(get_chunk_is_small_chunk(chunk))
    return SIZECLASS_SIZE(get_small_chunk_size((small_chunk_t*)chunk));

  return ((large_chunk_t*)chunk)->size;
}

// C99 requires inline symbols to be present in a compilation unit for un-
// optimized code

uint32_t __pony_clz(uint32_t x);
uint32_t __pony_clzzu(size_t x);
uint32_t __pony_ffszu(size_t x);
uint32_t __pony_ctz(uint32_t x);
uint32_t __pony_ffs(uint32_t x);
uint32_t __pony_ffsll(uint64_t x);
uint32_t __pony_clzll(uint64_t x);
uint32_t __pony_popcount(uint32_t x);
