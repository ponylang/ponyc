#include <stdio.h>

#include <benchmark/benchmark.h>

int main(int argc, char** argv) {

  ::benchmark::Initialize(&argc, argv);
  ::benchmark::RunSpecifiedBenchmarks();

  printf("\n");

  printf("********* benchmarkX\n");
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

