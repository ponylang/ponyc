#include <platform/platform.h>
#include <stdio.h>

PONY_DIR* pony_opendir(const char* path, PONY_ERRNO* err)
{
#ifdef PLATFORM_IS_WINDOWS
  size_t path_len = strlen(path);

  if (path_len > (MAX_PATH - 3))
  {
    *err = PONY_IO_PATH_TOO_LONG;
    return NULL;
  }

  TCHAR win_path[MAX_PATH];
  strcpy(win_path, path);
  strcat(win_path, "\\*");

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
  if (GetFullPathName(path, FILENAME_MAX, resolved, NULL))
  {
    if(GetFileAttributes(resolved) != INVALID_FILE_ATTRIBUTES)
      return resolved;
  }

  return NULL;
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
    return false;
  }

  *res = NULL;
  return true;
#elif defined(PLATFORM_IS_POSIX_BASED)
  return readdir_r(dir, entry, res) != 0;
#else
  return false;
#endif
}
