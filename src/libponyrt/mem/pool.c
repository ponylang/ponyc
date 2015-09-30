#include "pool.h"
#include "alloc.h"
#include "../ds/fun.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <platform.h>

#ifdef USE_VALGRIND
#include <valgrind/valgrind.h>
#endif

/* Because of the way free memory is reused as its own linked list container,
 * the minimum allocation size is 32 bytes.
 */

#define POOL_MAX_BITS 20
#define POOL_MAX (1 << POOL_MAX_BITS)
#define POOL_MIN (1 << POOL_MIN_BITS)
#define POOL_COUNT (POOL_MAX_BITS - POOL_MIN_BITS + 1)

/// Allocations this size and above are aligned on this size. This is needed
/// so that the pagemap for the heap is aligned.
#define POOL_ALIGN_INDEX (POOL_ALIGN_BITS - POOL_MIN_BITS)
#define POOL_ALIGN (1 << POOL_ALIGN_BITS)
#define POOL_ALIGN_MASK (POOL_ALIGN - 1)

/// When we mmap, pull at least this many bytes.
#define POOL_MMAP (128 * 1024 * 1024) // 128 MB

/// Combines an ABA counter with a pointer.
typedef __int128_t pool_aba_t;

/// An item on a per-size thread-local free list.
typedef struct pool_item_t
{
  struct pool_item_t* next;
} pool_item_t;

/// A per-size thread-local free list header.
typedef struct pool_local_t
{
  pool_item_t* pool;
  size_t length;
  char* start;
  char* end;
} pool_local_t;

/// A free list on a per-size global list of free lists.
typedef struct pool_central_t
{
  pool_item_t* next;
  uintptr_t length;
  struct pool_central_t* central;
} pool_central_t;

/// An ABA protected CAS pointer to a per-size global free list.
typedef struct pool_cmp_t
{
  union
  {
    struct
    {
      uint64_t aba;
      pool_central_t* node;
    };

    pool_aba_t dw;
  };
} pool_cmp_t;

/// A per-size global list of free lists header.
typedef struct pool_global_t
{
  size_t size;
  size_t count;
  pool_aba_t central;
} pool_global_t;

/// An item on a thread-local list of free blocks.
typedef struct pool_block_t
{
  struct pool_block_t* prev;
  struct pool_block_t* next;
  size_t size;
} pool_block_t;

/// A thread local list of free blocks header.
typedef struct pool_block_header_t
{
  pool_block_t* head;
  size_t total_size;
  size_t largest_size;
} pool_block_header_t;

static pool_global_t pool_global[POOL_COUNT] =
{
  {POOL_MIN << 0, POOL_MAX / (POOL_MIN << 0), 0},
  {POOL_MIN << 1, POOL_MAX / (POOL_MIN << 1), 0},
  {POOL_MIN << 2, POOL_MAX / (POOL_MIN << 2), 0},
  {POOL_MIN << 3, POOL_MAX / (POOL_MIN << 3), 0},
  {POOL_MIN << 4, POOL_MAX / (POOL_MIN << 4), 0},
  {POOL_MIN << 5, POOL_MAX / (POOL_MIN << 5), 0},
  {POOL_MIN << 6, POOL_MAX / (POOL_MIN << 6), 0},
  {POOL_MIN << 7, POOL_MAX / (POOL_MIN << 7), 0},
  {POOL_MIN << 8, POOL_MAX / (POOL_MIN << 8), 0},
  {POOL_MIN << 9, POOL_MAX / (POOL_MIN << 9), 0},
  {POOL_MIN << 10, POOL_MAX / (POOL_MIN << 10), 0},
  {POOL_MIN << 11, POOL_MAX / (POOL_MIN << 11), 0},
  {POOL_MIN << 12, POOL_MAX / (POOL_MIN << 12), 0},
  {POOL_MIN << 13, POOL_MAX / (POOL_MIN << 13), 0},
  {POOL_MIN << 14, POOL_MAX / (POOL_MIN << 14), 0},
  {POOL_MIN << 15, POOL_MAX / (POOL_MIN << 15), 0},
};

