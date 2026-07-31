#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <benchmark/benchmark.h>
#include <platform.h>
#include <mem/pool.h>

int main(int argc, char** argv) {

#ifdef POOL_USE_ARENA
  // Select the arena memory profile from the environment, so one binary can be
  // measured under each rung of the --ponymemoryprofile dial. Accepts a rung
  // number 1-10, or the names low/balanced/throughput (rungs 1/3/10). Leaving
  // it unset keeps the default (rung 3). Only the arena backend has this call.
  const char* prof = getenv("PONY_MEMPROFILE");
  if(prof != NULL)
  {
    uint32_t rung;
    if(strcmp(prof, "low") == 0)
      rung = 1;
    else if(strcmp(prof, "balanced") == 0)
      rung = 3;
    else if(strcmp(prof, "throughput") == 0)
      rung = 10;
    else
      rung = (uint32_t)atoi(prof);
    ponyint_pool_set_memory_profile(rung);
  }
#endif

  ::benchmark::Initialize(&argc, argv);
  ::benchmark::RunSpecifiedBenchmarks();

  printf("\n");

  printf("*********\n");
  printf(">> NOTE: Benchmarks ending in `$` use PauseTiming/ResumeTiming.\n");
  printf(">> NOTE: Using PauseTiming/ResumeTiming adds time/overhead to benchmarks.\n");
  printf(">> NOTE: Overhead seems to be about 200ns on a modern intel processor.\n");
  printf(">> NOTE: See output of `$$$$$$_PauseResumeOverhead*` benchmarks for approximate\n");
  printf(">> NOTE: overhead on the benchmark machine.\n");
  printf(">> NOTE: See: https://github.com/google/benchmark/issues/179.\n");
  printf("*********\n");
  printf(">> NOTE: Benchmarks ending in `$_` use PauseTiming/ResumeTiming and\n");
  printf(">> NOTE: run the function multiple times and need to be divided by\n");
  printf(">> NOTE: the count (last argument) to get a value for a single call.\n");
  printf("*********\n");

}

