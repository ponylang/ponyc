#ifndef PLATFORM_IO_H
#define PLATFORM_IO_H

/** I/O support.
 *
 */
#ifdef PLATFORM_IS_WINDOWS
#  include <io.h>
#  include <sys/types.h> 
#  include <sys/stat.h>
typedef struct PONY_DIR { HANDLE ptr; WIN32_FIND_DATA info; } PONY_DIR;
#  define PONY_DIRINFO WIN32_FIND_DATA
#  define PONY_IO_PATH_TOO_LONG UINT32_MAX
#ifndef S_ISDIR
#  define S_IFDIR _S_IFDIR
#  define S_ISDIR(mode) (((mode) & S_IFDIR) == S_IFDIR)
#endif
#else
#  include <dirent.h>
#  include <limits.h>
#  include <stdlib.h>
#  include <fcntl.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <sys/uio.h>
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

// Make the specified directory, including any intermediate directories
void pony_mkdir(const char* path);

#endif
