#ifndef PONY_COMMON_EXE_PATH_H
#define PONY_COMMON_EXE_PATH_H

#include <platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_IS_LINUX)
#  include <unistd.h>
#elif defined(PLATFORM_IS_MACOSX)
#  include <mach-o/dyld.h>
#elif defined(PLATFORM_IS_BSD)
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/sysctl.h>
#  include <sys/stat.h>
#endif

// The absolute path of path with every symlink resolved, written to resolved
// (at least FILENAME_MAX bytes). Returns resolved, or NULL if the path doesn't
// exist or can't be resolved. On Windows the result uses forward slashes and
// has no trailing slash.
static inline char* pony_exe_path_realpath(const char* path, char* resolved)
{
#ifdef PLATFORM_IS_WINDOWS
  if(GetFullPathName(path, FILENAME_MAX, resolved, NULL) == 0 ||
    GetFileAttributes(resolved) == INVALID_FILE_ATTRIBUTES)
    return NULL;

  for(size_t i = 0; resolved[i] != '\0'; i++)
    if(resolved[i] == '\\')
      resolved[i] = '/';

  for(size_t len = strlen(resolved); len > 0 && resolved[len - 1] == '/'; --len)
    resolved[len - 1] = '\0';

  return resolved;
#else
  return realpath(path, resolved);
#endif
}

// The path of the running executable, written to output_path (at least
// FILENAME_MAX bytes). OpenBSD has no way to ask the system, so there the path
// is found from argv0 the way a shell would have found it: argv0 itself if it
// contains a '/', otherwise the first match on PATH. Returns false if the path
// can't be found.
static inline bool pony_exe_path_get(char* output_path, const char* argv0)
{
  bool success = false;
  (void)argv0;
#ifdef PLATFORM_IS_WINDOWS
  // Specified size *includes* nul terminator
  GetModuleFileName(NULL, output_path, FILENAME_MAX);
  success = (GetLastError() == ERROR_SUCCESS);

  if(success)
  {
    for(size_t i = 0; output_path[i] != '\0'; i++)
      if(output_path[i] == '\\')
        output_path[i] = '/';
  }
#elif defined PLATFORM_IS_LINUX
  // Specified size *excludes* nul terminator
  ssize_t r = readlink("/proc/self/exe", output_path, FILENAME_MAX - 1);
  success = (r >= 0);

  if(success)
    output_path[r] = '\0';
#elif defined PLATFORM_IS_OPENBSD
  if(argv0 == NULL)
    return false;

  if(strchr(argv0, '/') != NULL)
    return pony_exe_path_realpath(argv0, output_path) != NULL;

  char* env_path = getenv("PATH");
  char* token;
  char* string;
  char* tofree;
  char try_path[FILENAME_MAX];
  struct stat sb;

  if(env_path == NULL)
    return false;

  tofree = string = strdup(env_path);
  while((token = strsep(&string, ":")) != NULL)
  {
    snprintf(try_path, sizeof(try_path), "%s/%s", token, argv0);
    if(access(try_path, X_OK) == 0 &&
      stat(try_path, &sb) == 0 &&
      (sb.st_mode & S_IFREG) == S_IFREG)
    {
      if(pony_exe_path_realpath(try_path, output_path) != NULL)
      {
        success = true;
        break;
      }
    }
  }
  free(tofree);
#elif defined PLATFORM_IS_BSD
  int mib[4];
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = -1;

  size_t len = FILENAME_MAX;
  int r = sysctl(mib, 4, output_path, &len, NULL, 0);
  success = (r == 0);
#elif defined PLATFORM_IS_MACOSX
  char exec_path[FILENAME_MAX];
  uint32_t size = sizeof(exec_path);
  int r = _NSGetExecutablePath(exec_path, &size);
  success = (r == 0) &&
    (pony_exe_path_realpath(exec_path, output_path) != NULL);
#else
#  error Unsupported platform for pony_exe_path_get()
#endif
  return success;
}

// The directory of the running executable, with a trailing '/', written to
// output_path (at least FILENAME_MAX bytes). Returns false if it can't be
// found.
static inline bool pony_exe_path_directory(char* output_path, const char* argv0)
{
  if(!pony_exe_path_get(output_path, argv0))
    return false;

  char* p = strrchr(output_path, '/');
  if(p == NULL)
    return false;

  p[1] = '\0';
  return true;
}

#endif
