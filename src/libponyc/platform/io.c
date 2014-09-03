#include "platform.h"
#include <stdio.h>

intptr_t pony_openr(const char* file)
{
#ifdef PLATFORM_IS_WINDOWS
  //OpenFile has a 128 character path limit, therefore we use CreateFile (which
  //does not have this limit) to open files ...
  return (intptr_t)CreateFile(file, GENERIC_READ, 0, NULL, OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL, NULL);
#else
  return open(file, O_RDONLY);
#endif
}

void pony_close(intptr_t fileHandle)
{
#ifdef PLATFORM_IS_WINDOWS
  CloseHandle((HANDLE)fileHandle);
#else
  close(fileHandle);
#endif
}

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
  strcpy_s(win_path, strlen(path), path);
  strcat_s(win_path, 3, TEXT("\\*"));

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
  if (PathCanonicalize(resolved, path))
  {
    return resolved;
  }

  return NULL;
#elif PLATFORM_IS_POSIX_BASED
  return realpath(path, resolved);
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

void* pony_map_read(size_t size, intptr_t fd)
{
#ifdef PLATFORM_IS_WINDOWS
  HANDLE map = CreateFileMapping((HANDLE)fd, NULL, PAGE_READONLY, 0, 0, NULL);
  void* p = MapViewOfFile(map, FILE_MAP_READ, 0, 0, size);
  CloseHandle(map);
  return p;
#elif PLATFORM_IS_POSIX_BASED
  mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
#endif
}

void pony_unmap(void* p, size_t size)
{
#ifdef PLATFORM_IS_WINDOWS
  UnmapViewOfFile(p);
#elif PLATFORM_IS_POSIX_BASED
  munmap(file, size);
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
#elif PLATFORM_IS_POSIX_BASED
  return readdir_r(dir, entry, res) != 0;
#else
  return false;
#endif
}
