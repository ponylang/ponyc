#include <platform.h>

#include <mem/alloc.h>
#include <mem/pool.h>
#include <sched/scheduler.h>

#include <gtest/gtest.h>

#include <stdint.h>
#include <string.h>

#ifdef PLATFORM_IS_LINUX
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#include <atomic>
#include <map>
#include <set>
#include <thread>
#include <vector>

/* These tests assert the pool interface's contract — a returned pointer is
 * non-null, aligned, overlaps no live allocation, keeps its content until
 * freed, and freed memory is reused — rather than any backend's exact
 * addresses, so they run under the classic, memalign, and arena backends.
 * Backend-specific behavior sits under that backend's ifdef. AddressSanitizer
 * cannot track the arena backend's allocations — it instruments the arena's
 * one mmap, not the objects carved from it — so the LiveTracker tests below
 * are that backend's use-after-free and overlap protection.
 */

namespace
{

uint64_t splitmix64(uint64_t* state)
{
  uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

uint8_t pattern_byte(uint64_t seed, size_t offset)
{
  // Derived from the offset alone (plus the seed) so a moved allocation
  // (realloc) can be verified at its new address.
  uint64_t s = seed + offset;
  return (uint8_t)splitmix64(&s);
}

/// An independent record of live allocations: what the design discussion
/// calls "its own record of which memory is live, updated as it allocates
/// and frees, separate from the allocator's own bookkeeping."
class LiveTracker
{
public:
  // Patterns are written in full up to this many bytes; above it, one
  // 64-byte stamp per 16 KiB (the arena's unit granularity — every
  // whole-unit decommit or unmap hits at least one stamp) plus the last
  // 64 bytes.
  static const size_t full_fill_limit = 64 * 1024;
  static const size_t stamp_stride = 16 * 1024;
  static const size_t stamp_size = 64;

  struct Rec
  {
    size_t extent;
    uint64_t seed;
  };

  std::map<uintptr_t, Rec> live;
  uint64_t next_seed;

  LiveTracker() : next_seed(0x5eed) {}

  size_t count() const
  {
    return live.size();
  }

  // Non-overlap, alignment, and non-null checks for a fresh allocation,
  // then pattern fill and record. extent is what the interface says the
  // allocation consumes: POOL_SIZE(index) or ponyint_pool_used_size(size).
  void on_alloc(void* p, size_t extent)
  {
    ASSERT_NE(p, (void*)NULL);

    uintptr_t addr = (uintptr_t)p;
    ASSERT_EQ(addr % POOL_MIN, (uintptr_t)0);

    if(extent >= POOL_ALIGN)
      ASSERT_EQ(addr % POOL_ALIGN, (uintptr_t)0);

    // No overlap with any live interval. The map is ordered by address, so
    // only the neighbors can overlap: the predecessor must end at or before
    // addr, the successor must start at or after addr + extent. This also
    // catches full containment — a contained interval's predecessor-or-self
    // fails one of the two comparisons.
    auto after = live.lower_bound(addr);

    if(after != live.begin())
    {
      auto before = std::prev(after);
      ASSERT_LE(before->first + before->second.extent, addr);
    }

    if(after != live.end())
      ASSERT_LE(addr + extent, after->first);

    uint64_t seed = next_seed++;
    fill(p, extent, seed);
    live[addr] = {extent, seed};
  }

  // Pattern verify and unrecord. The caller frees afterwards.
  void on_free(void* p)
  {
    auto it = live.find((uintptr_t)p);
    ASSERT_NE(it, live.end());
    verify(p, it->second.extent, it->second.seed);
    live.erase(it);
  }

  // Re-record a realloc: verify the surviving prefix at the new address,
  // then treat the result as a fresh allocation (without refilling the
  // surviving prefix would be redundant; fill writes it all).
  void on_realloc(void* old_p, void* new_p, size_t new_extent,
    size_t preserved)
  {
    auto it = live.find((uintptr_t)old_p);
    ASSERT_NE(it, live.end());
    uint64_t old_seed = it->second.seed;
    size_t old_extent = it->second.extent;
    live.erase(it);

    // The first min(old, new) bytes moved with the data. Only offsets the
    // fill actually wrote can be checked: whether that was every byte or
    // one stamp per stride is a fact about the old extent, not about how
    // much survived.
    if(old_extent <= full_fill_limit)
    {
      size_t n = (preserved < old_extent) ? preserved : old_extent;

      for(size_t i = 0; i < n; i++)
      {
        ASSERT_EQ(((uint8_t*)new_p)[i], pattern_byte(old_seed, i))
          << "offset " << i;
      }
    } else {
      for(size_t off = 0; off < preserved; off += stamp_stride)
        stamp_verify(new_p, preserved, old_seed, off);
    }

    on_alloc(new_p, new_extent);
  }

  void verify_all()
  {
    for(auto& e : live)
      verify((void*)e.first, e.second.extent, e.second.seed);
  }

  static void fill(void* p, size_t extent, uint64_t seed)
  {
    if(extent <= full_fill_limit)
    {
      for(size_t i = 0; i < extent; i++)
        ((uint8_t*)p)[i] = pattern_byte(seed, i);
      return;
    }

    for(size_t off = 0; off < extent; off += stamp_stride)
      stamp_fill(p, extent, seed, off);

    stamp_fill(p, extent, seed, extent - stamp_size);
  }

  static void verify(void* p, size_t extent, uint64_t seed)
  {
    if(extent <= full_fill_limit)
    {
      verify_prefix(p, extent, seed);
      return;
    }

    for(size_t off = 0; off < extent; off += stamp_stride)
      stamp_verify(p, extent, seed, off);

    stamp_verify(p, extent, seed, extent - stamp_size);
  }

  static void verify_prefix(void* p, size_t len, uint64_t seed)
  {
    if(len <= full_fill_limit)
    {
      for(size_t i = 0; i < len; i++)
        ASSERT_EQ(((uint8_t*)p)[i], pattern_byte(seed, i)) << "offset " << i;
      return;
    }

    for(size_t off = 0; off < len; off += stamp_stride)
      stamp_verify(p, len, seed, off);
  }

private:
  static void stamp_fill(void* p, size_t extent, uint64_t seed, size_t off)
  {
    size_t end = off + stamp_size;

    if(end > extent)
      end = extent;

    for(size_t i = off; i < end; i++)
      ((uint8_t*)p)[i] = pattern_byte(seed, i);
  }

  static void stamp_verify(void* p, size_t extent, uint64_t seed, size_t off)
  {
    size_t end = off + stamp_size;

    if(end > extent)
      end = extent;

    for(size_t i = off; i < end; i++)
      ASSERT_EQ(((uint8_t*)p)[i], pattern_byte(seed, i)) << "offset " << i;
  }
};

// Runs a test body on a fresh thread and joins it. A fresh thread has a
// virgin thread-local allocator, which makes exact-address assertions
// sound; the body allocates and frees only its own memory, so the arena
// backend's cross-thread rules are never involved.
template<typename F>
void on_fresh_thread(F f)
{
  std::thread t(f);
  t.join();
}

#if defined(POOL_USE_ARENA) && defined(PLATFORM_IS_LINUX)
// Whether any mapping covers the address. Line counts can lie — the
// kernel may merge adjacent anonymous mappings into one line — so the
// mapped-or-not question is asked per address.
bool maps_covers(void* p)
{
  FILE* f = fopen("/proc/self/maps", "r");

  if(f == NULL)
    return false;

  uintptr_t addr = (uintptr_t)p;
  char line[256];
  bool covered = false;

  while(fgets(line, sizeof(line), f) != NULL)
  {
    uintptr_t lo;
    uintptr_t hi;

    if(sscanf(line, "%lx-%lx ", &lo, &hi) == 2)
    {
      if((addr >= lo) && (addr < hi))
      {
        covered = true;
        break;
      }
    }
  }

  fclose(f);
  return covered;
}

// Bytes of the range holding physical pages (or swap), from
// /proc/self/pagemap. Parked address space stays mapped for the life of
// the process, so mapped-or-not cannot show a release; pages-or-not can.
size_t resident_bytes(void* p, size_t len)
{
  int fd = open("/proc/self/pagemap", O_RDONLY);

  if(fd < 0)
    return SIZE_MAX;

  size_t page = (size_t)sysconf(_SC_PAGESIZE);
  uintptr_t from = (uintptr_t)p / page;
  size_t npages = (len + page - 1) / page;
  size_t resident = 0;

  static const size_t batch = 4096;
  uint64_t entries[batch];

  for(size_t done = 0; done < npages;)
  {
    size_t n = npages - done;

    if(n > batch)
      n = batch;

    ssize_t got = pread(fd, entries, n * sizeof(uint64_t),
      (off_t)((from + done) * sizeof(uint64_t)));

    if(got <= 0)
    {
      close(fd);
      return SIZE_MAX;
    }

    size_t read_entries = (size_t)got / sizeof(uint64_t);

    for(size_t i = 0; i < read_entries; i++)
    {
      // Bit 63: present. Bit 62: swapped — still held, not returned.
      if((entries[i] & ((uint64_t)3 << 62)) != 0)
        resident += page;
    }

    done += read_entries;
  }

  close(fd);
  return resident;
}
#endif

} // namespace

TEST(Pool, Index)
{
  size_t expected_index = 0;
  size_t index = ponyint_pool_index(1);
  ASSERT_TRUE(index == 0);

  index = ponyint_pool_index(7);
  ASSERT_TRUE(index == expected_index);

  index = ponyint_pool_index(8);
  ASSERT_TRUE(index == expected_index);

  index = ponyint_pool_index(9);
  ASSERT_TRUE(index == expected_index);

  index = ponyint_pool_index(15);
  ASSERT_TRUE(index == expected_index);

  index = ponyint_pool_index(16);
  ASSERT_TRUE(index == expected_index);

#if !defined(POOL_USE_CLASSIC) && !defined(POOL_USE_ARENA)
  // The memalign pool's minimum allocation is 16 bytes, so 17 falls in the
  // second size class there; the classic and arena pools start at 32.
  expected_index++;
#endif

  index = ponyint_pool_index(17);
  ASSERT_TRUE(index == expected_index);

  index = ponyint_pool_index(31);
  ASSERT_TRUE(index == expected_index);

  index = ponyint_pool_index(32);
  ASSERT_TRUE(index == expected_index);

  expected_index++;

  index = ponyint_pool_index(33);
  ASSERT_TRUE(index == expected_index);

  index = ponyint_pool_index(63);
  ASSERT_TRUE(index == expected_index);

  index = ponyint_pool_index(64);
  ASSERT_TRUE(index == expected_index);

  expected_index++;

  index = ponyint_pool_index(65);
  ASSERT_TRUE(index == expected_index);

  index = ponyint_pool_index(POOL_MAX - 1);
  ASSERT_TRUE(index == POOL_COUNT - 1);

  index = ponyint_pool_index(POOL_MAX);
  ASSERT_TRUE(index == POOL_COUNT - 1);

  index = ponyint_pool_index(POOL_MAX + 1);
  ASSERT_TRUE(index == POOL_COUNT);
}

// Every size up to POOL_MAX maps to the smallest class that fits it.
// Pool.Index above pins a handful of edges by hand; this sweeps them all.
TEST(Pool, IndexProperty)
{
  for(size_t s = 1; s <= POOL_MAX; s++)
  {
    size_t index = ponyint_pool_index(s);
    ASSERT_LT(index, (size_t)POOL_COUNT) << "size " << s;
    ASSERT_GE(POOL_SIZE(index), s) << "size " << s;

    if(index > 0)
      ASSERT_LT(POOL_SIZE(index - 1), s) << "size " << s;
  }

  ASSERT_EQ(ponyint_pool_index(POOL_MAX + 1), (size_t)POOL_COUNT);
}

// The compile-time POOL_INDEX macro and the runtime function must agree:
// memory allocated through one and freed through the other lands in the
// wrong class if they diverge. POOL_INDEX only accepts sizes up to
// POOL_MAX, so the checks stop there.
TEST(Pool, IndexMacroAgreement)
{
#define AGREE(N) \
  ASSERT_EQ((size_t)POOL_INDEX(N), ponyint_pool_index(N)) << "size " << (N)

  AGREE(1);
  AGREE(POOL_SIZE(0));
  AGREE(POOL_SIZE(0) + 1);
  AGREE(POOL_SIZE(1));
  AGREE(POOL_SIZE(1) + 1);
  AGREE(POOL_SIZE(2));
  AGREE(POOL_SIZE(2) + 1);
  AGREE(POOL_SIZE(3));
  AGREE(POOL_SIZE(3) + 1);
  AGREE(POOL_SIZE(4));
  AGREE(POOL_SIZE(4) + 1);
  AGREE(POOL_SIZE(5));
  AGREE(POOL_SIZE(5) + 1);
  AGREE(POOL_SIZE(6));
  AGREE(POOL_SIZE(6) + 1);
  AGREE(POOL_SIZE(7));
  AGREE(POOL_SIZE(7) + 1);
  AGREE(POOL_SIZE(8));
  AGREE(POOL_SIZE(8) + 1);
  AGREE(POOL_SIZE(9));
  AGREE(POOL_SIZE(9) + 1);
  AGREE(POOL_SIZE(10));
  AGREE(POOL_SIZE(10) + 1);
  AGREE(POOL_SIZE(11));
  AGREE(POOL_SIZE(11) + 1);
  AGREE(POOL_SIZE(12));
  AGREE(POOL_SIZE(12) + 1);
  AGREE(POOL_SIZE(13));
  AGREE(POOL_SIZE(13) + 1);
  AGREE(POOL_SIZE(14));
  AGREE(POOL_SIZE(14) + 1);
  AGREE(POOL_SIZE(POOL_COUNT - 1));

#ifdef POOL_USE_MEMALIGN
  // One more class exists when the minimum allocation is 16 bytes.
  AGREE(POOL_SIZE(15));
  AGREE(POOL_SIZE(15) + 1);
#endif

#undef AGREE
}

// used_size reports the class size below POOL_MAX and the POOL_ALIGN-
// rounded size above it; adjust_size rounds up to the next POOL_ALIGN
// multiple and changes nothing that is already one.
TEST(Pool, SizeMath)
{
  for(size_t s = 1; s <= POOL_MAX; s++)
  {
    ASSERT_EQ(ponyint_pool_used_size(s), POOL_SIZE(ponyint_pool_index(s)))
      << "size " << s;
  }

  static const size_t above[] =
  {
    POOL_MAX + 1, POOL_MAX + 2, POOL_MAX + POOL_ALIGN - 1,
    POOL_MAX + POOL_ALIGN, POOL_MAX + POOL_ALIGN + 1,
    (3 * 1024 * 1024) + 123
  };

  for(size_t i = 0; i < (sizeof(above) / sizeof(above[0])); i++)
  {
    size_t s = above[i];
    ASSERT_EQ(ponyint_pool_used_size(s), ponyint_pool_adjust_size(s))
      << "size " << s;
  }

  for(size_t s = 1; s <= (4 * POOL_ALIGN); s++)
  {
    size_t r = ponyint_pool_adjust_size(s);
    ASSERT_GE(r, s) << "size " << s;
    ASSERT_EQ(r % POOL_ALIGN, (size_t)0) << "size " << s;
    ASSERT_LT(r - s, (size_t)POOL_ALIGN) << "size " << s;
    ASSERT_EQ(ponyint_pool_adjust_size(r), r) << "size " << s;
  }

  for(size_t i = 0; i < (sizeof(above) / sizeof(above[0])); i++)
  {
    size_t s = above[i];
    size_t r = ponyint_pool_adjust_size(s);
    ASSERT_GE(r, s);
    ASSERT_EQ(r % POOL_ALIGN, (size_t)0);
    ASSERT_LT(r - s, (size_t)POOL_ALIGN);
    ASSERT_EQ(ponyint_pool_adjust_size(r), r);
  }
}

// Near SIZE_MAX, rounding up would wrap to zero; adjust_size saturates to
// SIZE_MAX instead. The saturated value is the one result that is not a
// POOL_ALIGN multiple — never smaller than the request wins.
TEST(Pool, AdjustSizeOverflow)
{
  size_t m = SIZE_MAX & ~((size_t)POOL_ALIGN - 1);

  ASSERT_EQ(ponyint_pool_adjust_size(m), m);
  ASSERT_EQ(ponyint_pool_adjust_size(m + 1), SIZE_MAX);
  ASSERT_EQ(ponyint_pool_adjust_size(SIZE_MAX), SIZE_MAX);

  ASSERT_GE(ponyint_pool_adjust_size(m), m);
  ASSERT_GE(ponyint_pool_adjust_size(m + 1), m + 1);
  ASSERT_GE(ponyint_pool_adjust_size(SIZE_MAX), (size_t)SIZE_MAX);
}

// The interface contract — non-null, aligned, non-overlapping, usable to
// the granted extent — for every size class and across the size ladder's
// boundary points.
TEST(Pool, AllocContract)
{
  LiveTracker t;
  std::vector<std::pair<void*, size_t>> by_index;
  std::vector<std::pair<void*, size_t>> by_size;

  for(size_t index = 0; index < POOL_COUNT; index++)
  {
    for(int r = 0; r < 8; r++)
    {
      void* p = ponyint_pool_alloc(index);
      t.on_alloc(p, POOL_SIZE(index));

      if(::testing::Test::HasFatalFailure())
        return;

      by_index.push_back({p, index});
    }
  }

  static const size_t ladder[] =
  {
    1, POOL_MIN - 1, POOL_MIN, POOL_MIN + 1, 1023, 1024, 1025,
    (16 * 1024) - 1, 16 * 1024, (16 * 1024) + 1, 32 * 1024,
    POOL_MAX - 1, POOL_MAX, POOL_MAX + 1, POOL_MAX + POOL_ALIGN,
    (2 * 1024 * 1024) - 1, 2 * 1024 * 1024, (2 * 1024 * 1024) + 1,
    3 * 1024 * 1024
  };

  for(size_t i = 0; i < (sizeof(ladder) / sizeof(ladder[0])); i++)
  {
    size_t s = ladder[i];
    void* p = ponyint_pool_alloc_size(s);
    t.on_alloc(p, ponyint_pool_used_size(s));

    if(::testing::Test::HasFatalFailure())
      return;

    by_size.push_back({p, s});
  }

  // Free half in allocation order, half in reverse, verifying content.
  for(size_t i = 0; i < (by_index.size() / 2); i++)
  {
    t.on_free(by_index[i].first);
    ponyint_pool_free(by_index[i].second, by_index[i].first);
  }

  for(size_t i = by_index.size(); i > (by_index.size() / 2); i--)
  {
    t.on_free(by_index[i - 1].first);
    ponyint_pool_free(by_index[i - 1].second, by_index[i - 1].first);
  }

  for(size_t i = 0; i < (by_size.size() / 2); i++)
  {
    t.on_free(by_size[i].first);
    ponyint_pool_free_size(by_size[i].second, by_size[i].first);
  }

  for(size_t i = by_size.size(); i > (by_size.size() / 2); i--)
  {
    t.on_free(by_size[i - 1].first);
    ponyint_pool_free_size(by_size[i - 1].second, by_size[i - 1].first);
  }

  ASSERT_EQ(t.count(), (size_t)0);
}

// Allocations too large for one arena take their own mapping under the
// arena backend; elsewhere this is simply a very large allocation. The
// second size sits past a power-of-two so the mapping must be rounded up,
// not sized to the payload's own power of two.
TEST(Pool, VeryLarge)
{
#ifdef PLATFORM_IS_ILP32
  static const size_t sizes[] = {24 * 1024 * 1024, 32 * 1024 * 1024};
#else
  static const size_t sizes[] =
    {192 * 1024 * 1024, (size_t)256 * 1024 * 1024};
#endif

  for(size_t i = 0; i < (sizeof(sizes) / sizeof(sizes[0])); i++)
  {
    size_t s = sizes[i];
    char* p = (char*)ponyint_pool_alloc_size(s);
    ASSERT_NE(p, (char*)NULL);
    ASSERT_EQ((uintptr_t)p % POOL_ALIGN, (uintptr_t)0);

    // Touch only the edges; the mapping is large.
    memset(p, 0xa5, 1024);
    memset(p + s - 1024, 0x5a, 1024);
    ASSERT_EQ((uint8_t)p[0], 0xa5);
    ASSERT_EQ((uint8_t)p[512], 0xa5);
    ASSERT_EQ((uint8_t)p[s - 1], 0x5a);
    ASSERT_EQ((uint8_t)p[s - 1024], 0x5a);

    ponyint_pool_free_size(s, p);

    // The size still works after the free returned the mapping.
    p = (char*)ponyint_pool_alloc_size(s);
    ASSERT_NE(p, (char*)NULL);
    p[0] = 'x';
    p[s - 1] = 'y';
    ASSERT_EQ(p[0], 'x');
    ASSERT_EQ(p[s - 1], 'y');
    ponyint_pool_free_size(s, p);
  }

  // Growing between two of these sizes crosses realloc's own-mapping
  // paths; the copied prefix must arrive.
  {
    size_t s1 = sizes[0];
    size_t s2 = sizes[1];
    char* p = (char*)ponyint_pool_alloc_size(s1);
    ASSERT_NE(p, (char*)NULL);
    memset(p, 0x42, 1024);
    memset(p + s1 - 1024, 0x24, 1024);

    char* q = (char*)ponyint_pool_realloc_size(s1, s2, p);
    ASSERT_NE(q, (char*)NULL);
    ASSERT_EQ((uint8_t)q[0], 0x42);
    ASSERT_EQ((uint8_t)q[512], 0x42);
    ASSERT_EQ((uint8_t)q[s1 - 1024], 0x24);
    ASSERT_EQ((uint8_t)q[s1 - 1], 0x24);
    ponyint_pool_free_size(s2, q);
  }
}

// Every branch of realloc: the two return-the-same-pointer branches, the
// four copying branches, and NULL-as-alloc. Content must survive to
// min(old, new) on every branch.
TEST(Pool, ReallocContract)
{
  LiveTracker t;

  // NULL acts as alloc.
  {
    void* p = ponyint_pool_realloc_size(0, 100, NULL);
    t.on_alloc(p, ponyint_pool_used_size(100));
    t.on_free(p);
    ponyint_pool_free_size(100, p);
  }

  // Same class: the pointer comes straight back. This branch is shared,
  // identical code in all three backends, and the pointer equality is the
  // only assertion that can tell the branch from an alloc-copy-free.
  {
    void* p = ponyint_pool_alloc_size(33);
    t.on_alloc(p, ponyint_pool_used_size(33));
    void* q = ponyint_pool_realloc_size(33, 64, p);
    ASSERT_EQ(q, p);
    t.on_free(q);
    ponyint_pool_free_size(64, q);
  }

  // Large with the same adjusted size: also the same pointer.
  {
    size_t s1 = POOL_MAX + 100;
    size_t s2 = POOL_MAX + 200;
    ASSERT_EQ(ponyint_pool_adjust_size(s1), ponyint_pool_adjust_size(s2));
    void* p = ponyint_pool_alloc_size(s1);
    t.on_alloc(p, ponyint_pool_used_size(s1));
    void* q = ponyint_pool_realloc_size(s1, s2, p);
    ASSERT_EQ(q, p);
    t.on_free(q);
    ponyint_pool_free_size(s2, q);
  }

  struct Case
  {
    size_t old_size;
    size_t new_size;
  };

  static const Case copying[] =
  {
    {17, 100},                          // class grow
    {4096, 64},                         // class shrink
    {1000, POOL_MAX + 1000},            // class -> large
    {POOL_MAX + 1, 3 * 1024 * 1024},    // large grow
    {2 * POOL_MAX, 100},                // large -> class shrink
  };

  for(size_t i = 0; i < (sizeof(copying) / sizeof(copying[0])); i++)
  {
    size_t old_size = copying[i].old_size;
    size_t new_size = copying[i].new_size;

    void* p = ponyint_pool_alloc_size(old_size);
    t.on_alloc(p, ponyint_pool_used_size(old_size));

    void* q = ponyint_pool_realloc_size(old_size, new_size, p);
    size_t preserved = (old_size < new_size) ? old_size : new_size;
    t.on_realloc(p, q, ponyint_pool_used_size(new_size), preserved);

    if(::testing::Test::HasFatalFailure())
      return;

    t.on_free(q);
    ponyint_pool_free_size(new_size, q);
  }

  ASSERT_EQ(t.count(), (size_t)0);
}

#if defined(POOL_USE_CLASSIC) || defined(POOL_USE_ARENA)

// Freed memory comes back: over many alloc/free cycles the set of distinct
// addresses stays near the per-cycle working set instead of growing with
// the cycle count. Not asserted under memalign, whose frees go straight to
// libc free — that backend makes no reuse promise, and under
// AddressSanitizer freed memory is quarantined rather than reused.
TEST(Pool, FreedMemoryIsReused)
{
  // Keeps the arena mapped: without a live object the arena backend
  // returns an emptied arena to the OS and the next cycle's addresses are
  // the kernel's choice, not the allocator's.
  void* pin = ponyint_pool_alloc(0);

  struct Leg
  {
    size_t size;
    size_t per_cycle;
  };

  // The block leg stays well inside one arena alongside the pin (an arena
  // holds ~7.9 MiB usable on 64-bit, ~1.9 MiB on ILP32).
#ifdef PLATFORM_IS_ILP32
  static const Leg legs[] =
    {{32, 64}, {4096, 64}, {512 * 1024, 3}};
#else
  static const Leg legs[] =
    {{32, 64}, {4096, 64}, {2 * 1024 * 1024, 3}};
#endif

  for(size_t l = 0; l < (sizeof(legs) / sizeof(legs[0])); l++)
  {
    std::set<void*> distinct;
    std::vector<void*> cycle(legs[l].per_cycle);

    for(int c = 0; c < 50; c++)
    {
      for(size_t i = 0; i < legs[l].per_cycle; i++)
      {
        cycle[i] = ponyint_pool_alloc_size(legs[l].size);
        ASSERT_NE(cycle[i], (void*)NULL);
        distinct.insert(cycle[i]);
      }

      for(size_t i = legs[l].per_cycle; i > 0; i--)
        ponyint_pool_free_size(legs[l].size, cycle[i - 1]);
    }

    // Without reuse this reaches 50 * per_cycle.
    ASSERT_LE(distinct.size(), 2 * legs[l].per_cycle)
      << "size " << legs[l].size;
  }

  ponyint_pool_free(0, pin);
}

#endif

// The design discussion's independent-record program, permanent form: a
// seeded random workload over every class, both entry APIs, blocks, and
// realloc, with an independent live map asserting non-null, alignment,
// non-overlap, and content survival on every operation.
TEST(Pool, LiveRecordChurn)
{
  uint64_t rng = 0x5eed;

  LiveTracker t;

  struct Live
  {
    void* p;
    size_t size;
    bool by_index;
  };

  std::vector<Live> live;
  size_t live_bytes = 0;
  static const size_t live_cap = 64 * 1024 * 1024;

  // Coverage counters: every class through both APIs, and blocks, must
  // actually be drawn — a bucket the generator never hits is a bucket
  // nothing here tested.
  int hit_index[POOL_COUNT] = {0};
  int hit_size_class = 0;
  int hit_block = 0;

  static const int ops = 100000;

  for(int op = 0; op < ops; op++)
  {
    if(::testing::Test::HasFatalFailure())
      return;

    uint64_t roll = splitmix64(&rng);
    // Grow to the cap, then hover: free when over, allocate when under.
    bool do_free = (live_bytes > live_cap) ||
      (!live.empty() && ((roll % 100) < 40));

    if(do_free && !live.empty())
    {
      size_t victim = splitmix64(&rng) % live.size();
      Live v = live[victim];
      live[victim] = live.back();
      live.pop_back();

      t.on_free(v.p);

      if(v.by_index)
        ponyint_pool_free(ponyint_pool_index(v.size), v.p);
      else
        ponyint_pool_free_size(v.size, v.p);

      live_bytes -= ponyint_pool_used_size(v.size);
      continue;
    }

    uint64_t kind = roll % 100;

    if(kind < 45)
    {
      // Size-class allocation through the index API.
      size_t index = splitmix64(&rng) % POOL_COUNT;
      void* p = ponyint_pool_alloc(index);
      t.on_alloc(p, POOL_SIZE(index));
      live.push_back({p, POOL_SIZE(index), true});
      live_bytes += POOL_SIZE(index);
      hit_index[index]++;
    } else if(kind < 90) {
      // Size-class allocation through the size API.
      size_t index = splitmix64(&rng) % POOL_COUNT;
      size_t lo = (index == 0) ? 1 : (POOL_SIZE(index - 1) + 1);
      size_t s = lo + (splitmix64(&rng) % (POOL_SIZE(index) - lo + 1));
      void* p = ponyint_pool_alloc_size(s);
      t.on_alloc(p, ponyint_pool_used_size(s));
      live.push_back({p, s, false});
      live_bytes += ponyint_pool_used_size(s);
      hit_size_class++;
    } else if(kind < 97) {
      // A block above POOL_MAX, up to 4 MiB.
      size_t s = POOL_MAX + 1 +
        (splitmix64(&rng) % (3 * 1024 * 1024));
      void* p = ponyint_pool_alloc_size(s);
      t.on_alloc(p, ponyint_pool_used_size(s));
      live.push_back({p, s, false});
      live_bytes += ponyint_pool_used_size(s);
      hit_block++;
    } else if(!live.empty()) {
      // Realloc a size-API victim to a new size.
      size_t victim = splitmix64(&rng) % live.size();

      if(live[victim].by_index)
        continue;

      Live v = live[victim];
      size_t new_size = 1 +
        (splitmix64(&rng) % (2 * 1024 * 1024));
      void* q = ponyint_pool_realloc_size(v.size, new_size, v.p);
      size_t preserved = (v.size < new_size) ? v.size : new_size;
      t.on_realloc(v.p, q, ponyint_pool_used_size(new_size), preserved);
      live_bytes -= ponyint_pool_used_size(v.size);
      live_bytes += ponyint_pool_used_size(new_size);
      live[victim] = {q, new_size, false};
    }
  }

  t.verify_all();

  for(auto& v : live)
  {
    t.on_free(v.p);

    if(v.by_index)
      ponyint_pool_free(ponyint_pool_index(v.size), v.p);
    else
      ponyint_pool_free_size(v.size, v.p);
  }

  ASSERT_EQ(t.count(), (size_t)0);

  for(size_t i = 0; i < POOL_COUNT; i++)
    ASSERT_GT(hit_index[i], 0) << "class " << i << " never drawn";

  ASSERT_GT(hit_size_class, 0);
  ASSERT_GT(hit_block, 0);
}

#ifdef POOL_USE_ARENA

/* The arena backend's own geometry and registry sizing. These constants
 * mirror pool_arena.c (which cannot export them without widening its
 * interface); change both together.
 */
#ifdef PLATFORM_IS_ILP32
#define TEST_REGION_SIZE ((size_t)64 * 1024 * 1024)
#define TEST_ARENA_SIZE ((size_t)2 * 1024 * 1024)
#else
#define TEST_REGION_SIZE ((size_t)256 * 1024 * 1024)
#define TEST_ARENA_SIZE ((size_t)8 * 1024 * 1024)
#endif
#define TEST_UNIT_SIZE ((size_t)16 * 1024)
#define TEST_SWEEP_THRESHOLD ((TEST_ARENA_SIZE / TEST_UNIT_SIZE) / 4)
#define TEST_SEGMENT_SLOTS 256
#define TEST_CHAIN_MAP_INITIAL 128

static_assert((TEST_REGION_SIZE / TEST_ARENA_SIZE) == 32,
  "a region's slot bitmap is one 32-bit word; mirrors pool_arena.c");

/// A block sized so exactly one fits per arena (its span is more than
/// half the arena's units), on both geometries. One block, one arena.
#define TEST_ARENA_FILLING_BLOCK (TEST_ARENA_SIZE - (4 * TEST_UNIT_SIZE))

namespace
{

uintptr_t arena_base_of(void* p)
{
  return (uintptr_t)p & ~(TEST_ARENA_SIZE - 1);
}

} // namespace

// The slab state machine, asserted at exact addresses on a virgin
// thread: bump order, the full slab's trip through the partial list, slab
// release, re-carving a released unit for another class, and the
// current slab's in-place reset.
extern "C" void ponyint_pool_arena_cache_disable_for_test();

// The arena's integrity checks are compiled out in release (no release cost for
// debug safety), and the cache-disable seam takes effect only where they are
// active. So the death tests that assert those aborts, and the slab-layer tests
// that disable the cache, skip in a release build (they still run in debug and
// in a release-safe -DPONY_ALWAYS_ASSERT build).
#if defined(NDEBUG) && !defined(PONY_ALWAYS_ASSERT)
#define SKIP_WITHOUT_ARENA_CHECKS() \
  GTEST_SKIP() << "arena integrity checks are compiled out in release"
#else
#define SKIP_WITHOUT_ARENA_CHECKS() ((void)0)
#endif

TEST(PoolArena, SlabLifecycle)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  on_fresh_thread([]{
    // Asserts slab release/geometry directly; the thread cache would defer it,
    // so route frees straight to the slab path.
    ponyint_pool_arena_cache_disable_for_test();
    static const size_t cap = 16 * 1024 / 32; // objects per 32-byte slab

    // Fill slab A exactly, plus one object to move the current slab on.
    std::vector<char*> a_objs;

    char* first = (char*)ponyint_pool_alloc(0);
    ASSERT_NE(first, (char*)NULL);
    a_objs.push_back(first);

    for(size_t i = 1; i < cap; i++)
    {
      char* p = (char*)ponyint_pool_alloc(0);
      ASSERT_EQ(p, first + (i * 32)); // bump order within the first slab
      a_objs.push_back(p);
    }

    char* b_first = (char*)ponyint_pool_alloc(0);
    ASSERT_EQ(b_first, first + TEST_UNIT_SIZE); // next unit begins slab B

    // A full slab that gets a free goes to the partial list; draining the
    // current slab's bump space must then pop it back and return exactly
    // the freed address.
    ponyint_pool_free(0, a_objs[0]);

    char* back = NULL;

    for(size_t i = 0; i < cap; i++)
    {
      back = (char*)ponyint_pool_alloc(0);

      if(back == a_objs[0])
        break;
    }

    ASSERT_EQ(back, a_objs[0]);

    // Slab B is now the full orphan. Free all of it, ascending; the last
    // free must release the slab (it is neither current nor retained).
    ponyint_pool_free(0, b_first);

    for(size_t i = 1; i < cap; i++)
      ponyint_pool_free(0, b_first + (i * 32));

    // B's unit, released, is the lowest free unit; a 16 KiB-class
    // allocation re-carves it for a different size class.
    char* recarved = (char*)ponyint_pool_alloc_size(16 * 1024);
    ASSERT_EQ(recarved, b_first);

    // Drain slab A (current): the last free resets it in place, and the
    // next fill bumps from its base again, in order. Ascending frees so a
    // reset that leaves the free list populated hands the addresses back
    // in the wrong order and fails the bump assertion.
    for(size_t i = 0; i < cap; i++)
      ponyint_pool_free(0, first + (i * 32));

    for(size_t i = 0; i < cap; i++)
    {
      char* p = (char*)ponyint_pool_alloc(0);
      ASSERT_EQ(p, first + (i * 32));
    }

    for(size_t i = 0; i < cap; i++)
      ponyint_pool_free(0, first + (i * 32));

    ponyint_pool_free_size(16 * 1024, recarved);
  });
}

