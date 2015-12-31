#include <platform.h>

#ifdef PLATFORM_IS_POSIX_BASED

#include "lsda.h"
#include <unwind.h>
#include <stdlib.h>
#include <assert.h>

PONY_EXTERN_C_BEGIN

#ifdef PLATFORM_IS_ARM
#include <string.h>
#include <stdio.h>
#endif

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
#ifdef PLATFORM_IS_ARM
  memcpy(exception.exception_class, "Pony\0\0\0\0", 8);
#else
  exception.exception_class = 0x506F6E7900000000; // "Pony"
#endif
  exception.exception_cleanup = exception_cleanup;
  _Unwind_RaiseException(&exception);

  abort();
}

static void set_registers(struct _Unwind_Exception* exception,
  struct _Unwind_Context* context)
{
  _Unwind_SetGR(context, __builtin_eh_return_data_regno(0),
    (uintptr_t)exception);
  _Unwind_SetGR(context, __builtin_eh_return_data_regno(1), 0);
  _Unwind_SetIP(context, landing_pad);
}

#ifdef PLATFORM_IS_ARM

_Unwind_Reason_Code __gnu_unwind_frame(_Unwind_Exception*, _Unwind_Context*);

static _Unwind_Reason_Code continue_unwind(_Unwind_Exception* exception,
  _Unwind_Context* context)
{
  if(__gnu_unwind_frame(exception, context) != _URC_OK)
    return _URC_FAILURE;

  return _URC_CONTINUE_UNWIND;
}

_Unwind_Reason_Code pony_personality_v0(_Unwind_State state,
  _Unwind_Exception* exception, _Unwind_Context* context)
{
  if(exception == NULL || context == NULL)
    return _URC_FAILURE;

  // Save exception in r12.
  _Unwind_SetGR(context, 12, (uintptr_t)exception);

  switch(state & _US_ACTION_MASK)
  {
    case _US_VIRTUAL_UNWIND_FRAME:
    {
      if(!lsda_scan(context, &landing_pad))
        return continue_unwind(exception, context);

      // Save r13.
      exception->barrier_cache.sp = _Unwind_GetGR(context, 13);

      // Save barrier.
      exception->barrier_cache.bitpattern[0] = 0;
      exception->barrier_cache.bitpattern[1] = 0;
      exception->barrier_cache.bitpattern[2] =
        (uint32_t)_Unwind_GetLanguageSpecificData(context);
      exception->barrier_cache.bitpattern[3] = (uint32_t)landing_pad;
      exception->barrier_cache.bitpattern[4] = 0;
      return _URC_HANDLER_FOUND;
    }

    case _US_UNWIND_FRAME_STARTING:
    {

      if(exception->barrier_cache.sp == _Unwind_GetGR(context, 13))
      {
        // Load barrier.
        landing_pad = exception->barrier_cache.bitpattern[3];

        // No need to search again, just set the registers.
        set_registers(exception, context);
        return _URC_INSTALL_CONTEXT;
      }

      return continue_unwind(exception, context);
    }

    case _US_UNWIND_FRAME_RESUME:
      return continue_unwind(exception, context);

    default:
      abort();
  }

  return _URC_FAILURE;
}

#else

_Unwind_Reason_Code pony_personality_v0(int version, _Unwind_Action actions,
  uint64_t ex_class, struct _Unwind_Exception* exception,
  struct _Unwind_Context* context)
{
  (void)ex_class;

  if(version != 1 || exception == NULL || context == NULL)
    return _URC_FATAL_PHASE1_ERROR;

  // The search phase sets up the landing pad.
  if(actions & _UA_SEARCH_PHASE)
  {
    if(!lsda_scan(context, &landing_pad))
      return _URC_CONTINUE_UNWIND;

    return _URC_HANDLER_FOUND;
  }

  if(actions & _UA_CLEANUP_PHASE)
  {
    if(!(actions & _UA_HANDLER_FRAME))
      return _URC_CONTINUE_UNWIND;

    // No need to search again, just set the registers.
    set_registers(exception, context);
    return _URC_INSTALL_CONTEXT;
  }

  return _URC_FATAL_PHASE1_ERROR;
}

#endif

PONY_EXTERN_C_END

#endif
