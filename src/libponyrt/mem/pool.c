#define PONY_WANT_ATOMIC_DEFS

#include "pool.h"
#include "ponyassert.h"
#include <stdio.h>

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
  // size_t align = (size >= POOL_ALIGN) ? POOL_ALIGN : size;
  // TODO: invalid use of aligned_alloc!
  // void* p = aligned_alloc(align, size);
  void* p = ponyint_alloc(size);
  // void* p = rust_alloc(align, size);
  if (p == NULL)
  {
    perror("out of memory: ");
    abort();
  }
  return p;
}

void ponyint_pool_free(size_t size, void* p)
{
  // (void)size;
  // free(p);
  size = ponyint_pool_alloc_size(size);
  // size_t align = (size >= POOL_ALIGN) ? POOL_ALIGN : size;
  ponyint_dealloc(p, size);
  // rust_dealloc(p, align, size);
}

void* ponyint_pool_realloc(size_t old_size, size_t new_size, void* p)
{
  if(p == NULL)
    return ponyint_pool_alloc(new_size);

  old_size = ponyint_pool_alloc_size(old_size);
  new_size = ponyint_pool_alloc_size(new_size);
  // size_t align = (new_size >= POOL_ALIGN) ? POOL_ALIGN : new_size;
  // return rust_realloc(p, align, old_size, new_size);
  // return realloc(p, new_size);
  return ponyint_realloc(p, old_size, new_size);
}

/// Allocations this size and above are aligned on this size. This is needed
/// so that the pagemap for the heap is aligned.
size_t ponyint_pool_adjust_size(size_t size)
{
  // align to `POOL_ALIGN`
  if((size & (POOL_ALIGN - 1)) != 0)
    size = (size & ~(POOL_ALIGN - 1)) + POOL_ALIGN;

  // we've overflowed the `size_t` datatype
  if(size == 0)
    size = size - 1;

  return size;
}