// Arena-level geometry: dirty units are re-carved with their dirty bit
// cleared, the sweep drops only free units, fragmentation with plenty of
// free-but-scattered units opens a new arena, and emptied arenas release.
TEST(PoolArena, ArenaGeometry)
{
  on_fresh_thread([]{
    static const size_t obj = 16 * 1024; // class 9: one slab per unit

    // Fill the first arena completely; the object that lands in a second
    // arena reveals both the boundary and the usable unit count.
    std::vector<char*> a1;
    char* p = (char*)ponyint_pool_alloc_size(obj);
    ASSERT_NE(p, (char*)NULL);
    uintptr_t base1 = arena_base_of(p);
    a1.push_back(p);

    char* overflow = NULL;

    for(;;)
    {
      char* q = (char*)ponyint_pool_alloc_size(obj);

      if(arena_base_of(q) != base1)
      {
        overflow = q;
        break;
      }

      a1.push_back(q);
    }

    size_t usable = a1.size();
    uintptr_t base2 = arena_base_of(overflow);

    // Stamp a canary in every arena-1 object.
    for(size_t i = 0; i < usable; i++)
      memset(a1[i], (int)(0x40 + (i % 64)), 64);

    // Fill arena 2 exactly: allocation walks arenas newest-first, so
    // arena 2 must be full before any free unit of arena 1 is taken.
    std::vector<char*> a2;
    a2.push_back(overflow);

    while(a2.size() < usable)
    {
      char* q = (char*)ponyint_pool_alloc_size(obj);
      ASSERT_EQ(arena_base_of(q), base2) << "arena 2 not full yet";
      a2.push_back(q);
    }

    // Free alternate arena-1 objects, but stay below the sweep threshold.
    // (The margin scales down for the ILP32 arena's smaller unit count.)
    size_t below = (TEST_SWEEP_THRESHOLD > 512)
      ? (TEST_SWEEP_THRESHOLD - 256) : (TEST_SWEEP_THRESHOLD / 2);
    size_t freed = 0;

    for(size_t i = 0; (i < usable) && (freed < below); i += 2, freed++)
      ponyint_pool_free_size(obj, a1[i]);

    // Re-carve a handful of those dirty units. If re-reserving failed to
    // clear their dirty bits, the sweep below would drop these pages and
    // the canaries with them.
    char* recarved[8];

    for(int i = 0; i < 8; i++)
    {
      recarved[i] = (char*)ponyint_pool_alloc_size(obj);
      ASSERT_EQ(arena_base_of(recarved[i]), base1);
      memset(recarved[i], 0x7e, 64);
    }

    // Free the remaining alternates; the dirty count crosses the
    // threshold and exactly one sweep runs, with live units interleaved.
    for(size_t i = 2 * below; i < usable; i += 2)
      ponyint_pool_free_size(obj, a1[i]);

    // Everything still live kept its content through the sweep.
    for(size_t i = 1; i < usable; i += 2)
    {
      ASSERT_EQ((uint8_t)a1[i][0], (uint8_t)(0x40 + (i % 64)))
        << "arena-1 canary " << i;
      ASSERT_EQ((uint8_t)a1[i][63], (uint8_t)(0x40 + (i % 64)));
    }

    for(int i = 0; i < 8; i++)
      ASSERT_EQ((uint8_t)recarved[i][0], 0x7e) << "re-carved canary " << i;

    // Arena 1 is a checkerboard: about half its units free, no two in a
    // row. Arena 2 is full. A two-unit span must open a third arena.
    char* span2 = (char*)ponyint_pool_alloc_size(32 * 1024);
    ASSERT_NE(arena_base_of(span2), base1);
    ASSERT_NE(arena_base_of(span2), base2);

    // Drain everything; the allocator still serves after releasing.
    ponyint_pool_free_size(32 * 1024, span2);

    for(size_t i = 1; i < usable; i += 2)
      ponyint_pool_free_size(obj, a1[i]);

    for(int i = 0; i < 8; i++)
      ponyint_pool_free_size(obj, recarved[i]);

    for(size_t i = 0; i < usable; i++)
      ponyint_pool_free_size(obj, a2[i]);

    char* last = (char*)ponyint_pool_alloc_size(obj);
    ASSERT_NE(last, (char*)NULL);
    last[0] = 'z';
    ASSERT_EQ(last[0], 'z');
    ponyint_pool_free_size(obj, last);
  });
}

