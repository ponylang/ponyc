#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <platform.h>
#include <string.h>
#include <stdio.h>

#ifdef PLATFORM_IS_POSIX_BASED
#  include <sys/mman.h>
#ifdef USE_NUMA
#  include <numa.h>
#endif
#endif

void* virtual_alloc(size_t bytes)
{
  void* p;

#if defined(PLATFORM_IS_WINDOWS)
  p = VirtualAlloc(NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#elif defined(PLATFORM_IS_POSIX_BASED)
#if defined(USE_NUMA)
  p = numa_alloc(bytes);
#else
  p = mmap(0, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

  if(p == MAP_FAILED)
  {
    perror("out of memory: ");
    abort();
  }
#endif
#endif

  return p;
}

bool virtual_free(void* p, size_t bytes)
{
#if defined(PLATFORM_IS_WINDOWS)
  return VirtualFree(p, 0, MEM_RELEASE) != 0;
#elif defined(PLATFORM_IS_POSIX_BASED)
#if defined(USE_NUMA)
  numa_free(p, bytes);
  return true;
#else
  return munmap(p, bytes) == 0;
#endif
#endif
}
