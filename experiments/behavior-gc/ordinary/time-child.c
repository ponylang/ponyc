#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int64_t nanos(struct timespec t)
{
  return t.tv_sec * INT64_C(1000000000) + t.tv_nsec;
}

int main(int argc, char** argv)
{
  if(argc < 2) return 2;
  struct timespec start, end;
  if(clock_gettime(CLOCK_MONOTONIC, &start) != 0) return 2;
  pid_t child = fork();
  if(child < 0) return 2;
  if(child == 0)
  {
    execv(argv[1], argv + 1);
    _exit(127);
  }
  int status;
  struct rusage usage;
  if(wait4(child, &status, 0, &usage) != child ||
    clock_gettime(CLOCK_MONOTONIC, &end) != 0)
    return 2;
  if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) return 1;
  fprintf(stderr, "%lld %ld\n", (long long)(nanos(end) - nanos(start)),
    usage.ru_maxrss);
  return 0;
}