// Block placement: exact block reuse inside a pinned arena, and
// releasing one arena leaves another's contents alone.
TEST(PoolArena, BlockPlacement)
{
  on_fresh_thread([]{
    // The pin keeps the first arena alive across the frees below.
    void* pin = ponyint_pool_alloc(0);
    ASSERT_NE(pin, (void*)NULL);

    // A freed block's units are the lowest free span, so the same size
    // comes straight back to the same address.
    size_t block = 2 * 1024 * 1024;
    char* p1 = (char*)ponyint_pool_alloc_size(block);
    ASSERT_EQ(arena_base_of(p1), arena_base_of(pin));
    ponyint_pool_free_size(block, p1);
    char* p2 = (char*)ponyint_pool_alloc_size(block);
    ASSERT_EQ(p2, p1);
    ponyint_pool_free_size(block, p2);

#ifndef PLATFORM_IS_ILP32
    // Two 4 MiB blocks cannot share an 8 MiB arena with the pin; the
    // second one opens its own arena, and freeing it drops that arena's
    // pages without touching the first block's memory.
    size_t big = 4 * 1024 * 1024;
    char* b1 = (char*)ponyint_pool_alloc_size(big);
    ASSERT_EQ(arena_base_of(b1), arena_base_of(pin));
    char* b2 = (char*)ponyint_pool_alloc_size(big);
    ASSERT_NE(arena_base_of(b2), arena_base_of(pin));

    for(size_t off = 0; off < big; off += (1024 * 1024))
      b1[off] = (char)(0x30 + ((off >> 20) % 64));

    ponyint_pool_free_size(big, b2);

    for(size_t off = 0; off < big; off += (1024 * 1024))
      ASSERT_EQ(b1[off], (char)(0x30 + ((off >> 20) % 64)));

    ponyint_pool_free_size(big, b1);
#endif

    ponyint_pool_free(0, pin);
  });
}

