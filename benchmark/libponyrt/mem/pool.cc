#include <benchmark/benchmark.h>
#include <platform.h>
#include <mem/pool.h>

#define LARGE_ALLOC 1 << 21

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
    ponyint_pool_index(st.range(0));
  }
  st.SetItemsProcessed(st.iterations());
}

BENCHMARK_REGISTER_F(PoolBench, pool_index)->RangeMultiplier(3)->Ranges({{1, 1024<<10}});

BENCHMARK_DEFINE_F(PoolBench, POOL_ALLOC)(benchmark::State& st) {
  while (st.KeepRunning()) {
    void* p = POOL_ALLOC(block_t);
    st.PauseTiming();
    POOL_FREE(block_t, p);
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations());
}

BENCHMARK_REGISTER_F(PoolBench, POOL_ALLOC);

BENCHMARK_DEFINE_F(PoolBench, POOL_ALLOC_multiple)(benchmark::State& st) {
  void* p[100];
  while (st.KeepRunning()) {
    st.PauseTiming();
    for(int i = 0; i < 100; i++)
    {
      st.ResumeTiming();
      p[i] = POOL_ALLOC(block_t);
      st.PauseTiming();
    }
    for(int i = 99; i >= 0; i--)
      POOL_FREE(block_t, p[i]);
  }
  st.SetItemsProcessed(st.iterations()*100);
}

BENCHMARK_REGISTER_F(PoolBench, POOL_ALLOC_multiple);

BENCHMARK_DEFINE_F(PoolBench, POOL_FREE)(benchmark::State& st) {
  while (st.KeepRunning()) {
    st.PauseTiming();
    void* p = POOL_ALLOC(block_t);
    st.ResumeTiming();
    POOL_FREE(block_t, p);
  }
  st.SetItemsProcessed(st.iterations());
}

BENCHMARK_REGISTER_F(PoolBench, POOL_FREE);

BENCHMARK_DEFINE_F(PoolBench, POOL_FREE_multiple)(benchmark::State& st) {
  void* p[100];
  while (st.KeepRunning()) {
    st.PauseTiming();
    for(int i = 0; i < 100; i++)
      p[i] = POOL_ALLOC(block_t);
    for(int i = 99; i >= 0; i--)
    {
      st.ResumeTiming();
      POOL_FREE(block_t, p[i]);
      st.PauseTiming();
    }
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations()*100);
}

BENCHMARK_REGISTER_F(PoolBench, POOL_FREE_multiple);

BENCHMARK_DEFINE_F(PoolBench, pool_alloc_size)(benchmark::State& st) {
  while (st.KeepRunning()) {
    void* p = ponyint_pool_alloc_size(LARGE_ALLOC);
    st.PauseTiming();
    ponyint_pool_free_size(LARGE_ALLOC, p);
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations());
}

BENCHMARK_REGISTER_F(PoolBench, pool_alloc_size);

BENCHMARK_DEFINE_F(PoolBench, pool_alloc_size_multiple)(benchmark::State& st) {
  void* p[100];
  while (st.KeepRunning()) {
    st.PauseTiming();
    for(int i = 0; i < 100; i++)
    {
      st.ResumeTiming();
      p[i] = ponyint_pool_alloc_size(LARGE_ALLOC);
      st.PauseTiming();
    }
    for(int i = 99; i >= 0; i--)
      ponyint_pool_free_size(LARGE_ALLOC, p[i]);
  }
  st.SetItemsProcessed(st.iterations()*100);
}

BENCHMARK_REGISTER_F(PoolBench, pool_alloc_size_multiple);

BENCHMARK_DEFINE_F(PoolBench, pool_free_size)(benchmark::State& st) {
  while (st.KeepRunning()) {
    st.PauseTiming();
    void* p = ponyint_pool_alloc_size(LARGE_ALLOC);
    st.ResumeTiming();
    ponyint_pool_free_size(LARGE_ALLOC, p);
  }
  st.SetItemsProcessed(st.iterations());
}

BENCHMARK_REGISTER_F(PoolBench, pool_free_size);

BENCHMARK_DEFINE_F(PoolBench, pool_free_size_multiple)(benchmark::State& st) {
  void* p[100];
  while (st.KeepRunning()) {
    st.PauseTiming();
    for(int i = 0; i < 100; i++)
      p[i] = ponyint_pool_alloc_size(LARGE_ALLOC);
    for(int i = 99; i >= 0; i--)
    {
      st.ResumeTiming();
      ponyint_pool_free_size(LARGE_ALLOC, p[i]);
      st.PauseTiming();
    }
    st.ResumeTiming();
  }
  st.SetItemsProcessed(st.iterations()*100);
}

BENCHMARK_REGISTER_F(PoolBench, pool_free_size_multiple);
