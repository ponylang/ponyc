#include "platform.h"

char* map_file(size_t size, uint32_t protection, intptr_t fd)
{
#ifdef PLATFORM_IS_WINDOWS
  return NULL;
#elif PLATFORM_IS_POSIX_BASED
  return mmap(0, size, protection, MAP_PRIVATE, fd, 0);
#endif
}

void unmap_file(void* p, size_t size)
{
#ifdef PLATFORM_IS_WINDOWS
#elif PLATFORM_IS_POSIX_BASED
  munmap(p, size);
#endif
}