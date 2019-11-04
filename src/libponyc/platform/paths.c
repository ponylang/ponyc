#include <platform.h>
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <stdio.h>

#if defined(PLATFORM_IS_LINUX)
#include <unistd.h>
#elif defined(PLATFORM_IS_MACOSX)
#include <mach-o/dyld.h>
#elif defined(PLATFORM_IS_BSD)
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#endif

#ifdef PLATFORM_IS_WINDOWS
# define PATH_SLASH '\\'
#else
# define PATH_SLASH '/'
#endif

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

PONY_DIRINFO* pony_dir_entry_next(PONY_DIR* dir)
{
#ifdef PLATFORM_IS_WINDOWS
  if(FindNextFile(dir->ptr, &dir->info) != 0)
    return &dir->info;

  return NULL;
#elif defined(PLATFORM_IS_POSIX_BASED)
  return readdir(dir);
#else
  return NULL;
#endif
}

void pony_mkdir(const char* path)
{
  // Copy the given path into a new buffer, one directory at a time, creating
  // each as we go
  size_t path_len = strlen(path);
  char* buf = (char*)ponyint_pool_alloc_size(path_len + 1); // +1 for terminator

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

  ponyint_pool_free_size(path_len + 1, buf);
}

#ifdef PLATFORM_IS_WINDOWS
#  include <shlwapi.h>
#  pragma comment(lib, "shlwapi.lib")
#else
#  include <libgen.h>
#endif


char* get_file_name(char* filename)
{
#ifdef PLATFORM_IS_WINDOWS
  PathStripPath((LPSTR) filename);
  return filename;
#else
  return basename(filename);
#endif
}

// https://stackoverflow.com/questions/2736753/how-to-remove-extension-from-file-name
// remove_ext: removes the "extension" from a file spec.
//   path is the string to process.
//   dot is the extension separator.
//   sep is the path separator (0 means to ignore).
// Returns an allocated string identical to the original but
//   with the extension removed. It must be freed when you're
//   finished with it.
// If you pass in NULL or the new string can't be allocated,
//   it returns NULL.
char* remove_ext(const char* path, char dot, char sep, size_t* allocated_size) 
{
    char *retstr, *lastdot, *lastsep;
    // Error checks and allocate string.
    if (path == NULL)
      return NULL;

    *allocated_size = strlen(path) + 1;

    retstr = (char*) ponyint_pool_alloc_size(*allocated_size);

    // Make a copy and find the relevant characters.
    strcpy(retstr, path);
    lastdot = strrchr(retstr, dot);
    lastsep = (sep == 0) ? NULL : strrchr(retstr, sep);

    // If it has an extension separator.
    if (lastdot != NULL) {
      // and it's before the extension separator.
      if (lastsep != NULL) {
        if (lastsep < lastdot) {
          // then remove it.
          *lastdot = '\0';
        }
      }
      else {
        // Has extension separator with no path separator.
        *lastdot = '\0';
      }
    }
    // Return the modified string.
    return retstr;
}

bool get_compiler_exe_path(char* output_path, const char* argv0)
{
  bool success = false;
  success = (argv0 == NULL) ? success : success; // hush compiler warning
#ifdef PLATFORM_IS_WINDOWS
  // Specified size *includes* nul terminator
  GetModuleFileName(NULL, output_path, FILENAME_MAX);
  success = (GetLastError() == ERROR_SUCCESS);
#elif defined PLATFORM_IS_LINUX
  // Specified size *excludes* nul terminator
  ssize_t r = readlink("/proc/self/exe", output_path, FILENAME_MAX - 1);
  success = (r >= 0);

  if(success)
    output_path[r] = '\0';
#elif defined PLATFORM_IS_OPENBSD
  if (argv0 != NULL && (*argv0 == '/' || *argv0 == '.'))
  {
    if (pony_realpath(argv0, output_path) != NULL)
    {
      return true;
    } else {
      return false;
    }
  }
  else {
    char *env_path = getenv("PATH");
    char *token, *string, *tofree;
    char try_path[FILENAME_MAX];
    struct stat sb;

    if (env_path == NULL)
    {
      return false;
    }
    tofree = string = strdup(env_path);
    while ((token = strsep(&string, ":")) != NULL)
    {
      snprintf(try_path, sizeof(try_path), "%s/%s", token, argv0);
      if (access(try_path, X_OK) == 0 &&
        stat(try_path, &sb) == 0 &&
        (sb.st_mode & S_IFREG) == S_IFREG)
      {
        if (pony_realpath(try_path, output_path) != NULL)
        {
          success = true;
          break;
        }
      }
    }
    free(tofree);
  }
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
  success = (r == 0);

  if(success)
  {
    pony_realpath(exec_path, output_path);
  }
#else
#  error Unsupported platform for exec_path()
#endif
  return success;
}

bool get_compiler_exe_directory(char* output_path, const char* argv0)
{
  bool can_get_compiler_path = get_compiler_exe_path(output_path, argv0);
  if (can_get_compiler_path)
  {
    char *p = strrchr(output_path, PATH_SLASH);
    if(p == NULL)
    {
      return false;
    }
    p++;
    *p = '\0';
    return true;
  } else {
    return false;
  }
}
