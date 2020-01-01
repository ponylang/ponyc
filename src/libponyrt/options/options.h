#ifndef options_options_h
#define options_options_h

#include <stdint.h>

#define OPT_ARG_REQUIRED 1 << 0
#define OPT_ARG_OPTIONAL 1 << 1
#define OPT_ARG_NONE     1 << 2
#define OPT_ARGS_FINISH {NULL, 0, UINT32_MAX, UINT32_MAX}

/* NOTE: if you change any of the argument help details, update the docstrings
 *       in `RuntimeOptions` in the `builtin` package to keep them in sync.
 */
#define PONYRT_HELP \
  "Runtime options for Pony programs (not for use with ponyc):\n" \
  "  --ponymaxthreads Use N scheduler threads. Defaults to the number of\n" \
  "                   cores (not hyperthreads) available.\n" \
  "                   This can't be larger than the number of cores available.\n" \
  "  --ponyminthreads Minimum number of active scheduler threads allowed.\n" \
  "                   Defaults to 0, meaning that all scheduler threads are\n" \
  "                   allowed to be suspended when no work is available.\n" \
  "                   This can't be larger than --ponymaxthreads if provided,\n" \
  "                   or the physical cores available\n" \
  "  --ponynoscale    Don't scale down the scheduler threads.\n" \
  "                   See --ponymaxthreads on how to specify the number of threads\n" \
  "                   explicitly. Can't be used with --ponyminthreads.\n" \
  "  --ponysuspendthreshold\n" \
  "                   Amount of idle time before a scheduler thread suspends\n" \
  "                   itself to minimize resource consumption (max 1000 ms,\n" \
  "                   min 1 ms).\n" \
  "                   Defaults to 1 ms.\n" \
  "  --ponycdinterval Run cycle detection every N ms (max 1000 ms, min 10 ms).\n" \
  "                   Defaults to 100 ms.\n" \
  "  --ponygcinitial  Defer garbage collection until an actor is using at\n" \
  "                   least 2^N bytes. Defaults to 2^14.\n" \
  "  --ponygcfactor   After GC, an actor will next be GC'd at a heap memory\n" \
  "                   usage N times its current value. This is a floating\n" \
  "                   point value. Defaults to 2.0.\n" \
  "  --ponynoyield    Do not yield the CPU when no work is available.\n" \
  "  --ponynoblock    Do not send block messages to the cycle detector.\n" \
  "  --ponypin        Pin scheduler threads to CPU cores. The ASIO thread\n" \
  "                   can also be pinned if `--ponypinasio` is set.\n" \
  "  --ponypinasio    Pin the ASIO thread to a CPU the way scheduler\n" \
  "                   threads are pinned to CPUs. Requires `--ponypin` to\n" \
  "                   be set to have any effect.\n" \
  "  --ponyversion    Print the version of the compiler and exit.\n" \
  "  --ponyhelp       Print the runtime usage options and exit.\n" \
  "\n" \
  "NOTE: These can be programmatically overridden. See the docstring in the\n" \
  "      `RuntimeOptions` struct in the `builtin` package.\n"

typedef struct opt_arg_t
{
  const char* long_opt;
  const char short_opt;
  uint32_t flag;
  uint32_t id;
} opt_arg_t;

typedef struct opt_state_t
{
  const opt_arg_t* args;

  int* argc;
  char** argv;
  char* arg_val;

  //do not touch :-)
  char* opt_start;
  char* opt_end;
  int match_type;
  int idx;
  int remove;
} opt_state_t;

void ponyint_opt_init(const opt_arg_t* args, opt_state_t* s, int* argc,
  char** argv);

/** Find the unsigned identifier of the next option.
 *
 * Special return values:
 *
 * -1 when there are no more options to process.
 * -2 when an error is detected and an error message is printed.
 */
int ponyint_opt_next(opt_state_t* s);

#endif
