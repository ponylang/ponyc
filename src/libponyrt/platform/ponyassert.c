#define PONY_WANT_ATOMIC_DEFS

#include "ponyassert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef PLATFORM_IS_POSIX_BASED
#  include <execinfo.h>
#  include <unistd.h>
#else
#  include <Windows.h>
#  include <DbgHelp.h>
#endif

#include <pony/detail/atomics.h>

static PONY_ATOMIC(bool) assert_guard = false;

#ifdef PLATFORM_IS_POSIX_BASED

#ifdef PLATFORM_IS_FREEBSD
typedef size_t stack_depth_t;
#else
typedef int stack_depth_t;
#endif

void ponyint_assert_fail(const char* expr, const char* file, size_t line,
  const char* func)
{
  while(atomic_exchange_explicit(&assert_guard, true, memory_order_acq_rel))
  {
    // If the guard is already set, an assertion fired in another thread. The
    // things here aren't thread safe, so we just start an infinite loop.
    struct timespec ts = {1, 0};
    nanosleep(&ts, NULL);
  }

  fprintf(stderr, "%s:" __zu ": %s: Assertion `%s` failed.\n\n", file, line,
    func, expr);

#ifdef PLATFORM_IS_FREEBSD
  void* addrs[256];
  size_t depth = backtrace(addrs, 256);
  char** strings = backtrace_symbols(addrs, depth);

  fputs("Backtrace:\n", stderr);

  for(size_t i = 0; i < depth; i++)
    printf("  %s\n", strings[i]);
#else
  void* addrs[256];
  stack_depth_t depth = backtrace(addrs, 256);
  char** strings = backtrace_symbols(addrs, depth);

  fputs("Backtrace:\n", stderr);

  for(stack_depth_t i = 0; i < depth; i++)
    printf("  %s\n", strings[i]);
#endif


  free(strings);

  if(strcmp(PONY_BUILD_CONFIG, "release") == 0)
  {
    fputs("This is an optimised version of ponyc: the backtrace may be "
      "imprecise or incorrect.\nUse a debug version to get more meaningful "
      "information.\n", stderr);
  }

  fflush(stderr);
  abort();
}

#elif defined(PLATFORM_IS_WINDOWS)

void ponyint_assert_fail(const char* expr, const char* file, size_t line,
  const char* func)
{
  while(atomic_exchange_explicit(&assert_guard, true, memory_order_acq_rel))
  {
    // If the guard is already set, an assertion fired in another thread. The
    // things here aren't thread safe, so we just start an infinite loop.
    Sleep(1);
  }

  fprintf(stderr, "%s:" __zu ": %s: Assertion `%s` failed.\n\n", file, line,
    func, expr);

  HANDLE process = GetCurrentProcess();
  HANDLE thread = GetCurrentThread();

  CONTEXT context;
  memset(&context, 0, sizeof(context));
  context.ContextFlags = CONTEXT_ALL;
  RtlCaptureContext(&context);
  SymInitialize(process, NULL, true);

  STACKFRAME64 frame;
  memset(&frame, 0, sizeof(frame));
  frame.AddrPC.Mode = AddrModeFlat;
  frame.AddrStack.Mode = AddrModeFlat;
  frame.AddrFrame.Mode = AddrModeFlat;

#  ifdef _M_IX86
  frame.AddrPC.Offset = context.Eip;
  frame.AddrStack.Offset = context.Esp;
  frame.AddrFrame.Offset = context.Ebp;
  DWORD machine = IMAGE_FILE_MACHINE_I386;
#  elif defined(_M_X64)
  frame.AddrPC.Offset = context.Rip;
  frame.AddrStack.Offset = context.Rsp;
  frame.AddrFrame.Offset = context.Rbp;
  DWORD machine = IMAGE_FILE_MACHINE_AMD64;
#  endif

  fputs("Backtrace:\n", stderr);

  while(true)
  {
    SetLastError(0);
    if(!StackWalk64(machine, process, thread, &frame, &context, 0,
      SymFunctionTableAccess64, SymGetModuleBase64, 0))
      break;

    DWORD64 addr = frame.AddrFrame.Offset;
    if(addr == 0)
      break;

    union
    {
      SYMBOL_INFO sym;
      BYTE buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
    } sym_buf;
    PSYMBOL_INFO symbol = &sym_buf.sym;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    if(SymFromAddr(process, frame.AddrPC.Offset, NULL, symbol))
    {
      fprintf(stderr, "  (%s) [%p]\n", symbol->Name,
        (void*)frame.AddrPC.Offset);
    } else {
      fprintf(stderr, "  () [%p]\n", (void*)frame.AddrPC.Offset);
    }
  }

  if(strcmp(PONY_BUILD_CONFIG, "release") == 0)
  {
    fputs("This is an optimised version of ponyc: the backtrace may be "
      "imprecise or incorrect.\nUse a debug version to get more meaningful "
      "information.\n", stderr);
  }

  SymCleanup(process);
  fflush(stderr);
  abort();
}

#endif
