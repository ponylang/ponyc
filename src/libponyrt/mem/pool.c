#define PONY_WANT_ATOMIC_DEFS

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
#include <pony/detail/atomics.h>

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
#define POOL_ALIGN_MASK (POOL_ALIGN - 1)

/// When we mmap, pull at least this many bytes.
#ifdef PLATFORM_IS_ILP32
#define POOL_MMAP (16 * 1024 * 1024) // 16 MB
#else
#define POOL_MMAP (128 * 1024 * 1024) // 128 MB
#endif

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

/// A per-size global list of free lists header.
typedef struct pool_global_t
{
  size_t size;
  size_t count;
  PONY_ATOMIC(pool_central_t*) central;
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
  {POOL_MIN << 0, POOL_MAX / (POOL_MIN << 0), NULL},
  {POOL_MIN << 1, POOL_MAX / (POOL_MIN << 1), NULL},
  {POOL_MIN << 2, POOL_MAX / (POOL_MIN << 2), NULL},
  {POOL_MIN << 3, POOL_MAX / (POOL_MIN << 3), NULL},
  {POOL_MIN << 4, POOL_MAX / (POOL_MIN << 4), NULL},
  {POOL_MIN << 5, POOL_MAX / (POOL_MIN << 5), NULL},
  {POOL_MIN << 6, POOL_MAX / (POOL_MIN << 6), NULL},
  {POOL_MIN << 7, POOL_MAX / (POOL_MIN << 7), NULL},
  {POOL_MIN << 8, POOL_MAX / (POOL_MIN << 8), NULL},
  {POOL_MIN << 9, POOL_MAX / (POOL_MIN << 9), NULL},
  {POOL_MIN << 10, POOL_MAX / (POOL_MIN << 10), NULL},
  {POOL_MIN << 11, POOL_MAX / (POOL_MIN << 11), NULL},
  {POOL_MIN << 12, POOL_MAX / (POOL_MIN << 12), NULL},
  {POOL_MIN << 13, POOL_MAX / (POOL_MIN << 13), NULL},
  {POOL_MIN << 14, POOL_MAX / (POOL_MIN << 14), NULL},
  {POOL_MIN << 15, POOL_MAX / (POOL_MIN << 15), NULL},
};

static __pony_thread_local pool_local_t pool_local[POOL_COUNT];
static __pony_thread_local pool_block_header_t pool_block_header;

#ifdef USE_POOLTRACK
#include "../ds/stack.h"
#include "../sched/cpu.h"

#define POOL_TRACK_FREE ((void*)0)
#define POOL_TRACK_ALLOC ((void*)1)
#define POOL_TRACK_PUSH ((void*)2)
#define POOL_TRACK_PULL ((void*)3)
#define POOL_TRACK_PUSH_LIST ((void*)4)
#define POOL_TRACK_PULL_LIST ((void*)5)
#define POOL_TRACK_MAX_THREADS 64

DECLARE_STACK(pool_track, pool_track_t, void);
DEFINE_STACK(pool_track, pool_track_t, void);

typedef struct
{
  bool init;
  bool internal;
  int thread_id;
  pool_track_t* stack;
} pool_track_info_t;

static __pony_thread_local pool_track_info_t track;
static PONY_ATOMIC(int) track_global_thread_id;
static pool_track_info_t* track_global_info[POOL_TRACK_MAX_THREADS];

