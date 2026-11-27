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
// HEAP_MIN is the minimum guaranteed alignment and size of every heap
// allocation. Lowering it below a target's maximum field alignment can
// under-align object fields. See issue #5462.
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

typedef bool (*heap_final_visit_fn)(void* arg, void* p, bool traceable);
bool ponyint_heap_visit_finalisers(heap_t* heap, heap_final_visit_fn visit,
  void* arg, size_t* local_used);
PONY_API void ponyint_gc_final_ready(pony_ctx_t* ctx, void* p);

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

void ponyint_heap_startgc(heap_t* heap
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

typedef enum
{
  // p starts its allocation, which this call marked.
  HEAP_MARK_NEW,
  // p starts its allocation, which was already marked.
  HEAP_MARK_MARKED,
  // p starts an allocation whose start is shared. This function never sets
  // such an allocation's full mark: it is kept with a shallow mark.
  HEAP_MARK_SHARED_START,
  // p is inside its allocation, which isn't marked, and this call set the
  // allocation's shallow mark, which keeps it.
  HEAP_MARK_INTERIOR_FIRST,
  // p is inside its allocation, which isn't marked, and the allocation's
  // shallow mark was already set, by an earlier visit or an opaque one.
  HEAP_MARK_INTERIOR,
  // p is inside its allocation, which is already marked.
  HEAP_MARK_COVERED
} heap_mark_t;

/**
 * Mark the allocation that p is in, if p starts it, and report what was found.
 * A pointer inside the allocation keeps it with a shallow mark.
 */
heap_mark_t ponyint_heap_mark_object(chunk_t* chunk, void* p);

/**
 * Records that the allocation p starts is a struct that starts with an
 * embedded field with a non-zero size, so an object traced at that address may
 * be only that field. ponyint_heap_mark_object never sets such an allocation's
 * full mark. The allocation must not have a finaliser.
 */
void ponyint_heap_set_shared_start(void* p);

uint32_t ponyint_heap_base_slot(chunk_t* chunk, void* p);
uint32_t* ponyint_heap_local_index(chunk_t* chunk);
void ponyint_heap_mark_slots(chunk_t* chunk, uint32_t slots, bool shallow);

/**
 * Marks an address, but does not affect the return value of ponyint_heap_mark()
 * or ponyint_heap_mark_object() for the same address, nor does it indicate
 * previous mark status.
 */
void ponyint_heap_mark_shallow(chunk_t* chunk, void* p);

/**
 * Forcibly free this address.
 */
void ponyint_heap_free(chunk_t* chunk, void* p);

bool ponyint_heap_endgc(heap_t* heap, uint64_t num_actor_references_deleted
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
