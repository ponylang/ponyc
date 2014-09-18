#include <platform.h>
#include <stdio.h>
#include "lsda.h"

#ifdef PLATFORM_IS_VISUAL_STUDIO
#define PONY_EXCEPTION_CLASS 0xE506E79 //"Pny"

PONY_EXTERN_C_BEGIN

static __declspec(thread) uintptr_t landing_pad;

void pony_throw()
{
  //Continuable exception with no arguments. RaiseException involves a kernel
  //mode transition.
  RaiseException(PONY_EXCEPTION_CLASS, 0, 0, NULL);

  //Never reaches here, since exceptions are always handled.
  abort();
}

/** Personality function that is called to request a stack unwinding operation.
 *  The purpose of this function is essentially to jump to the nearest then
 *  (or else) branch of a Pony try/then/else expression, relative to the 'error'
 *  expression that triggered this unwind request.
 *
 *  The EstablisherFrame given to this function describes the stack pointer
 *  (RSP) value of the target of this unwind operation.
 *
 *  Hence, the EstablisherFrame refers to the stack pointer of the caller
 *  of the function that caused the exception that we are inspecting here.
 *  Depending on the current exception handling state, this frame pointer
 *  is the target to a call to RtlUnwindEx, which implements the logic
 *  to unwind the stack to some frame further down the call stack.
 *
 *  Most importantly, RtlUnwindEx is capable of unwinding past multiple
 *  procedure frames, such that the cost of a Pony exception (neglecting
 *  the kernel transition of RaiseException) ends up in being a single
 *  longjump to the correct stack frame, where the handling else (and
 *  possibly then block) resides. We statically know that there will
 *  be one.
 *
 *  Signature of RtlUnwindEx:
 *
 *  VOID
 *  NTAPI
 *  RtlUnwindEx(
 *    __in_opt ULONG64               TargetFrame,
 *    __in_opt ULONG64               TargetIp,
 *    __in_opt PEXCEPTION_RECORD     ExceptionRecord,
 *    __in     PVOID                 ReturnValue,
 *    __out    PCONTEXT              OriginalContext,
 *    __in_opt PUNWIND_HISTORY_TABLE HistoryTable
 *  );
 *
 *  TargetFrame is the stack pointer value to which we want to unwind to.
 *  Consequently, TargetIp is the instruction pointer (which must be valid
 *  within the provided stack frame) to an instruction where execution
 *  should be continued. For our purposes, the ExceptionRecord argument
 *  is optional and describes the reason of an exception. In Pony, there
 *  is only one reason for stack unwinding, so we will basically ignore
 *  the ExceptionRecord and simply pass it through from this personality
 *  function. The motivation for this is performance, as otherwise
 *  RtlUnwindEx would allocate a default record type STATUS_UNWIND).
 *
 *  The ReturnValue argument can be used to place a value into the return
 *  register once stack unwinding is completed. Since we are unwinding
 *  function calls, we ignore this parameter. The OriginalContext can be
 *  used to supply user defined scratch space to store information between
 *  unwind operations. The runtimes exception dispatcher 'pony_throw' does
 *  not supply any arguments, so this parameter is ignored as well.
 *
 *  The HistoryTable can be used as a cache to improve performance of
 *  repeated function entry lookups. If unwinding non-trivial call frames
 *  becomes critical, using it might improve performance.
 *
 *  RtlUnwindEx does not ever return to its caller (i.e. this personality
 *  function). If it does, the process will (most likely) be terminated.
 */
EXCEPTION_DISPOSITION pony_personality_v0(EXCEPTION_RECORD *ExcRecord,
  void* EstablisherFrame, _CONTEXT *ContextRecord, DISPATCHER_CONTEXT* DispatcherContext)
{
  if(ExcRecord->ExceptionCode != PONY_EXCEPTION_CLASS)
    return ExceptionContinueSearch;

  if(!(ExcRecord->ExceptionFlags  &
    (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND)))
  {
    lsda_t lsda;

    lsda_init(&lsda, DispatcherContext);

    if(lsda_scan(&lsda, NULL, &landing_pad) == _URC_CONTINUE_UNWIND)
      return ExceptionContinueSearch;

    RtlUnwindEx(EstablisherFrame, (PVOID)landing_pad, ExcRecord,
      NULL, ContextRecord, DispatcherContext->HistoryTable);
  }

  if(ExcRecord->ExceptionFlags & EXCEPTION_TARGET_UNWIND)
  {
    DispatcherContext->ContextRecord->Rdx = landing_pad;
    return ExceptionContinueSearch;
  }

  //never reaches here, if it does, impending doom incoming
  abort();
}

PONY_EXTERN_C_END

#endif