static void pool_event_print(int thread, void* op, size_t event, size_t tsc,
  void* addr, size_t size)
{
  if(op == POOL_TRACK_ALLOC)
    printf("%d ALLOC "__zu" ("__zu"): %p, "__zu"\n",
      thread, event, tsc, addr, size);
  else if(op == POOL_TRACK_FREE)
    printf("%d FREE "__zu" ("__zu"): %p, "__zu"\n",
      thread, event, tsc, addr, size);
  else if(op == POOL_TRACK_PUSH)
    printf("%d PUSH "__zu" ("__zu"): %p, "__zu"\n",
      thread, event, tsc, addr, size);
  else if(op == POOL_TRACK_PULL)
    printf("%d PULL "__zu" ("__zu"): %p, "__zu"\n",
      thread, event, tsc, addr, size);
  else if(op == POOL_TRACK_PUSH_LIST)
    printf("%d PUSH LIST "__zu" ("__zu"): "__zu", "__zu"\n",
      thread, event, tsc, (size_t)addr, size);
  else if(op == POOL_TRACK_PULL_LIST)
    printf("%d PULL LIST "__zu" ("__zu"): "__zu", "__zu"\n",
      thread, event, tsc, (size_t)addr, size);
}

static void pool_track(int thread_filter, void* addr_filter, int op_filter,
  size_t event_filter)
{
  for(int i = 0; i < POOL_TRACK_MAX_THREADS; i++)
  {
    if((thread_filter != -1) && (thread_filter != i))
      continue;

    pool_track_info_t* track = track_global_info[i];

    if(track == NULL)
      continue;

    Stack* t = (Stack*)track->stack;
    size_t event = 0;

    int state = 0;
    void* op;
    void* addr;
    size_t size;
    size_t tsc;

    while(t != NULL)
    {
      for(int j = t->index - 1; j >= 0; j--)
      {
        switch(state)
        {
          case 0:
            tsc = (size_t)t->data[j];
            state = 1;
            break;

          case 1:
            size = (size_t)t->data[j];
            state = 2;
            break;

          case 2:
            addr = t->data[j];
            state = 3;
            break;

          case 3:
          {
            bool print = true;
            op = t->data[j];
            state = 0;

            if((op_filter != -1) && (op_filter != (int)op))
              print = false;

            if((event_filter != (size_t)-1) && (event_filter != event))
              print = false;

            if((addr_filter != NULL) &&
              ((addr > addr_filter) || ((addr + size) <= addr_filter)))
            {
              print = false;
            }

            if(print)
            {
              pool_event_print(i, op, event, tsc, addr, size);

              if(event_filter != (size_t)-1)
                return;
            }

            event++;
            break;
          }

          default: {}
        }
      }

      t = t->prev;
    }
  }
}

static void track_init()
{
  if(track.init)
    return;

  track.init = true;
  track.thread_id = atomic_fetch_add(&track_global_thread_id, 1,
    memory_order_relaxed);
  track_global_info[track.thread_id] = &track;

  // Force the symbol to be linked.
  pool_track(track.thread_id, NULL, -1, 0);
}

static void track_alloc(void* p, size_t size)
{
  track_init();

  if(track.internal)
    return;

  track.internal = true;

  track.stack = pool_track_push(track.stack, POOL_TRACK_ALLOC);
  track.stack = pool_track_push(track.stack, p);
  track.stack = pool_track_push(track.stack, (void*)size);
  track.stack = pool_track_push(track.stack, (void*)ponyint_cpu_tick());

  track.internal = false;
}

static void track_free(void* p, size_t size)
{
  track_init();
  assert(!track.internal);

  track.internal = true;

  track.stack = pool_track_push(track.stack, POOL_TRACK_FREE);
  track.stack = pool_track_push(track.stack, p);
  track.stack = pool_track_push(track.stack, (void*)size);
  track.stack = pool_track_push(track.stack, (void*)ponyint_cpu_tick());

  track.internal = false;
}

static void track_push(pool_item_t* p, size_t length, size_t size)
{
  track_init();
  assert(!track.internal);

  track.internal = true;
  uint64_t tsc = ponyint_cpu_tick();

  track.stack = pool_track_push(track.stack, POOL_TRACK_PUSH_LIST);
  track.stack = pool_track_push(track.stack, (void*)length);
  track.stack = pool_track_push(track.stack, (void*)size);
  track.stack = pool_track_push(track.stack, (void*)tsc);

  while(p != NULL)
  {
    track.stack = pool_track_push(track.stack, POOL_TRACK_PUSH);
    track.stack = pool_track_push(track.stack, p);
    track.stack = pool_track_push(track.stack, (void*)size);
    track.stack = pool_track_push(track.stack, (void*)tsc);
    p = p->next;
  }

  track.internal = false;
}