// The release-retained ARENA_CHECKs are the backend's memory-safety net —
// AddressSanitizer does not instrument the objects the arena carves out of
// its own mapping — and a check only shows up in a test when it aborts. Each death
// test proves one check still does, matching a fragment of its message.
TEST(PoolArenaDeath, FreeWrongSizeClass)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";
  void* p = ponyint_pool_alloc(2);
  EXPECT_DEATH(ponyint_pool_free(3, p), "size_class == index");
  ponyint_pool_free(2, p);
}

TEST(PoolArenaDeath, FreeMisalignedPointer)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";
  char* p = (char*)ponyint_pool_alloc(0);
  EXPECT_DEATH(ponyint_pool_free(0, p + 8), "size - 1");
  ponyint_pool_free(0, p);
}

TEST(PoolArenaDeath, FreeWhenNoneLive)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  // The double free must happen inside the death statement: the child
  // process does both frees, and the parent's allocator state carries
  // neither.
  EXPECT_DEATH({
    ponyint_pool_arena_cache_disable_for_test();
    void* p = ponyint_pool_alloc(4);
    ponyint_pool_free(4, p);
    ponyint_pool_free(4, p);
  }, "live > 0");
}

TEST(PoolArenaDeath, CacheDoubleFree)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  // With the cache enabled (the default), a same-thread block freed twice
  // while still cached is caught by the cache's sentinel guard, not the slab
  // live count: a cached block stays live in its slab.
  EXPECT_DEATH({
    void* p = ponyint_pool_alloc(4);
    ponyint_pool_free(4, p);
    ponyint_pool_free(4, p);
  }, "POOL_CACHE_SENTINEL");
}

TEST(PoolArenaDeath, CorruptedFreeListLink)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  // A freed object's first word is the free-list link. A program that
  // writes to freed memory can forge it; the pop must refuse to hand the
  // forged pointer out — whether it points outside the slab or inside at
  // a non-object boundary.
  EXPECT_DEATH({
    void* keep = ponyint_pool_alloc(1); // holds the slab live and current
    void* p = ponyint_pool_alloc(1);
    ponyint_pool_free(1, p);
    *(void**)p = (void*)0x1000; // outside the slab
    ponyint_pool_alloc(1);
    ponyint_pool_alloc(1);
    (void)keep;
  }, "pool_arena check failed");

  EXPECT_DEATH({
    void* keep = ponyint_pool_alloc(1);
    char* p = (char*)ponyint_pool_alloc(1);
    ponyint_pool_free(1, p);
    *(void**)p = (void*)(p + 8); // inside, off the object grid
    ponyint_pool_alloc(1);
    ponyint_pool_alloc(1);
    (void)keep;
  }, "pool_arena check failed");
}

TEST(PoolArenaDeath, BlockInteriorFree)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";
  size_t block = 2 * 1024 * 1024;
  char* p = (char*)ponyint_pool_alloc_size(block);

  // Not at the block's start, same unit: the base check fires. One unit
  // in: the record is a continuation, not a slab head.
  EXPECT_DEATH(ponyint_pool_free_size(block, p + 64), "unit_base");
  EXPECT_DEATH(ponyint_pool_free_size(block, p + TEST_UNIT_SIZE),
    "UNIT_STATE_HEAD");

  ponyint_pool_free_size(block, p);
}

TEST(PoolArenaDeath, BlockDoubleFree)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  EXPECT_DEATH({
    // The pin keeps the arena mapped, so the second free reaches the
    // check instead of faulting on an unmapped header.
    void* pin = ponyint_pool_alloc(0);
    void* p = ponyint_pool_alloc_size(2 * 1024 * 1024);
    ponyint_pool_free_size(2 * 1024 * 1024, p);
    ponyint_pool_free_size(2 * 1024 * 1024, p);
    (void)pin;
  }, "UNIT_STATE_HEAD");
}

TEST(PoolArenaDeath, FreeIntoFreeUnit)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  EXPECT_DEATH({
    void* pin = ponyint_pool_alloc(0);
    char* p = (char*)ponyint_pool_alloc_size(16 * 1024);
    // One unit past a single-unit slab: a unit nothing has carved.
    ponyint_pool_free_size(16 * 1024, p + TEST_UNIT_SIZE);
    (void)pin;
  }, "UNIT_STATE_HEAD");
}

// The crediting checks run when an owner drains its inbox, a moment no
// test can time; this seam credits one run directly. Each test below
// forges the corruption a hostile or broken freeing thread could put in
// an inbox and proves the credit refuses it. The layout mirrors
// run_header_t in pool_arena.c; change both together.
extern "C" void ponyint_pool_arena_credit_run_for_test(void* run_tail);

namespace
{

struct forged_run_t
{
  void* next_run;
  void* first;
  uint16_t len;
};

} // namespace

// A run whose tail sits in one slab and claims an object from another:
// the membership walk must refuse the foreign object.
TEST(PoolArenaDeath, CreditRunObjectOutsideSlab)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  EXPECT_DEATH({
    char* tail = (char*)ponyint_pool_alloc(0);
    char* outside = (char*)ponyint_pool_alloc(1); // a different slab
    forged_run_t* h = (forged_run_t*)tail;
    h->next_run = NULL;
    h->first = outside;
    h->len = 2;
    ponyint_pool_arena_credit_run_for_test(tail);
  }, "span_bytes");
}

