#ifndef PLATFORM_NIXENV_H
#define PLATFORM_NIXENV_H

#include <stddef.h>
#include <stdbool.h>

#define NIXENV_MAX_DIRS 64

typedef struct strtable_t strtable_t;

typedef struct nixenv_t
{
  const char* dynamic_linker;
  const char* lib_dirs[NIXENV_MAX_DIRS];
  size_t lib_dir_count;
  const char* include_dirs[NIXENV_MAX_DIRS];
  size_t include_dir_count;
} nixenv_t;

PONY_EXTERN_C_BEGIN

bool nixenv_probe(nixenv_t* nix, strtable_t* strtab);

PONY_EXTERN_C_END

#endif
