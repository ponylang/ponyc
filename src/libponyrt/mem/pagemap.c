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

/* The radix tree maps each POOL_ALIGN-byte region to its chunk and actor.
 * Below PAGEMAP_ADDRESSBITS, bits L2_SHIFT and above select a leaf.
 * Published leaves must stay allocated because ponyint_pagemap_get can cache
 * each thread's last leaf pointer.
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

static PONY_ATOMIC_RVALUE(void*)* find_leaf(const void* addr)
{
  void* node = atomic_load_explicit(&root, memory_order_acquire);
  for(size_t i = 0; i < PAGEMAP_LEVELS - 1; i++)
  {
    if(node == NULL)
      return NULL;

    size_t ix = ((uintptr_t)addr >> level[i].shift) & level[i].mask;
    node = atomic_load_explicit(&((PONY_ATOMIC_RVALUE(void*)*)node)[ix],
      memory_order_acquire);
  }
  return (PONY_ATOMIC_RVALUE(void*)*)node;
}

static size_t leaf_index(const void* addr)
{
  return 2 * (((uintptr_t)addr >> L1_SHIFT) & level[PAGEMAP_LEVELS - 1].mask);
}

chunk_t* ponyint_pagemap_get_chunk(const void* addr)
{
  PONY_ATOMIC_RVALUE(void*)* leaf = find_leaf(addr);
  if(leaf == NULL)
    return NULL;

  return (chunk_t*)atomic_load_explicit(&leaf[leaf_index(addr)],
    memory_order_acquire);
}

/* This function looks up the chunk_t* to returns it and also looks
 * up the pony_actor_t* to return in the out paramter `actor`. This
 * is done in order to avoid allocating a struct to return as a single
 * return value while minimizing the need to traverse the pagemap multiple
 * times in order to retrieve both the chunk_t* and the pony_actor_t*.
 *
 * NOTE: the multiple atomic instructions to retrieve the chunk_t* followed
 * by the pony_actor_t* should be safe because every update to the pagemap
 * happens while at most one actor can hold a reference to memory that would
 * resolve to that pagemap entry.
 */
chunk_t* ponyint_pagemap_get(const void* addr, pony_actor_t** actor)
{
#ifdef PLATFORM_IS_MACOSX
  // Each thread-local access on macOS goes through a function call. With the
  // cache, that made lookups slower on several workloads, so macOS walks the
  // tree every time.
  PONY_ATOMIC_RVALUE(void*)* leaf = find_leaf(addr);
  if(leaf == NULL)
    return NULL;
#else
  static __pony_thread_local struct
  {
    uintptr_t prefix;
    PONY_ATOMIC_RVALUE(void*)* leaf;
  } cached;
  uintptr_t prefix = (uintptr_t)addr >> L2_SHIFT;
  PONY_ATOMIC_RVALUE(void*)* leaf = cached.leaf;

  if((leaf == NULL) || (prefix != cached.prefix))
  {
    leaf = find_leaf(addr);
    if(leaf == NULL)
      return NULL;

    cached.leaf = leaf;
    cached.prefix = prefix;
  }
#endif

  size_t ix = leaf_index(addr);
  chunk_t* chunk = (chunk_t*)atomic_load_explicit(&leaf[ix],
    memory_order_acquire);
  *actor = (pony_actor_t*)atomic_load_explicit(&leaf[ix + 1],
    memory_order_acquire);
  return chunk;
}

/* This function sets the chunk_t* and the pony_actor_t* in the pagemap for
 * a small chunk_t (exactly one pagemap entry).
 *
 * NOTE: the multiple atomic instructions to set the chunk_t* followed
 * by the pony_actor_t* should be safe because every update to the pagemap
 * happens while at most one actor can hold a reference to memory that would
 * resolve to that pagemap entry.
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
 * by the pony_actor_t* should be safe because every update to the pagemap
 * happens while at most one actor can hold a reference to memory that would
 * resolve to that pagemap entry.
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
