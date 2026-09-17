#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <platform.h>
#include <string.h>
#include <stdio.h>
#include "alloc.h"
#include "ponyassert.h"

#ifdef PLATFORM_IS_POSIX_BASED
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(PLATFORM_IS_MACOSX)
#include <mach/vm_statistics.h>
#endif

void* ponyint_virt_alloc(size_t bytes)
{
  void* p;
  bool ok = true;

#if defined(PLATFORM_IS_WINDOWS)
  p = VirtualAlloc(NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if(p == NULL)
    ok = false;
#elif defined(PLATFORM_IS_POSIX_BASED)
#if defined(PLATFORM_IS_LINUX) || defined(PLATFORM_IS_EMSCRIPTEN)
  p = mmap(0, bytes, PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
#elif defined(PLATFORM_IS_MACOSX)
  p = mmap(0, bytes, PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANON, -1, 0);
#elif defined(PLATFORM_IS_DRAGONFLY)
  p = mmap(0, bytes, PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANON, -1, 0);
#elif defined(PLATFORM_IS_OPENBSD)
  p = mmap(0, bytes, PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANON, -1, 0);
#elif defined(PLATFORM_IS_BSD)
#ifndef MAP_ALIGNED_SUPER
#define MAP_ALIGNED_SUPER 0
#endif
  p = mmap(0, bytes, PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANON | MAP_ALIGNED_SUPER, -1, 0);
#endif
  if(p == MAP_FAILED)
    ok = false;
#endif

  if(!ok)
  {
    perror("out of memory: "); // print this first without allocating memory - fprintf might allocate
    fprintf(stderr, "(tried to allocate %zu bytes)\n", bytes);
    pony_assert(0);
    abort();
  }

  return p;
}

void* ponyint_virt_reserve_aligned(size_t size)
{
  pony_assert((size & (size - 1)) == 0);
  pony_assert((size % ponyint_page_size()) == 0);

  if((size == 0) || (size > (SIZE_MAX / 2)))
    return NULL;

#if defined(PLATFORM_IS_WINDOWS)
  // VirtualAlloc2 takes the alignment, so the reservation is exactly size.
  pony_assert(size >= (64 * 1024));

  MEM_ADDRESS_REQUIREMENTS req;
  MEM_EXTENDED_PARAMETER param;

  ZeroMemory(&req, sizeof(req));
  ZeroMemory(&param, sizeof(param));
  req.Alignment = size;
  param.Type = MemExtendedParameterAddressRequirements;
  param.Pointer = &req;

  return VirtualAlloc2(GetCurrentProcess(), NULL, size, MEM_RESERVE,
    PAGE_READWRITE, &param, 1);
#elif defined(PLATFORM_IS_POSIX_BASED)
  // The non-Linux POSIX paths map twice the size, then unmap the slack on
  // each side of the aligned span. Linux uses MAP_FIXED_NOREPLACE first
  // and falls back to the 2x approach only when that fails.
  size_t span = size * 2;
  void* base;

#if defined(PLATFORM_IS_LINUX)
  // MAP_FIXED_NOREPLACE (Linux 4.17+) lets us request a specific aligned
  // address without clobbering existing mappings. This avoids the 2x
  // overallocation that otherwise wastes half the address space on ILP32.
  {
    // Probe for free space, then try aligned addresses nearby.
    void* probe = mmap(0, size, PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

    if(probe != MAP_FAILED)
    {
      uintptr_t hint = (uintptr_t)probe;

      // If the kernel handed back an aligned address, keep it.
      if((hint & ((uintptr_t)size - 1)) == 0)
        return probe;

      munmap(probe, size);

      uintptr_t try = (hint + (size - 1)) & ~((uintptr_t)size - 1);

      for(int i = 0; i < 32; i++)
      {
        if(try < size)
          break;

        base = mmap((void*)try, size, PROT_READ | PROT_WRITE,
          MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE,
          -1, 0);

        if(base != MAP_FAILED)
          return base;

        if(try > (SIZE_MAX - size))
          break;

        try += size;
      }
    }

    // Fall back to the 2x overallocation.
    base = mmap(0, span, PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  }
#elif defined(PLATFORM_IS_EMSCRIPTEN)
  base = mmap(0, span, PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
#elif defined(PLATFORM_IS_MACOSX)
  base = mmap(0, span, PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANON, -1, 0);
#elif defined(PLATFORM_IS_DRAGONFLY)
  base = mmap(0, span, PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANON, -1, 0);
#elif defined(PLATFORM_IS_OPENBSD)
  base = mmap(0, span, PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANON, -1, 0);
#elif defined(PLATFORM_IS_BSD)
  // No MAP_ALIGNED_SUPER here, unlike ponyint_virt_alloc: the mapping is
  // trimmed to a self-computed alignment right below, so superpage
  // alignment buys nothing for it.
  base = mmap(0, span, PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANON, -1, 0);
#endif

  if(base == MAP_FAILED)
    return NULL;

  uintptr_t addr = (uintptr_t)base;
  uintptr_t aligned = (addr + (size - 1)) & ~((uintptr_t)size - 1);
  size_t lead = aligned - addr;
  size_t trail = span - size - lead;

  if(lead > 0)
    munmap(base, lead);

  if(trail > 0)
    munmap((void*)(aligned + size), trail);

  return (void*)aligned;
#endif
}

void ponyint_virt_commit(void* p, size_t bytes)
{
#if defined(PLATFORM_IS_WINDOWS)
  if(VirtualAlloc(p, bytes, MEM_COMMIT, PAGE_READWRITE) == NULL)
  {
    // VirtualAlloc sets the last error, not errno, so perror has nothing to
    // report here. The error code separates exhaustion from a bad range.
    fprintf(stderr, "out of memory: (tried to commit " __zu " bytes, error "
      "%lu)\n", bytes, GetLastError());
    pony_assert(0);
    abort();
  }
#elif defined(PLATFORM_IS_POSIX_BASED)
  // Reserved pages commit on first touch; see alloc.h.
  (void)p;
  (void)bytes;
#endif
}

void ponyint_virt_free(void* p, size_t bytes)
{
#if defined(PLATFORM_IS_WINDOWS)
  VirtualFree(p, 0, MEM_RELEASE);
#elif defined(PLATFORM_IS_POSIX_BASED)
  munmap(p, bytes);
#endif
}

void ponyint_virt_discard(void* p, size_t bytes)
{
#if defined(PLATFORM_IS_WINDOWS)
  // MEM_RESET keeps the pages committed, which is what leaves the range
  // usable; it only says their contents need not survive.
  VirtualAlloc(p, bytes, MEM_RESET, PAGE_READWRITE);
#elif defined(PLATFORM_IS_POSIX_BASED)
  madvise(p, bytes, MADV_DONTNEED);
#endif
}

void ponyint_virt_decommit(void* p, size_t bytes)
{
#if defined(PLATFORM_IS_WINDOWS)
  VirtualFree(p, bytes, MEM_DECOMMIT);
#elif defined(PLATFORM_IS_POSIX_BASED)
  madvise(p, bytes, MADV_DONTNEED);
#endif
}

size_t ponyint_page_size()
{
  static size_t page_size = 0;

  if(page_size == 0)
  {
#if defined(PLATFORM_IS_WINDOWS)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    page_size = si.dwPageSize;
#elif defined(PLATFORM_IS_POSIX_BASED)
    page_size = (size_t)sysconf(_SC_PAGESIZE);
#endif
  }

  return page_size;
}
