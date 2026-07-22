#include <benchmark/benchmark.h>
#include <platform.h>
#include <mem/pool.h>
#include <atomic>

#define LARGE_ALLOC ponyint_pool_adjust_size(POOL_MAX + 1)

/// When we mmap, pull at least this many bytes.
#ifdef PLATFORM_IS_ILP32
#define POOL_MMAP (16 * 1024 * 1024) // 16 MB
#else
#define POOL_MMAP (128 * 1024 * 1024) // 128 MB
#endif

typedef char block_t[32];

class PoolBench: public ::benchmark::Fixture
{ 
  protected:
    virtual void SetUp(const ::benchmark::State& st);
    virtual void TearDown(const ::benchmark::State& st);
};

void PoolBench::SetUp(const ::benchmark::State& st)
{
  (void)st;
}

void PoolBench::TearDown(const ::benchmark::State& st)
{
  (void)st;
}

BENCHMARK_DEFINE_F(PoolBench, pool_index)(benchmark::State& st) {
  while (st.KeepRunning()) {
    ponyint_pool_index(static_cast<size_t>(st.range(0)));
  }
  st.SetItemsProcessed((int64_t)st.iterations());
}

BENCHMARK_REGISTER_F(PoolBench, pool_index)->RangeMultiplier(3)->Ranges({{1, 1024<<10}});

BENCHMARK_DEFINE_F(PoolBench, POOL_ALLOC$)(benchmark::State& st) {
  while (st.KeepRunning()) {
    void* p = POOL_ALLOC(block_t);
    st.PauseTiming();
    POOL_FREE(block_t, p);
    st.ResumeTiming();
  }
  st.SetItemsProcessed((int64_t)st.iterations());
}

BENCHMARK_REGISTER_F(PoolBench, POOL_ALLOC$);

BENCHMARK_DEFINE_F(PoolBench, POOL_ALLOC_multiple$_)(benchmark::State& st) {
  size_t num_allocs = static_cast<size_t>(st.range(0));
  void** p = (void**)alloca(sizeof(void *) * num_allocs);
  while (st.KeepRunning()) {
    for(size_t i = 0; i < num_allocs; i++)
      p[i] = POOL_ALLOC(block_t);
    st.PauseTiming();
    for(size_t i = num_allocs; i > 0; i--)
      POOL_FREE(block_t, p[i-1]);
    st.ResumeTiming();
  }
  st.SetItemsProcessed((int64_t)(st.iterations() * num_allocs));
}

BENCHMARK_REGISTER_F(PoolBench, POOL_ALLOC_multiple$_)->Arg(1<<10);

BENCHMARK_DEFINE_F(PoolBench, POOL_FREE$)(benchmark::State& st) {
  while (st.KeepRunning()) {
    st.PauseTiming();
    void* p = POOL_ALLOC(block_t);
    st.ResumeTiming();
    POOL_FREE(block_t, p);
  }
  st.SetItemsProcessed((int64_t)st.iterations());
}

BENCHMARK_REGISTER_F(PoolBench, POOL_FREE$);

BENCHMARK_DEFINE_F(PoolBench, POOL_ALLOC_FREE)(benchmark::State& st) {
  while (st.KeepRunning()) {
    void* p = POOL_ALLOC(block_t);
    POOL_FREE(block_t, p);
  }
  st.SetItemsProcessed((int64_t)st.iterations());
}

BENCHMARK_REGISTER_F(PoolBench, POOL_ALLOC_FREE);

BENCHMARK_DEFINE_F(PoolBench, POOL_FREE_multiple$_)(benchmark::State& st) {
  size_t num_allocs = static_cast<size_t>(st.range(0));
  void** p = (void**)alloca(sizeof(void *) * num_allocs);
  while (st.KeepRunning()) {
    st.PauseTiming();
    for(size_t i = 0; i < num_allocs; i++)
      p[i] = POOL_ALLOC(block_t);
    st.ResumeTiming();
    for(size_t i = num_allocs; i > 0; i--)
      POOL_FREE(block_t, p[i-1]);
  }
  st.SetItemsProcessed((int64_t)(st.iterations() * num_allocs));
}

BENCHMARK_REGISTER_F(PoolBench, POOL_FREE_multiple$_)->Arg(1<<10);

BENCHMARK_DEFINE_F(PoolBench, POOL_ALLOC_FREE_multiple)(benchmark::State& st) {
  size_t num_allocs = static_cast<size_t>(st.range(0));
  void** p = (void**)alloca(sizeof(void *) * num_allocs);
  while (st.KeepRunning()) {
    for(size_t i = 0; i < num_allocs; i++)
      p[i] = POOL_ALLOC(block_t);
    for(size_t i = num_allocs; i > 0; i--)
      POOL_FREE(block_t, p[i-1]);
  }
  st.SetItemsProcessed((int64_t)(st.iterations() * num_allocs));
}

