#include <benchmark/benchmark.h>

static void $$$$$$_PauseResumeOverheadOnce(benchmark::State& st) {
  while (st.KeepRunning()) {
    st.PauseTiming();
    st.ResumeTiming();
  }
  st.SetItemsProcessed((int64_t)st.iterations());
}

BENCHMARK($$$$$$_PauseResumeOverheadOnce);

static void $$$$$$_PauseResumeOverheadTwice(benchmark::State& st) {
  while (st.KeepRunning()) {
    st.PauseTiming();
    st.ResumeTiming();
    st.PauseTiming();
    st.ResumeTiming();
  }
  st.SetItemsProcessed((int64_t)st.iterations());
}

BENCHMARK($$$$$$_PauseResumeOverheadTwice);

static void $$$$$$_PauseResumeOverheadThrice(benchmark::State& st) {
  while (st.KeepRunning()) {
    st.PauseTiming();
    st.ResumeTiming();
    st.PauseTiming();
    st.ResumeTiming();
    st.PauseTiming();
    st.ResumeTiming();
  }
  st.SetItemsProcessed((int64_t)st.iterations());
}

BENCHMARK($$$$$$_PauseResumeOverheadThrice);
