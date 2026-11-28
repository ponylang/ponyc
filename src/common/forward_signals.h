#ifndef PONY_COMMON_FORWARD_SIGNALS_H
#define PONY_COMMON_FORWARD_SIGNALS_H

#include <platform.h>

#ifndef __cplusplus
#  error "forward_signals.h is C++"
#endif

#ifdef PLATFORM_IS_POSIX_BASED

#include "run_command.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>

// Signal forwarding for a process that runs its children one at a time and
// must outlive them. A terminating signal (SIGINT, SIGQUIT, SIGTERM, SIGHUP)
// sent to the process is recorded and passed on to the child it is running,
// and once the child has been reaped, the process can end by the same signal
// with pony_forward_reraise. A signal that was ignored when the process started
// stays ignored, in the process and in its children.
//
// The handler's kill() is safe because the stored pid is always a child that
// hasn't been reaped, or 0: a pid can't be reused until its parent reaps it.

struct pony_forward_state_t
{
  volatile sig_atomic_t child;
  volatile sig_atomic_t signal;
  sigset_t inherited_mask;
  // The four forwarded signals.
  sigset_t forwarded;
  // The four forwarded signals and SIGCHLD.
  sigset_t blocked;
};

// An inline function with external linkage: every translation unit of a
// program shares the one state.
inline pony_forward_state_t* pony_forward_state()
{
  static pony_forward_state_t state;
  return &state;
}

static inline void pony_forward_handler(int sig)
{
  int saved_errno = errno;
  pony_forward_state_t* state = pony_forward_state();

  if(state->signal == 0)
    state->signal = sig;

  pid_t child = (pid_t)state->child;
  if(child > 0)
    kill(child, sig);

  errno = saved_errno;
}

static inline void pony_forward_sigchld(int sig)
{
  (void)sig;
}

// Install the handlers. Call once, before starting any child. Returns false if
// a handler couldn't be installed.
static inline bool pony_forward_start()
{
  const int forwarded[] = {SIGINT, SIGQUIT, SIGTERM, SIGHUP};
  pony_forward_state_t* state = pony_forward_state();
  state->child = 0;
  state->signal = 0;

  if(sigprocmask(SIG_BLOCK, NULL, &state->inherited_mask) != 0)
    return false;

  sigemptyset(&state->forwarded);

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  for(int sig : forwarded)
  {
    sigaddset(&state->forwarded, sig);
    sigaddset(&sa.sa_mask, sig);
  }

  state->blocked = state->forwarded;
  sigaddset(&state->blocked, SIGCHLD);

  for(int sig : forwarded)
  {
    struct sigaction old;
    if(sigaction(sig, NULL, &old) != 0)
      return false;

    if(old.sa_handler == SIG_IGN)
      continue;

    // A caught signal goes back to its default action in a child that
    // execs, so the children start with default dispositions.
    sa.sa_handler = pony_forward_handler;
    if(sigaction(sig, &sa, NULL) != 0)
      return false;
  }

  // An inherited SIG_IGN for SIGCHLD would have children reaped automatically
  // and SIGCHLD discarded, and pony_forward_wait sleeps until SIGCHLD
  // arrives.
  sa.sa_handler = pony_forward_sigchld;
  return sigaction(SIGCHLD, &sa, NULL) == 0;
}

// The first terminating signal the process received, or 0.
static inline int pony_forward_received()
{
  return (int)pony_forward_state()->signal;
}

// Block the four forwarded signals in this process. A signal sent after this
// is neither recorded nor forwarded.
static inline void pony_forward_block()
{
  sigprocmask(SIG_BLOCK, &pony_forward_state()->forwarded, NULL);
}

// Whether a child starts even if a terminating signal has been received.
enum pony_forward_when_t
{
  PONY_FORWARD_ALWAYS,
  // Nothing starts if a terminating signal has already been received; the
  // check and the start can't be separated by one.
  PONY_FORWARD_UNLESS_SIGNALED
};

