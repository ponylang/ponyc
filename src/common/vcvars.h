#ifndef PLATFORM_VCVARS_H
#define PLATFORM_VCVARS_H

typedef struct compile_t compile_t;
typedef struct errors_t errors_t;

typedef struct vcvars_t
{
  char link[MAX_PATH];
  char ar[MAX_PATH];
  char kernel32[MAX_PATH];
  char ucrt[MAX_PATH];
  char msvcrt[MAX_PATH];
  char default_libs[MAX_PATH];
} vcvars_t;

bool vcvars_get(compile_t *c, vcvars_t* vcvars, errors_t* errors);

#endif
