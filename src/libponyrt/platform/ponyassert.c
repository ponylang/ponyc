#define PONY_WANT_ATOMIC_DEFS

#include "ponyassert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef PLATFORM_IS_POSIX_BASED
#  include <unistd.h>
#  ifdef PLATFORM_IS_MACOSX
#    include <execinfo.h>
#  else
#    define UNW_LOCAL_ONLY
#    include <libunwind.h>
#  endif
#else
#  include <Windows.h>
#  include <DbgHelp.h>
#endif

#include <pony/detail/atomics.h>

static void print_backtrace(FILE* out)
{
  fputs("Backtrace:\n", out);

#ifdef PLATFORM_IS_MACOSX
  void* addrs[256];
  int depth = backtrace(addrs, 256);
  char** strings = backtrace_symbols(addrs, depth);

  for(int i = 0; i < depth; i++)
    fprintf(stderr, "  %s\n", strings[i]);

#elif defined(PLATFORM_IS_POSIX_BASED)
  unw_context_t context;
  unw_getcontext(&context);

  unw_cursor_t cursor;
  unw_init_local(&cursor, &context);

  while(unw_step(&cursor) > 0)
  {
    unw_word_t pc;
    unw_get_reg(&cursor, UNW_REG_IP, &pc);

    if(pc == 0)
      break;

    char sym[1024];
    unw_word_t offset;

    if(unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0)
      fprintf(out, "  (%s+%p) [%p]\n", sym, (void*)offset, (void*)pc);
    else
      fprintf(out, "  (?\?) [%p]\n", (void*)pc);
  }

#else
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
      fprintf(out, "  (%s) [%p]\n", symbol->Name,
        (void*)frame.AddrPC.Offset);
    } else {
      fprintf(out, "  (?\?) [%p]\n", (void*)frame.AddrPC.Offset);
    }
  }

  SymCleanup(process);

#endif
  if(strcmp(PONY_BUILD_CONFIG, "release") == 0)
  {
    fputs("This is an optimised version of ponyc: the backtrace may be "
      "imprecise or incorrect.\nUse a debug version to get more meaningful "
      "information.\n", out);
  }
}

static PONY_ATOMIC(bool) assert_guard = false;

void ponyint_assert_fail(const char* expr, const char* file, size_t line,
  const char* func)
{
  while(atomic_exchange_explicit(&assert_guard, true, memory_order_acq_rel))
  {
    // If the guard is already set, an assertion fired in another thread. The
    // things here aren't thread safe, so we just start an infinite loop.
#ifdef PLATFORM_IS_POSIX_BASED
    struct timespec ts = {1, 0};
    nanosleep(&ts, NULL);
#else
    Sleep(1000);
#endif
  }

  fprintf(stderr, "%s:" __zu ": %s: Assertion `%s` failed.\n\n", file, line,
    func, expr);
  print_backtrace(stderr);
  fflush(stderr);
  abort();
}
