#include <platform.h>
#ifdef PLATFORM_IS_POSIX_BASED
#include "lsda.h"
#include <unwind.h>
#include <stdlib.h>
#include <assert.h>

PONY_EXTERN_C_BEGIN

static const uint64_t exception_class = 0x506F6E7900000000; // "Pony"
static __thread struct _Unwind_Exception exception;
static __thread uintptr_t landing_pad;

static void exception_cleanup(_Unwind_Reason_Code reason,
  struct _Unwind_Exception* exception)
{
  (void)reason;
  (void)exception;
}

void pony_throw()
{
  exception.exception_class = exception_class;
  exception.exception_cleanup = exception_cleanup;
  _Unwind_RaiseException(&exception);

  abort();
}

_Unwind_Reason_Code pony_personality_v0(int version, _Unwind_Action actions,
  uint64_t ex_class, struct _Unwind_Exception* exception,
  struct _Unwind_Context* context)
{
  if(version != 1 || exception == NULL || context == NULL)
    return _URC_FATAL_PHASE1_ERROR;

  if(actions & _UA_SEARCH_PHASE)
  {
    // We only need to parse lsda in the search phase.
    lsda_t lsda;

    if(!lsda_init(&lsda, context))
      return _URC_CONTINUE_UNWIND;

    // We're guaranteed to find a cleanup clause at some point.
    return lsda_scan(&lsda, actions, &landing_pad);
  }

  if(actions & _UA_CLEANUP_PHASE)
  {
    if(!(actions & _UA_HANDLER_FRAME))
      return _URC_CONTINUE_UNWIND;

    // No need to search again, just set the registers.
    _Unwind_SetGR(context, __builtin_eh_return_data_regno(0),
      (uintptr_t)exception);

    _Unwind_SetGR(context, __builtin_eh_return_data_regno(1), 0);
    _Unwind_SetIP(context, landing_pad);
    return _URC_INSTALL_CONTEXT;
  }

  return _URC_FATAL_PHASE1_ERROR;
}

PONY_EXTERN_C_END

#endif
