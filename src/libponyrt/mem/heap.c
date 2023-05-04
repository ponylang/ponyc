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
  // immutable
  pony_actor_t* actor;
  char* m;
  size_t size;

  // mutable
  uint32_t slots;
  uint32_t shallow;
  uint32_t finalisers;

  struct chunk_t* next;
} chunk_t;

enum
{
  CHUNK_NEEDS_TO_BE_CLEARED = 0xFFFFFFFF,
};

enum
{
  FORCE_NO_FINALISERS = 0,
  FORCE_ALL_FINALISERS = 0xFFFFFFFF,
};

typedef char block_t[POOL_ALIGN];
typedef void (*chunk_fn)(chunk_t* chunk, uint32_t mark);

#define SIZECLASS_SIZE(sizeclass) (HEAP_MIN << (sizeclass))
#define SIZECLASS_MASK(sizeclass) (~(SIZECLASS_SIZE(sizeclass) - 1))

#define EXTERNAL_PTR(p, sizeclass) \
  ((void*)((uintptr_t)p & SIZECLASS_MASK(sizeclass)))

#define FIND_SLOT(ext, base) \
  (1 << ((uintptr_t)((char*)(ext) - (char*)(base)) >> HEAP_MINBITS))

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

static void large_pagemap(char* m, size_t size, chunk_t* chunk)
{
  ponyint_pagemap_set_bulk(m, chunk, size);
}

static void clear_chunk(chunk_t* chunk, uint32_t mark)
{
  chunk->slots = mark;
  chunk->shallow = mark;
}

static void maybe_clear_chunk(chunk_t* chunk)
{
  // handle delayed clearing of the chunk since we don't do it
  // before starting GC anymore as an optimization
  // This is ignored in case of `sizeclass == 0` because
  // the sizeclass_empty` value for it is also `0xFFFFFFFF`
  // and it is still cleared befor starting GC
  if(chunk->size != 0 && chunk->shallow == CHUNK_NEEDS_TO_BE_CLEARED)
  {
    if(chunk->size >= HEAP_SIZECLASSES)
    {
      chunk->slots = 1;
      chunk->shallow = 1;
    }
    else
    {
      chunk->slots = sizeclass_empty[chunk->size];
      chunk->shallow = sizeclass_empty[chunk->size];
    }
  }
}

static void final_small(chunk_t* chunk, uint32_t force_finalisers_mask)
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
    p = chunk->m + (bit << HEAP_MINBITS);

    // run finaliser
    pony_assert((*(pony_type_t**)p)->final != NULL);
    (*(pony_type_t**)p)->final(p);

    // clear bit just found in our local finaliser map
    finalisers &= (finalisers - 1);
  }
}

static void final_large(chunk_t* chunk, uint32_t mark)
{
  if(chunk->finalisers == 1)
  {
    // run finaliser
    pony_assert((*(pony_type_t**)chunk->m)->final != NULL);
    (*(pony_type_t**)chunk->m)->final(chunk->m);
    chunk->finalisers = 0;
  }
  (void)mark;
}

static void destroy_small(chunk_t* chunk, uint32_t mark)
{
  (void)mark;

  // run any finalisers that need running
  final_small(chunk, FORCE_ALL_FINALISERS);

  ponyint_pagemap_set(chunk->m, NULL);
  POOL_FREE(block_t, chunk->m);
  POOL_FREE(chunk_t, chunk);
}

static void destroy_large(chunk_t* chunk, uint32_t mark)
{

  (void)mark;

  // run any finalisers that need running
  final_large(chunk, mark);

  large_pagemap(chunk->m, chunk->size, NULL);

  if(chunk->m != NULL)
    ponyint_pool_free_size(chunk->size, chunk->m);

  POOL_FREE(chunk_t, chunk);
}