static __pony_thread_local pool_local_t pool_local[POOL_COUNT];
static __pony_thread_local pool_block_header_t pool_block_header;

static void pool_block_remove(pool_block_t* block)
{
  if(block->prev != NULL)
    block->prev->next = block->next;
  else
    pool_block_header.head = block->next;

  if(block->next != NULL)
    block->next->prev = block->prev;
}

static void pool_block_insert(pool_block_t* block)
{
  pool_block_t* next = pool_block_header.head;
  pool_block_t* prev = NULL;

  while(next != NULL)
  {
    if(block->size < next->size)
      break;

    prev = next;
    next = next->next;
  }

  block->prev = prev;
  block->next = next;

  if(prev != NULL)
    prev->next = block;
  else
    pool_block_header.head = block;

  if(next != NULL)
    next->prev = block;
}

static void* pool_alloc_pages(size_t size)
{
  if(pool_block_header.total_size >= size)
  {
    pool_block_t* block = pool_block_header.head;

    while(block != NULL)
    {
      if(block->size > size)
      {
        // Use size bytes from the end of the block. This allows us to keep the
        // block info inside the block instead of using another data structure.
        size_t rem = block->size - size;
        block->size = rem;
        pool_block_header.total_size -= size;

        if((block->prev != NULL) && (block->prev->size > block->size))
        {
          // If we are now smaller than the previous block, move us forward in
          // the list.
          pool_block_remove(block);
          pool_block_insert(block);
        }

        return (char*)block + rem;
      } else if(block->size == size) {
        // Remove the block from the list.
        pool_block_remove(block);

        // Return the block pointer itself.
        pool_block_header.total_size -= size;
        return block;
      }

      block = block->next;
    }
  }

  // We have no free blocks big enough.
  if(size >= POOL_MMAP)
    return virtual_alloc(size);

  pool_block_t* block = (pool_block_t*)virtual_alloc(POOL_MMAP);
  size_t rem = POOL_MMAP - size;

  block->size = rem;
  block->next = NULL;
  block->prev = NULL;
  pool_block_header.total_size += rem;
  pool_block_insert(block);

  return (char*)block + rem;
}

static void pool_free_pages(void* p, size_t size)
{
  if(pool_block_header.total_size >= POOL_MMAP)
  {
    // TODO: ???
  }

  pool_block_t* block = (pool_block_t*)p;
  block->prev = NULL;
  block->next = NULL;
  block->size = size;

  pool_block_insert(block);
  pool_block_header.total_size += size;
}

static void pool_push(pool_local_t* thread, pool_global_t* global)
{
  pool_cmp_t cmp, xchg;
  pool_central_t* p = (pool_central_t*)thread->pool;
  p->length = thread->length;
  cmp.dw = global->central;

  do
  {
    p->central = cmp.node;
    xchg.node = p;
    xchg.aba = cmp.aba + 1;
  } while(!_atomic_dwcas_weak(&global->central, &cmp.dw, xchg.dw));

  thread->pool = NULL;
  thread->length = 0;
}

static pool_item_t* pool_pull(pool_local_t* thread, pool_global_t* global)
{
  pool_cmp_t cmp, xchg;
  pool_central_t* next;
  cmp.dw = global->central;

  do
  {
    next = cmp.node;

    if(next == NULL)
      return NULL;

    xchg.node = next->central;
    xchg.aba = cmp.aba + 1;
  } while(!_atomic_dwcas_weak(&global->central, &cmp.dw, xchg.dw));

  pool_item_t* p = (pool_item_t*)next;
  thread->pool = p->next;
  thread->length = next->length - 1;

  return p;
}

