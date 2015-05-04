#include "pool.h"
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

#define POOL_MAX_BITS 21
#define POOL_MAX (1 << POOL_MAX_BITS)
#define POOL_MIN (1 << POOL_MIN_BITS)
#define POOL_COUNT (POOL_MAX_BITS - POOL_MIN_BITS)
#define POOL_THRESHOLD (POOL_MAX >> 1)
#define POOL_MMAP (POOL_MAX << 2)

typedef struct pool_item_t
{
  struct pool_item_t* next;
} pool_item_t;

typedef struct pool_local_t
{
  pool_item_t* pool;
  size_t length;
  char* start;
  char* end;
} pool_local_t;

typedef struct pool_global_t
{
  size_t size;
  size_t count;
  uintptr2_t central;
} pool_global_t;

typedef struct pool_central_t
{
  pool_item_t* next;
  uintptr_t length;
  struct pool_central_t* central;
} pool_central_t;

typedef struct pool_cmp_t
{
  union
  {
    struct
    {
      uintptr_t aba;
      pool_central_t* node;
    };

    uintptr2_t dw;
  };
} pool_cmp_t;

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
};

static __pony_thread_local pool_local_t pool_local[POOL_COUNT];

static pool_item_t* pool_block(pool_local_t* thread, pool_global_t* global)
{
  if(thread->start < thread->end)
  {
    pool_item_t* p = (pool_item_t*)thread->start;
    thread->start += global->size;
    return p;
  }

  return NULL;
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
  } while(!_atomic_dwcas(&global->central, &cmp.dw, xchg.dw,
    __ATOMIC_RELAXED, __ATOMIC_RELAXED));

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
  } while(!_atomic_dwcas(&global->central, &cmp.dw, xchg.dw,
      __ATOMIC_RELAXED, __ATOMIC_RELAXED));

  pool_item_t* p = (pool_item_t*)next;
  thread->pool = p->next;
  thread->length = next->length - 1;

  return p;
}

static pool_item_t* pool_pages(pool_local_t* thread, pool_global_t* global)
{
  char* p = (char*)virtual_alloc(POOL_MMAP);
  thread->start = p + global->size;
  thread->end = p + POOL_MMAP;
  return (pool_item_t*)p;
}

static void* pool_get(size_t index)
{
#ifdef USE_VALGRIND
  VALGRIND_DISABLE_ERROR_REPORTING;
#endif

  pool_local_t* thread = &pool_local[index];
  pool_global_t* global = &pool_global[index];
  pool_item_t* p = thread->pool;

  if(p != NULL)
  {
    thread->pool = p->next;
    thread->length--;
  } else {
    p = pool_block(thread, global);

    if(p == NULL)
    {
      p = pool_pull(thread, global);

      if(p == NULL)
        p = pool_pages(thread, global);
    }
  }

#ifdef USE_VALGRIND
  VALGRIND_ENABLE_ERROR_REPORTING;
#endif

  return p;
}

void* pool_alloc(size_t index)
{
  void* p = pool_get(index);

#ifdef USE_VALGRIND
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
  void* p;

  if(index < POOL_COUNT)
    return pool_alloc(index);
  //   p = pool_get(index);
  // else
    p = virtual_alloc(size);

#ifdef USE_VALGRIND
  VALGRIND_MALLOCLIKE_BLOCK(p, size, 0, 0);
#endif

  return p;
}

void pool_free_size(size_t size, void* p)
{
  size_t index = pool_index(size);

  if(index < POOL_COUNT)
    return pool_free(index, p);

  virtual_free(p, size);

#ifdef USE_VALGRIND
  VALGRIND_FREELIKE_BLOCK(p, 0);
#endif
}

size_t pool_index(size_t size)
{
  if(size <= POOL_MIN)
    return 0;

  size = next_pow2(size);
  return __pony_ffsl(size) - (POOL_MIN_BITS + 1);
}

size_t pool_size(size_t index)
{
  return (size_t)1 << (POOL_MIN_BITS + index);
}

bool pool_debug_appears_freed()
{
  for(int i = 0; i < POOL_COUNT; i++)
  {
    pool_local_t* thread = &pool_local[i];
    pool_global_t* global = &pool_global[i];

    size_t avail = (thread->length * global->size) +
      (thread->end - thread->start);

    if((avail != 0) && (avail != POOL_MMAP))
      return false;

    pool_cmp_t cmp;
    cmp.dw = global->central;
    pool_central_t* next = cmp.node;

    while(next != NULL)
    {
      if(next->length != global->count)
        return false;

      next = next->central;
    }
  }

  return true;
}

size_t pool_leak()
{
  size_t leak = 0;

  for(int i = 0; i < POOL_COUNT; i++)
  {
    pool_local_t* thread = &pool_local[i];
    pool_global_t* global = &pool_global[i];

    size_t avail = (thread->length * global->size) +
      (thread->end - thread->start);

    if((avail != 0) && (avail != POOL_MMAP))
    {
      size_t amount = POOL_MMAP - avail;
      leak += amount;

      printf("POOL LEAK, SIZE " __zu " COUNT " __zu "\n",
        global->size, amount / global->size);
    }

    pool_cmp_t cmp;
    cmp.dw = global->central;
    pool_central_t* next = cmp.node;

    while(next != NULL)
    {
      if(next->length != global->count)
      {
        printf("POOL CENTRAL " __zu " of " __zu "\n", next->length, global->count);
      }

      next = next->central;
    }
  }

  if(leak > 0)
    printf("POOL TOTAL LEAK " __zu "\n", leak);

  return leak;
}
