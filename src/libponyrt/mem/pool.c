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

void* ponyint_pool_alloc(size_t index)
{
  void* p = ponyint_alloc(POOL_SIZE(index));
  memset(p, 0, POOL_SIZE(index));
  if (p == NULL)
  {
    perror("out of memory: ");
    abort();
  }
  return p;
}

void* ponyint_pool_alloc_size(size_t size)
{
  return ponyint_pool_alloc(ponyint_pool_index(size));
}

void ponyint_pool_free(size_t index, void* p)
{
  ponyint_dealloc(p, POOL_SIZE(index));
}

void ponyint_pool_free_size(size_t size, void* p)
{
  ponyint_pool_free(ponyint_pool_index(size), p);
}

void* ponyint_pool_realloc_size(size_t old_size, size_t new_size, void* p)
{
  // Can only reuse the old pointer if the old index/adjusted size is equal to
  // the new one, not greater.

  old_size = POOL_SIZE(ponyint_pool_index(old_size));
  new_size = POOL_SIZE(ponyint_pool_index(new_size));

  if(p == NULL)
    return ponyint_pool_alloc_size(new_size);

  // void* new_p = ponyint_pool_alloc_size(new_size);
  // memcpy(new_p, p, old_size < new_size ? old_size : new_size);
  // ponyint_pool_free_size(old_size, p);
  // return new_p;

  return ponyint_realloc(p, old_size, new_size);
}

void ponyint_pool_thread_cleanup()
{
}

size_t ponyint_pool_index(size_t size)
{
#ifdef PLATFORM_IS_ILP32
#define BITS (32 - POOL_MIN_BITS)
#else
#define BITS (64 - POOL_MIN_BITS)
#endif

  // The condition is in that order for better branch prediction: non-zero pool
  // indices are more likely than zero pool indices.
  if(size > POOL_MIN)
    return (size_t)BITS - __pony_clzzu(size) - (!(size & (size - 1)));

  return 0;
}

size_t ponyint_pool_used_size(size_t size)
{
  size_t index = ponyint_pool_index(size);

  if(index < POOL_COUNT)
    return POOL_SIZE(index);

  return ponyint_pool_adjust_size(size);
}

size_t ponyint_pool_adjust_size(size_t size)
{
  if((size & POOL_ALIGN_MASK) != 0)
    size = (size & ~POOL_ALIGN_MASK) + POOL_ALIGN;

  // we've overflowed the `size_t` datatype
  if(size == 0)
    size = size - 1;

  return size;
}
