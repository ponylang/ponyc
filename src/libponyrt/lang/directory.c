#include <platform.h>
#include <pony.h>
#include "lang.h"
#include <string.h>

#if defined(PLATFORM_IS_WINDOWS)
#include <direct.h>
#include <errno.h>
#elif defined(PLATFORM_IS_POSIX_BASED)
#include <unistd.h>
#include <stdio.h>
#endif

PONY_EXTERN_C_BEGIN

const char* cwd;

int pony_os_eexist()
{
  return EEXIST;
}

int pony_os_errno()
{
  return errno;
}

static bool skip_entry(const char* entry, size_t len)
{
  if((len == 1) && (entry[0] == '.'))
    return true;

  if((len == 2) && (entry[0] == '.') && (entry[1] == '.'))
    return true;

  return false;
}

char* pony_os_cwd()
{
  if(cwd == NULL)
  {
#if defined(PLATFORM_IS_WINDOWS)
    cwd = _getcwd(NULL, 0);
#else
    cwd = getcwd(NULL, 0);
#endif

    if(cwd == NULL)
      cwd = strdup(".");
  }

  size_t len = strlen(cwd) + 1;
  char* cstring = (char*)pony_alloc(pony_ctx(), len);
  memcpy(cstring, cwd, len);

  return cstring;
}

#if defined(PLATFORM_IS_WINDOWS)

WIN32_FIND_DATA* ponyint_windows_find_data()
{
  return (WIN32_FIND_DATA*)malloc(sizeof(WIN32_FIND_DATA));
}

const char* ponyint_windows_readdir(WIN32_FIND_DATA* find)
{
  size_t len = strlen(find->cFileName) + 1;

  if(skip_entry(find->cFileName, len - 1))
    return NULL;

  char* cstring = (char*)pony_alloc(pony_ctx(), len);
  memcpy(cstring, find->cFileName, len);

  return cstring;
}

#endif

#if defined(PLATFORM_IS_POSIX_BASED)

int ponyint_o_rdonly()
{
  return O_RDONLY;
}

int ponyint_o_rdwr()
{
  return O_RDWR;
}

int ponyint_o_creat()
{
  return O_CREAT;
}

int ponyint_o_trunc()
{
  return O_TRUNC;
}

int ponyint_o_directory()
{
  return O_DIRECTORY;
}

int ponyint_o_cloexec()
{
  return O_CLOEXEC;
}

#if !defined(PLATFORM_IS_MACOSX)
int ponyint_at_removedir()
{
  return AT_REMOVEDIR;
}
#endif

const char* ponyint_unix_readdir(DIR* dir)
{
  struct dirent* d;

  while(true)
  {
    d = readdir(dir);

    if(d == NULL)
      break;

#if defined(PLATFORM_IS_LINUX)
    size_t len = strlen(d->d_name);
#elif defined(PLATFORM_IS_FREEBSD) || defined(PLATFORM_IS_MACOSX)
    size_t len = d->d_namlen;
#endif

    if(skip_entry(d->d_name, len))
      continue;

    char* cstring = pony_alloc(pony_ctx(), len + 1);
    memcpy(cstring, d->d_name, len + 1);
    return cstring;
  }

  return NULL;
}

#endif

PONY_EXTERN_C_END
