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

#ifdef POOL_USE_MEMALIGN

void* ponyint_pool_alloc(size_t index)
{
  void* p;
  size_t size = POOL_SIZE(index);
  if(size < POOL_ALIGN)
  {
    p = malloc(size);
  }
  else
  {
    int code = posix_memalign(&p, POOL_ALIGN, size);
    (void)code;
    pony_assert(code == 0);
  }

  return p;
}

void ponyint_pool_free(size_t index, void* p)
{
  (void)index;
  pony_assert(index < POOL_COUNT);

  free(p);
}

static void* pool_alloc_size(size_t size)
{
  void* p;
  int code = posix_memalign(&p, POOL_ALIGN, size);
  (void)code;
  pony_assert(code == 0);

  return p;
}

void* ponyint_pool_alloc_size(size_t size)
{
  size_t index = ponyint_pool_index(size);

  if(index < POOL_COUNT)
    return ponyint_pool_alloc(index);

  size = ponyint_pool_adjust_size(size);
  void* p = pool_alloc_size(size);

  return p;
}

static void pool_free_size(size_t size, void* p)
{
  free(p);
  (void)size;
}

void ponyint_pool_free_size(size_t size, void* p)
{
  size_t index = ponyint_pool_index(size);

  if(index < POOL_COUNT)
    return ponyint_pool_free(index, p);

  size = ponyint_pool_adjust_size(size);
  pool_free_size(size, p);
}

void* ponyint_pool_realloc_size(size_t old_size, size_t new_size, void* p)
{
  // Can only reuse the old pointer if the old index/adjusted size is equal to
  // the new one, not greater.

  if(p == NULL)
    return ponyint_pool_alloc_size(new_size);

  size_t old_index = ponyint_pool_index(old_size);
  size_t new_index = ponyint_pool_index(new_size);
  size_t old_adj_size = 0;

  void* new_p;

  if(new_index < POOL_COUNT)
  {
    if(old_index == new_index)
      return p;

    new_p = ponyint_pool_alloc(new_index);
  } else {
    size_t new_adj_size = ponyint_pool_adjust_size(new_size);

    if(old_index >= POOL_COUNT)
    {
      old_adj_size = ponyint_pool_adjust_size(old_size);

      if(old_adj_size == new_adj_size)
        return p;
    }

    new_p = pool_alloc_size(new_adj_size);
  }

  memcpy(new_p, p, old_size < new_size ? old_size : new_size);

  if(old_index < POOL_COUNT)
    ponyint_pool_free(old_index, p);
  else
    pool_free_size(old_adj_size, p);

  return new_p;
}

void ponyint_pool_thread_cleanup()
{
}

#endif
