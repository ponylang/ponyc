#ifndef lang_stdfd_h
#define lang_stdfd_h

#include <platform.h>
#include <stdbool.h>

PONY_EXTERN_C_BEGIN

#ifdef PLATFORM_IS_WINDOWS

// True when stdin is a console input handle, false when it is a redirected
// pipe, file, or other device.
bool ponyint_stdin_is_console();

#endif

PONY_EXTERN_C_END

#endif
