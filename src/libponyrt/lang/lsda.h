#ifndef LANG_LSDA_H
#define LANG_LSDA_H

#include <platform.h>

#if defined(PLATFORM_IS_CLANG_OR_GCC)
#include <unwind.h>
#endif

#include <stdint.h>
#include <stdbool.h>

PONY_EXTERN_C_BEGIN

#ifdef PLATFORM_IS_POSIX_BASED
typedef struct _Unwind_Context exception_context_t;
#elif defined(PLATFORM_IS_VISUAL_STUDIO)
typedef DISPATCHER_CONTEXT exception_context_t;
#endif

bool ponyint_lsda_scan(exception_context_t* context, uintptr_t* lp);

PONY_EXTERN_C_END

#endif
