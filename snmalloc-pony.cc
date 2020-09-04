#include "../snmalloc.h"

#include <cstring>

#ifndef SNMALLOC_EXPORT
#  define SNMALLOC_EXPORT
#endif

using namespace snmalloc;

extern "C" SNMALLOC_EXPORT uint32_t ponyint_size_to_sizeclass(size_t size)
{
  if (size == 0)
    return 1;

  if (size > sizeclass_to_size(NUM_SIZECLASSES - 1))
    return (uint32_t)bits::next_pow2_bits(size) << 8;

  return (uint32_t)size_to_sizeclass(size);
}

extern "C" SNMALLOC_EXPORT uint32_t ponyint_sizeclass_to_size(uint32_t sc)
{
  if (sc > (NUM_SIZECLASSES - 1))
    return (size_t)1 << (sc >> 8);

  return sizeclass_to_size(sc >> 8);
}

extern "C" SNMALLOC_EXPORT size_t ponyint_round_alloc_size(size_t size)
{
  return ponyint_sizeclass_to_size(ponyint_size_to_sizeclass(size));
}

extern "C" SNMALLOC_EXPORT void* ponyint_alloc(size_t size)
{
  return ThreadAlloc::get_noncachable()->alloc(size);
}

extern "C" SNMALLOC_EXPORT void ponyint_dealloc(void* ptr, size_t size)
{
  ThreadAlloc::get_noncachable()->dealloc(ptr, size);
}

extern "C" SNMALLOC_EXPORT void*
ponyint_realloc(void* ptr, size_t old_size, size_t new_size)
{
  if (size_to_sizeclass(old_size) == size_to_sizeclass(new_size))
    return ptr;

  void* p = ThreadAlloc::get_noncachable()->alloc(new_size);
  if (p)
  {
    std::memcpy(p, ptr, old_size < new_size ? old_size : new_size);
    ThreadAlloc::get_noncachable()->dealloc(ptr, old_size);
  }
  return p;
}
