#include <platform.h>

#include <mem/heap.h>
#include <mem/pool.h>
#include <mem/pagemap.h>
#include <actor/actor.h>

#include <gtest/gtest.h>

#define CHUNK_T_SIZE 32
#define CHUNK_T_ALLOC 32
#define SMALL_CHUNK_BLOCK_SIZE 1024

TEST(Heap, Init)
{
#ifdef USE_RUNTIMESTATS
  pony_actor_t* actor = (pony_actor_t*)ponyint_pool_alloc_size(1024);
  memset(actor, 0, 1024);
#else
  pony_actor_t* actor = (pony_actor_t*)0xDEADBEEFDEADBEEF;
#endif

  heap_t heap;
  ponyint_heap_init(&heap);

  void* p = ponyint_heap_alloc(actor, &heap, 127, TRACK_NO_FINALISERS);
  ASSERT_EQ((size_t)128, heap.used);

#ifdef USE_RUNTIMESTATS
  ASSERT_EQ((size_t)1, actor->actorstats.heap_alloc_counter);
  ASSERT_EQ((size_t)1, actor->actorstats.heap_num_allocated);
  ASSERT_EQ((size_t)0, actor->actorstats.heap_free_counter);
  ASSERT_EQ((size_t)(128 + CHUNK_T_SIZE), actor->actorstats.heap_mem_used);
  ASSERT_EQ((size_t)(CHUNK_T_ALLOC + SMALL_CHUNK_BLOCK_SIZE), actor->actorstats.heap_mem_allocated);
#endif

  pony_actor_t* pagemap_actor = NULL;
  chunk_t* chunk = (chunk_t*)ponyint_pagemap_get(p, &pagemap_actor);
  ASSERT_EQ(actor, pagemap_actor);

  size_t size = ponyint_heap_size(chunk);
  ASSERT_EQ(size, (size_t)128);

  void* p2 = ponyint_heap_alloc(actor, &heap, 99, TRACK_NO_FINALISERS);
  ASSERT_NE(p, p2);
  ASSERT_EQ((size_t)256, heap.used);

#ifdef USE_RUNTIMESTATS
  ASSERT_EQ((size_t)2, actor->actorstats.heap_alloc_counter);
  ASSERT_EQ((size_t)2, actor->actorstats.heap_num_allocated);
  ASSERT_EQ((size_t)0, actor->actorstats.heap_free_counter);
  ASSERT_EQ((size_t)(256 + CHUNK_T_SIZE), actor->actorstats.heap_mem_used);
  ASSERT_EQ((size_t)(CHUNK_T_ALLOC + SMALL_CHUNK_BLOCK_SIZE), actor->actorstats.heap_mem_allocated);
#endif

  heap.next_gc = 0;
  ponyint_heap_startgc(&heap
#ifdef USE_RUNTIMESTATS
  , actor);
#else
  );
#endif

  ponyint_heap_mark(chunk, p);

  ponyint_heap_endgc(&heap, 0
#ifdef USE_RUNTIMESTATS
  , actor);
#else
  );
#endif

  ASSERT_EQ((size_t)128, heap.used);

#ifdef USE_RUNTIMESTATS
  ASSERT_EQ((size_t)2, actor->actorstats.heap_alloc_counter);
  ASSERT_EQ((size_t)1, actor->actorstats.heap_num_allocated);
  ASSERT_EQ((size_t)1, actor->actorstats.heap_free_counter);
  ASSERT_EQ((size_t)(128 + CHUNK_T_SIZE), actor->actorstats.heap_mem_used);
  ASSERT_EQ((size_t)(CHUNK_T_ALLOC + SMALL_CHUNK_BLOCK_SIZE), actor->actorstats.heap_mem_allocated);
#endif

  void* p3 = ponyint_heap_alloc(actor, &heap, 107, TRACK_NO_FINALISERS);
  ASSERT_EQ(p2, p3);

#ifdef USE_RUNTIMESTATS
  ASSERT_EQ((size_t)3, actor->actorstats.heap_alloc_counter);
  ASSERT_EQ((size_t)2, actor->actorstats.heap_num_allocated);
  ASSERT_EQ((size_t)1, actor->actorstats.heap_free_counter);
  ASSERT_EQ((size_t)(256 + CHUNK_T_SIZE), actor->actorstats.heap_mem_used);
  ASSERT_EQ((size_t)(CHUNK_T_ALLOC + SMALL_CHUNK_BLOCK_SIZE), actor->actorstats.heap_mem_allocated);
#endif

  ponyint_heap_used(&heap, 1024);
  ASSERT_EQ((size_t)1280, heap.used);

#ifdef USE_RUNTIMESTATS
  ASSERT_EQ((size_t)3, actor->actorstats.heap_alloc_counter);
  ASSERT_EQ((size_t)2, actor->actorstats.heap_num_allocated);
  ASSERT_EQ((size_t)1, actor->actorstats.heap_free_counter);
  ASSERT_EQ((size_t)(256 + CHUNK_T_SIZE), actor->actorstats.heap_mem_used);
  ASSERT_EQ((size_t)(CHUNK_T_ALLOC + SMALL_CHUNK_BLOCK_SIZE), actor->actorstats.heap_mem_allocated);