static void* pool_get(pool_local_t* pool, size_t index)
{
  // Try per-size thread-local free list first.
  pool_local_t* thread = &pool[index];
  pool_global_t* global = &pool_global[index];
  pool_item_t* p = thread->pool;

  if(p != NULL)
  {
    thread->pool = p->next;
    thread->length--;
    return p;
  }

  // Try to get a new free list from the per-size global list of free lists.
  p = pool_pull(thread, global);

  if(p != NULL)
    return p;

  if(global->size < POOL_ALIGN)
  {
    // Check our per-size thread-local free block.
    if(thread->start < thread->end)
    {
      pool_item_t* p = (pool_item_t*)thread->start;
      thread->start += global->size;
      return p;
    }

    // Use the pool allocator to get a block POOL_ALIGN bytes in size
    // and treat it as a free block.
    char* block = (char*)pool_get(pool, POOL_ALIGN_INDEX);
    thread->start = block + global->size;
    thread->end = block + POOL_ALIGN;
    return (pool_item_t*)block;
  }

  // Pull size bytes from the list of free blocks. Don't use a size-specific
  // free block.
  return (pool_item_t*)pool_alloc_pages(global->size);
}

void* pool_alloc(size_t index)
{
#ifdef USE_VALGRIND
  VALGRIND_DISABLE_ERROR_REPORTING;
#endif

  pool_local_t* pool = pool_local;
  void* p = pool_get(pool, index);

#ifdef USE_VALGRIND
  VALGRIND_ENABLE_ERROR_REPORTING;
  VALGRIND_MALLOCLIKE_BLOCK(p, pool_size(index), 0, 0);
#endif

  return p;
}

void pool_free(size_t index, void* p)
{
#ifdef USE_VALGRIND
  VALGRIND_DISABLE_ERROR_REPORTING;
#endif

  pool_local_t* thread = &pool_local[index];
  pool_global_t* global = &pool_global[index];

  if(thread->length >= global->count)
    pool_push(thread, global);

  pool_item_t* lp = (pool_item_t*)p;
  lp->next = thread->pool;
  thread->pool = (pool_item_t*)p;
  thread->length++;

#ifdef USE_VALGRIND
  VALGRIND_ENABLE_ERROR_REPORTING;
  VALGRIND_FREELIKE_BLOCK(p, 0);
#endif
}

void* pool_alloc_size(size_t size)
{
  size_t index = pool_index(size);

  if(index < POOL_COUNT)
    return pool_alloc(index);

#ifdef USE_VALGRIND
  VALGRIND_DISABLE_ERROR_REPORTING;
#endif

  if((size & POOL_ALIGN_MASK) != 0)
    size = (size & ~POOL_ALIGN_MASK) + POOL_ALIGN;

  void* p = pool_alloc_pages(size);

#ifdef USE_VALGRIND
  VALGRIND_ENABLE_ERROR_REPORTING;
  VALGRIND_MALLOCLIKE_BLOCK(p, size, 0, 0);
#endif

  return p;
}

void pool_free_size(size_t size, void* p)
{
  size_t index = pool_index(size);

  if(index < POOL_COUNT)
    return pool_free(index, p);

#ifdef USE_VALGRIND
  VALGRIND_DISABLE_ERROR_REPORTING;
#endif

  if((size & POOL_ALIGN_MASK) != 0)
    size = (size & ~POOL_ALIGN_MASK) + POOL_ALIGN;

  pool_free_pages(p, size);

#ifdef USE_VALGRIND
  VALGRIND_ENABLE_ERROR_REPORTING;
  VALGRIND_FREELIKE_BLOCK(p, 0);
#endif
}

size_t pool_index(size_t size)
{
  if(size <= POOL_MIN)
    return 0;

  if(size > POOL_MAX)
    return POOL_COUNT;

  size = next_pow2(size);
  return __pony_ffsl(size) - (POOL_MIN_BITS + 1);
}

size_t pool_size(size_t index)
{
  return (size_t)1 << (POOL_MIN_BITS + index);
}
