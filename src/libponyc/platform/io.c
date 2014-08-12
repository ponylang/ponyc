#include "platform.h"
#include <stdio.h>
#include <strsafe.h>

intptr_t pony_open(const char* file, uint32_t mode)
{
#ifdef PLATFORM_IS_WINDOWS
  _open(file, mode);
#else
  open(file, mode);
#endif
}

void pony_close(intptr_t fileHandle)
{
#ifdef PLATFORM_IS_WINDOWS
  _close(fileHandle);
#else
  close(fileHandle);
#endif
}

PONY_DIR* pony_opendir(const char* path, PONY_ERRNO* err)
{
#ifdef PLATFORM_IS_WINDOWS
  size_t path_len = 0;
  StringCchLength(path, MAX_PATH, &path_len);

  if (path_len > (MAX_PATH - 3))
  {
    *err = PONY_IO_PATH_TOO_LONG;
    return NULL;
  }

  TCHAR win_path[MAX_PATH];
  strcpy_s(win_path, path);
  strcat_s(win_path, TEXT("\\*"));

  PONY_DIR* dir = (PONY_DIR*)malloc(sizeof(PONY_DIR));
  
  dir->ptr = FindFirstFile(win_path, &dir->info);

  if (dir->ptr == INVALID_HANDLE_VALUE)
  {
    *err = GetLastError();
    FindClose(dir->ptr);
    free(dir);

    return NULL;
  }

  return dir;    
#elif PLATFORM_IS_POSIX_BASED
  return opendir(path);
#else
  return NULL;
#endif
}

char* pony_realpath(const char* path, char* resolved)
{
#ifdef PLATFORM_IS_WINDOWS
  if (PathCanonicalize(resolved, path))
  {
    return resolved;
  }

  return NULL;
#elif PLATFORM_IS_POSIX_BASED
  return realpath(path, resolved);
#else
  return NULL;
#endif
}

char* pony_get_dir_name(PONY_DIRINFO* info)
{
#ifdef PLATFORM_IS_WINDOWS
  return info->cFileName;
#elif PLATFORM_IS_POSIX_BASED
  return info->d_name;
#endif
}

void pony_closedir(PONY_DIR* dir)
{
#ifdef PLATFORM_IS_WINDOWS
  FindClose(dir->ptr);
#elif PLATFORM_IS_POSIX_BASED
  closedir(dir);
#endif
}

bool pony_dir_entry_next(PONY_DIR* dir, PONY_DIRINFO* entry, PONY_DIRINFO** res)
{
#ifdef PLATFORM_IS_WINDOWS
  if (FindNextFile(dir->ptr, &dir->info) != NULL)
  {
    *res = &dir->info;
    return true;
  }

  *res = NULL;
  return false;
#elif PLATFORM_IS_POSIX_BASED
  return readdir_r(dir, entry, res) != 0;
#else
  return false;
#endif
}

