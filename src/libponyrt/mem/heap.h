#ifndef mem_heap_h
#define mem_heap_h

#include "pool.h"
#include "../pony.h"
#include <platform.h>
#include <stdlib.h>
#include <stdbool.h>

PONY_EXTERN_C_BEGIN

#define HEAP_MINBITS 5
#define HEAP_MAXBITS (POOL_ALIGN_BITS - 1)
#define HEAP_SIZECLASSES (HEAP_MAXBITS - HEAP_MINBITS + 1)
#define HEAP_MIN (1 << HEAP_MINBITS)
#define HEAP_MAX (1 << HEAP_MAXBITS)

// The number of size classes for recycling. The last size class is for anything bigger than (HEAP_RECYCLE_MIXED_SIZE << (HEAP_RECYCLE_SIZECLASSES = 2))
#define HEAP_RECYCLE_SIZECLASSES 4
#define HEAP_RECYCLE_MIXED 0
#define HEAP_RECYCLE_MIXED_SIZE 1024
#define HEAP_RECYCLE_LARGE_OVERFLOW (HEAP_RECYCLE_SIZECLASSES - 1)

// check HEAP_RECYCLE_SIZECLASSES size
pony_static_assert(HEAP_RECYCLE_LARGE_OVERFLOW > HEAP_RECYCLE_MIXED, "Too few HEAP_RECYCLE_SIZECLASSES! There must be at least 2!");

typedef struct chunk_t chunk_t;

typedef struct heap_t
{
  chunk_t* small_free[HEAP_SIZECLASSES];
  chunk_t* small_full[HEAP_SIZECLASSES];
  chunk_t* large;
  chunk_t* recyclable[HEAP_RECYCLE_SIZECLASSES];

  size_t used;
  size_t next_gc;
} heap_t;

enum
{
  TRACK_NO_FINALISERS = 0,
  TRACK_ALL_FINALISERS = 0xFFFFFFFF,
};

uint32_t ponyint_heap_index(size_t size);

void ponyint_heap_setinitialgc(size_t size);

void ponyint_heap_setnextgcfactor(double factor);

void ponyint_heap_init(heap_t* heap);

void ponyint_heap_destroy(heap_t* heap);

void ponyint_heap_final(heap_t* heap);

__pony_spec_malloc__(
  void* ponyint_heap_alloc(pony_actor_t* actor, heap_t* heap, size_t size,
    uint32_t track_finalisers_mask)
  );

__pony_spec_malloc__(
void* ponyint_heap_alloc_small(pony_actor_t* actor, heap_t* heap,
  uint32_t sizeclass, uint32_t track_finalisers_mask)
  );

__pony_spec_malloc__(
void* ponyint_heap_alloc_large(pony_actor_t* actor, heap_t* heap, size_t size,
  uint32_t track_finalisers_mask)
  );

void* ponyint_heap_realloc(pony_actor_t* actor, heap_t* heap, void* p,
  size_t size, size_t copy);

/**
 * Adds to the used memory figure kept by the heap. This allows objects
 * received in messages to count towards the GC heuristic.
 */
void ponyint_heap_used(heap_t* heap, size_t size);

bool ponyint_heap_startgc(heap_t* heap
#ifdef USE_RUNTIMESTATS
  , pony_actor_t* actor);
#else
  );
#endif

/**
 * Mark an address in a chunk. Returns true if it was already marked, or false
 * if you have just marked it.
 */
bool ponyint_heap_mark(chunk_t* chunk, void* p);

/**
 * Marks an address, but does not affect the return value of ponyint_heap_mark()
 * for the same address, nor does it indicate previous mark status.
 */
void ponyint_heap_mark_shallow(chunk_t* chunk, void* p);

/**
 * Forcibly free this address.
 */
void ponyint_heap_free(chunk_t* chunk, void* p);

void ponyint_heap_endgc(heap_t* heap, uint64_t num_actor_references_deleted
#ifdef USE_RUNTIMESTATS
  , pony_actor_t* actor);
#else
  );
#endif

size_t ponyint_heap_size(chunk_t* chunk);

#ifdef USE_RUNTIMESTATS
/** Get the total number of allocations on the heap.
 */
size_t ponyint_heap_alloc_counter(pony_actor_t* actor);

/** Get the total number of heap allocations freed.
 */
size_t ponyint_heap_free_counter(pony_actor_t* actor);

/** Get the total number of GC iterations run.
 */
size_t ponyint_heap_gc_counter(pony_actor_t* actor);

/** Get the memory used by the heap.
 */
size_t ponyint_heap_mem_size(pony_actor_t* actor);

/** Get the memory overhead used by the heap.
 */
size_t ponyint_heap_overhead_size(pony_actor_t* actor);

/** Get the memory allocated by the heap.
 */
size_t ponyint_heap_alloc_size(pony_actor_t* actor);
#endif

PONY_EXTERN_C_END

#endif