// A run claiming more objects than the slab has live: the credit must
// refuse to drive the live count below zero.
TEST(PoolArenaDeath, CreditRunLenExceedsLive)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  EXPECT_DEATH({
    ponyint_pool_arena_cache_disable_for_test();
    char* a = (char*)ponyint_pool_alloc(0);
    char* b = (char*)ponyint_pool_alloc(0);
    char* c = (char*)ponyint_pool_alloc(0);
    ponyint_pool_free(0, a); // live drops to two; the run claims three
    *(void**)a = b;
    *(void**)b = c;
    forged_run_t* h = (forged_run_t*)c;
    h->next_run = NULL;
    h->first = a;
    h->len = 3;
    ponyint_pool_arena_credit_run_for_test(c);
  }, "live >= len");
}

// A run whose tail lands in a unit nothing occupies: the credit must
// refuse a slab that is not there.
TEST(PoolArenaDeath, CreditRunTailInFreeUnit)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  EXPECT_DEATH({
    void* pin = ponyint_pool_alloc(0); // keeps the arena occupied
    char* p = (char*)ponyint_pool_alloc_size(2 * 1024 * 1024);
    ponyint_pool_free_size(2 * 1024 * 1024, p);
    // The released span's pages went back to the operating system, so take
    // them back before forging a header in them. The unit stays free either
    // way, and a run whose tail sits in a free unit is what the credit must
    // refuse.
    ponyint_virt_commit(p, sizeof(forged_run_t));
    forged_run_t* h = (forged_run_t*)p;
    h->next_run = NULL;
    h->first = p;
    h->len = 1;
    ponyint_pool_arena_credit_run_for_test(p);
    (void)pin;
  }, "UNIT_STATE_HEAD");
}

// A run credited by a thread that does not own the slab's arena: the
// owner check must refuse it before any bookkeeping is touched. The
// crediting runs on a spawned thread because the death statement's
// process inherits this thread's slot, which does own the arena.
TEST(PoolArenaDeath, CreditRunForeignArena)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  char* p = (char*)ponyint_pool_alloc(0);

  EXPECT_DEATH({
    std::thread([&]{
      forged_run_t* h = (forged_run_t*)p;
      h->next_run = NULL;
      h->first = p;
      h->len = 1;
      ponyint_pool_arena_credit_run_for_test(p);
    }).join();
  }, "owner_slot == this_thread.slot");

  ponyint_pool_free(0, p);
}

// A run whose tail sits in an oversized mapping rather than an arena:
// the mapping-kind check must refuse it before the header is read as an
// arena's.
TEST(PoolArenaDeath, CreditRunOversizedMapping)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  EXPECT_DEATH({
    // Past the arena capacity: an own mapping, payload one unit in.
    char* p = (char*)ponyint_pool_alloc_size(9 * 1024 * 1024);
    forged_run_t* h = (forged_run_t*)p;
    h->next_run = NULL;
    h->first = p;
    h->len = 1;
    ponyint_pool_arena_credit_run_for_test(p);
  }, "MAPPING_ARENA");
}

// A run claiming two objects in a block slab, which holds exactly one:
// the block arm's length check must refuse it.
TEST(PoolArenaDeath, CreditRunBlockLenNotOne)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  EXPECT_DEATH({
    void* pin = ponyint_pool_alloc(0);
    char* b = (char*)ponyint_pool_alloc_size(2 * 1024 * 1024);
    forged_run_t* h = (forged_run_t*)b;
    h->next_run = NULL;
    h->first = b;
    h->len = 2;
    ponyint_pool_arena_credit_run_for_test(b);
    (void)pin;
  }, "len == 1");
}

// A run whose object is not at its block slab's base: the block arm's
// base check must refuse the interior pointer.
TEST(PoolArenaDeath, CreditRunBlockFirstNotBase)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  EXPECT_DEATH({
    void* pin = ponyint_pool_alloc(0);
    char* b = (char*)ponyint_pool_alloc_size(2 * 1024 * 1024);
    forged_run_t* h = (forged_run_t*)b;
    h->next_run = NULL;
    h->first = b + 64;
    h->len = 1;
    ponyint_pool_arena_credit_run_for_test(b);
    (void)pin;
  }, "first == ");
}

// A run whose tail sits off the slab's object grid: the alignment check
// must refuse it.
TEST(PoolArenaDeath, CreditRunMisalignedMember)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  EXPECT_DEATH({
    char* obj = (char*)ponyint_pool_alloc(0);
    forged_run_t* h = (forged_run_t*)(obj + 8);
    h->next_run = NULL;
    h->first = obj + 8;
    h->len = 1;
    ponyint_pool_arena_credit_run_for_test(obj + 8);
  }, "size - 1");
}

// A run whose chain does not end at the tail that carries the header:
// the walk must refuse the broken chain rather than splice it.
TEST(PoolArenaDeath, CreditRunBrokenChain)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  EXPECT_DEATH({
    char* a = (char*)ponyint_pool_alloc(0);
    char* b = (char*)ponyint_pool_alloc(0);
    *(void**)a = a; // the chain loops back instead of reaching b
    forged_run_t* h = (forged_run_t*)b;
    h->next_run = NULL;
    h->first = a;
    h->len = 2;
    ponyint_pool_arena_credit_run_for_test(b);
  }, "it == last");
}

// A block freed on another thread goes home through the owner's inbox and
// its units are reused for the next block: the cross-thread stranding that
// motivated the design, in miniature. On the classic pool this exact churn
// reserves fresh address space per block and never reuses any.
TEST(PoolArena, CrossThreadBlockChurn)
{
  on_fresh_thread([]{
    void* pin = ponyint_pool_alloc(0); // keeps the arena alive throughout
    static const size_t block = 4 * 1024 * 1024;

    std::set<void*> distinct;

    // The spawned threads only free, so none of them takes an owner
    // slot.
    for(int i = 0; i < 100; i++)
    {
      // The owner allocates; a second thread frees. The owner's next
      // allocation drains its inbox first, so the freed units are back in
      // the bitmap before the allocator looks for space.
      char* p = (char*)ponyint_pool_alloc_size(block);
      ASSERT_NE(p, (char*)NULL);
      p[0] = 'a';
      p[block - 1] = 'z';
      distinct.insert(p);

      std::thread([p]{
        ponyint_pool_free_size(block, p);
        ponyint_pool_thread_cleanup(); // delivers the sub-batch chain
      }).join();
    }

    ASSERT_LE(distinct.size(), (size_t)4);
    ponyint_pool_free(0, pin);
  });
}

// Small objects freed on another thread return to their slabs: the slab
// empties through the inbox path and its unit is re-carved. The freeing
// thread batches 32 objects per handoff, so the counts here cross several
// batch boundaries, and the explicit cleanup delivers the remainder.
TEST(PoolArena, CrossThreadSmallFrees)
{
  on_fresh_thread([]{
    static const size_t cap = 16 * 1024 / 32;

    // Fill slab A exactly, plus one to move the current slab to B.
    std::vector<char*> objs;

    for(size_t i = 0; i < (cap + 1); i++)
      objs.push_back((char*)ponyint_pool_alloc(0));

    char* a_base = objs[0];

    // A second thread frees all of slab A's objects: 16 full batches.
    std::thread([&]{
      for(size_t i = 0; i < cap; i++)
        ponyint_pool_free(0, objs[i]);

      ponyint_pool_thread_cleanup();
    }).join();

    // The owner's next slow-path allocation drains: slab A empties and is
    // released, then its unit serves a different size class.
    char* recarved = (char*)ponyint_pool_alloc_size(16 * 1024);
    ASSERT_EQ(recarved, a_base);

    ponyint_pool_free_size(16 * 1024, recarved);
    ponyint_pool_free(0, objs[cap]);
  });
}

// Foreign frees of three threads' memory interleaved through one freeing
// thread. Each owner stays alive until the frees are delivered, then
// drains its inbox and proves the crediting worked: its slabs empty and
// their units are re-carved for another class.
TEST(PoolArena, CrossThreadManyOwners)
{
  static const int owners = 3;
  static const size_t cap = 16 * 1024 / 64; // objects per class-1 slab

  std::vector<char*> bundles[owners];
  char* first_obj[owners];
  std::thread owner_threads[owners];
  std::atomic<int> stage[owners];
  std::atomic<bool> freed(false);

  for(int t = 0; t < owners; t++)
    stage[t].store(0);

  // Each owner fills one class-1 slab exactly, plus one object so the
  // slab it fills is not its current slab, then parks until the freeing
  // thread is done, then drains by allocating.
  for(int t = 0; t < owners; t++)
  {
    owner_threads[t] = std::thread([&, t]{
      for(size_t i = 0; i < (cap + 1); i++)
      {
        char* p = (char*)ponyint_pool_alloc(1);
        memset(p, 0x11 * (t + 1), 64);
        bundles[t].push_back(p);
      }

      first_obj[t] = bundles[t][0];
      stage[t].store(1);

      while(!freed.load())
        std::this_thread::yield();

      // The slow-path allocation drains the inbox: the filled slab's
      // objects come home, its live count reaches zero, the slab is
      // released, and its unit serves a different size class at the same
      // address.
      char* recarved = (char*)ponyint_pool_alloc_size(16 * 1024);
      ASSERT_EQ(recarved, first_obj[t]);

      ponyint_pool_free_size(16 * 1024, recarved);
      ponyint_pool_free(1, bundles[t][cap]);
    });
  }

  for(int t = 0; t < owners; t++)
  {
    while(stage[t].load() != 1)
      std::this_thread::yield();
  }

  // One thread frees every owner's filled slab, round-robin across
  // owners so each per-owner chain grows in interleaved order, verifying
  // the stamps first.
  std::thread([&]{
    for(size_t i = 0; i < cap; i++)
    {
      for(int t = 0; t < owners; t++)
      {
        char* p = bundles[t][i];
        ASSERT_EQ((uint8_t)p[0], (uint8_t)(0x11 * (t + 1)));
        ASSERT_EQ((uint8_t)p[63], (uint8_t)(0x11 * (t + 1)));
        ponyint_pool_free(1, p);
      }
    }

    ponyint_pool_thread_cleanup();
  }).join();

  freed.store(true);

  for(int t = 0; t < owners; t++)
    owner_threads[t].join();
}

extern "C" uint32_t ponyint_pool_arena_owner_slots_for_test();

