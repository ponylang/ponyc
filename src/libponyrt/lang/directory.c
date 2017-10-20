#define PONY_WANT_ATOMIC_DEFS

#include <platform.h>
#include <pony.h>
#include "lang.h"
#include <string.h>

#if defined(PLATFORM_IS_WINDOWS)
#include "../mem/pool.h"
#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#elif defined(PLATFORM_IS_POSIX_BASED)
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#endif

PONY_EXTERN_C_BEGIN

PONY_ATOMIC(const char*) cwd_cache;

PONY_API int pony_os_eexist()
{
  return EEXIST;
}

PONY_API int pony_os_errno()
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

PONY_API char* pony_os_cwd()
{
  const char* cwd = atomic_load_explicit(&cwd_cache, memory_order_relaxed);

  if(cwd == NULL)
  {
    char* cwd_alloc;
#if defined(PLATFORM_IS_WINDOWS)
    cwd_alloc = _getcwd(NULL, 0);
#else
    cwd_alloc = getcwd(NULL, 0);
#endif

    if(cwd_alloc == NULL)
      cwd_alloc = strdup(".");

#ifdef USE_VALGRIND
    ANNOTATE_HAPPENS_BEFORE(&cwd_cache);
#endif
    if(!atomic_compare_exchange_strong_explicit(&cwd_cache, &cwd,
      (const char*)cwd_alloc, memory_order_release, memory_order_acquire))
    {
#ifdef USE_VALGRIND
      ANNOTATE_HAPPENS_AFTER(&cwd_cache);
#endif
      free(cwd_alloc);
    } else {
      cwd = cwd_alloc;
    }
  } else {
    atomic_thread_fence(memory_order_acquire);
#ifdef USE_VALGRIND
    ANNOTATE_HAPPENS_AFTER(&cwd_cache);
#endif
  }

  size_t len = strlen(cwd) + 1;
  char* cstring = (char*)pony_alloc(pony_ctx(), len);
  memcpy(cstring, cwd, len);

  return cstring;
}

#if defined(PLATFORM_IS_WINDOWS)

PONY_API WIN32_FIND_DATA* ponyint_windows_find_data()
{
  return POOL_ALLOC(WIN32_FIND_DATA);
}

PONY_API void ponyint_windows_find_data_free(WIN32_FIND_DATA* data)
{
  POOL_FREE(WIN32_FIND_DATA, data);
}

PONY_API const char* ponyint_windows_readdir(WIN32_FIND_DATA* find)
{
  size_t len = strlen(find->cFileName) + 1;

  if(skip_entry(find->cFileName, len - 1))
    return NULL;

  char* cstring = (char*)pony_alloc(pony_ctx(), len);
  memcpy(cstring, find->cFileName, len);

  return cstring;
}

#endif

PONY_API int ponyint_o_rdonly()
{
  return O_RDONLY;
}

PONY_API int ponyint_o_rdwr()
{
  return O_RDWR;
}

PONY_API int ponyint_o_creat()
{
  return O_CREAT;
}

PONY_API int ponyint_o_trunc()
{
  return O_TRUNC;
}

#if defined(PLATFORM_IS_POSIX_BASED)

PONY_API int ponyint_o_directory()
{
  return O_DIRECTORY;
}

PONY_API int ponyint_o_cloexec()
{
  return O_CLOEXEC;
}

#if !defined(PLATFORM_IS_MACOSX)
PONY_API int ponyint_at_removedir()
{
  return AT_REMOVEDIR;
}
#endif

PONY_API const char* ponyint_unix_readdir(DIR* dir)
{
  struct dirent* d;

  while(true)
  {
    d = readdir(dir);

    if(d == NULL)
      break;

#if defined(PLATFORM_IS_LINUX)
    size_t len = strlen(d->d_name);
#elif defined(PLATFORM_IS_BSD) || defined(PLATFORM_IS_MACOSX)
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