#endif

  heap.next_gc = 0;
  ponyint_heap_startgc(&heap
#ifdef USE_RUNTIMESTATS
  , actor);
#else
  );
#endif

  ponyint_heap_mark_shallow(chunk, p3);
  ponyint_heap_endgc(&heap, 0
#ifdef USE_RUNTIMESTATS
  , actor);
#else
  );
#endif

  ASSERT_EQ((size_t)128, heap.used);

#ifdef USE_RUNTIMESTATS
  ASSERT_EQ((size_t)3, actor->actorstats.heap_alloc_counter);
  ASSERT_EQ((size_t)1, actor->actorstats.heap_num_allocated);
  ASSERT_EQ((size_t)2, actor->actorstats.heap_free_counter);
  ASSERT_EQ((size_t)(128 + CHUNK_T_SIZE), actor->actorstats.heap_mem_used);
  ASSERT_EQ((size_t)(CHUNK_T_ALLOC + SMALL_CHUNK_BLOCK_SIZE), actor->actorstats.heap_mem_allocated);
#endif

  void* p4 = ponyint_heap_alloc(actor, &heap, 67, TRACK_NO_FINALISERS);
  ASSERT_EQ(p, p4);

#ifdef USE_RUNTIMESTATS
  ASSERT_EQ((size_t)4, actor->actorstats.heap_alloc_counter);
  ASSERT_EQ((size_t)2, actor->actorstats.heap_num_allocated);
  ASSERT_EQ((size_t)2, actor->actorstats.heap_free_counter);
  ASSERT_EQ((size_t)(256 + CHUNK_T_SIZE), actor->actorstats.heap_mem_used);
  ASSERT_EQ((size_t)(CHUNK_T_ALLOC + SMALL_CHUNK_BLOCK_SIZE), actor->actorstats.heap_mem_allocated);
#endif

  size_t large_size = (1 << 22) - 7;
  void* p5 = ponyint_heap_alloc(actor, &heap, large_size, TRACK_NO_FINALISERS);
  chunk_t* chunk5 = (chunk_t*)ponyint_pagemap_get(p5, &pagemap_actor);
  ASSERT_EQ(actor, pagemap_actor);

#ifdef USE_RUNTIMESTATS
  ASSERT_EQ((size_t)5, actor->actorstats.heap_alloc_counter);
  ASSERT_EQ((size_t)3, actor->actorstats.heap_num_allocated);
  ASSERT_EQ((size_t)2, actor->actorstats.heap_free_counter);
  ASSERT_EQ((size_t)(256 + CHUNK_T_SIZE + 4194304 + CHUNK_T_SIZE), actor->actorstats.heap_mem_used);
  ASSERT_EQ((size_t)(CHUNK_T_ALLOC + SMALL_CHUNK_BLOCK_SIZE + 4194304 + CHUNK_T_ALLOC), actor->actorstats.heap_mem_allocated);
#endif

  size_t adjust_size = ponyint_pool_adjust_size(large_size);
  ASSERT_NE(chunk5, (chunk_t*)NULL);

  char* p5_end = (char*)p5 + adjust_size;
  char* p5_curr = (char*)p5;
  chunk_t* p5_chunk = NULL;

  while(p5_curr < p5_end)
  {
    p5_chunk = (chunk_t*)ponyint_pagemap_get(p5_curr, &pagemap_actor);
    p5_curr += POOL_ALIGN;
    ASSERT_EQ(chunk5, p5_chunk);
    ASSERT_EQ(actor, pagemap_actor);
  }
  p5_chunk = (chunk_t*)ponyint_pagemap_get(p5_end, &pagemap_actor);
  ASSERT_NE(chunk5, p5_chunk);
  ASSERT_NE(actor, pagemap_actor);

  size_t size5 = ponyint_heap_size(chunk5);
  ASSERT_EQ(adjust_size, size5);
  ASSERT_EQ(256 + adjust_size, heap.used);

  heap.next_gc = 0;
  ponyint_heap_startgc(&heap
#ifdef USE_RUNTIMESTATS
  , actor);
#else
  );
#endif

  ponyint_heap_mark_shallow(chunk5, p5);
  ponyint_heap_endgc(&heap, 0
#ifdef USE_RUNTIMESTATS
  , actor);
#else
  );
#endif

  ASSERT_EQ(adjust_size, heap.used);

#ifdef USE_RUNTIMESTATS
  ASSERT_EQ((size_t)5, actor->actorstats.heap_alloc_counter);
  ASSERT_EQ((size_t)1, actor->actorstats.heap_num_allocated);
  ASSERT_EQ((size_t)4, actor->actorstats.heap_free_counter);
  ASSERT_EQ((size_t)(4194304 + CHUNK_T_SIZE), actor->actorstats.heap_mem_used);
  ASSERT_EQ((size_t)(4194304 + CHUNK_T_ALLOC), actor->actorstats.heap_mem_allocated);
#endif

  ponyint_heap_destroy(&heap);

#ifdef USE_RUNTIMESTATS
  ponyint_pool_free_size(1024, actor);
#endif
}
