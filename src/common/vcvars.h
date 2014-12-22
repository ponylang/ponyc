#ifndef PLATFORM_VCVARS_H
#define PLATFORM_VCVARS_H

typedef struct vcvars_t
{
  char link[MAX_PATH];
  char ar[MAX_PATH];
  char kernel32[MAX_PATH];
  char msvcrt[MAX_PATH];
} vcvars_t;

bool vcvars_get(vcvars_t* vcvars);

#endif
