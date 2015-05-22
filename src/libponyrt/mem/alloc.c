#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <platform.h>
#include <string.h>
#include <stdio.h>

#ifdef PLATFORM_IS_POSIX_BASED
#include <sys/mman.h>
#endif

void* virtual_alloc(size_t bytes)
{
  void* p;

#if defined(PLATFORM_IS_WINDOWS)
  p = VirtualAlloc(NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#elif defined(PLATFORM_IS_LINUX)
  p = pony_numa_alloc(bytes);
#elif defined(PLATFORM_IS_POSIX_BASED)
  p = mmap(0, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

  if(p == MAP_FAILED)
  {
    perror("out of memory: ");
    abort();
  }
#endif

  return p;
}

void virtual_free(void* p, size_t bytes)
{
#if defined(PLATFORM_IS_WINDOWS)
  VirtualFree(p, 0, MEM_RELEASE);
#elif defined(PLATFORM_IS_LINUX)
  pony_numa_free(p, bytes);
#elif defined(PLATFORM_IS_POSIX_BASED)
  munmap(p, bytes);
#endif
}