// The signal mask a child starts with.
enum pony_forward_mask_t
{
  // The mask the process started with.
  PONY_FORWARD_INHERITED_MASK,
  // That mask with the four forwarded signals added, so that they stay
  // pending in the child until it handles them.
  PONY_FORWARD_BLOCK_FORWARDED
};

// How starting a child with pony_forward_spawn went.
struct pony_forward_spawn_t
{
  enum kind_t
  {
    // It is running as the current child; pid is its process ID.
    STARTED,
    // It wasn't started because a terminating signal had been received.
    CANCELLED,
    // It couldn't be started; error is the error number.
    NOT_STARTED
  };

  kind_t kind;
  pid_t pid;
  int error;
};

// Start path as the current child.
static inline pony_forward_spawn_t pony_forward_spawn(const char* path,
  const char* const argv[], pony_forward_when_t when, pony_forward_mask_t mask)
{
  pony_forward_state_t* state = pony_forward_state();
  sigset_t old_mask;
  sigprocmask(SIG_BLOCK, &state->blocked, &old_mask);

  pony_forward_spawn_t result;
  result.pid = 0;
  result.error = 0;

  if((when == PONY_FORWARD_UNLESS_SIGNALED) && (state->signal != 0))
  {
    result.kind = pony_forward_spawn_t::CANCELLED;
  } else {
    sigset_t child_mask = state->inherited_mask;

    if(mask == PONY_FORWARD_BLOCK_FORWARDED)
    {
      for(int sig = 1; sig < NSIG; sig++)
      {
        if(sigismember(&state->forwarded, sig))
          sigaddset(&child_mask, sig);
      }
    }

    result.error = pony_spawn_command(&result.pid, path, argv, &child_mask);

    if(result.error == 0)
    {
      state->child = (sig_atomic_t)result.pid;
      result.kind = pony_forward_spawn_t::STARTED;
    } else {
      result.kind = pony_forward_spawn_t::NOT_STARTED;
    }
  }

  sigprocmask(SIG_SETMASK, &old_mask, NULL);
  return result;
}

// Wait for the child pony_forward_spawn started, and stop forwarding signals
// to it. NOT_WAITED with ECHILD if it started nothing.
static inline pony_command_result_t pony_forward_wait(
  const pony_forward_spawn_t& started)
{
  if(started.kind != pony_forward_spawn_t::STARTED)
    return pony_command_result(pony_command_result_t::NOT_WAITED, ECHILD);

  pid_t pid = started.pid;
  pony_forward_state_t* state = pony_forward_state();
  sigset_t old_mask;
  sigprocmask(SIG_BLOCK, &state->blocked, &old_mask);

  sigset_t suspend_mask = state->inherited_mask;
  sigdelset(&suspend_mask, SIGCHLD);

  int status = 0;
  int result;
  int wait_errno = 0;

  while(true)
  {
    result = waitpid(pid, &status, WNOHANG);

    if(result == pid)
      break;

    if((result == -1) && (errno != EINTR))
    {
      wait_errno = errno;
      break;
    }

    if(result == 0)
      sigsuspend(&suspend_mask);
  }

  state->child = 0;
  sigprocmask(SIG_SETMASK, &old_mask, NULL);

  if(result != pid)
    return pony_command_result(pony_command_result_t::NOT_WAITED, wait_errno);

  return pony_wait_result(status);
}

// End the process by sig, without a core dump. Exits with status 255 if the
// signal doesn't end the process.
static inline void pony_forward_reraise(int sig)
{
  fflush(NULL);

  struct rlimit no_core;
  no_core.rlim_cur = 0;
  no_core.rlim_max = 0;
  setrlimit(RLIMIT_CORE, &no_core);

  signal(sig, SIG_DFL);

  sigset_t unblock;
  sigemptyset(&unblock);
  sigaddset(&unblock, sig);
  sigprocmask(SIG_UNBLOCK, &unblock, NULL);

  raise(sig);
  exit(255);
}

#endif

#endif
