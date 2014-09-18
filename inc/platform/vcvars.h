#ifndef PLATFORM_VCVARS_H
#define PLATFORM_VCVARS_H

typedef struct vcvars_t
{
  char sdk_lib_dir[MAX_PATH + 1];
  char vc_lib_dir[MAX_PATH + 1];
} vcvars_t;

bool vcvars_get(vcvars_t* vcvars);

#endif