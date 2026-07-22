#include <platform.h>

#include <mem/alloc.h>

#include <gtest/gtest.h>

#include <string.h>

TEST(Alloc, ReserveAlignedIsAlignedAndDisjoint)
{
  // The contract is the code under test: a reservation's address must be a
  // multiple of its size, and repeated reservations must hand out
  // non-overlapping ranges.
  static const size_t sizes[] =
    {64 * 1024, 1024 * 1024, 4 * 1024 * 1024};
  static const int reps = 4;

  void* got[sizeof(sizes) / sizeof(sizes[0]) * reps];
  size_t got_size[sizeof(sizes) / sizeof(sizes[0]) * reps];
  int n = 0;

  for(size_t si = 0; si < (sizeof(sizes) / sizeof(sizes[0])); si++)
  {
    for(int r = 0; r < reps; r++)
    {
      void* p = ponyint_virt_reserve_aligned(sizes[si]);
      ASSERT_NE(p, (void*)NULL);
      ASSERT_EQ(((uintptr_t)p & (sizes[si] - 1)), (uintptr_t)0);

      for(int i = 0; i < n; i++)
      {
        bool disjoint = (((char*)p + sizes[si]) <= (char*)got[i]) ||
          (((char*)got[i] + got_size[i]) <= (char*)p);
        ASSERT_TRUE(disjoint);
      }

      got[n] = p;
      got_size[n] = sizes[si];
      n++;
    }
  }

  for(int i = 0; i < n; i++)
    ponyint_virt_free(got[i], got_size[i]);
}

TEST(Alloc, ReserveAlignedWholeRangeUsable)
{
  // A committed reservation must be writable end to end. Write and read back
  // at the edges and in the middle of the whole span: a reservation that
  // handed back less than it promised faults on one of the three.
  size_t size = 1024 * 1024;
  char* p = (char*)ponyint_virt_reserve_aligned(size);
  ASSERT_NE(p, (char*)NULL);

  ponyint_virt_commit(p, size);

  p[0] = 'a';
  p[size / 2] = 'b';
  p[size - 1] = 'c';

  ASSERT_EQ(p[0], 'a');
  ASSERT_EQ(p[size / 2], 'b');
  ASSERT_EQ(p[size - 1], 'c');

  ponyint_virt_free(p, size);
}

TEST(Alloc, ReserveAlignedDecommitThenRewrite)
{
  // A decommitted range must stay reserved, so committing it again makes it
  // writable: the arena allocator decommits an emptied slab's pages and later
  // reuses the same units. Content across a decommit is not asserted -- the
  // range is given back to the OS.
  size_t size = 1024 * 1024;
  char* p = (char*)ponyint_virt_reserve_aligned(size);
  ASSERT_NE(p, (char*)NULL);

  ponyint_virt_commit(p, size);
  memset(p, 0x5a, size);

  ponyint_virt_decommit(p, size);

  ponyint_virt_commit(p, size);
  p[0] = 'x';
  p[size - 1] = 'y';
  ASSERT_EQ(p[0], 'x');
  ASSERT_EQ(p[size - 1], 'y');

  ponyint_virt_free(p, size);
}

#ifdef PLATFORM_IS_WINDOWS

TEST(Alloc, DecommitReturnsThePages)
{
  // The observation is Windows-only because it is only true there: on POSIX
  // decommit and discard are one madvise call, and neither returns the
  // address space. Without this, a decommit that quietly returned nothing --
  // MEM_RESET, or no call at all -- passes every other test in the suite
  // while the allocator holds each page it ever touched for the life of the
  // process.
  size_t size = 1024 * 1024;
  char* p = (char*)ponyint_virt_reserve_aligned(size);
  ASSERT_NE(p, (char*)NULL);

  MEMORY_BASIC_INFORMATION mbi;

  ASSERT_NE(VirtualQuery(p, &mbi, sizeof(mbi)), (SIZE_T)0);
  ASSERT_EQ(mbi.State, (DWORD)MEM_RESERVE);

  ponyint_virt_commit(p, size);
  ASSERT_NE(VirtualQuery(p, &mbi, sizeof(mbi)), (SIZE_T)0);
  ASSERT_EQ(mbi.State, (DWORD)MEM_COMMIT);

  ponyint_virt_decommit(p, size);
  ASSERT_NE(VirtualQuery(p, &mbi, sizeof(mbi)), (SIZE_T)0);
  ASSERT_EQ(mbi.State, (DWORD)MEM_RESERVE);

  ponyint_virt_free(p, size);
}

#endif

TEST(Alloc, DiscardLeavesRangeWritable)
{
  // Unlike a decommit, a discarded range takes no commit to use again: the
  // classic pool discards a freed block's pages and hands the block straight
  // back out. The memory here comes from ponyint_virt_alloc, which is where
  // that pool's blocks come from. Content across a discard is not asserted --
  // the OS may drop it.
  size_t size = 1024 * 1024;
  char* p = (char*)ponyint_virt_alloc(size);
  ASSERT_NE(p, (char*)NULL);

  memset(p, 0x5a, size);

  ponyint_virt_discard(p, size);

  p[0] = 'x';
  p[size / 2] = 'y';
  p[size - 1] = 'z';
  ASSERT_EQ(p[0], 'x');
  ASSERT_EQ(p[size / 2], 'y');
  ASSERT_EQ(p[size - 1], 'z');

  ponyint_virt_free(p, size);
}
