#define _GNU_SOURCE
#include <platform.h>
#include <string.h>
#include <stdio.h>

#ifdef PLATFORM_IS_POSIX_BASED
#  include <sys/mman.h>
#endif

void* virtual_alloc(size_t bytes)
{
#if defined(PLATFORM_IS_WINDOWS)
  return VirtualAlloc(NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#elif defined(PLATFORM_IS_POSIX_BASED)
  void* p = mmap(0, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1,
    0);

  if(p == MAP_FAILED)
  {
    perror("out of memory: ");
    abort();
  }

  return p;
#endif
}

void* virtual_realloc(void* p, size_t old_bytes, size_t new_bytes)
{
#if defined(PLATFORM_IS_LINUX)
  void* q = mremap(p, old_bytes, new_bytes, MREMAP_MAYMOVE);

  if(q == MAP_FAILED)
  {
    perror("out of memory: ");
    abort();
  }

  return q;
#else
  void* q = virtual_alloc(new_bytes);
  memcpy(q, p, old_bytes);
  virtual_free(p, old_bytes);
  return q;
#endif
}

bool virtual_free(void* p, size_t bytes)
{
#if defined(PLATFORM_IS_WINDOWS)
  return VirtualFree(p, 0, MEM_RELEASE) != 0;
#elif defined(PLATFORM_IS_POSIX_BASED)
  return munmap(p, bytes) == 0;
#endif
}
