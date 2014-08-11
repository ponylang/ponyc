#ifndef PLATFORM_IO_H
#define PLATFORM_IO_H

#ifdef PLATFORM_IS_WINDOWS
#  include <io.h>
typedef struct PONY_DIR { HANDLE ptr; WIN32_FIND_DATA info; } PONY_DIR;
#  define PONY_DIRINFO WIN32_FIND_DATA
#  define PONY_IO_EACCES 0x01
#  define PONY_IO_ENOENT 0x02
#  define PONY_IO_ENOTDIR 0x04
#  define PONY_IO_PATH_TOO_LONG 0x08
#else
#  include <dirent.h>
#  define PONY_DIR DIR
#  define PONY_DIRRINFO struct dirent
#  define PONY_IO_EACCES EACCES
#  define PONY_IO_ENOENT ENOENT
#  define PONY_IO_ENOTDIR ENOTDIR
#  define PONY_IO_PATH_TOO_LONG UINT32_MAX
#endif

PONY_DIR* pony_opendir(const char* path, PONY_ERRNO* err);

char* pony_get_dir_name(PONY_DIRINFO* info);

void pony_closedir(PONY_DIR* dir);

bool pony_dir_entry_next(PONY_DIR* dir, PONY_DIRINFO* entry, PONY_DIRINFO** res);

#endif