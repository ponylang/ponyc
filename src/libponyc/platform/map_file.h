#ifndef PLATFORM_MAP_FILE_H
#define PLATFORM_MAP_FILE_H

/** Memory mapped files.
*
*/
#ifndef PLATFORM_IS_WINDOWS
#  include <sys/mman.h>
#  include <stdbool.h>
#  define PONY_PROT_READ PROT_READ
#  define PONY_PROT_READ_WRITE (PROT_READ | PROT_WRITE)
#  define PONY_MAP_FAILED MAP_FAILED
#else
#  define PONY_PROT_READ FILE_MAP_READ
#  define PONY_PROT_READ_WRITE FILE_MAP_ALL_ACCESS
#  define PONY_PROT_WRITE FILE_MAP_WRITE
#  define PONY_MAP_FAILED 0
#endif

char* map_file(size_t size, uint32_t protection, intptr_t fd);

void unmap_file(void* p, size_t size);

#endif