BENCHMARK_REGISTER_F(PoolBench, POOL_ALLOC_FREE_multiple)->Arg(1<<10);

BENCHMARK_DEFINE_F(PoolBench, pool_alloc_size$)(benchmark::State& st) {
  while (st.KeepRunning()) {
    void* p = ponyint_pool_alloc_size(LARGE_ALLOC);
    st.PauseTiming();
    ponyint_pool_free_size(LARGE_ALLOC, p);
    st.ResumeTiming();
  }
  st.SetItemsProcessed((int64_t)st.iterations());
}

BENCHMARK_REGISTER_F(PoolBench, pool_alloc_size$);

BENCHMARK_DEFINE_F(PoolBench, pool_alloc_size_multiple$_)(benchmark::State& st) {
  size_t num_allocs = static_cast<size_t>(st.range(0));
  void** p = (void**)alloca(sizeof(void *) * num_allocs);
  while (st.KeepRunning()) {
    for(size_t i = 0; i < num_allocs; i++)
      p[i] = ponyint_pool_alloc_size(LARGE_ALLOC);
    st.PauseTiming();
    for(size_t i = num_allocs; i > 0; i--)
      ponyint_pool_free_size(LARGE_ALLOC, p[i-1]);
    st.ResumeTiming();
  }
  st.SetItemsProcessed((int64_t)(st.iterations() * num_allocs));
}

BENCHMARK_REGISTER_F(PoolBench, pool_alloc_size_multiple$_)->Arg((int)(POOL_MMAP/LARGE_ALLOC));

BENCHMARK_DEFINE_F(PoolBench, pool_free_size$)(benchmark::State& st) {
  while (st.KeepRunning()) {
    st.PauseTiming();
    void* p = ponyint_pool_alloc_size(LARGE_ALLOC);
    st.ResumeTiming();
    ponyint_pool_free_size(LARGE_ALLOC, p);
  }
  st.SetItemsProcessed((int64_t)st.iterations());
}

BENCHMARK_REGISTER_F(PoolBench, pool_free_size$);

BENCHMARK_DEFINE_F(PoolBench, pool_alloc_free_size)(benchmark::State& st) {
  while (st.KeepRunning()) {
    void* p = ponyint_pool_alloc_size(LARGE_ALLOC);
    ponyint_pool_free_size(LARGE_ALLOC, p);
  }
  st.SetItemsProcessed((int64_t)st.iterations());
}

BENCHMARK_REGISTER_F(PoolBench, pool_alloc_free_size);

BENCHMARK_DEFINE_F(PoolBench, pool_free_size_multiple$_)(benchmark::State& st) {
  size_t num_allocs = static_cast<size_t>(st.range(0));
  void** p = (void**)alloca(sizeof(void *) * num_allocs);
  while (st.KeepRunning()) {
    st.PauseTiming();
    for(size_t i = 0; i < num_allocs; i++)
      p[i] = ponyint_pool_alloc_size(LARGE_ALLOC);
    st.ResumeTiming();
    for(size_t i = num_allocs; i > 0; i--)
      ponyint_pool_free_size(LARGE_ALLOC, p[i-1]);
  }
  st.SetItemsProcessed((int64_t)(st.iterations() * num_allocs));
}

BENCHMARK_REGISTER_F(PoolBench, pool_free_size_multiple$_)->Arg((int)(POOL_MMAP/LARGE_ALLOC));

BENCHMARK_DEFINE_F(PoolBench, pool_alloc_free_size_multiple)(benchmark::State& st) {
  size_t num_allocs = static_cast<size_t>(st.range(0));
  void** p = (void**)alloca(sizeof(void *) * num_allocs);
  while (st.KeepRunning()) {
    for(size_t i = 0; i < num_allocs; i++)
      p[i] = ponyint_pool_alloc_size(LARGE_ALLOC);
    for(size_t i = num_allocs; i > 0; i--)
      ponyint_pool_free_size(LARGE_ALLOC, p[i-1]);
  }
  st.SetItemsProcessed((int64_t)(st.iterations() * num_allocs));
}

BENCHMARK_REGISTER_F(PoolBench, pool_alloc_free_size_multiple)->Arg((int)(POOL_MMAP/LARGE_ALLOC));

