#include <platform.h>
#include "../../libponyrt/mem/pool.h"
#include <string.h>

PONY_DIR* pony_opendir(const char* path, PONY_ERRNO* err)
{
#ifdef PLATFORM_IS_WINDOWS
  size_t path_len = strlen(path);

  if(path_len > (MAX_PATH - 3))
  {
    *err = PONY_IO_PATH_TOO_LONG;
    return NULL;
  }

  TCHAR win_path[MAX_PATH];
  strcpy(win_path, path);
  strcat(win_path, "\\*");

  PONY_DIR* dir = POOL_ALLOC(PONY_DIR);

  dir->ptr = FindFirstFile(win_path, &dir->info);

  if(dir->ptr == INVALID_HANDLE_VALUE)
  {
    *err = GetLastError();
    FindClose(dir->ptr);
    POOL_FREE(PONY_DIR, dir);

    return NULL;
  }

  return dir;
#elif defined(PLATFORM_IS_POSIX_BASED)
  PONY_DIR* dir = opendir(path);
  if(dir == NULL)
  {
    *err = errno;
    return NULL;
  }

  return dir;
#else
  return NULL;
#endif
}

char* pony_realpath(const char* path, char* resolved)
{
#ifdef PLATFORM_IS_WINDOWS
  if(GetFullPathName(path, FILENAME_MAX, resolved, NULL) == 0 ||
    GetFileAttributes(resolved) == INVALID_FILE_ATTRIBUTES)
    return NULL;

  // Strip any trailing backslashes
  for(size_t len = strlen(resolved); resolved[len - 1] == '\\'; --len)
    resolved[len - 1] = '\0';

  return resolved;
#elif defined(PLATFORM_IS_POSIX_BASED)
  return realpath(path, resolved);
#endif
}

char* pony_dir_info_name(PONY_DIRINFO* info)
{
#ifdef PLATFORM_IS_WINDOWS
  return info->cFileName;
#elif defined(PLATFORM_IS_POSIX_BASED)
  return info->d_name;
#endif
}

void pony_closedir(PONY_DIR* dir)
{
#ifdef PLATFORM_IS_WINDOWS
  FindClose(dir->ptr);
  POOL_FREE(PONY_DIR, dir);
#elif defined(PLATFORM_IS_POSIX_BASED)
  closedir(dir);
#endif
}

bool pony_dir_entry_next(PONY_DIR* dir, PONY_DIRINFO* entry, PONY_DIRINFO** res)
{
#ifdef PLATFORM_IS_WINDOWS
  if (FindNextFile(dir->ptr, &dir->info) != 0)
  {
    *res = &dir->info;
    return true;
  }

  *res = NULL;
  return false;
#elif defined(PLATFORM_IS_POSIX_BASED)
  return readdir_r(dir, entry, res) == 0;
#else
  return false;
#endif
}

void pony_mkdir(const char* path)
{
  // Copy the given path into a new buffer, one directory at a time, creating
  // each as we go
  size_t path_len = strlen(path);
  char* buf = (char*)pool_alloc_size(path_len + 1); // + 1 for terminator

  for(size_t i = 0; i < path_len; i++)
  {
    buf[i] = path[i];

    if(path[i] == '/'
#ifdef PLATFORM_IS_WINDOWS
      || path[i] == '\\'
#endif
      )
    {
      // Create an intermediate dir
      buf[i + 1] = '\0';

#ifdef PLATFORM_IS_WINDOWS
      CreateDirectory(buf, NULL);
#else
      mkdir(buf, 0777);
#endif
    }
  }

  // Create final directory
#ifdef PLATFORM_IS_WINDOWS
  CreateDirectory(path, NULL);
#else
  mkdir(path, 0777);
#endif

  pool_free_size(path_len + 1, buf);
}