static size_t sweep_small(chunk_t* chunk, chunk_t** avail, chunk_t** full,
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
  chunk_t* next;

  while(chunk != NULL)
  {
    next = chunk->next;

    maybe_clear_chunk(chunk);

    chunk->slots &= chunk->shallow;

    // set `chunk->shallow` to sentinel to avoid clearing
    // for next GC cycle
    // This is ignored in case of `sizeclass == 0` because
    // the sizeclass_empty` value for it is also `0xFFFFFFFF`
    chunk->shallow = CHUNK_NEEDS_TO_BE_CLEARED;

    if(chunk->slots == 0)
    {
#ifdef USE_RUNTIMESTATS
      *mem_allocated += POOL_ALLOC_SIZE(chunk_t);
      *mem_allocated += POOL_ALLOC_SIZE(block_t);
      *mem_used += sizeof(chunk_t);
      *mem_used += sizeof(block_t);
      num_slots_used += sizeclass_slot_count[ponyint_heap_index(size)];
#endif
      used += sizeof(block_t);
      chunk->next = *full;
      *full = chunk;
    } else if(chunk->slots == empty) {
      destroy_small(chunk, 0);
    } else {
#ifdef USE_RUNTIMESTATS
      *mem_allocated += POOL_ALLOC_SIZE(chunk_t);
      *mem_allocated += POOL_ALLOC_SIZE(block_t);
      *mem_used += sizeof(chunk_t);
      *mem_used += (sizeof(block_t) - (__pony_popcount(chunk->slots) * size));
      num_slots_used += (sizeclass_slot_count[ponyint_heap_index(size)] - __pony_popcount(chunk->slots));
#endif
      used += (sizeof(block_t) -
        (__pony_popcount(chunk->slots) * size));

      // run finalisers for freed slots
      final_small(chunk, FORCE_NO_FINALISERS);

      // make chunk available for allocations only after finalisers have been
      // run to prevent premature reuse of memory slots by an allocation
      // required for finaliser execution
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
static chunk_t* sweep_large(chunk_t* chunk, size_t* used, size_t* mem_allocated,
  size_t* mem_used, size_t* num_allocated)
#else
static chunk_t* sweep_large(chunk_t* chunk, size_t* used)
#endif
{
#ifdef USE_RUNTIMESTATS
  size_t num_slots_used = 0;
#endif

  chunk_t* list = NULL;
  chunk_t* next;

  while(chunk != NULL)
  {
    next = chunk->next;

    maybe_clear_chunk(chunk);

    chunk->slots &= chunk->shallow;

    // set `chunk->shallow` to sentinel to avoid clearing
    // for next GC cycle
    chunk->shallow = CHUNK_NEEDS_TO_BE_CLEARED;

    if(chunk->slots == 0)
    {
      chunk->next = list;
      list = chunk;
#ifdef USE_RUNTIMESTATS
      *mem_allocated += POOL_ALLOC_SIZE(chunk_t);
      *mem_allocated += ponyint_pool_used_size(chunk->size);
      *mem_used += sizeof(chunk_t);
      *mem_used += chunk->size;
      num_slots_used++;
#endif
      *used += chunk->size;
    } else {
      destroy_large(chunk, 0);
    }

    chunk = next;
  }

#ifdef USE_RUNTIMESTATS
  *num_allocated += num_slots_used;
#endif

  return list;
}

static void chunk_list(chunk_fn f, chunk_t* current, uint32_t mark)
{
  chunk_t* next;

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
  chunk_list(destroy_large, heap->large, 0);

  for(int i = 0; i < HEAP_SIZECLASSES; i++)
  {
    chunk_list(destroy_small, heap->small_free[i], 0);
    chunk_list(destroy_small, heap->small_full[i], 0);
  }
}

void ponyint_heap_final(heap_t* heap)
{
  chunk_list(final_large, heap->large, 0);

  for(int i = 0; i < HEAP_SIZECLASSES; i++)
  {
    chunk_list(final_small, heap->small_free[i], FORCE_ALL_FINALISERS);
    chunk_list(final_small, heap->small_full[i], FORCE_ALL_FINALISERS);
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

  chunk_t* chunk = heap->small_free[sizeclass];
  void* m;

  // If there are none in this size class, get a new one.
  if(chunk != NULL)
  {
    // Clear and use the first available slot.
    uint32_t slots = chunk->slots;
    uint32_t bit = __pony_ctz(slots);
    slots &= ~((uint32_t)1 << bit);

    m = chunk->m + (bit << HEAP_MINBITS);
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
    chunk_t* n = (chunk_t*) POOL_ALLOC(chunk_t);
    n->actor = actor;
    n->m = (char*) POOL_ALLOC(block_t);
    n->size = sizeclass;
#ifdef USE_RUNTIMESTATS
    actor->actorstats.heap_mem_used += sizeof(chunk_t);
    actor->actorstats.heap_mem_allocated += POOL_ALLOC_SIZE(chunk_t);
    actor->actorstats.heap_mem_allocated += POOL_ALLOC_SIZE(block_t);
#endif

    // note if a finaliser needs to run or not
    n->finalisers = (track_finalisers_mask & 1);

    // Clear the first bit.
    n->slots = sizeclass_init[sizeclass];
    n->next = NULL;

    // set `chunk->shallow` to sentinel to avoid clearing
    // for next GC cycle
    // This is ignored in case of `sizeclass == 0` because
    // the sizeclass_empty` value for it is also `0xFFFFFFFF`
    n->shallow = CHUNK_NEEDS_TO_BE_CLEARED;

    ponyint_pagemap_set(n->m, n);

    heap->small_free[sizeclass] = n;
    chunk = n;

    // Use the first slot.
    m = chunk->m;
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

  chunk_t* chunk = (chunk_t*) POOL_ALLOC(chunk_t);
  chunk->actor = actor;
  chunk->size = size;
  chunk->m = (char*) ponyint_pool_alloc_size(size);
#ifdef USE_RUNTIMESTATS
  actor->actorstats.heap_mem_used += sizeof(chunk_t);
  actor->actorstats.heap_mem_used += chunk->size;
  actor->actorstats.heap_mem_allocated += POOL_ALLOC_SIZE(chunk_t);
  actor->actorstats.heap_mem_allocated += ponyint_pool_used_size(size);
  actor->actorstats.heap_alloc_counter++;
  actor->actorstats.heap_num_allocated++;
#endif
  chunk->slots = 0;

  // set `chunk->shallow` to sentinel to avoid clearing
  // for next GC cycle
  // This is ignored in case of `sizeclass == 0` because
  // the sizeclass_empty` value for it is also `0xFFFFFFFF`
  chunk->shallow = CHUNK_NEEDS_TO_BE_CLEARED;

  // note if a finaliser needs to run or not
  chunk->finalisers = (track_finalisers_mask & 1);

  large_pagemap(chunk->m, size, chunk);

  chunk->next = heap->large;
  heap->large = chunk;
  heap->used += chunk->size;

  return chunk->m;
}

void* ponyint_heap_realloc(pony_actor_t* actor, heap_t* heap, void* p,
  size_t size, size_t copy)
{
  if(p == NULL)
    return ponyint_heap_alloc(actor, heap, size, TRACK_NO_FINALISERS);

#ifdef USE_RUNTIMESTATS
  actor->actorstats.heap_realloc_counter++;
#endif

  chunk_t* chunk = ponyint_pagemap_get(p);

  // We can't realloc memory that wasn't pony_alloc'ed since we can't know how
  // much to copy from the previous location.
  pony_assert(chunk != NULL);

  size_t oldsize;

  if(chunk->size < HEAP_SIZECLASSES)
  {
    // Previous allocation was a ponyint_heap_alloc_small.
    void* ext = EXTERNAL_PTR(p, chunk->size);

    oldsize = SIZECLASS_SIZE(chunk->size) - ((uintptr_t)p - (uintptr_t)ext);
  } else {
    // Previous allocation was a ponyint_heap_alloc_large.

    oldsize = chunk->size - ((uintptr_t)p - (uintptr_t)chunk->m);
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

  // only need to clear `sizeclass == 0` due to others
  // being cleared as part of GC as an optimization
  uint32_t mark = sizeclass_empty[0];
  pony_assert(mark == 0xFFFFFFFF);
  chunk_list(clear_chunk, heap->small_free[0], mark);
  chunk_list(clear_chunk, heap->small_full[0], mark);

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

  if(chunk->size >= HEAP_SIZECLASSES)
  {
    marked = chunk->slots == 0;

    if(p == chunk->m)
      chunk->slots = 0;
    else
      chunk->shallow = 0;
  } else {
    // Calculate the external pointer.
    void* ext = EXTERNAL_PTR(p, chunk->size);

    // Shift to account for smallest allocation size.
    uint32_t slot = FIND_SLOT(ext, chunk->m);

    // Check if it was already marked.
    marked = (chunk->slots & slot) == 0;

    // A clear bit is in-use, a set bit is available.
    if(p == ext)
      chunk->slots &= ~slot;
    else
      chunk->shallow &= ~slot;
  }

  return marked;
}

void ponyint_heap_mark_shallow(chunk_t* chunk, void* p)
{
  maybe_clear_chunk(chunk);

  if(chunk->size >= HEAP_SIZECLASSES)
  {
    chunk->shallow = 0;
  } else {
    // Calculate the external pointer.
    void* ext = EXTERNAL_PTR(p, chunk->size);

    // Shift to account for smallest allocation size.
    uint32_t slot = FIND_SLOT(ext, chunk->m);

    // A clear bit is in-use, a set bit is available.
    chunk->shallow &= ~slot;
  }
}

void ponyint_heap_free(chunk_t* chunk, void* p)
{
  if(chunk->size >= HEAP_SIZECLASSES)
  {
    if(p == chunk->m)
    {
      // run finaliser if needed
      final_large(chunk, 0);

      ponyint_pool_free_size(chunk->size, chunk->m);
      chunk->m = NULL;
      chunk->slots = 1;
    }
    return;
  }

  // Calculate the external pointer.
  void* ext = EXTERNAL_PTR(p, chunk->size);

  if(p == ext)
  {
    // Shift to account for smallest allocation size.
    uint32_t slot = FIND_SLOT(ext, chunk->m);

    // check if there's a finaliser to run
    if((chunk->finalisers & slot) != 0)
    {
      // run finaliser
      (*(pony_type_t**)p)->final(p);

      // clear finaliser
      chunk->finalisers &= ~slot;
    }

    // free slot
    chunk->slots |= slot;
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

  for(int i = 0; i < HEAP_SIZECLASSES; i++)
  {
    chunk_t* list1 = heap->small_free[i];
    chunk_t* list2 = heap->small_full[i];

    heap->small_free[i] = NULL;
    heap->small_full[i] = NULL;

    chunk_t** avail = &heap->small_free[i];
    chunk_t** full = &heap->small_full[i];

    size_t size = SIZECLASS_SIZE(i);
    uint32_t empty = sizeclass_empty[i];

#ifdef USE_RUNTIMESTATS
    used += sweep_small(list1, avail, full, empty, size,
      &mem_allocated, &mem_used, &num_allocated);
    used += sweep_small(list2, avail, full, empty, size,
      &mem_allocated, &mem_used, &num_allocated);
#else
    used += sweep_small(list1, avail, full, empty, size);
    used += sweep_small(list2, avail, full, empty, size);
#endif
  }

#ifdef USE_RUNTIMESTATS
  heap->large = sweep_large(heap->large, &used, &mem_allocated, &mem_used,
    &num_allocated);
#else
  heap->large = sweep_large(heap->large, &used);
#endif

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

pony_actor_t* ponyint_heap_owner(chunk_t* chunk)
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

size_t ponyint_heap_size(chunk_t* chunk)
{
  if(chunk->size >= HEAP_SIZECLASSES)
    return chunk->size;

  return SIZECLASS_SIZE(chunk->size);
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