// --- Arena throughput benchmarks (task #17) ---------------------------------
//
// size_sweep_churn: same-thread allocate-and-free throughput at a range of
// sizes. Each trial allocates a working set of two blocks at one size, frees
// them, and times both. The two blocks (rather than one) matter only at the
// sizes that reach the slab, where freeing one of the two empties a slab that
// is not the current one, so the allocator releases it instead of resetting it
// in place.
//
// Which path a size reaches depends on the per-class thread cache. The cache
// holds up to cache_cap(class) blocks of a class -- POOL_CACHE_BUDGET divided
// by the class size -- so its per-class room shrinks as the class grows. A
// same-thread free below that count fills the cache and skips the slab, and an
// alloc empties the cache before it touches the slab. So a working set of two
// never reaches the slab for any class whose cache_cap is two or more, which is
// every class of 128 KB and smaller; only larger sizes drive the slab.
BENCHMARK_DEFINE_F(PoolBench, size_sweep_churn)(benchmark::State& st) {
  size_t size = static_cast<size_t>(st.range(0));
  size_t ws = static_cast<size_t>(st.range(1));
  void** p = (void**)alloca(sizeof(void*) * ws);
  while (st.KeepRunning()) {
    for(size_t i = 0; i < ws; i++)
      p[i] = ponyint_pool_alloc_size(size);
    for(size_t i = ws; i > 0; i--)
      ponyint_pool_free_size(size, p[i-1]);
  }
  st.SetItemsProcessed((int64_t)(st.iterations() * ws));
}

// Each size drives a distinct path:
//   32 B, 128 KB  cache pop and push (cache_cap >= 2). 128 KB is the largest
//                 such class, so it sits next to 256 KB to show the step from
//                 the cache to the slab.
//   256 KB        cache_cap 1: one of the two frees overflows to the slab.
//   1 MB          cache_cap 0: every alloc and free reaches the slab, driving
//                 slab reserve and release.
//   2 MB, 4 MB    above POOL_MAX: the multi-unit block path within an arena.
//   16 MB         above one arena: its own mapping.
BENCHMARK_REGISTER_F(PoolBench, size_sweep_churn)
  ->Args({32, 2})->Args({131072, 2})->Args({262144, 2})->Args({1048576, 2})
  ->Args({2*1024*1024, 2})->Args({4*1024*1024, 2})->Args({16*1024*1024, 2});

// size_sweep_depth: how allocate-and-free throughput scales with the number of
// blocks held outstanding at once, at a fixed size. Each trial allocates
// `depth` blocks of one size, holds them all live, then frees them. The per-
// class thread cache holds only cache_cap blocks, so it absorbs the first
// cache_cap of the batch and the rest reach the slab. For a capacity-1 class
// (16 KB and up, one object per slab) there is no partial-slab list to reuse a
// held slab, so past the cache every alloc drives a slab reserve and every free
// a slab release -- the work the classic pool does with a free-list push and
// pop. The 16 KB rows ramp the depth to show the crossover as the cache is
// outrun; 256 KB and 1 MB (cache_cap 1 and 0) reach the slab almost at once.
// The 32 B row is the control: a class with many objects per slab bump-
// allocates within one slab and stays cheap at any depth.
BENCHMARK_DEFINE_F(PoolBench, size_sweep_depth)(benchmark::State& st) {
  size_t size = static_cast<size_t>(st.range(0));
  size_t depth = static_cast<size_t>(st.range(1));
  void** p = (void**)malloc(sizeof(void*) * depth);
  while (st.KeepRunning()) {
    for(size_t i = 0; i < depth; i++)
      p[i] = ponyint_pool_alloc_size(size);
    for(size_t i = depth; i > 0; i--)
      ponyint_pool_free_size(size, p[i-1]);
  }
  st.SetItemsProcessed((int64_t)(st.iterations() * depth));
  free(p);
}
BENCHMARK_REGISTER_F(PoolBench, size_sweep_depth)
  ->Args({32, 1024})
  ->Args({16384, 16})->Args({16384, 48})->Args({16384, 64})->Args({16384, 96})
  ->Args({262144, 1})->Args({262144, 4})->Args({262144, 16})->Args({262144, 64})
  ->Args({524288, 1})->Args({524288, 2})->Args({524288, 8})
  ->Args({1048576, 1})->Args({1048576, 2})->Args({1048576, 8})->Args({1048576, 64});

