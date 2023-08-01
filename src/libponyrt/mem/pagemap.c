#define PONY_WANT_ATOMIC_DEFS

#include "pagemap.h"
#include "alloc.h"
#include "ponyassert.h"
#include "pool.h"
#include <string.h>

#include <platform.h>
#include <pony/detail/atomics.h>

#ifdef USE_VALGRIND
#include <valgrind/helgrind.h>
#endif

#ifdef PLATFORM_IS_ILP32
# define PAGEMAP_ADDRESSBITS 32
# define PAGEMAP_LEVELS 2
#else
# define PAGEMAP_ADDRESSBITS 48
# define PAGEMAP_LEVELS 3
#endif

#define PAGEMAP_BITS (PAGEMAP_ADDRESSBITS - POOL_ALIGN_BITS) / PAGEMAP_LEVELS
#define PAGEMAP_EXCESS (PAGEMAP_ADDRESSBITS - POOL_ALIGN_BITS) % PAGEMAP_LEVELS

#define L1_MASK (PAGEMAP_BITS)
#define L2_MASK (PAGEMAP_BITS + (PAGEMAP_EXCESS > 1))
#define L3_MASK (PAGEMAP_BITS + (PAGEMAP_EXCESS > 0))

#define L1_SHIFT (POOL_ALIGN_BITS)
#define L2_SHIFT (L1_SHIFT + L1_MASK)
#define L3_SHIFT (L2_SHIFT + L2_MASK)

typedef struct pagemap_level_t
{
  int shift;
  int mask;
  size_t size;
  size_t size_index;
} pagemap_level_t;

/* The pagemap is a prefix tree that maps heap memory to its corresponding
 * bookkeeping data structure. It is constructed for a 48 bit address space
 * where we are interested in memory with POOL_ALIGN_BITS granularity.
 *
 * At the first level, we shift 35 and mask on the low 13 bits, giving us bits
 * 35-47. At the second level, we shift 23 and mask on the low 12 bits, giving
 * us bits 23-34. At the third level, we shift 11 and mask on the low 12 bits,
 * giving us bits 11-22. Bits 0-10 and 48-63 are always ignored.
 */
static const pagemap_level_t level[PAGEMAP_LEVELS] =
{
#if PAGEMAP_LEVELS >= 3
  { L3_SHIFT, (1 << L3_MASK) - 1, (1 << L3_MASK) * sizeof(void*),
    POOL_INDEX((1 << L3_MASK) * sizeof(void*)) },
#endif

#if PAGEMAP_LEVELS >= 2
  { L2_SHIFT, (1 << L2_MASK) - 1, (1 << L2_MASK) * sizeof(void*),
    POOL_INDEX((1 << L2_MASK) * sizeof(void*)) },
#endif

  // The lowest level is sized to hold 2 `void*` entries (the `* 2` in the
  // `size`/`size_index` fields), one for `chunk_t*` and one for `pony_actor_t*`.
  { L1_SHIFT, (1 << L1_MASK) - 1, (1 << L1_MASK) * sizeof(void*) * 2,
    POOL_INDEX((1 << L1_MASK) * sizeof(void*) * 2) }
};

static PONY_ATOMIC(void*) root;

#ifdef USE_RUNTIMESTATS
static PONY_ATOMIC(size_t) mem_allocated;
static PONY_ATOMIC(size_t) mem_used;

/** Get the memory used by the pagemap.
 */
size_t ponyint_pagemap_mem_size()
{
  return atomic_load_explicit(&mem_used, memory_order_relaxed);
}

/** Get the memory allocated by the pagemap.
 */
size_t ponyint_pagemap_alloc_size()
{
  return atomic_load_explicit(&mem_allocated, memory_order_relaxed);
}
#endif

chunk_t* ponyint_pagemap_get_chunk(const void* addr)
{
  PONY_ATOMIC(void*)* next_node = &root;
  void* node = atomic_load_explicit(next_node, memory_order_acquire);
  uintptr_t ix = 0;

  for(size_t i = 0; i < PAGEMAP_LEVELS; i++)
  {
    if(node == NULL)
      return NULL;

    ix = ((uintptr_t)addr >> level[i].shift) & level[i].mask;
    if (i == PAGEMAP_LEVELS - 1)
    {
      // at the lowest level we have to double the index since we store
      // two pointers (`chunk_t*` and `pony_actor_t*`) instead of one
      ix = ix * 2;
      next_node = &(((PONY_ATOMIC_RVALUE(void*)*)node)[ix]);
      pony_assert(ix <= (uintptr_t)level[i].size);
      pony_assert(ix/2 <= (uintptr_t)level[i].mask);
    }
    else
    {
      next_node = &(((PONY_ATOMIC_RVALUE(void*)*)node)[ix]);
      node = atomic_load_explicit(next_node, memory_order_acquire);
      pony_assert(ix <= (uintptr_t)level[i].size);
      pony_assert(ix <= (uintptr_t)level[i].mask);
    }
  }

  chunk_t* chunk = (chunk_t*)atomic_load_explicit(next_node, memory_order_acquire);
  return chunk;
}

