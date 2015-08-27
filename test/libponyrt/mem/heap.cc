#include <platform.h>

#include <mem/heap.h>
#include <mem/pool.h>
#include <mem/pagemap.h>

#include <gtest/gtest.h>

TEST(Heap, Init)
{
  pony_actor_t* actor = (pony_actor_t*)0xDEADBEEF;

  heap_t heap;
  heap_init(&heap);

  void* p = heap_alloc(actor, &heap, 127);
  ASSERT_EQ((size_t)128, heap.used);

  chunk_t* chunk = (chunk_t*)pagemap_get(p);
  ASSERT_EQ(actor, heap_owner(chunk));

  size_t size = heap_size(chunk);
  ASSERT_EQ(size, (size_t)128);

  void* p2 = heap_alloc(actor, &heap, 99);
  ASSERT_NE(p, p2);
  ASSERT_EQ((size_t)256, heap.used);

  heap.next_gc = 0;
  heap_startgc(&heap);
  heap_mark(chunk, p);
  heap_endgc(&heap);
  ASSERT_EQ((size_t)128, heap.used);

  void* p3 = heap_alloc(actor, &heap, 107);
  ASSERT_EQ(p2, p3);

  heap_used(&heap, 1024);
  ASSERT_EQ((size_t)1280, heap.used);

  heap.next_gc = 0;
  heap_startgc(&heap);
  heap_mark_shallow(chunk, p3);
  heap_endgc(&heap);
  ASSERT_EQ((size_t)128, heap.used);

  void* p4 = heap_alloc(actor, &heap, 67);
  ASSERT_EQ(p, p4);

  size_t large_size = (1 << 15) - 7;
  void* p5 = heap_alloc(actor, &heap, large_size);
  chunk_t* chunk5 = (chunk_t*)pagemap_get(p5);
  ASSERT_EQ(actor, heap_owner(chunk5));

  size_t size5 = heap_size(chunk5);
  ASSERT_EQ(large_size, size5);
  ASSERT_EQ(256 + large_size, heap.used);

  heap.next_gc = 0;
  heap_startgc(&heap);
  heap_mark_shallow(chunk5, p5);
  heap_endgc(&heap);
  ASSERT_EQ(large_size, heap.used);

  heap_destroy(&heap);

  ASSERT_TRUE(pool_debug_appears_freed());
}