static void track_pull(pool_item_t* p, size_t length, size_t size)
{
  track_init();
  assert(!track.internal);

  track.internal = true;
  uint64_t tsc = ponyint_cpu_tick();

  track.stack = pool_track_push(track.stack, POOL_TRACK_PULL_LIST);
  track.stack = pool_track_push(track.stack, (void*)length);
  track.stack = pool_track_push(track.stack, (void*)size);
  track.stack = pool_track_push(track.stack, (void*)tsc);

  while(p != NULL)
  {
    track.stack = pool_track_push(track.stack, POOL_TRACK_PULL);
    track.stack = pool_track_push(track.stack, p);
    track.stack = pool_track_push(track.stack, (void*)size);
    track.stack = pool_track_push(track.stack, (void*)tsc);
    p = p->next;
  }

  track.internal = false;
}

#define TRACK_ALLOC(PTR, SIZE) track_alloc(PTR, SIZE)
#define TRACK_FREE(PTR, SIZE) track_free(PTR, SIZE)
#define TRACK_PUSH(PTR, LEN, SIZE) track_push(PTR, LEN, SIZE)
#define TRACK_PULL(PTR, LEN, SIZE) track_pull(PTR, LEN, SIZE)
#define TRACK_EXTERNAL() (!track.internal)

#else

#define TRACK_ALLOC(PTR, SIZE)
#define TRACK_FREE(PTR, SIZE)
#define TRACK_PUSH(PTR, LEN, SIZE)
#define TRACK_PULL(PTR, LEN, SIZE)
#define TRACK_EXTERNAL() (true)

#endif

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

        // TODO: can track largest size
        // if we're the last element, it's either our new size or the previous
        // block size, if that's larger.

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
    return ponyint_virt_alloc(size);

  pool_block_t* block = (pool_block_t*)ponyint_virt_alloc(POOL_MMAP);
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
  pool_central_t* cmp;
  pool_central_t* xchg;
  pool_central_t* p = (pool_central_t*)thread->pool;
  p->length = thread->length;

  thread->pool = NULL;
  thread->length = 0;

  assert(p->length == global->count);
  TRACK_PUSH((pool_item_t*)p, p->length, global->size);

  cmp = atomic_load_explicit(&global->central, memory_order_acquire);

  uintptr_t mask = UINTPTR_MAX ^ ((1 << (POOL_MIN_BITS - 1)) - 1);

  do
  {
    // We know the alignment boundary of the objects in the stack so we use the
    // low bits for ABA protection.
    uintptr_t aba = (uintptr_t)cmp & ~mask;
    p->central = (pool_central_t*)((uintptr_t)cmp & mask);

    xchg = (pool_central_t*)((uintptr_t)p | ((aba + 1) & ~mask));
  } while(!atomic_compare_exchange_weak_explicit(&global->central, &cmp, xchg,
    memory_order_release, memory_order_relaxed));
}

