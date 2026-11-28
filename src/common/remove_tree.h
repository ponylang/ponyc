#ifndef PONY_COMMON_REMOVE_TREE_H
#define PONY_COMMON_REMOVE_TREE_H

#include <platform.h>

#ifdef PLATFORM_IS_POSIX_BASED

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Recursively delete a directory tree without running a shell or any other
// program. Symlinks are removed as links, never followed, including when path
// itself is a symlink. Best effort: errors are ignored, and a missing path is
// not an error.
static inline void pony_remove_tree(const char* path)
{
  struct stat path_st;
  if((lstat(path, &path_st) != 0) || !S_ISDIR(path_st.st_mode))
  {
    unlink(path);
    return;
  }

  DIR* dir = opendir(path);
  if(dir == NULL)
    return;

  // Each entry is removed as readdir returns it. Deleting the just-returned
  // entry and continuing to iterate is the standard recursive-delete idiom
  // (glibc, macOS, and the BSDs all tolerate it); don't "fix" this into a
  // collect-then-delete pass.
  struct dirent* entry;
  while((entry = readdir(dir)) != NULL)
  {
    if((strcmp(entry->d_name, ".") == 0) ||
      (strcmp(entry->d_name, "..") == 0))
      continue;

    char child[PATH_MAX];
    int n = snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);

    // Skip rather than act on a truncated path: a shortened path could name a
    // different filesystem object, and this code deletes.
    if((n < 0) || ((size_t)n >= sizeof(child)))
      continue;

    struct stat st;
    if((lstat(child, &st) == 0) && S_ISDIR(st.st_mode))
      pony_remove_tree(child);
    else
      unlink(child);
  }

  closedir(dir);
  rmdir(path);
}

#endif

#endif
