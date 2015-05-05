#include "pagemap.h"
#include "heap.h"
#include <stddef.h>
#include <stdint.h>

#include <platform.h>

/*
 * |<---------(SIZEOF_POINTER*8)------------>|
 *       |<-------PAGEMAP_ADDRESSBITS------->|
 *       |                  |<-HEAP_MAXBITS->|
 *       |<-L3-><-L2-><-L1->|
 */
#if SIZEOF_POINTER * 8 == 32
# define PAGEMAP_ADDRESSBITS 29
# define PAGEMAP_LEVELS 2
#else
# define PAGEMAP_ADDRESSBITS 48
# define PAGEMAP_LEVELS 3
#endif
#define PAGEMAP_BITS (PAGEMAP_ADDRESSBITS - HEAP_MAXBITS) / PAGEMAP_LEVELS
#define PAGEMAP_EXCESS (PAGEMAP_ADDRESSBITS - HEAP_MAXBITS) % PAGEMAP_LEVELS

#define L1_MASK (PAGEMAP_BITS)
#define L2_MASK (PAGEMAP_BITS + (PAGEMAP_EXCESS > (PAGEMAP_LEVELS - 2)))
#define L3_MASK (PAGEMAP_BITS + (PAGEMAP_EXCESS > (PAGEMAP_LEVELS - 3)))

#define L1_SHIFT (HEAP_MAXBITS)
#define L2_SHIFT (L1_SHIFT + L1_MASK)
#define L3_SHIFT (L2_SHIFT + L2_MASK)

typedef struct pagemap_level_t
{
  int shift;
  int mask;
  size_t size;
} pagemap_level_t;

/* Constructed for a 48 bit address space where we are interested in memory
 * with 11 bit (2048 byte) granularity.
 *
 * At the first level, we shift 35 and mask on the low 13 bits, giving us bits
 * 35-47. At the second level, we shift 23 and mask on the low 12 bits, giving
 * us bits 23-34. At the third level, we shift 11 and mask on the low 12 bits,
 * giving us bits 11-22. Bits 0-10 and 48-63 are always ignored.
 */
static const pagemap_level_t level[PAGEMAP_LEVELS] =
{
#if PAGEMAP_LEVELS >= 3
  { L3_SHIFT, (1 << L3_MASK) - 1, (1 << L3_MASK) * sizeof(void*) },
#endif
  { L2_SHIFT, (1 << L2_MASK) - 1, (1 << L2_MASK) * sizeof(void*) },
  { L1_SHIFT, (1 << L1_MASK) - 1, (1 << L1_MASK) * sizeof(void*) }
};

static void** root = NULL;

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
      p = virtual_alloc(level[i].size);
      void** prev = NULL;

      if(!_atomic_cas(pv, &prev, p, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
      {
        virtual_free(p, level[i].size);
      }
    }

    uintptr_t ix = ((uintptr_t)m >> level[i].shift) & level[i].mask;
    pv = (void***)&((*pv)[ix]);
  }

  *pv = (void**)v;
}