/* This function looks up the chunk_t* to returns it and also looks
 * up the pony_actor_t* to return in the out paramter `actor`. This
 * is done in order to avoid allocating a struct to return as a single
 * return value while minimizing the need to traverse the pagemap multiple
 * times in order to retrieve both the chunk_t* and the pony_actor_t*.
 *
 * NOTE: the multiple atomic instructions to retrieve the chunk_t* followed
 * by the pony_actor_t* should be safe because the only time the pagemap
 * is updated is when a chunk_t is first allocated by an actor or when the
 * chunk_t is deallocated/freed by GC and in both scenarios the only actor
 * with a reference to memory that would resolve to that pagemap entry is
 * the owning actor of the chunk_t.
 */
chunk_t* ponyint_pagemap_get(const void* addr, pony_actor_t** actor)
{
  PONY_ATOMIC(void*)* next_node = &root;
  void* node = atomic_load_explicit(next_node, memory_order_acquire);
  uintptr_t ix = 0;

  for(size_t i = 0; i < PAGEMAP_LEVELS; i++)
  {
    if(node == NULL)
      return NULL;

    ix = ((uintptr_t)addr >> level[i].shift) & level[i].mask;
    if (i == PAGEMAP_LEVELS - 1)
    {
      // at the lowest level we have to double the index since we store
      // two pointers (`chunk_t*` and `pony_actor_t*`) instead of one
      ix = ix * 2;
      next_node = &(((PONY_ATOMIC_RVALUE(void*)*)node)[ix]);
      pony_assert(ix <= (uintptr_t)level[i].size);
      pony_assert(ix/2 <= (uintptr_t)level[i].mask);
    }
    else
    {
      next_node = &(((PONY_ATOMIC_RVALUE(void*)*)node)[ix]);
      node = atomic_load_explicit(next_node, memory_order_acquire);
      pony_assert(ix <= (uintptr_t)level[i].size);
      pony_assert(ix <= (uintptr_t)level[i].mask);
    }
  }

  chunk_t* chunk = (chunk_t*)atomic_load_explicit(next_node, memory_order_acquire);
  ix++;
  pony_assert(ix <= (uintptr_t)level[PAGEMAP_LEVELS - 1].size);
  pony_assert(ix/2 <= (uintptr_t)level[PAGEMAP_LEVELS - 1].mask);
  next_node = &(((PONY_ATOMIC_RVALUE(void*)*)node)[ix]);
  void* pagemap_actor = atomic_load_explicit(next_node, memory_order_acquire);
  *actor = (pony_actor_t*)pagemap_actor;

  return chunk;
}

/* This function sets the chunk_t* and the pony_actor_t* in the pagemap for
 * a small chunk_t (exactly one pagemap entry).
 *
 * NOTE: the multiple atomic instructions to set the chunk_t* followed
 * by the pony_actor_t* should be safe because the only time the pagemap
 * is updated is when a chunk_t is first allocated by an actor or when the
 * chunk_t is deallocated/freed by GC and in both scenarios the only actor
 * with a reference to memory that would resolve to that pagemap entry is
 * the owning actor of the chunk_t.
 */
void ponyint_pagemap_set(const void* addr, chunk_t* chunk, pony_actor_t* actor)
{
  PONY_ATOMIC(void*)* next_node = &root;
  void* node = NULL;
  uintptr_t ix = 0;

  for(size_t i = 0; i < PAGEMAP_LEVELS; i++)
  {
    node = atomic_load_explicit(next_node, memory_order_relaxed);

    if(node == NULL)
    {
      void* new_node = ponyint_pool_alloc(level[i].size_index);
#ifdef USE_RUNTIMESTATS
      atomic_fetch_add_explicit(&mem_used, level[i].size, memory_order_relaxed);
      atomic_fetch_add_explicit(&mem_allocated, POOL_SIZE(level[i].size_index),
        memory_order_relaxed);
#endif
      memset(new_node, 0, level[i].size);

#ifdef USE_VALGRIND
      ANNOTATE_HAPPENS_BEFORE(next_node);
#endif
      if(!atomic_compare_exchange_strong_explicit(next_node, &node, new_node,
        memory_order_acq_rel, memory_order_acquire))
      {
#ifdef USE_VALGRIND
        ANNOTATE_HAPPENS_AFTER(next_node);
#endif
        ponyint_pool_free(level[i].size_index, new_node);
#ifdef USE_RUNTIMESTATS
        atomic_fetch_sub_explicit(&mem_used, level[i].size, memory_order_relaxed);
        atomic_fetch_sub_explicit(&mem_allocated, POOL_SIZE(level[i].size_index),
          memory_order_relaxed);
#endif
      } else {
        node = new_node;
      }
    } else {
      atomic_thread_fence(memory_order_acquire);
#ifdef USE_VALGRIND
      ANNOTATE_HAPPENS_AFTER(next_node);
#endif
    }

    ix = ((uintptr_t)addr >> level[i].shift) & level[i].mask;
    pony_assert(ix <= (uintptr_t)level[i].size);
    pony_assert(ix <= (uintptr_t)level[i].mask);

    // at the lowest level we have to double the index since we store
    // two pointers (`chunk_t*` and `pony_actor_t*`) instead of one
    if (i == PAGEMAP_LEVELS - 1)
      ix = ix * 2;
    next_node = &(((PONY_ATOMIC_RVALUE(void*)*)node)[ix]);
  }

  pony_assert(ix <= (uintptr_t)level[PAGEMAP_LEVELS - 1].size);
  pony_assert(ix/2 <= (uintptr_t)level[PAGEMAP_LEVELS - 1].mask);
  atomic_store_explicit(next_node, chunk, memory_order_release);
  ix++;
  pony_assert(ix <= (uintptr_t)level[PAGEMAP_LEVELS - 1].size);
  pony_assert(ix/2 <= (uintptr_t)level[PAGEMAP_LEVELS - 1].mask);
  next_node = &(((PONY_ATOMIC_RVALUE(void*)*)node)[ix]);
  atomic_store_explicit(next_node, actor, memory_order_release);
}

