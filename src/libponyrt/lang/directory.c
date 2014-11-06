#include <platform.h>
#include <pony.h>
#include "lang.h"
#include <string.h>

#if defined(PLATFORM_IS_WINDOWS)
#include <direct.h>
static __pony_thread_local HANDLE opendir_handle = INVALID_HANDLE_VALUE;
static __pony_thread_local WIN32_FIND_DATA opendir_data;
#elif defined(PLATFORM_IS_POSIX_BASED)
#include <unistd.h>
static __pony_thread_local DIR* opendir_handle;
#endif

PONY_EXTERN_C_BEGIN

static bool skip_entry(const char* entry, size_t len)
{
  if((len == 1) && (entry[0] == '.'))
    return true;

  if((len == 2) && (entry[0] == '.') && (entry[1] == '.'))
    return true;

  return false;
}

char* os_cwd()
{
#if defined(PLATFORM_IS_WINDOWS)
  char* cwd = _getcwd(NULL, 0);
#else
  char* cwd = getcwd(NULL, 0);
#endif

  size_t len = strlen(cwd) + 1;
  char* cstring = (char*)pony_alloc(len);
  memcpy(cstring, cwd, len);
  free(cwd);

  return cstring;
}

void os_opendir(const char* path)
{
#if defined(PLATFORM_IS_WINDOWS)
  size_t path_len = strlen(path);

  if(path_len > (MAX_PATH - 3))
    return;

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

void os_closedir()
{
#if defined(PLATFORM_IS_WINDOWS)
  if(opendir_handle != INVALID_HANDLE_VALUE)
  {
    FindClose(opendir_handle);
    opendir_handle = INVALID_HANDLE_VALUE;
  }
#elif defined(PLATFORM_IS_POSIX_BASED)
  if(opendir_handle != NULL)
  {
    closedir(opendir_handle);
    opendir_handle = NULL;
  }
#endif
}

const char* os_readdir()
{
#if defined(PLATFORM_IS_WINDOWS)
  if(opendir_handle == INVALID_HANDLE_VALUE)
    return NULL;

  size_t len = strlen(opendir_data.cFileName) + 1;

  if(skip_entry(opendir_data.cFileName, len - 1))
  {
    if(!FindNextFile(opendir_handle, &opendir_data))
    {
      os_closedir();
      return NULL;
    }

    return os_readdir();
  }

  char* cstring = (char*)pony_alloc(len);
  memcpy(cstring, opendir_data.cFileName, len);

  if(!FindNextFile(opendir_handle, &opendir_data))
    os_closedir();

  return cstring;
#elif defined(PLATFORM_IS_POSIX_BASED)
  if(opendir_handle == NULL)
    return NULL;

  struct dirent entry;
  struct dirent* result;

  if(readdir_r(opendir_handle, &entry, &result) != 0)
    return NULL;

  if(result == NULL)
    return NULL;

  size_t len = strlen(result->d_name) + 1;

  if(skip_entry(result->d_name, len - 1))
    return os_readdir();

  char* cstring = pony_alloc(len);
  memcpy(cstring, result->d_name, len);

  return cstring;
#else
  return NULL;
#endif
}

PONY_EXTERN_C_END