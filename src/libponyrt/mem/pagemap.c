#define PONY_WANT_ATOMIC_DEFS

#include "pagemap.h"
#include "alloc.h"
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

  { L1_SHIFT, (1 << L1_MASK) - 1, (1 << L1_MASK) * sizeof(void*),
    POOL_INDEX((1 << L1_MASK) * sizeof(void*)) }
};

static PONY_ATOMIC(void*) root;

chunk_t* ponyint_pagemap_get(const void* addr)
{
  PONY_ATOMIC(void*)* next_node = &root;
  void* node = atomic_load_explicit(next_node, memory_order_relaxed);

  for(size_t i = 0; i < PAGEMAP_LEVELS; i++)
  {
    if(node == NULL)
      return NULL;

    uintptr_t ix = ((uintptr_t)addr >> level[i].shift) & level[i].mask;
    next_node = &(((PONY_ATOMIC_RVALUE(void*)*)node)[ix]);
    node = atomic_load_explicit(next_node, memory_order_relaxed);
  }

  return (chunk_t*)node;
}

void ponyint_pagemap_set(const void* addr, chunk_t* chunk)
{
  PONY_ATOMIC(void*)* next_node = &root;

  for(size_t i = 0; i < PAGEMAP_LEVELS; i++)
  {
    void* node = atomic_load_explicit(next_node, memory_order_relaxed);

    if(node == NULL)
    {
      void* new_node = ponyint_pool_alloc(level[i].size_index);
      memset(new_node, 0, level[i].size);

#ifdef USE_VALGRIND
      ANNOTATE_HAPPENS_BEFORE(next_node);
#endif
      if(!atomic_compare_exchange_strong_explicit(next_node, &node, new_node,
        memory_order_release, memory_order_acquire))
      {
#ifdef USE_VALGRIND
        ANNOTATE_HAPPENS_AFTER(next_node);
#endif
        ponyint_pool_free(level[i].size_index, new_node);
      } else {
        node = new_node;
      }
    } else {
      atomic_thread_fence(memory_order_acquire);
#ifdef USE_VALGRIND
      ANNOTATE_HAPPENS_AFTER(next_node);
#endif
    }

    uintptr_t ix = ((uintptr_t)addr >> level[i].shift) & level[i].mask;
    next_node = &(((PONY_ATOMIC_RVALUE(void*)*)node)[ix]);
  }

  atomic_store_explicit(next_node, chunk, memory_order_relaxed);
}

void ponyint_pagemap_set_bulk(const void* addr, chunk_t* chunk, size_t size)
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
        memset(new_node, 0, level[i].size);

#ifdef USE_VALGRIND
        ANNOTATE_HAPPENS_BEFORE(next_node);
#endif
        if(!atomic_compare_exchange_strong_explicit(next_node, &node, new_node,
          memory_order_release, memory_order_acquire))
        {
#ifdef USE_VALGRIND
          ANNOTATE_HAPPENS_AFTER(next_node);
#endif
          ponyint_pool_free(level[i].size_index, new_node);
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
      next_node = &(((PONY_ATOMIC_RVALUE(void*)*)node)[ix]);
    }

    // Store as many pagemap entries as would fit into this pagemap level
    // segment.
    do
    {
      atomic_store_explicit(next_node, chunk, memory_order_relaxed);
      addr_ptr += POOL_ALIGN;
      ix++;
      next_node = &(((PONY_ATOMIC_RVALUE(void*)*)node)[ix]);
      // If ix is greater than mask we need to move to the next pagemap level
      // segment.
    } while((addr_ptr < addr_end) &&
        (ix <= (uintptr_t)level[PAGEMAP_LEVELS-1].mask));
  }
}
