#ifndef PLATFORM_IO_H
#define PLATFORM_IO_H

#ifdef PLATFORM_IS_WINDOWS
#  include <io.h>
#  include <Shlwapi.h>
typedef struct PONY_DIR { HANDLE ptr; WIN32_FIND_DATA info; } PONY_DIR;
#  define PONY_DIRINFO WIN32_FIND_DATA
#  define PONY_IO_PATH_TOO_LONG UINT32_MAX
#else
#  include <dirent.h>
#  include <limits.h>
#  include <stdlib.h>
#  include <fcntl.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <errno.h>
#  define PONY_DIR DIR
#  define PONY_DIRINFO struct dirent
#  define PONY_IO_PATH_TOO_LONG UINT32_MAX
#endif

PONY_DIR* pony_opendir(const char* path, PONY_ERRNO* err);

char* pony_realpath(const char* path, char* resolved);

char* pony_dir_info_name(PONY_DIRINFO* info);

void pony_closedir(PONY_DIR* dir);

bool pony_dir_entry_next(PONY_DIR* dir, PONY_DIRINFO* entry,
  PONY_DIRINFO** res);

#endif
