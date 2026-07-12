#ifndef lang_stdfd_h
#define lang_stdfd_h

#include <platform.h>
#include <stdbool.h>

PONY_EXTERN_C_BEGIN

#ifdef PLATFORM_IS_WINDOWS

typedef enum
{
  // A console input handle. Reading it means peeking at its input records.
  STDIN_CONSOLE,

  // A pipe. PeekNamedPipe reports what is in it without taking it and without
  // waiting, so a read of one never waits and the backend has something to
  // poll. A socket reports the same file type, but PeekNamedPipe does not
  // answer for one, so a socket on stdin reads as the end of input -- which is
  // what it read as before, when a ReadFile of it failed.
  STDIN_PIPE,

  // A redirected file, NUL, a character device, or no stdin at all. A read of
  // one returns at once, with data or with the end of the input.
  STDIN_OTHER
} stdin_kind_t;

// Work out what stdin is, before the runtime starts its threads. It is fixed
// for a run, and both the asio backend and the read itself use it.
void ponyint_stdin_init();

stdin_kind_t ponyint_stdin_kind();

// True when a read of stdin would return something -- bytes, or the end of the
// input. False when the pipe is open and empty. Never waits, and takes nothing
// out of the pipe. For STDIN_PIPE stdin.
bool ponyint_stdin_pipe_ready();

#endif

PONY_EXTERN_C_END

#endif