static pool_item_t* pool_pull(pool_local_t* thread, pool_global_t* global)
{
  pool_central_t* cmp;
  pool_central_t* xchg;
  pool_central_t* next;

  cmp = atomic_load_explicit(&global->central, memory_order_acquire);

  uintptr_t mask = UINTPTR_MAX ^ ((1 << (POOL_MIN_BITS - 1)) - 1);

  do
  {
    // We know the alignment boundary of the objects in the stack so we use the
    // low bits for ABA protection.
    uintptr_t aba = (uintptr_t)cmp & ~mask;
    next = (pool_central_t*)((uintptr_t)cmp & mask);

    if(next == NULL)
      return NULL;

    xchg = (pool_central_t*)((uintptr_t)next->central | ((aba + 1) & ~mask));
  } while(!atomic_compare_exchange_weak_explicit(&global->central, &cmp, xchg,
    memory_order_acq_rel, memory_order_relaxed));

  pool_item_t* p = (pool_item_t*)next;

  assert(next->length == global->count);
  TRACK_PULL(p, next->length, global->size);

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

  if(TRACK_EXTERNAL())
  {
    // Try to get a new free list from the per-size global list of free lists.
    p = pool_pull(thread, global);

    if(p != NULL)
      return p;
  }

  if(global->size < POOL_ALIGN)
  {
    // Check our per-size thread-local free block.
    if(thread->start < thread->end)
    {
      void* p = thread->start;
      thread->start += global->size;
      return p;
    }

    // Use the pool allocator to get a block POOL_ALIGN bytes in size
    // and treat it as a free block.
    char* block = (char*)pool_get(pool, POOL_ALIGN_INDEX);
    thread->start = block + global->size;
    thread->end = block + POOL_ALIGN;
    return block;
  }

  // Pull size bytes from the list of free blocks. Don't use a size-specific
  // free block.
  return pool_alloc_pages(global->size);
}

void* ponyint_pool_alloc(size_t index)
{
#ifdef USE_VALGRIND
  VALGRIND_DISABLE_ERROR_REPORTING;
#endif

  pool_local_t* pool = pool_local;
  void* p = pool_get(pool, index);

  TRACK_ALLOC(p, POOL_MIN << index);

#ifdef USE_VALGRIND
  VALGRIND_ENABLE_ERROR_REPORTING;
  VALGRIND_MALLOCLIKE_BLOCK(p, ponyint_pool_size(index), 0, 0);
#endif

  return p;
}

void ponyint_pool_free(size_t index, void* p)
{
#ifdef USE_VALGRIND
  VALGRIND_DISABLE_ERROR_REPORTING;
#endif

  TRACK_FREE(p, POOL_MIN << index);
  assert(index < POOL_COUNT);

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

void* ponyint_pool_alloc_size(size_t size)
{
  size_t index = ponyint_pool_index(size);

  if(index < POOL_COUNT)
    return ponyint_pool_alloc(index);

#ifdef USE_VALGRIND
  VALGRIND_DISABLE_ERROR_REPORTING;
#endif

  size = ponyint_pool_adjust_size(size);
  void* p = pool_alloc_pages(size);

  TRACK_ALLOC(p, size);

#ifdef USE_VALGRIND
  VALGRIND_ENABLE_ERROR_REPORTING;
  VALGRIND_MALLOCLIKE_BLOCK(p, size, 0, 0);
#endif

  return p;
}

void ponyint_pool_free_size(size_t size, void* p)
{
  size_t index = ponyint_pool_index(size);

  if(index < POOL_COUNT)
    return ponyint_pool_free(index, p);

#ifdef USE_VALGRIND
  VALGRIND_DISABLE_ERROR_REPORTING;
#endif

  size = ponyint_pool_adjust_size(size);
  pool_free_pages(p, size);

  TRACK_FREE(p, size);

#ifdef USE_VALGRIND
  VALGRIND_ENABLE_ERROR_REPORTING;
  VALGRIND_FREELIKE_BLOCK(p, 0);
#endif
}

size_t ponyint_pool_index(size_t size)
{
  if(size <= POOL_MIN)
    return 0;

  if(size > POOL_MAX)
    return POOL_COUNT;

  size = ponyint_next_pow2(size);
  return __pony_ffsl(size) - (POOL_MIN_BITS + 1);
}

size_t ponyint_pool_size(size_t index)
{
  return (size_t)1 << (POOL_MIN_BITS + index);
}

size_t ponyint_pool_adjust_size(size_t size)
{
  if((size & POOL_ALIGN_MASK) != 0)
    size = (size & ~POOL_ALIGN_MASK) + POOL_ALIGN;

  return size;
}
