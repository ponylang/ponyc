#include "pagemap.h"
#include "alloc.h"
#include "pool.h"
#include <string.h>

#include <platform.h>

#define PAGEMAP_ADDRESSBITS 48
#define PAGEMAP_LEVELS 3
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
  { L3_SHIFT, (1 << L3_MASK) - 1, (1 << L3_MASK) * sizeof(void*),
    POOL_INDEX((1 << L3_MASK) * sizeof(void*)) },
  { L2_SHIFT, (1 << L2_MASK) - 1, (1 << L2_MASK) * sizeof(void*),
    POOL_INDEX((1 << L3_MASK) * sizeof(void*)) },
  { L1_SHIFT, (1 << L1_MASK) - 1, (1 << L1_MASK) * sizeof(void*),
    POOL_INDEX((1 << L1_MASK) * sizeof(void*)) }
};

static void** root;

void* pagemap_get(const void* m)
{
  void** v = root;

  for(int i = 0; i < PAGEMAP_LEVELS; i++)
  {
    if(v == NULL)
      return NULL;

    uintptr_t ix = ((uintptr_t)m >> level[i].shift) & level[i].mask;
    v = (void**)v[ix];
  }

  return v;
}

void pagemap_set(const void* m, void* v)
{
  void*** pv = &root;
  void* p;

  for(int i = 0; i < PAGEMAP_LEVELS; i++)
  {
    if(*pv == NULL)
    {
      p = pool_alloc(level[i].size_index);
      memset(p, 0, level[i].size);
      void** prev = NULL;

      if(!_atomic_cas_strong(pv, &prev, p))
      {
        pool_free(level[i].size_index, p);
      }
    }

    uintptr_t ix = ((uintptr_t)m >> level[i].shift) & level[i].mask;
    pv = (void***)&((*pv)[ix]);
  }

  *pv = (void**)v;
}
