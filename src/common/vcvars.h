#ifndef PLATFORM_VCVARS_H
#define PLATFORM_VCVARS_H

// pass_opt_t is fully defined in src/libponyc/pass/pass.h; this forward
// (duplicate) typedef mirrors the compile_t one this file used to carry, and
// keeps the whole pass header out of every Windows TU that pulls vcvars.h in
// through platform.h. vcvars_get only ever reads opt->verbosity.
typedef struct pass_opt_t pass_opt_t;
typedef struct errors_t errors_t;

typedef struct vcvars_t
{
  char link[MAX_PATH];
  char ar[MAX_PATH];
  char kernel32[MAX_PATH];
  char ucrt[MAX_PATH];
  char msvcrt[MAX_PATH];
  char default_libs[MAX_PATH];
  // System header search dirs, siblings of the lib dirs above. Filled in
  // alongside them so the C-shim compile (genc) sees the same toolchain the
  // link resolves against. Empty if discovery couldn't build the path.
  char msvc_include[MAX_PATH];    // MSVC    <install>\VC\Tools\MSVC\<ver>\include
  char ucrt_include[MAX_PATH];    // Win SDK Include\<sdkver>\ucrt
  char shared_include[MAX_PATH];  // Win SDK Include\<sdkver>\shared
  char um_include[MAX_PATH];      // Win SDK Include\<sdkver>\um
  // cl.exe's version (e.g. "19.50.35727"), for -fms-compatibility-version so a
  // shim's _MSC_VER matches the toolchain. Empty if cl.exe couldn't be read.
  char msvc_version[MAX_PATH];
} vcvars_t;

bool vcvars_get(pass_opt_t* opt, vcvars_t* vcvars, errors_t* errors);

#endif
