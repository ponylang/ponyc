#ifndef PONY_COMMON_PONYC_EXIT_H
#define PONY_COMMON_PONYC_EXIT_H

#include <platform.h>

// The exit status of a failed ponyc, both the driver (src/ponyc) and the
// compiler (src/ponyc-compiler).
#ifdef PLATFORM_IS_WINDOWS
#  define PONYC_EXIT_FAILED (-1)
#else
#  define PONYC_EXIT_FAILED 255
#endif

#endif