// The owner registry grows a segment at a time as threads take slots,
// and resolving a slot's inbox walks the segment links to reach it. Two
// owners whose slots differ by exactly one segment must get distinct
// inboxes: a walk that resolves within the wrong segment lands both
// owners on one inbox, and whichever drains first exchanges runs
// addressed to the other, which the crediting checks refuse. Delivery
// to both owners proves the walk and the append.
TEST(PoolArena, OwnerRegistryGrowth)
{
  static const size_t cap = 16 * 1024 / 32;

  struct Owner
  {
    std::vector<char*> objs;
    std::atomic<int> stage;
    std::atomic<bool> freed;
    std::thread thread;
  };

  Owner owners[2];
  uint32_t before = ponyint_pool_arena_owner_slots_for_test();

  auto spawn = [&](Owner& o)
  {
    o.stage.store(0);
    o.freed.store(false);
    o.thread = std::thread([&o]{
      // Fill slab A exactly, plus one to move the current slab off it.
      for(size_t i = 0; i < (cap + 1); i++)
        o.objs.push_back((char*)ponyint_pool_alloc(0));

      o.stage.store(1);

      while(!o.freed.load())
        std::this_thread::yield();

      // The drain credits the foreign frees: slab A empties, releases,
      // and its unit is re-carved for another class at the same
      // address.
      char* recarved = (char*)ponyint_pool_alloc_size(16 * 1024);
      ASSERT_EQ(recarved, o.objs[0]);

      ponyint_pool_free_size(16 * 1024, recarved);
      ponyint_pool_free(0, o.objs[cap]);
    });

    while(o.stage.load() != 1)
      std::this_thread::yield();
  };

  spawn(owners[0]);

  // Slots are taken at a thread's first arena and never reused, so each
  // filler advances the count by one and the second owner's slot lands
  // exactly one segment past the first owner's.
  for(size_t i = 0; i < (TEST_SEGMENT_SLOTS - 1); i++)
  {
    std::thread([]{
      void* p = ponyint_pool_alloc(0);
      ponyint_pool_free(0, p);
    }).join();
  }

  spawn(owners[1]);

  ASSERT_EQ(ponyint_pool_arena_owner_slots_for_test(),
    before + TEST_SEGMENT_SLOTS + 1);

  // One thread frees both owners' slabs, interleaved; its resolution of
  // the second owner's inbox crosses the segment link.
  std::thread([&]{
    for(size_t i = 0; i < cap; i++)
    {
      ponyint_pool_free(0, owners[0].objs[i]);
      ponyint_pool_free(0, owners[1].objs[i]);
    }

    ponyint_pool_thread_cleanup();
  }).join();

  for(int t = 0; t < 2; t++)
  {
    owners[t].freed.store(true);
    owners[t].thread.join();
  }
}

// A freeing thread's per-owner chains live in an open-addressing map
// that doubles when it fills; the doubling rehash must carry every live
// chain with it. One thread frees to more owners than the initial map
// admits, forcing a grow with chains in hand, then every owner proves
// its object came home.
TEST(PoolArena, ChainMapGrowth)
{
  // Past half the initial capacity, where the map doubles mid-stream.
  static const int owners = (TEST_CHAIN_MAP_INITIAL / 2) + 12;

  std::vector<char*> objs(owners, NULL);
  std::vector<std::thread> owner_threads(owners);
  std::vector<std::atomic<int>> stage(owners);
  std::atomic<bool> freed(false);

  for(int t = 0; t < owners; t++)
    stage[t].store(0);

  for(int t = 0; t < owners; t++)
  {
    owner_threads[t] = std::thread([&, t]{
      // The first allocation on a fresh thread sits at its slab's base.
      char* obj = (char*)ponyint_pool_alloc(0);
      objs[t] = obj;
      stage[t].store(1);

      while(!freed.load())
        std::this_thread::yield();

      // The block allocation drains the inbox. The credited object
      // empties the slab, the slab resets in place, and the next bump
      // allocation hands the base address out again; an object lost in
      // the rehash leaves the slab occupied and the addresses differ.
      void* blk = ponyint_pool_alloc_size(16 * 1024);
      ponyint_pool_free_size(16 * 1024, blk);

      char* again = (char*)ponyint_pool_alloc(0);
      ASSERT_EQ(again, obj);
      ponyint_pool_free(0, again);
    });
  }

  for(int t = 0; t < owners; t++)
  {
    while(stage[t].load() != 1)
      std::this_thread::yield();
  }

  // A fresh thread's chain map is virgin, so the owner count alone
  // decides when it grows.
  std::thread([&]{
    for(int t = 0; t < owners; t++)
      ponyint_pool_free(0, objs[t]);

    ponyint_pool_thread_cleanup();
  }).join();

  freed.store(true);

  for(int t = 0; t < owners; t++)
    owner_threads[t].join();
}

// A thread takes its owner slot at its first arena, not its first pool
// call: a thread that only frees owns nothing another thread could
// address, so it never takes a slot.
TEST(PoolArena, FreeOnlyThreadTakesNoSlot)
{
  // Owned by this thread, so the spawned thread's free is foreign.
  void* p = ponyint_pool_alloc(0);

  uint32_t before = ponyint_pool_arena_owner_slots_for_test();

  std::thread([&]{
    ponyint_pool_free(0, p);
    ponyint_pool_thread_cleanup();
  }).join();

  ASSERT_EQ(ponyint_pool_arena_owner_slots_for_test(), before);

  std::thread([]{
    void* q = ponyint_pool_alloc(0);
    ponyint_pool_free(0, q);
  }).join();

  ASSERT_EQ(ponyint_pool_arena_owner_slots_for_test(), before + 1);
}

extern "C" bool ponyint_pool_arena_inbox_empty_for_test();

// A foreign free waits in the owner's inbox until the owner drains it;
// nothing notifies the owner. suspend_flush is the producer side — it
// delivers this thread's pending chains — and drain is the owner side,
// taking the whole inbox back. The producer holds a lone free below the
// batch threshold, so only its suspend_flush delivers it; the owner
// observes the delivery waiting and reclaims it with drain, emptying
// the slab and re-serving the same address. The producer parks after
// the flush so its thread_cleanup cannot be the delivery under test.
TEST(PoolArena, DrainReclaimsDeliveredForeignFree)
{
  std::atomic<int> stage(0);
  char* obj = NULL;
  bool inbox_filled = false;
  bool inbox_emptied = false;
  char* again = NULL;

  std::thread owner([&]{
    obj = (char*)ponyint_pool_alloc(0);
    stage.store(1);

    while(stage.load() != 2) // the producer delivered via suspend_flush
      std::this_thread::yield();

    inbox_filled = !ponyint_pool_arena_inbox_empty_for_test();
    ponyint_pool_drain();
    inbox_emptied = ponyint_pool_arena_inbox_empty_for_test();

    // The freed object was the slab's only live one, so the drain
    // emptied and reset the slab; the next allocation returns the same
    // address.
    again = (char*)ponyint_pool_alloc(0);
    ponyint_pool_free(0, again);
    stage.store(3);
  });

  while(stage.load() != 1)
    std::this_thread::yield();

  // One foreign free sits on the producer's chain, under the batch
  // threshold, so only suspend_flush hands it to the owner.
  std::thread producer([&]{
    ponyint_pool_free(0, obj);
    ponyint_pool_suspend_flush();
    stage.store(2);

    while(stage.load() != 3) // hold the chain map until the owner drained
      std::this_thread::yield();

    ponyint_pool_thread_cleanup();
  });

  owner.join();
  producer.join();

  ASSERT_TRUE(inbox_filled);
  ASSERT_TRUE(inbox_emptied);
  ASSERT_EQ(again, obj);
}

// A slot outlives its thread: a foreign free of an exited owner's
// object pushes to an inbox nothing will ever drain, and that must be
// safe — the segment holding it is never freed or moved. The freeing
// thread takes no slot, and the allocator keeps serving.
TEST(PoolArena, FreeToExitedOwner)
{
  char* obj = NULL;

  std::thread([&]{
    obj = (char*)ponyint_pool_alloc(0);
  }).join();

  uint32_t before = ponyint_pool_arena_owner_slots_for_test();

  std::thread([&]{
    ponyint_pool_free(0, obj);
    ponyint_pool_thread_cleanup();
  }).join();

  ASSERT_EQ(ponyint_pool_arena_owner_slots_for_test(), before);

  void* p = ponyint_pool_alloc(0);
  ASSERT_NE(p, (void*)NULL);
  ponyint_pool_free(0, p);
}

// Slot assignment racing across a segment boundary: the append is one
// compare-and-swap, the losers adopt the winner's segment, and every
// owner's inbox must end up where a foreign freeing thread's walk finds
// it. Delivery to all racers proves no owner cached an inbox in a
// segment the list does not hold.
TEST(PoolArena, OwnerRegistryAppendRace)
{
  static const int racers = 16;

  // Park slot assignment just short of a boundary so the racers cross
  // it together and contend on the append.
  uint32_t count = ponyint_pool_arena_owner_slots_for_test();
  size_t fillers = (TEST_SEGMENT_SLOTS
    - (count % TEST_SEGMENT_SLOTS) + TEST_SEGMENT_SLOTS - (racers / 2))
    % TEST_SEGMENT_SLOTS;

  for(size_t i = 0; i < fillers; i++)
  {
    std::thread([]{
      void* p = ponyint_pool_alloc(0);
      ponyint_pool_free(0, p);
    }).join();
  }

  struct Racer
  {
    char* obj;
    std::atomic<int> stage;
    std::atomic<bool> freed;
    std::thread thread;
  };

  std::vector<Racer> r(racers);
  std::atomic<int> ready(0);
  std::atomic<bool> go(false);

  for(int i = 0; i < racers; i++)
  {
    r[i].obj = NULL;
    r[i].stage.store(0);
    r[i].freed.store(false);
  }

  for(int i = 0; i < racers; i++)
  {
    r[i].thread = std::thread([&, i]{
      ready.fetch_add(1);

      while(!go.load())
        std::this_thread::yield();

      // Takes a slot; the racers around the boundary contend to append
      // the segment.
      r[i].obj = (char*)ponyint_pool_alloc(0);
      r[i].stage.store(1);

      while(!r[i].freed.load())
        std::this_thread::yield();

      // The block allocation drains: the credited object empties the
      // slab, it resets in place, and the base address comes back.
      void* blk = ponyint_pool_alloc_size(16 * 1024);
      ponyint_pool_free_size(16 * 1024, blk);

      char* again = (char*)ponyint_pool_alloc(0);
      EXPECT_EQ(again, r[i].obj);
      ponyint_pool_free(0, again);
    });
  }

  while(ready.load() != racers)
    std::this_thread::yield();

  go.store(true);

  for(int i = 0; i < racers; i++)
  {
    while(r[i].stage.load() != 1)
      std::this_thread::yield();
  }

  // One foreign thread frees every racer's object; its inbox resolution
  // walks the same list the owners populated.
  std::thread([&]{
    for(int i = 0; i < racers; i++)
      ponyint_pool_free(0, r[i].obj);

    ponyint_pool_thread_cleanup();
  }).join();

  for(int i = 0; i < racers; i++)
  {
    r[i].freed.store(true);
    r[i].thread.join();
  }
}

// One owner's objects spread across several slabs, freed by one foreign
// thread in single batches: the batch sorts into multiple runs, and the
// owner's drain credits every one of them. Each slab must empty, shown by
// its unit being re-carved at the same address.
TEST(PoolArena, CrossThreadMultiSlabBatch)
{
  on_fresh_thread([]{
    static const size_t cap = 16 * 1024 / 32; // objects per class-0 slab
    static const size_t slabs = 3;

    // Fill three class-0 slabs exactly, plus one object to move the
    // current slab off the last of them.
    std::vector<char*> objs;

    for(size_t i = 0; i < ((slabs * cap) + 1); i++)
      objs.push_back((char*)ponyint_pool_alloc(0));

    char* bases[slabs];

    for(size_t s = 0; s < slabs; s++)
      bases[s] = objs[s * cap];

    // The freeing thread interleaves objects of all three slabs, so every
    // batch of 32 sorts into three runs for the same owner.
    std::thread([&]{
      for(size_t i = 0; i < cap; i++)
      {
        for(size_t s = 0; s < slabs; s++)
          ponyint_pool_free(0, objs[(s * cap) + i]);
      }

      ponyint_pool_thread_cleanup();
    }).join();

    // The drain credits all three slabs empty; their units are the lowest
    // free units, re-carved in order for a different class.
    for(size_t s = 0; s < slabs; s++)
    {
      char* recarved = (char*)ponyint_pool_alloc_size(16 * 1024);
      ASSERT_EQ(recarved, bases[s]) << "slab " << s;
    }

    for(size_t s = 0; s < slabs; s++)
      ponyint_pool_free_size(16 * 1024, bases[s]);

    ponyint_pool_free(0, objs[slabs * cap]);
  });
}

