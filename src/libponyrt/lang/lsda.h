#ifndef LANG_LSDA_H
#define LANG_LSDA_H

#include <platform.h>

#ifdef PLATFORM_IS_CLANG_OR_GCC
#include <unwind.h>
#endif

#include <stdint.h>
#include <stdbool.h>

#ifdef PLATFORM_IS_POSIX_BASED
typedef struct _Unwind_Context exception_context_t;
#elif defined(PLATFORM_IS_VISUAL_STUDIO)
typedef DISPATCHER_CONTEXT exception_context_t;
#define _URC_CONTINUE_UNWIND 0x01
#define _URC_HANDLER_FOUND 0x02
#define _Unwind_Action void*
#define _Unwind_Reason_Code uint8_t
#endif

typedef struct lsda_t
{
  uintptr_t region_start;
  uintptr_t ip;
  uintptr_t ip_offset;
  uintptr_t landing_pads;

  const uint8_t* type_table;
  const uint8_t* call_site_table;
  const uint8_t* action_table;

  uint8_t type_table_encoding;
  uint8_t call_site_encoding;
} lsda_t;

bool lsda_init(lsda_t* lsda, exception_context_t* context);

_Unwind_Reason_Code lsda_scan(lsda_t* lsda, _Unwind_Action actions,
  uintptr_t* lp);

#endif