/* This function sets the chunk_t* and the pony_actor_t* in the pagemap for
 * a large chunk_t's (more than one pagemap entry required).
 *
 * NOTE: the multiple atomic instructions to set the chunk_t* followed
 * by the pony_actor_t* should be safe because the only time the pagemap
 * is updated is when a chunk_t is first allocated by an actor or when the
 * chunk_t is deallocated/freed by GC and in both scenarios the only actor
 * with a reference to memory that would resolve to that pagemap entry is
 * the owning actor of the chunk_t.
 */
void ponyint_pagemap_set_bulk(const void* addr, chunk_t* chunk, pony_actor_t* actor, size_t size)
{
  PONY_ATOMIC(void*)* next_node = NULL;
  void* node = NULL;
  uintptr_t ix = 0;
  uintptr_t addr_ptr = (uintptr_t)addr;
  uintptr_t addr_end = (uintptr_t)addr + size;

  while(addr_ptr < addr_end)
  {
    next_node = &root;

    for(size_t i = 0; i < PAGEMAP_LEVELS; i++)
    {
      node = atomic_load_explicit(next_node, memory_order_relaxed);

      if(node == NULL)
      {
        void* new_node = ponyint_pool_alloc(level[i].size_index);
#ifdef USE_RUNTIMESTATS
        atomic_fetch_add_explicit(&mem_used, level[i].size, memory_order_relaxed);
        atomic_fetch_add_explicit(&mem_allocated, POOL_SIZE(level[i].size_index),
          memory_order_relaxed);
#endif
        memset(new_node, 0, level[i].size);

#ifdef USE_VALGRIND
        ANNOTATE_HAPPENS_BEFORE(next_node);
#endif
        if(!atomic_compare_exchange_strong_explicit(next_node, &node, new_node,
          memory_order_acq_rel, memory_order_acquire))
        {
#ifdef USE_VALGRIND
          ANNOTATE_HAPPENS_AFTER(next_node);
#endif
          ponyint_pool_free(level[i].size_index, new_node);
#ifdef USE_RUNTIMESTATS
          atomic_fetch_sub_explicit(&mem_used, level[i].size, memory_order_relaxed);
          atomic_fetch_sub_explicit(&mem_allocated, POOL_SIZE(level[i].size_index),
            memory_order_relaxed);
#endif
        } else {
          node = new_node;
        }
      } else {
        atomic_thread_fence(memory_order_acquire);
#ifdef USE_VALGRIND
        ANNOTATE_HAPPENS_AFTER(next_node);
#endif
      }

      ix = (addr_ptr >> level[i].shift) & level[i].mask;
      pony_assert(ix <= (uintptr_t)level[i].size);
      pony_assert(ix <= (uintptr_t)level[i].mask);
      // at the lowest level we have to double the index since we store
      // two pointers (`chunk_t*` and `pony_actor_t*`) instead of one
      if (i == PAGEMAP_LEVELS - 1)
        ix = ix * 2;
      next_node = &(((PONY_ATOMIC_RVALUE(void*)*)node)[ix]);
    }

    // Store as many pagemap entries as would fit into this pagemap level
    // segment.
    do
    {
      atomic_store_explicit(next_node, chunk, memory_order_release);
      addr_ptr += POOL_ALIGN;
      ix++;
      pony_assert(ix <= (uintptr_t)level[PAGEMAP_LEVELS - 1].size);
      pony_assert(ix/2 <= (uintptr_t)level[PAGEMAP_LEVELS - 1].mask);
      next_node = &(((PONY_ATOMIC_RVALUE(void*)*)node)[ix]);
      atomic_store_explicit(next_node, actor, memory_order_release);
      ix++;
      next_node = &(((PONY_ATOMIC_RVALUE(void*)*)node)[ix]);
      // If ix/2 is greater than mask we need to move to the next pagemap level
      // segment. Note: we use `ix/2` instead of `ix` since `ix` has already been
      // doubled to account for our storage of two pointers instead of one and
      // `ix/2` represents the actual entry offset in the array.
    } while((addr_ptr < addr_end) &&
        (ix/2 <= (uintptr_t)level[PAGEMAP_LEVELS-1].mask));
  }
}