// A block too large for an arena freed on another thread: oversized
// mappings share no bookkeeping, so the foreign thread returns the
// mapping directly and a fresh allocation of the same size still works.
TEST(PoolArena, CrossThreadOversizedFree)
{
#ifdef PLATFORM_IS_ILP32
  static const size_t size = 24 * 1024 * 1024;
#else
  static const size_t size = 192 * 1024 * 1024;
#endif

  char* p = (char*)ponyint_pool_alloc_size(size);
  ASSERT_NE(p, (char*)NULL);
  p[0] = 'q';
  p[size - 1] = 'r';

  std::thread([p]{
    ponyint_pool_free_size(size, p);
    ponyint_pool_thread_cleanup();
  }).join();

#ifdef PLATFORM_IS_LINUX
  // The foreign thread's free must have returned the mapping itself, not
  // merely routed the object somewhere: the address is no longer mapped.
  ASSERT_FALSE(maps_covers(p));
#endif

  char* q = (char*)ponyint_pool_alloc_size(size);
  ASSERT_NE(q, (char*)NULL);
  q[0] = 's';
  ponyint_pool_free_size(size, q);
}

// One empty arena stays in reserve with only its payload pages dropped;
// a second empty arena is released whole, its slot cleared and every
// page — header included — given back, while its address space stays
// parked in the region. Address equality alone cannot tell the two
// apart — a released slot is re-carved at the same address — so on
// Linux the test reads /proc/self/pagemap to see which pages each arena
// still holds.
TEST(PoolArena, EmptyArenaReserve)
{
  on_fresh_thread([]{
    static const size_t big = TEST_ARENA_FILLING_BLOCK;

    // Two blocks that cannot share one arena, in two arenas; freeing both
    // empties both. b1's arena empties first, so it is the one kept; b2's
    // arena, the second to empty, is released.
    char* b1 = (char*)ponyint_pool_alloc_size(big);
    char* b2 = (char*)ponyint_pool_alloc_size(big);
    ASSERT_NE(b1, (char*)NULL);
    ASSERT_NE(b2, (char*)NULL);
    memset(b1, 0x11, 4096);
    memset(b2, 0x22, 4096);

#ifdef PLATFORM_IS_LINUX
    void* arena1 = (void*)arena_base_of(b1);
    void* arena2 = (void*)arena_base_of(b2);

    // Touched pages are resident before the frees, so the zero counts
    // below show a return, not a block that never held pages. The
    // SIZE_MAX check keeps an unreadable pagemap from passing anything.
    size_t r1 = resident_bytes(b1, big);
    ASSERT_NE(r1, (size_t)SIZE_MAX);
    ASSERT_GT(r1, (size_t)0);
    ASSERT_GT(resident_bytes(b2, big), (size_t)0);

    ponyint_pool_free_size(big, b1);
    // Kept in reserve: the block's pages went back, the header's stayed.
    ASSERT_EQ(resident_bytes(b1, big), (size_t)0);
    ASSERT_GT(resident_bytes(arena1, TEST_UNIT_SIZE), (size_t)0);

    ponyint_pool_free_size(big, b2);
    // Released: every page went back, and the reserve survived.
    ASSERT_EQ(resident_bytes(arena2, TEST_ARENA_SIZE), (size_t)0);
    ASSERT_GT(resident_bytes(arena1, TEST_UNIT_SIZE), (size_t)0);
    // The released arena's address space is parked, not unmapped.
    ASSERT_TRUE(maps_covers(b2));
#else
    ponyint_pool_free_size(big, b1);
    ponyint_pool_free_size(big, b2);
#endif

    char* b3 = (char*)ponyint_pool_alloc_size(big);
    ASSERT_EQ(b3, b1);
    b3[0] = 'k';
    b3[big - 1] = 'l';
    ponyint_pool_free_size(big, b3);
  });
}

namespace
{

uintptr_t region_base_of(void* p)
{
  return (uintptr_t)p & ~(TEST_REGION_SIZE - 1);
}

} // namespace

// Fills arenas with one-per-arena blocks until some region holds the
// complete slot set {1..REGION_ARENAS-1} of the caller's blocks: a
// region proven to have had every slot free, whether fresh from the
// operating system or fully parked. Prior tests leave partially-free
// regions, and those can never accumulate the complete set. Appends
// every allocated block to blocks, records the proven region's blocks
// by arena base in by_arena, and returns the region's base, or 0 if
// the bound runs out.
uintptr_t fill_whole_region(std::vector<char*>& blocks,
  std::map<uintptr_t, char*>& by_arena)
{
  static const size_t big = TEST_ARENA_FILLING_BLOCK;
  static const size_t usable = (TEST_REGION_SIZE / TEST_ARENA_SIZE) - 1;

  std::map<uintptr_t, std::map<uintptr_t, char*>> per_region;

  for(int i = 0; i < 2048; i++)
  {
    char* p = (char*)ponyint_pool_alloc_size(big);

    if(p == NULL)
      return 0;

    blocks.push_back(p);

    std::map<uintptr_t, char*>& mine = per_region[region_base_of(p)];
    mine[arena_base_of(p)] = p;

    if(mine.size() == usable)
    {
      uintptr_t region = region_base_of(p);

      // The complete set is exactly slots 1..usable: the header's slot
      // is never carved, and no arena strays outside its slot grid.
      for(size_t s = 1; s <= usable; s++)
      {
        if(mine.find(region + (s * TEST_ARENA_SIZE)) == mine.end())
          return 0;
      }

      by_arena = mine;
      return region;
    }
  }

  return 0;
}

// A region hands out one arena per slot minus the header's, and the
// arena past the last slot comes from another region.
TEST(PoolArena, CrossRegionCarve)
{
  on_fresh_thread([]{
    static const size_t big = TEST_ARENA_FILLING_BLOCK;

    std::vector<char*> blocks;
    std::map<uintptr_t, char*> by_arena;
    uintptr_t full = fill_whole_region(blocks, by_arena);
    ASSERT_NE(full, (uintptr_t)0) << "no region filled within bound";

    // The region is full: the next arena lives somewhere else.
    char* over = (char*)ponyint_pool_alloc_size(big);
    ASSERT_NE(region_base_of(over), full);

    ponyint_pool_free_size(big, over);

    for(char* p : blocks)
      ponyint_pool_free_size(big, p);
  });
}

// Concurrent carving: every claim is one compare-and-swap on a shared
// region's slot bitmap, and no two threads may ever hold the same
// arena. All blocks stay live until the distinctness check is done, so
// a released slot legitimately reused by a racer is never mistaken for
// a double claim. Each thread stamps and verifies its blocks, so a
// slot handed out twice also shows up as a torn stamp.
TEST(PoolArena, RegionCarveRace)
{
  static const int threads = 8;
  static const int per_thread = 8;
  static const size_t big = TEST_ARENA_FILLING_BLOCK;

  std::vector<std::thread> carvers(threads);
  std::vector<std::vector<char*>> blocks(threads);
  std::atomic<int> ready(0);
  std::atomic<bool> go(false);
  std::atomic<int> done(0);
  std::atomic<bool> checked(false);

  for(int t = 0; t < threads; t++)
  {
    carvers[t] = std::thread([&, t]{
      ready.fetch_add(1);

      while(!go.load())
        std::this_thread::yield();

      // EXPECT rather than ASSERT throughout the thread body: an ASSERT
      // returns early, the done counter never rises, and the test hangs
      // instead of failing.
      for(int i = 0; i < per_thread; i++)
      {
        char* p = (char*)ponyint_pool_alloc_size(big);
        EXPECT_NE(p, (char*)NULL);

        if(p != NULL)
        {
          memset(p, 0x50 + t, 256);
          blocks[t].push_back(p);
        }
      }

      for(char* p : blocks[t])
      {
        EXPECT_EQ((uint8_t)p[0], (uint8_t)(0x50 + t));
        EXPECT_EQ((uint8_t)p[255], (uint8_t)(0x50 + t));
      }

      done.fetch_add(1);

      while(!checked.load())
        std::this_thread::yield();

      // Freed by their owner: the arenas empty and release locally.
      for(char* p : blocks[t])
        ponyint_pool_free_size(big, p);
    });
  }

  while(ready.load() != threads)
    std::this_thread::yield();

  go.store(true);

  while(done.load() != threads)
    std::this_thread::yield();

  // Every arena claimed exactly once across all threads. The release
  // gate opens before the assertion so a failure still lets every
  // thread finish and join.
  std::set<uintptr_t> arenas;
  size_t held = 0;

  for(int t = 0; t < threads; t++)
  {
    for(char* p : blocks[t])
      arenas.insert(arena_base_of(p));

    held += blocks[t].size();
  }

  checked.store(true);

  for(int t = 0; t < threads; t++)
    carvers[t].join();

  ASSERT_EQ(held, (size_t)(threads * per_thread));
  ASSERT_EQ(arenas.size(), (size_t)(threads * per_thread));
}

// An emptied region's address space parks on the region list: its pages
// are gone — the parked span reports no resident memory — but the next
// demand re-carves the same slots at the same addresses, with no new
// mapping involved.
TEST(PoolArena, ParkedRegionRecarve)
{
  on_fresh_thread([]{
    static const size_t big = TEST_ARENA_FILLING_BLOCK;
    static const size_t usable = (TEST_REGION_SIZE / TEST_ARENA_SIZE) - 1;

    // The pin shares its arena with the first block — the block's span
    // fits beside one slab — so that arena never empties. The second
    // block's arena is the first to empty and becomes the thread's kept
    // reserve; every arena emptied after it releases its slot with all
    // pages dropped.
    void* pin = ponyint_pool_alloc(0);

    std::vector<char*> blocks;
    std::map<uintptr_t, char*> by_arena;
    uintptr_t full = fill_whole_region(blocks, by_arena);
    ASSERT_NE(full, (uintptr_t)0) << "no region filled within bound";

    uintptr_t pinned = arena_base_of(blocks[0]);
    ASSERT_EQ(pinned, arena_base_of(pin));
    uintptr_t kept = arena_base_of(blocks[1]);

    for(char* p : blocks)
      ponyint_pool_free_size(big, p);

#ifdef PLATFORM_IS_LINUX
    // An unreadable pagemap must fail here, not pass the zero-checks.
    ASSERT_NE(resident_bytes((void*)full, TEST_UNIT_SIZE),
      (size_t)SIZE_MAX);

    size_t parked_seen = 0;

    for(size_t s = 1; s <= usable; s++)
    {
      uintptr_t arena = full + (s * TEST_ARENA_SIZE);

      if(arena == pinned)
        continue; // the pin holds this arena live
      else if(arena == kept)
        ASSERT_GT(resident_bytes((void*)arena, TEST_UNIT_SIZE),
          (size_t)0); // still claimed; the header's pages stay
      else
      {
        ASSERT_EQ(resident_bytes((void*)arena, TEST_ARENA_SIZE),
          (size_t)0) << "slot " << s;

        if(parked_seen++ == 0)
          ASSERT_TRUE(maps_covers((void*)arena)); // parked, not unmapped
      }
    }

    ASSERT_GT(parked_seen, (size_t)0);
#endif

    // Re-carving retraces the frees: owned space first — the kept
    // reserve, then the span beside the pin — then the current region's
    // parked slots, lowest first. Every block comes back at its old
    // address.
    std::vector<char*> expect;
    expect.push_back(blocks[1]);
    expect.push_back(blocks[0]);

    for(size_t s = 1; s <= usable; s++)
    {
      uintptr_t arena = full + (s * TEST_ARENA_SIZE);

      if((arena != pinned) && (arena != kept))
        expect.push_back(by_arena[arena]);
    }

    for(size_t i = 0; i < expect.size(); i++)
    {
      char* p = (char*)ponyint_pool_alloc_size(big);
      ASSERT_EQ(p, expect[i]) << "recarve " << i;
    }

    for(char* p : expect)
      ponyint_pool_free_size(big, p);

    ponyint_pool_free(0, pin);
  });
}

