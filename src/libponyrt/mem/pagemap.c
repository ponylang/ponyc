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

/* Constructed for a 48 bit address space where we are interested in memory
 * with POOL_ALIGN_BITS granularity.
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

static PONY_ATOMIC(void**) root;

void* ponyint_pagemap_get(const void* m)
{
  void** v = atomic_load_explicit(&root, memory_order_relaxed);

  for(int i = 0; i < PAGEMAP_LEVELS; i++)
  {
    if(v == NULL)
      return NULL;

    uintptr_t ix = ((uintptr_t)m >> level[i].shift) & level[i].mask;
    PONY_ATOMIC(void**)* av = (PONY_ATOMIC(void**)*)&(v[ix]);
    v = atomic_load_explicit(av, memory_order_relaxed);
  }

  return v;
}

void ponyint_pagemap_set(const void* m, void* v)
{
  PONY_ATOMIC(void**)* pv = &root;
  void* p;

  for(int i = 0; i < PAGEMAP_LEVELS; i++)
  {
    void** pv_ld = atomic_load_explicit(pv, memory_order_acquire);
    if(pv_ld == NULL)
    {
      p = ponyint_pool_alloc(level[i].size_index);
      memset(p, 0, level[i].size);
      void** prev = NULL;

#ifdef USE_VALGRIND
        ANNOTATE_HAPPENS_BEFORE(pv);
#endif
      if(!atomic_compare_exchange_strong_explicit(pv, &prev, (void**)p,
        memory_order_release, memory_order_acquire))
      {
#ifdef USE_VALGRIND
        ANNOTATE_HAPPENS_AFTER(pv);
#endif
        ponyint_pool_free(level[i].size_index, p);
        pv_ld = prev;
      } else {
        pv_ld = (void**)p;
      }
    }

    uintptr_t ix = ((uintptr_t)m >> level[i].shift) & level[i].mask;
    pv = (PONY_ATOMIC(void**)*)&(pv_ld[ix]);
  }

  atomic_store_explicit(pv, (void**)v, memory_order_relaxed);
}

void ponyint_pagemap_set_bulk(const void* m, void* v, size_t size)
{
  PONY_ATOMIC(void**)* pv = NULL;
  void* p;
  void** pv_ld = NULL;
  uintptr_t ix = 0;
  uintptr_t m_ptr = (uintptr_t)m;
  uintptr_t m_end = (uintptr_t)m + size;

  while(m_ptr < m_end)
  {
    pv = &root;
    for(int i = 0; i < PAGEMAP_LEVELS; i++)
    {
      pv_ld = atomic_load_explicit(pv, memory_order_acquire);
      if(pv_ld == NULL)
      {
        p = ponyint_pool_alloc(level[i].size_index);
        memset(p, 0, level[i].size);
        void** prev = NULL;

#ifdef USE_VALGRIND
        ANNOTATE_HAPPENS_BEFORE(pv);
#endif
        if(!atomic_compare_exchange_strong_explicit(pv, &prev, (void**)p,
          memory_order_release, memory_order_acquire))
        {
#ifdef USE_VALGRIND
          ANNOTATE_HAPPENS_AFTER(pv);
#endif
          ponyint_pool_free(level[i].size_index, p);
          pv_ld = prev;
        } else {
          pv_ld = (void**)p;
        }
      }

      ix = (m_ptr >> level[i].shift) & level[i].mask;
      pv = (PONY_ATOMIC(void**)*)&(pv_ld[ix]);
    }

    // store as many pagemap entries as would fit into this pagemap level segment
    do {
      atomic_store_explicit(pv, (void**)v, memory_order_relaxed);
      m_ptr += POOL_ALIGN;
      ix++;
      pv = (PONY_ATOMIC(void**)*)&(pv_ld[ix]);
      // if ix is greater than mask we need to move to the next pagement level segment
    } while((m_ptr < m_end) && (ix <= (uintptr_t)level[PAGEMAP_LEVELS-1].mask));
  }
}
