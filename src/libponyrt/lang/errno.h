#ifndef lang_errno_h
#define lang_errno_h

#include <platform.h>
#include <pony.h>

PONY_EXTERN_C_BEGIN

PONY_API void pony_os_clear_errno();
PONY_API int pony_os_errno();

PONY_EXTERN_C_END

#endif
