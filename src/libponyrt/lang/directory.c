#include <platform.h>
#include <pony.h>
#include "lang.h"
#include <string.h>

#if defined(PLATFORM_IS_WINDOWS)
static __thread HANDLE opendir_handle;
static __thread WIN32_FIND_DATA opendir_data;
#elif defined(PLATFORM_IS_POSIX_BASED)
static __thread DIR* opendir_handle;
#endif

void os_opendir(const char* path)
{
#if defined(PLATFORM_IS_WINDOWS)
  size_t path_len = strlen(path);

  if(path_len > (MAX_PATH - 3))
    return NULL;

  TCHAR win_path[MAX_PATH];
  memcpy(win_path, path, path_len);
  memcpy(win_path + path_len, "\\*", 3);

  opendir_handle = FindFirstFile(win_path, &opendir_data);

  if(opendir_handle == INVALID_HANDLE_VALUE)
    pony_throw();
#elif defined(PLATFORM_IS_POSIX_BASED)
  opendir_handle = opendir(path);

  if(opendir_handle == NULL)
    pony_throw();
#else
  pony_throw();
#endif
}

const char* os_readdir()
{
#if defined(PLATFORM_IS_WINDOWS)
  if(opendir_handle == NULL)
    return NULL;

  size_t len = strlen(opendir_data.cFileName) + 1;
  char* cstring = pony_alloc(len);
  memcpy(cstring, opendir_data.cFileName, len);

  opendir_handle = FindNextFile(opendir_handle, &opendir_data);
  return cstring;
#elif defined(PLATFORM_IS_POSIX_BASED)
  struct dirent entry;
  struct dirent* result;

  if(readdir_r(opendir_handle, &entry, &result) != 0)
    return NULL;

  if(result == NULL)
    return NULL;

  size_t len = strlen(result->d_name) + 1;
  char* cstring = pony_alloc(len);
  memcpy(cstring, result->d_name, len);

  return cstring;
#else
  return NULL;
#endif
}

void os_closedir()
{
#if defined(PLATFORM_IS_WINDOWS)
  FindClose(opendir_handle);
#elif defined(PLATFORM_IS_POSIX_BASED)
  closedir(opendir_handle);
#endif
}
