#ifndef PONY_COMMON_RUN_COMMAND_H
#define PONY_COMMON_RUN_COMMAND_H

#include <platform.h>

#ifndef __cplusplus
#  error "run_command.h is C++"
#endif

#ifdef PLATFORM_IS_POSIX_BASED

#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <string>

// Find name, which contains no slash, on PATH as execvp does: an empty entry
// is the current directory, and an unset PATH is the system's default search
// path (confstr's _CS_PATH, or "/usr/bin:/bin" if that fails). Returns 0 and
// sets *found, or returns EACCES if only files that can't be run were found,
// and ENOENT otherwise. Done here rather than by posix_spawnp because some
// libcs (OpenBSD's) report a name that isn't found as a child that exits 127,
// which would read as the program's own status.
static inline int pony_find_on_path(const char* name, std::string* found)
{
  std::string search;
  const char* env = getenv("PATH");

  if(env != NULL)
  {
    search = env;
  } else {
    size_t len = confstr(_CS_PATH, NULL, 0);
    if(len > 0)
    {
      search.resize(len);
      confstr(_CS_PATH, &search[0], len);
      search.resize(len - 1);
    } else {
      search = "/usr/bin:/bin";
    }
  }

  int error = ENOENT;
  size_t start = 0;

  while(true)
  {
    size_t end = search.find(':', start);
    std::string dir = search.substr(start,
      (end == std::string::npos) ? std::string::npos : end - start);
    std::string candidate = (dir.empty() ? std::string(".") : dir) + "/" + name;
    struct stat st;

    if((stat(candidate.c_str(), &st) == 0) && S_ISREG(st.st_mode))
    {
      if(access(candidate.c_str(), X_OK) == 0)
      {
        *found = candidate;
        return 0;
      }

      error = EACCES;
    }

    if(end == std::string::npos)
      return error;

    start = end + 1;
  }
}

// Start path with argv as its arguments, without a shell. path is looked up on
// PATH (pony_find_on_path) when it contains no slash. With a mask, the child
// starts with that signal mask; without one, it inherits the caller's. Returns
// 0 and sets *pid, or returns the error number.
static inline int pony_spawn_command(pid_t* pid, const char* path,
  const char* const argv[], const sigset_t* mask)
{
  extern char** environ;

  // Some posix_spawn implementations report a program that can't be run as a
  // child that exits 127, which would read as the program's own status.
  std::string found;

  if(strchr(path, '/') == NULL)
  {
    int error = pony_find_on_path(path, &found);
    if(error != 0)
      return error;

    path = found.c_str();
  } else if(access(path, X_OK) != 0) {
    return errno;
  }

  posix_spawnattr_t attr;
  posix_spawnattr_t* attrp = NULL;

  if(mask != NULL)
  {
    int error = posix_spawnattr_init(&attr);
    if(error != 0)
      return error;

    attrp = &attr;
    error = posix_spawnattr_setsigmask(attrp, mask);

    if(error == 0)
      error = posix_spawnattr_setflags(attrp, POSIX_SPAWN_SETSIGMASK);

    if(error != 0)
    {
      posix_spawnattr_destroy(attrp);
      return error;
    }
  }

  int error = posix_spawn(pid, path, NULL, attrp, (char* const*)argv, environ);

  if(attrp != NULL)
    posix_spawnattr_destroy(attrp);

  return error;
}

// How running a command ended.
struct pony_command_result_t
{
  enum kind_t
  {
    // It exited; value is its exit status.
    EXITED,
    // A signal killed it; value is the signal number.
    SIGNALED,
    // It couldn't be started; value is the error number.
    NOT_STARTED,
    // It started but couldn't be waited for; value is the error number.
    NOT_WAITED
  };

  kind_t kind;
  int value;

  bool succeeded() const
  {
    return (kind == EXITED) && (value == 0);
  }
};

static inline pony_command_result_t pony_command_result(
  pony_command_result_t::kind_t kind, int value)
{
  pony_command_result_t result;
  result.kind = kind;
  result.value = value;
  return result;
}

// The result for a wait status from waitpid.
static inline pony_command_result_t pony_wait_result(int status)
{
  if(WIFSIGNALED(status))
    return pony_command_result(pony_command_result_t::SIGNALED,
      WTERMSIG(status));

  if(!WIFEXITED(status))
    return pony_command_result(pony_command_result_t::NOT_WAITED, ECHILD);

  return pony_command_result(pony_command_result_t::EXITED,
    WEXITSTATUS(status));
}

// Run argv[0] with argv as its arguments, without a shell, and wait for it.
// argv[0] is looked up on PATH when it contains no slash.
static inline pony_command_result_t pony_run_command(const char* const argv[])
{
  pid_t pid;
  int spawn_error = pony_spawn_command(&pid, argv[0], argv, NULL);

  if(spawn_error != 0)
    return pony_command_result(pony_command_result_t::NOT_STARTED, spawn_error);

  // Retry on EINTR so a signal delivered while waiting isn't taken for a
  // failure of the command.
  int status;
  int result;
  do
  {
    result = waitpid(pid, &status, 0);
  } while((result == -1) && (errno == EINTR));

  if(result == -1)
    return pony_command_result(pony_command_result_t::NOT_WAITED, errno);

  return pony_wait_result(status);
}

// Describe how running a program that didn't succeed ended. `name` names the
// program in the text and `path` is its executable path.
static inline std::string pony_describe_command_status(
  pony_command_result_t result, const char* name, const char* path)
{
  char buf[256];

  switch(result.kind)
  {
    case pony_command_result_t::NOT_STARTED:
      return std::string("unable to start ") + name + " at " + path + ": " +
        strerror(result.value);

    case pony_command_result_t::NOT_WAITED:
      return std::string("unable to wait for ") + name + " at " + path +
        ": " + strerror(result.value);

    case pony_command_result_t::SIGNALED:
    {
      int sig = result.value;
      snprintf(buf, sizeof(buf), "%s was killed by signal %d (%s)", name, sig,
        strsignal(sig));
      std::string msg(buf);

      if(sig == SIGKILL)
        msg += "; the system may be out of memory";

      return msg;
    }

    case pony_command_result_t::EXITED:
      break;
  }

  snprintf(buf, sizeof(buf), "%s failed (exit %d)", name, result.value);
  return std::string(buf);
}

// Describe how a ponylink run that didn't succeed ended. `path` is ponylink's
// executable path.
static inline std::string pony_describe_link_status(
  pony_command_result_t result, const char* path)
{
  std::string msg = pony_describe_command_status(result, "ponylink", path);

  if(result.kind == pony_command_result_t::EXITED)
    msg += "; check linker errors above";

  return msg;
}

#endif

#endif