// Hold-then-free churn of large same-thread blocks: the workload the memory-
// profile tuning is measured against. A persistent base of `count` live blocks
// keeps the arenas partly occupied, so the churned frees land on the dirty-
// allotment / immediate-decommit path the thresholds govern, not the arena-
// empty release path, which decommits regardless. Each timed cycle allocates
// `count` more blocks, faults every page so they are resident, holds them, then
// frees them. How fast the allocator returns those pages is what the memory
// profile controls: return them and the next cycle refaults the whole set
// (slow, low resident); hold them and the next cycle reuses them (fast, high
// resident). Args are {block size, churn count}; the base equals the churn.
BENCHMARK_DEFINE_F(PoolBench, hold_then_free)(benchmark::State& st) {
  size_t size = static_cast<size_t>(st.range(0));
  size_t count = static_cast<size_t>(st.range(1));
  const size_t page = 4096;
  // Persistent base: allocated once, freed after the loop, never in between.
  void** base = (void**)malloc(sizeof(void*) * count);
  for(size_t i = 0; i < count; i++) {
    char* b = (char*)ponyint_pool_alloc_size(size);
    for(size_t off = 0; off < size; off += page)
      b[off] = (char)1;
    base[i] = b;
  }
  void** p = (void**)malloc(sizeof(void*) * count);
  while (st.KeepRunning()) {
    for(size_t i = 0; i < count; i++) {
      char* b = (char*)ponyint_pool_alloc_size(size);
      for(size_t off = 0; off < size; off += page)
        b[off] = (char)1;
      p[i] = b;
    }
    for(size_t i = count; i > 0; i--)
      ponyint_pool_free_size(size, p[i-1]);
  }
  for(size_t i = 0; i < count; i++)
    ponyint_pool_free_size(size, base[i]);
  st.SetItemsProcessed((int64_t)(st.iterations() * count));
  free(p);
  free(base);
}
BENCHMARK_REGISTER_F(PoolBench, hold_then_free)->UseRealTime()
  ->Args({131072, 128})->Args({262144, 64})->Args({1048576, 16});

// A single-producer/single-consumer ring, one per direction of the exchange
// below. Push fails when full so the producer never blocks: with google-
// benchmark handing both threads equal iteration counts but no per-iteration
// barrier, a blocking queue would let the faster thread stop draining and the
// slower one stall on a full queue at end-of-trial, hanging the join. On a
// full ring the caller frees to its own thread instead. N is a power of two.
template <size_t N>
struct SpscRing {
  std::atomic<size_t> head{0};
  std::atomic<size_t> tail{0};
  void* slots[N];
  bool push(void* p) {
    size_t h = head.load(std::memory_order_relaxed);
    size_t n = (h + 1) & (N - 1);
    if(n == tail.load(std::memory_order_acquire)) return false;
    slots[h] = p;
    head.store(n, std::memory_order_release);
    return true;
  }
  bool pop(void** out) {
    size_t t = tail.load(std::memory_order_relaxed);
    if(t == head.load(std::memory_order_acquire)) return false;
    *out = slots[t];
    tail.store((t + 1) & (N - 1), std::memory_order_release);
    return true;
  }
};

// g_xthread[t] holds blocks thread t allocated for the other thread to free.
static SpscRing<4096> g_xthread[2];

// Cross-thread free, symmetric two-thread exchange. Each thread allocates its
// own blocks and frees the other's, so the arena's cross-thread costs all land
// in the timed aggregate on both sides: chain_push per free, chain_flush per
// batch, and the owner-side inbox_drain + apply_run on the next alloc. Classic
// has no distinct cross-thread path (it keeps freed memory on the freer's own
// list), so this compares the two strategies on the same workload rather than
// one path against another. Fixed 32 B: cross-thread frees in Pony are message
// objects, which are small. A fixed size also keeps residual blocks in the
// rings a single size, so the reset below can free them without tracking it.
BENCHMARK_DEFINE_F(PoolBench, cross_thread_free)(benchmark::State& st) {
  const size_t size = 32;
  int me = st.thread_index();
  int other = me ^ 1;
  // Thread 0 clears residuals from the previous trial before the timed loop.
  // google-benchmark's start-of-loop barrier parks thread 1 until thread 0
  // reaches it, so this runs without a concurrent reader.
  if(me == 0) {
    for(int r = 0; r < 2; r++) {
      void* p;
      while(g_xthread[r].pop(&p)) ponyint_pool_free_size(size, p);
      g_xthread[r].head.store(0, std::memory_order_relaxed);
      g_xthread[r].tail.store(0, std::memory_order_relaxed);
    }
  }
  while(st.KeepRunning()) {
    void* q;
    if(g_xthread[other].pop(&q)) ponyint_pool_free_size(size, q);
    void* p = ponyint_pool_alloc_size(size);
    if(!g_xthread[me].push(p)) ponyint_pool_free_size(size, p);
  }
  st.SetItemsProcessed((int64_t)st.iterations());
}
BENCHMARK_REGISTER_F(PoolBench, cross_thread_free)->Threads(2)->UseRealTime();

