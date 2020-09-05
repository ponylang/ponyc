#define PONY_WANT_ATOMIC_DEFS

#include "pool.h"
#include "../ds/fun.h"
#include "../sched/cpu.h"
#include "ponyassert.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <platform.h>
#include <pony/detail/atomics.h>

#ifdef USE_VALGRIND
#include <valgrind/valgrind.h>
#include <valgrind/helgrind.h>
#endif

/// Allocations this size and above are aligned on this size. This is needed
/// so that the pagemap for the heap is aligned.
#define POOL_ALIGN_INDEX (POOL_ALIGN_BITS - POOL_MIN_BITS)
#define POOL_ALIGN_MASK (POOL_ALIGN - 1)

size_t ponyint_pool_alloc_size(size_t size)
{
  #ifdef PLATFORM_IS_ILP32
#define BITS (32 - POOL_MIN_BITS)
#else
#define BITS (64 - POOL_MIN_BITS)
#endif

  // The condition is in that order for better branch prediction: non-zero pool
  // indices are more likely than zero pool indices.
  uint32_t index = 0;
  if(size > POOL_MIN)
    index = (size_t)BITS - __pony_clzzu(size) - (!(size & (size - 1)));

  return ((size_t)1 << (POOL_MIN_BITS + index));
}

void* ponyint_pool_alloc(size_t size)
{
  size = ponyint_pool_alloc_size(size);
  size_t align = (size >= POOL_ALIGN) ? POOL_ALIGN : size;
  // TODO: invalid use of aligned_alloc!
  void* p = aligned_alloc(align, size);
  if (p == NULL)
  {
    perror("out of memory: ");
    abort();
  }
  return p;
}

void ponyint_pool_free(size_t size, void* p)
{
  (void)size;
  free(p);
}

void* ponyint_pool_realloc(size_t old_size, size_t new_size, void* p)
{

  if(p == NULL)
    return ponyint_pool_alloc(new_size);

  (void)old_size;
  new_size = ponyint_pool_alloc_size(new_size);
  return realloc(p, new_size);
}

void ponyint_pool_thread_cleanup()
{
}

size_t ponyint_pool_adjust_size(size_t size)
{
  // align to `POOL_ALIGN`
  if((size & POOL_ALIGN_MASK) != 0)
    size = (size & ~POOL_ALIGN_MASK) + POOL_ALIGN;

  // we've overflowed the `size_t` datatype
  if(size == 0)
    size = size - 1;

  return size;
}
