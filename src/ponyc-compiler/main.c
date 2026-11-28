#include "../libponyc/ponyc.h"
#include "../libponyc/ast/parserapi.h"
#include "../libponyc/ast/bnfprint.h"
#include "../libponyc/pkg/package.h"
#include "../libponyc/pkg/buildflagset.h"
#include "../libponyc/pass/pass.h"
#include "../libponyc/pass/timing.h"
#include "../libponyc/options/options.h"
#include "../libponyc/ast/stringtab.h"
#include "../libponyc/ast/treecheck.h"
#include <platform.h>
#include "../libponyrt/mem/pool.h"
#include "../common/ponyc_exit.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef PLATFORM_IS_POSIX_BASED
#  include <errno.h>
#  include <fcntl.h>
#  include <sys/ioctl.h>
#  include <poll.h>
#  include <signal.h>
#  include <sys/stat.h>
#  include <unistd.h>
#  include "../common/ponylink_job.h"
#endif

#if defined(PLATFORM_IS_LINUX)
#  include <sys/prctl.h>
#elif defined(PLATFORM_IS_FREEBSD)
#  include <sys/procctl.h>
#endif

static size_t get_width()
{
  size_t width = 80;
#ifdef PLATFORM_IS_WINDOWS
  if(_isatty(_fileno(stdout)))
  {
    CONSOLE_SCREEN_BUFFER_INFO info;

    if(GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
    {
      int cols = info.srWindow.Right - info.srWindow.Left + 1;

      if(cols > width)
        width = cols;
    }
  }
#else
  if(isatty(STDOUT_FILENO))
  {
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
    {
      if(ws.ws_col > width)
        width = ws.ws_col;
    }
  }
#endif
  return width;
}

static bool compile_module(const char* path, pass_opt_t* opt,
  bool print_program_ast, bool print_module_ast)
{
  ast_t* program = program_load(path, opt);

  if(program == NULL)
    return false;

  if(print_program_ast)
    ast_fprint(stderr, program, opt->ast_print_width);

  if(print_module_ast)
    ast_fprint(stderr, ast_child(program), opt->ast_print_width);

  bool ok = generate_passes(program, opt);
  ast_free(program);
  return ok;
}

#ifdef PLATFORM_IS_POSIX_BASED
typedef enum
{
  // No handoff option.
  LINK_HANDOFF_NONE,
  // The option named a usable pipe.
  LINK_HANDOFF_TAKEN,
  // The option was bad; an error has been printed.
  LINK_HANDOFF_INVALID
} link_handoff_t;

// End this process when ponyc (the driver), its parent, ends: once the
// driver is gone, nothing would run the link or read the job. Linux and
// FreeBSD only; elsewhere a compiler whose driver was killed runs to the end
// and then finds the pipe closed.
static void die_with_parent(pid_t parent, int fd)
{
#if defined(PLATFORM_IS_LINUX)
  prctl(PR_SET_PDEATHSIG, SIGKILL);
#elif defined(PLATFORM_IS_FREEBSD)
  int sig = SIGKILL;
  procctl(P_PID, 0, PROC_PDEATHSIG_CTL, &sig);
#else
  (void)parent;
  (void)fd;
  return;
#endif

#if defined(PLATFORM_IS_LINUX) || defined(PLATFORM_IS_FREEBSD)
  // The parent may have ended before the request above. Then this process
  // has a new parent, or the pipe has no reader left.
  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = POLLOUT;
  pfd.revents = 0;

  if((getppid() != parent) ||
    ((poll(&pfd, 1, 0) == 1) && ((pfd.revents & (POLLERR | POLLHUP)) != 0)))
    exit(PONYC_EXIT_FAILED);
#endif
}

// Take the handoff option ponyc (the driver) passes as the first argument, if
// it is there, and remove it from argv so that nothing after this sees it.
// Sets *fd to the pipe's fd when the result is LINK_HANDOFF_TAKEN.
static link_handoff_t take_link_handoff(int* argc, char* argv[], int* fd)
{
  size_t prefix_len = strlen(PONYC_LINK_HANDOFF_OPT);

  if((*argc < 2) || (strncmp(argv[1], PONYC_LINK_HANDOFF_OPT, prefix_len) != 0))
    return LINK_HANDOFF_NONE;

  const char* value = argv[1] + prefix_len;
  char* end;
  errno = 0;
  long n = strtol(value, &end, 10);

  // The driver opens fds 0 to 2 before it creates the pipe, so a handoff on
  // one of them is a mistake that would send the job to the terminal.
  if((*value < '0') || (*value > '9') || (*end != '\0') || (errno != 0) ||
    (n <= 2) || (n > INT32_MAX))
  {
    fprintf(stderr, "Error:\nponyc-compiler: invalid %s%s\n",
      PONYC_LINK_HANDOFF_OPT, value);
    return LINK_HANDOFF_INVALID;
  }

  // The ponylink processes for earlier packages must not inherit the pipe.
  int flags = fcntl((int)n, F_GETFD);
  struct stat st;

  if((flags == -1) || (fcntl((int)n, F_SETFD, flags | FD_CLOEXEC) == -1) ||
    (fstat((int)n, &st) != 0) || !S_ISFIFO(st.st_mode))
  {
    fprintf(stderr, "Error:\nponyc-compiler: %s%ld is not an open pipe\n",
      PONYC_LINK_HANDOFF_OPT, n);
    return LINK_HANDOFF_INVALID;
  }

  for(int i = 1; i < (*argc - 1); i++)
    argv[i] = argv[i + 1];

  (*argc)--;
  argv[*argc] = NULL;
  *fd = (int)n;
  return LINK_HANDOFF_TAKEN;
}
#endif

int main(int argc, char* argv[])
{
  int handoff_fd = -1;

#ifdef PLATFORM_IS_POSIX_BASED
  // Read before die_with_parent, which checks that the parent hasn't changed.
  pid_t parent = getppid();

  switch(take_link_handoff(&argc, argv, &handoff_fd))
  {
    case LINK_HANDOFF_NONE:
      break;

    case LINK_HANDOFF_TAKEN:
      die_with_parent(parent, handoff_fd);
      break;

    case LINK_HANDOFF_INVALID:
      return PONYC_EXIT_FAILED;
  }
#endif

  pass_opt_t opt;
  pass_opt_init(&opt);

  opt.release = true;
  opt.output = ".";
  opt.ast_print_width = get_width();
  opt.argv0 = argv[0];

  // we collect the entire argument string for LLVM debugging info
  // see init_module() in src/libponyc/codegen.c
  size_t args_size = 1;
  for (int i = 1; i < argc; i++)
    args_size += strlen(argv[i]) + 1;
  opt.all_args = (const char*)calloc(args_size, sizeof(const char));
  size_t size_left = args_size;
  for (int i = 1; i < argc; i++)
  {
    strncat((char*)opt.all_args, argv[i], size_left);
    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstringop-overflow"
    #endif

    strncat((char*)opt.all_args, " ", 1);

    #ifdef __GNUC__
    #pragma GCC diagnostic pop
    #endif

    size_left -= strlen(argv[i]) + 1;
  }

  ponyc_opt_process_t exit_code;
  bool print_program_ast;
  bool print_module_ast;

  opt_state_t s;
  ponyint_opt_init(ponyc_opt_std_args(), &s, &argc, argv);

  exit_code = ponyc_opt_process(&s, &opt, &print_program_ast,
                  &print_module_ast);

  if(exit_code != CONTINUE)
  {
    if(exit_code == EXIT_255)
      errors_print(opt.check.errors);

    pass_opt_done(&opt);
    free((void*)opt.all_args);
    return exit_code == EXIT_255 ? PONYC_EXIT_FAILED : 0;
  }

#ifdef PLATFORM_IS_POSIX_BASED
  if(opt.verbosity >= VERBOSITY_TOOL_INFO)
    fprintf(stderr, "compiler pid %d, parent %d\n", (int)getpid(),
      (int)getppid());
#endif

  // An init failure is a failed run: without this, ok stays true and a report
  // written below would claim a build succeeded that never started.
  bool ok = ponyc_init(&opt);

  if(ok)
  {
    if(argc == 1)
    {
      opt.defer_link = (handoff_fd != -1);
      ok &= compile_module(".", &opt, print_program_ast, print_module_ast);
    } else {
      for(int i = 1; i < argc; i++)
      {
        // Hand off only the last package's link.
        opt.defer_link = (handoff_fd != -1) && ok && (i == (argc - 1));
        ok &= compile_module(argv[i], &opt, print_program_ast,
          print_module_ast);
      }
    }
  }

  // Write the job before printing anything or tearing down: a compiler that
  // dies after this, of SIGPIPE while printing or during teardown, has already
  // handed it over, and the driver removes the link's files when it sees the
  // compiler fail.
  link_job_t* link = ponyc_take_pending_link(&opt);

#ifdef PLATFORM_IS_POSIX_BASED
  if(link != NULL)
  {
    if(!ok)
    {
      ponyc_abandon_link(link);
    } else if(ponyc_write_link_job(link, handoff_fd)) {
      close(handoff_fd);
    } else {
      fprintf(stderr, "Error:\nponyc-compiler: unable to hand the link to"
        " ponyc: %s\n", strerror(errno));
      ok = false;
    }
  }
#else
  // Links are never deferred here: the Windows driver takes no link.
  ponyc_abandon_link(link);
#endif

  if(!ok && errors_get_count(opt.check.errors) == 0)
    printf("Error: internal failure not reported\n");

  // Print the errors before the timings, not with the rest of the teardown in
  // ponyc_shutdown: a table is tens of lines, and the diagnostics have to come
  // out from under it.
  errors_print(opt.check.errors);

  // Report and free the timings before ponyc_shutdown, which tears LLVM down
  // (timing.h explains why that ordering matters).
  if(opt.timers != NULL)
  {
    // opt.triple is the effective target triple, resolved by ponyc_init before
    // any compilation, so a build that failed later still records what it was
    // aimed at.
    pass_timers_set_report_meta(opt.timers, ok, opt.triple);

    if(!pass_timers_report(opt.timers))
      ok = false;

    pass_timers_free(opt.timers);
    opt.timers = NULL;
  }

  ponyc_shutdown(&opt);
  pass_opt_done(&opt);
  free((void*)opt.all_args);

  return ok ? 0 : PONYC_EXIT_FAILED;
}
