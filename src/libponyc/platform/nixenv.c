#include <platform.h>
#include "../ast/stringtab.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sys/stat.h>

#if defined(PLATFORM_IS_POSIX_BASED)

static bool dir_exists(const char* path)
{
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// Read the first line from a nix-support file, trimmed.  Returns NULL when the
// file is missing, empty, or unreadable.
static const char* read_nix_support_line(const char* nix_cc,
  const char* filename, strtable_t* strtab)
{
  char path[PATH_MAX];
  int n = snprintf(path, sizeof(path), "%s/nix-support/%s", nix_cc, filename);
  if(n < 0 || (size_t)n >= sizeof(path))
    return NULL;

  FILE* f = fopen(path, "r");
  if(f == NULL)
    return NULL;

  char line[PATH_MAX];
  if(fgets(line, sizeof(line), f) == NULL)
  {
    fclose(f);
    return NULL;
  }
  fclose(f);

  size_t len = strlen(line);
  while(len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
    line[--len] = '\0';

  if(len == 0)
    return NULL;

  return stringtab(strtab, line);
}

static const char* read_dynamic_linker(const char* nix_cc, strtable_t* strtab)
{
  return read_nix_support_line(nix_cc, "dynamic-linker", strtab);
}

static size_t parse_l_dirs(const char* env_val, strtable_t* strtab,
  const char** dirs, size_t max_dirs)
{
  if(env_val == NULL)
    return 0;

  size_t count = 0;
  const char* p = env_val;

  while(*p != '\0' && count < max_dirs)
  {
    while(*p == ' ')
      p++;

    if(*p == '\0')
      break;

    if(strncmp(p, "-L", 2) == 0)
    {
      p += 2;
      // -L may be immediately followed by the path or separated by a space.
      while(*p == ' ')
        p++;

      const char* start = p;
      while(*p != '\0' && *p != ' ')
        p++;

      if(p > start)
      {
        size_t len = (size_t)(p - start);
        dirs[count++] = stringtab_len(strtab, start, len);
      }
    }
    else
    {
      // Skip unrecognized flags (e.g. -rpath).
      while(*p != '\0' && *p != ' ')
        p++;
      // Skip the flag's argument if present.
      while(*p == ' ')
        p++;
      if(*p != '-' && *p != '\0')
      {
        while(*p != '\0' && *p != ' ')
          p++;
      }
    }
  }

  if(count >= max_dirs && *p != '\0')
  {
    fprintf(stderr,
      "Warning: NIX_LDFLAGS has more than %d -L entries; extras ignored\n",
      (int)max_dirs);
  }

  return count;
}

static size_t parse_isystem_dirs(const char* env_val, strtable_t* strtab,
  const char** dirs, size_t max_dirs)
{
  if(env_val == NULL)
    return 0;

  size_t count = 0;
  const char* p = env_val;

  while(*p != '\0' && count < max_dirs)
  {
    while(*p == ' ')
      p++;

    if(*p == '\0')
      break;

    if(strncmp(p, "-isystem", 8) == 0 && (p[8] == ' ' || p[8] == '\0'))
    {
      p += 8;
      while(*p == ' ')
        p++;

      const char* start = p;
      while(*p != '\0' && *p != ' ')
        p++;

      if(p > start)
      {
        size_t len = (size_t)(p - start);
        dirs[count++] = stringtab_len(strtab, start, len);
      }
    }
    else
    {
      // Skip unrecognized flag.
      while(*p != '\0' && *p != ' ')
        p++;
    }
  }

  if(count >= max_dirs && *p != '\0')
  {
    fprintf(stderr,
      "Warning: NIX_CFLAGS_COMPILE has more than %d -isystem entries;"
      " extras ignored\n", (int)max_dirs);
  }

  return count;
}

bool nixenv_probe(nixenv_t* nix, strtable_t* strtab)
{
  memset(nix, 0, sizeof(nixenv_t));

  const char* nix_cc = getenv("NIX_CC");
  if(nix_cc == NULL || nix_cc[0] == '\0')
    return false;

  char nix_support[PATH_MAX];
  int n = snprintf(nix_support, sizeof(nix_support),
    "%s/nix-support", nix_cc);
  if(n < 0 || (size_t)n >= sizeof(nix_support))
    return false;

  if(!dir_exists(nix_support))
    return false;

  nix->dynamic_linker = read_dynamic_linker(nix_cc, strtab);

  nix->lib_dir_count = parse_l_dirs(getenv("NIX_LDFLAGS"), strtab,
    nix->lib_dirs, NIXENV_MAX_DIRS);

  if(nix->lib_dir_count < NIXENV_MAX_DIRS)
  {
    const char* orig_libc = read_nix_support_line(nix_cc, "orig-libc", strtab);
    if(orig_libc != NULL)
    {
      char buf[PATH_MAX];
      int m = snprintf(buf, sizeof(buf), "%s/lib", orig_libc);
      if(m > 0 && (size_t)m < sizeof(buf) && dir_exists(buf))
        nix->lib_dirs[nix->lib_dir_count++] = stringtab(strtab, buf);
    }
  }

  if(nix->lib_dir_count < NIXENV_MAX_DIRS)
  {
    char ccld_path[PATH_MAX];
    int m = snprintf(ccld_path, sizeof(ccld_path),
      "%s/nix-support/cc-ldflags", nix_cc);
    if(m > 0 && (size_t)m < sizeof(ccld_path))
    {
      FILE* f = fopen(ccld_path, "r");
      if(f != NULL)
      {
        char ccld_line[4096];
        if(fgets(ccld_line, sizeof(ccld_line), f) != NULL)
        {
          size_t len = strlen(ccld_line);
          while(len > 0 &&
            (ccld_line[len - 1] == '\n' || ccld_line[len - 1] == '\r'))
            ccld_line[--len] = '\0';

          size_t remaining = NIXENV_MAX_DIRS - nix->lib_dir_count;
          nix->lib_dir_count += parse_l_dirs(ccld_line, strtab,
            nix->lib_dirs + nix->lib_dir_count, remaining);
        }
        fclose(f);
      }
    }
  }

  nix->include_dir_count = parse_isystem_dirs(
    getenv("NIX_CFLAGS_COMPILE"), strtab,
    nix->include_dirs, NIXENV_MAX_DIRS);

  if(nix->include_dir_count < NIXENV_MAX_DIRS)
  {
    const char* orig_libc_dev = read_nix_support_line(nix_cc,
      "orig-libc-dev", strtab);
    if(orig_libc_dev != NULL)
    {
      char buf[PATH_MAX];
      int m = snprintf(buf, sizeof(buf), "%s/include", orig_libc_dev);
      if(m > 0 && (size_t)m < sizeof(buf) && dir_exists(buf))
        nix->include_dirs[nix->include_dir_count++] = stringtab(strtab, buf);
    }
  }

  return true;
}

#else

bool nixenv_probe(nixenv_t* nix, strtable_t* strtab)
{
  (void)strtab;
  memset(nix, 0, sizeof(nixenv_t));
  return false;
}

#endif
