#define PONY_WANT_ATOMIC_DEFS

#include "pool.h"
#include "alloc.h"
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

/// Allocations this size and above are aligned on this size. This is needed
/// so that the pagemap for the heap is aligned.
#define POOL_ALIGN_INDEX (POOL_ALIGN_BITS - POOL_MIN_BITS)
#define POOL_ALIGN_MASK (POOL_ALIGN - 1)

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