// Parked slots live in regions behind the list head, and only the walk
// across region links reaches them. Fill one fresh region, then a
// second so the first sits behind the head; a slot released in the
// first must come back through the walk.
TEST(PoolArena, RegionListWalkReclaims)
{
  on_fresh_thread([]{
    static const size_t big = TEST_ARENA_FILLING_BLOCK;

    void* pin = ponyint_pool_alloc(0);

    std::vector<char*> blocks;
    std::map<uintptr_t, char*> by_arena_a;
    uintptr_t region_a = fill_whole_region(blocks, by_arena_a);
    ASSERT_NE(region_a, (uintptr_t)0) << "no region filled within bound";

    std::map<uintptr_t, char*> by_arena_b;
    uintptr_t region_b = fill_whole_region(blocks, by_arena_b);
    ASSERT_NE(region_b, (uintptr_t)0) << "no second region within bound";
    ASSERT_NE(region_b, region_a);

    // Two of region A's arenas, neither shared with the pin: the first
    // freed becomes the kept reserve, the second releases its slot.
    uintptr_t pinned = arena_base_of(pin);
    char* p1 = NULL;
    char* p2 = NULL;

    for(std::pair<const uintptr_t, char*>& e : by_arena_a)
    {
      if(e.first == pinned)
        continue;

      if(p1 == NULL)
        p1 = e.second;
      else if(p2 == NULL)
      {
        p2 = e.second;
        break;
      }
    }

    ASSERT_NE(p2, (char*)NULL);
    ponyint_pool_free_size(big, p1);
    ponyint_pool_free_size(big, p2);

    // The kept reserve is the only owned space, so it serves first,
    // straight back at the first block's address.
    char* r1 = (char*)ponyint_pool_alloc_size(big);
    ASSERT_EQ(r1, p1);

    // The released slot sits in region A, behind region B on the list.
    // Leftover slots in regions between them may serve first, so soak
    // within a bound; the claim that lands at p2 walked the links past
    // the full head region.
    std::vector<char*> soak;
    char* found = NULL;

    for(int i = 0; (i < 128) && (found == NULL); i++)
    {
      char* q = (char*)ponyint_pool_alloc_size(big);

      if(q == p2)
        found = q;
      else
        soak.push_back(q);
    }

    ASSERT_EQ(found, p2);

    ponyint_pool_free_size(big, found);
    ponyint_pool_free_size(big, r1);

    for(char* q : soak)
      ponyint_pool_free_size(big, q);

    for(char* p : blocks)
    {
      if((p != p1) && (p != p2))
        ponyint_pool_free_size(big, p);
    }

    ponyint_pool_free(0, pin);
  });
}

// The lock-free protocols under real contention. The single-purpose
// tests above separate their phases, so a producer never flushes while
// its owner drains, no inbox ever has two producers at once, and no
// thread carves while another releases. This churn overlaps all three,
// with content canaries on every handoff; it is also what puts those
// interleavings in front of the thread sanitizer.
TEST(PoolArena, ConcurrentChurnStress)
{
  static const int nthreads = 4;
  static const int rounds = 200;
  static const size_t big = TEST_ARENA_FILLING_BLOCK;

  std::atomic<char*> mailbox[nthreads][nthreads]; // [to][from]
  std::atomic<int> received(0);
  std::atomic<int> senders_done(0);

  for(int i = 0; i < nthreads; i++)
  {
    for(int j = 0; j < nthreads; j++)
      mailbox[i][j].store(NULL);
  }

  auto drain_row = [&](int me)
  {
    for(int j = 0; j < nthreads; j++)
    {
      if(j == me)
        continue;

      char* got = mailbox[me][j].exchange(NULL);

      if(got != NULL)
      {
        EXPECT_EQ((uint8_t)got[0], (uint8_t)got[63]);
        EXPECT_NE((uint8_t)got[0] & 0x80, 0);
        ponyint_pool_free_size(big, got);
        received.fetch_add(1);
      }
    }
  };

  std::vector<std::thread> workers(nthreads);

  for(int i = 0; i < nthreads; i++)
  {
    workers[i] = std::thread([&, i]{
      for(int r = 0; r < rounds; r++)
      {
        // Every block allocation drains this thread's inbox, so blocks
        // credited by the frees below come home mid-churn and their
        // arenas release while other threads are carving.
        char* b = (char*)ponyint_pool_alloc_size(big);
        memset(b, 0x80 | ((i * 31 + r) & 0x3F), 64);

        int to = (i + 1 + (r % (nthreads - 1))) % nthreads;
        char* expected = NULL;

        while(!mailbox[to][i].compare_exchange_weak(expected, b))
        {
          expected = NULL;
          drain_row(i); // keep consuming so the ring cannot deadlock
          std::this_thread::yield();
        }

        drain_row(i);
      }

      senders_done.fetch_add(1);

      // Every put precedes its sender's counter bump, so one sweep
      // after the counter tops out cannot miss a block.
      while(senders_done.load() != nthreads)
      {
        drain_row(i);
        std::this_thread::yield();
      }

      drain_row(i);
      ponyint_pool_thread_cleanup();
    });
  }

  for(int i = 0; i < nthreads; i++)
    workers[i].join();

  // Conservation: every block sent was received, verified, and freed
  // exactly once.
  ASSERT_EQ(received.load(), nthreads * rounds);
}

// The block-or-own-mapping decision sits where a block no longer fits
// in an arena. Probing a descending ladder across that edge exercises
// the decision on both sides, and both must be usable edge to edge.
// Which side a probe took is read from what its free does — an own
// mapping is returned to the operating system, an in-arena block's
// region stays parked — because that is the contract, and it holds
// whatever the geometry is. The classification reads /proc, so it
// runs on Linux only; elsewhere the ladder still proves both sides
// usable edge to edge.
TEST(PoolArena, BlockOversizedBoundary)
{
  on_fresh_thread([]{
    static const size_t unit = TEST_UNIT_SIZE;
    int oversized_seen = 0;
    int block_seen = 0;

    for(size_t k = 0; k <= 30; k++)
    {
      size_t s = TEST_ARENA_SIZE - (k * unit);
      char* p = (char*)ponyint_pool_alloc_size(s);
      ASSERT_NE(p, (char*)NULL);

      p[0] = 'x';
      p[s - 1] = 'y';
      ASSERT_EQ(p[0], 'x');
      ASSERT_EQ(p[s - 1], 'y');

      ponyint_pool_free_size(s, p);

#ifdef PLATFORM_IS_LINUX
      if(maps_covers(p))
        block_seen++;
      else
        oversized_seen++;
#endif
    }

#ifdef PLATFORM_IS_LINUX
    // The ladder must cross the edge: both paths taken.
    ASSERT_GT(oversized_seen, 0);
    ASSERT_GT(block_seen, 0);
#else
    (void)oversized_seen;
    (void)block_seen;
#endif
  });
}

// An arena's longest-free-run bound falls when a span search fails and
// must rise again when a release merges a long-enough run, or the arena
// is skipped forever for that span while holding the space. The second
// arena is filled solid so the recovered run in the first is the only
// place the final allocation can land.
TEST(PoolArena, SpanBoundRecovers)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  on_fresh_thread([]{
    // Asserts span-bound recovery from a checkerboard of slab frees; the cache
    // would swallow those frees, so route them straight to the slab path.
    ponyint_pool_arena_cache_disable_for_test();
    static const size_t obj = 16 * 1024;

    // Fill the first arena, discovering its usable unit count.
    std::vector<char*> a1;
    char* first = (char*)ponyint_pool_alloc_size(obj);
    ASSERT_NE(first, (char*)NULL);
    uintptr_t base1 = arena_base_of(first);
    a1.push_back(first);

    char* overflow = NULL;

    for(;;)
    {
      char* q = (char*)ponyint_pool_alloc_size(obj);

      if(arena_base_of(q) != base1)
      {
        overflow = q;
        break;
      }

      a1.push_back(q);
    }

    size_t usable = a1.size();

    // Fill the second arena solid first. Allocation prefers the newest
    // arena, so as long as this one has room, no span search ever reaches
    // the first arena and its bound cannot drop.
    uintptr_t base2 = arena_base_of(overflow);
    std::vector<char*> a2;

    for(size_t i = 0; i < (usable - 1); i++)
    {
      char* q = (char*)ponyint_pool_alloc_size(obj);
      ASSERT_EQ(arena_base_of(q), base2) << "arena 2 not full yet";
      a2.push_back(q);
    }

    // Checkerboard the first arena: no two free units adjacent.
    for(size_t i = 1; i < usable; i += 2)
      ponyint_pool_free_size(obj, a1[i]);

    // With the second arena solid, this span-2 search reaches the first
    // arena, scans the checkerboard, fails, and drops the bound; the
    // allocation opens a third arena.
    char* span2a = (char*)ponyint_pool_alloc_size(32 * 1024);
    uintptr_t base3 = arena_base_of(span2a);
    ASSERT_NE(base3, base1);
    ASSERT_NE(base3, base2);

    // Fill the third arena solid too, so the recovered run in the first
    // arena is the only span-2 space anywhere.
    std::vector<char*> a3;

    for(size_t i = 0; i < (usable - 2); i++)
    {
      char* q = (char*)ponyint_pool_alloc_size(obj);
      ASSERT_EQ(arena_base_of(q), base3) << "arena 3 not full yet";
      a3.push_back(q);
    }

    // Free one even-indexed object: its unit merges with the freed odd
    // neighbors into a run of at least two, which must raise the bound.
    ponyint_pool_free_size(obj, a1[10]);

    char* span2b = (char*)ponyint_pool_alloc_size(32 * 1024);
    ASSERT_EQ(arena_base_of(span2b), base1);

    // Everything down, so later tests start from a clean slate.
    ponyint_pool_free_size(32 * 1024, span2b);
    ponyint_pool_free_size(32 * 1024, span2a);
    ponyint_pool_free_size(obj, overflow);

    for(size_t i = 0; i < usable; i += 2)
    {
      if(i != 10)
        ponyint_pool_free_size(obj, a1[i]);
    }

    for(char* q : a2)
      ponyint_pool_free_size(obj, q);

    for(char* q : a3)
      ponyint_pool_free_size(obj, q);
  });
}

// The oversized path aborts on a request whose size arithmetic would
// wrap, the same loud stop as an unsatisfiable reservation. The guard is
// release-retained and, uniquely among the checks, reachable straight
// from the public interface.
TEST(PoolArenaDeath, OversizedRequestOverflow)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  EXPECT_DEATH({
    ponyint_pool_alloc_size(SIZE_MAX);
  }, "tried to reserve");
}

#ifdef PLATFORM_IS_LINUX
// When no region has a free slot and the operating system refuses a new
// reservation, the allocator must stop loudly, not return garbage. The
// death statement runs in a re-executed child with a fresh allocator,
// so capping the address space below one region reservation makes the
// first allocation abort.
TEST(PoolArenaDeath, RegionReservationExhaustion)
{
  SKIP_WITHOUT_ARENA_CHECKS();
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  EXPECT_DEATH({
    size_t vm_kb = 0;
    FILE* f = fopen("/proc/self/status", "r");
    char line[256];

    while(fgets(line, sizeof(line), f) != NULL)
    {
      if(sscanf(line, "VmSize: %zu kB", &vm_kb) == 1)
        break;
    }

    fclose(f);

    // Reserving a region transiently maps twice its size; 300 MiB of
    // headroom is under that on both geometries.
    struct rlimit rl;
    rl.rlim_cur = (vm_kb * 1024) + (300 * 1024 * 1024);
    rl.rlim_max = rl.rlim_cur;
    setrlimit(RLIMIT_AS, &rl);

    for(int i = 0; i < 4096; i++)
      ponyint_pool_alloc_size(TEST_ARENA_FILLING_BLOCK);
  }, "tried to reserve");
}
#endif

#endif
