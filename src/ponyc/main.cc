// ponyc — the program users run. It starts the compiler (ponyc-compiler, in
// the same directory) with the same arguments.
//
// POSIX: the compiler hands the link of the last package back to this
// process (src/common/ponylink_job.h describes how), and this process runs
// ponylink for it once the compiler has exited, so the compiler's memory has
// been returned before the linker starts. SIGINT, SIGQUIT, SIGTERM or SIGHUP
// sent to this process reaches the compiler, or the ponylink this process
// runs, whichever is running, and this process then ends by the same signal.
//
// Windows: the compiler links everything itself; this process only waits for
// it and exits with its exit code.

#include <platform.h>
#include "ponyc_exit.h"

// The exit status for a failure of the driver itself or of ponylink, the same
// as for a failed build.
static const int exit_failed = PONYC_EXIT_FAILED;

#ifdef PLATFORM_IS_POSIX_BASED

#include "exe_path.h"
#include "forward_signals.h"
#include "ponylink_job.h"
#include "run_command.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

// How the driver ends, decided from how its children ended.
struct outcome_t
{
  int status = 0;
  // Nonzero: end by this signal instead of exiting with status.
  int signal = 0;
};

static outcome_t exited(int status)
{
  outcome_t o;
  o.status = status;
  return o;
}

static outcome_t signaled(int sig)
{
  outcome_t o;
  o.signal = sig;
  return o;
}

// The outcome for a ponylink run that didn't succeed: the same signal if one
// killed it, and failed otherwise.
static outcome_t link_outcome(pony_command_result_t result)
{
  if(result.kind == pony_command_result_t::SIGNALED)
    return signaled(result.value);

  return exited(exit_failed);
}

// Whether to describe a child killed by sig. Nothing is printed for SIGPIPE,
// which ends a program whose output reader has gone away, or for a signal the
// driver received and passed on.
static bool should_report_signal(int sig)
{
  return (sig != SIGPIPE) && (pony_forward_received() == 0);
}

[[noreturn]] static void finish(outcome_t outcome)
{
  // A terminating signal arriving after the check below would otherwise be
  // recorded and then lost.
  pony_forward_block();

  // A terminating signal sent to the driver wins over how its children ended.
  int sig = pony_forward_received();

  if(sig == 0)
    sig = outcome.signal;

  if(sig != 0)
    pony_forward_reraise(sig);

  fflush(NULL);
  exit(outcome.status);
}

// Make sure fds 0, 1 and 2 are open, so that pipe() can't return one of them
// and a child's standard stream can't be the handoff pipe.
static bool open_standard_fds()
{
  for(int fd = 0; fd <= 2; fd++)
  {
    if((fcntl(fd, F_GETFD) != -1) || (errno != EBADF))
      continue;

    int opened = open("/dev/null", (fd == 0) ? O_RDONLY : O_WRONLY);

    if(opened != fd)
    {
      if(fcntl(2, F_GETFD) != -1)
        fprintf(stderr, "Error:\nponyc: unable to open /dev/null for closed"
          " fd %d: %s\n", fd, strerror(errno));

      return false;
    }
  }

  return true;
}

// Split a job into its strings, without PONYC_JOB_END. Returns false if it
// isn't a well-formed job: every string NUL-terminated, ponylink's absolute
// path first, then PONYLINK_JOB_FLAG, and PONYC_JOB_END last.
static bool split_job(const std::string& data,
  std::vector<std::string>& strings)
{
  if(data.empty() || (data.back() != '\0'))
    return false;

  size_t start = 0;
  while(start < data.size())
  {
    size_t end = data.find('\0', start);
    strings.push_back(data.substr(start, end - start));
    start = end + 1;
  }

  if((strings.size() < 3) || strings[0].empty() || (strings[0][0] != '/') ||
    (strings[1] != PONYLINK_JOB_FLAG) || (strings.back() != PONYC_JOB_END))
    return false;

  strings.pop_back();
  return true;
}

// ponylink's argument vector for the job, NULL-terminated, with --abandon
// right after PONYLINK_JOB_FLAG when abandon is set. The pointers stay valid
// while the job does.
static std::vector<const char*> job_argv(const std::vector<std::string>& job,
  bool abandon)
{
  std::vector<const char*> argv;
  argv.push_back(job[0].c_str());
  argv.push_back(job[1].c_str());

  if(abandon)
    argv.push_back(PONYLINK_OPT_ABANDON);

  for(size_t i = 2; i < job.size(); i++)
    argv.push_back(job[i].c_str());

  argv.push_back(NULL);
  return argv;
}

// Remove the job's temporary files without linking. ponylink starts with the
// four forwarded signals blocked, so a further terminating signal can't end it
// before it has set them to be ignored. A ponylink that can't start can't
// remove them either; they stay.
static void abandon_job(const std::vector<std::string>& job)
{
  std::vector<const char*> argv = job_argv(job, true);
  pony_forward_spawn_t started = pony_forward_spawn(argv[0], argv.data(),
    PONY_FORWARD_ALWAYS, PONY_FORWARD_BLOCK_FORWARDED);

  if(started.kind == pony_forward_spawn_t::STARTED)
    pony_forward_wait(started);
}

int main(int argc, char* argv[])
{
  if(!open_standard_fds())
    return exit_failed;

  char dir[FILENAME_MAX];
  if(!pony_exe_path_directory(dir, argv[0]))
  {
    fprintf(stderr, "Error:\nponyc: unable to determine ponyc's own path\n");
    return exit_failed;
  }

  std::string compiler_path = std::string(dir) + PONY_COMPILER_NAME;

  if(!pony_forward_start())
  {
    fprintf(stderr, "Error:\nponyc: unable to set up signal handling: %s\n",
      strerror(errno));
    return exit_failed;
  }

  int fds[2];
  if((pipe(fds) != 0) || (fcntl(fds[0], F_SETFD, FD_CLOEXEC) == -1))
  {
    fprintf(stderr, "Error:\nponyc: unable to create a pipe: %s\n",
      strerror(errno));
    return exit_failed;
  }

  std::string handoff = PONYC_LINK_HANDOFF_OPT + std::to_string(fds[1]);

  // argv[0] stays as the user gave it, for the compiler's messages and for
  // finding its own path where that depends on argv[0].
  std::vector<const char*> compiler_argv;
  compiler_argv.push_back(argv[0]);
  compiler_argv.push_back(handoff.c_str());

  for(int i = 1; i < argc; i++)
    compiler_argv.push_back(argv[i]);

  compiler_argv.push_back(NULL);

  // A terminating signal received since the handlers were installed stops
  // the compiler from starting; finish() then ends the driver by it.
  pony_forward_spawn_t started = pony_forward_spawn(compiler_path.c_str(),
    compiler_argv.data(), PONY_FORWARD_UNLESS_SIGNALED,
    PONY_FORWARD_INHERITED_MASK);
  close(fds[1]);

  if(started.kind == pony_forward_spawn_t::CANCELLED)
  {
    close(fds[0]);
    finish(exited(exit_failed));
  }

  if(started.kind == pony_forward_spawn_t::NOT_STARTED)
  {
    fprintf(stderr,
      "Error:\nponyc: unable to start the ponyc compiler at %s: %s\n",
      compiler_path.c_str(), strerror(started.error));
    close(fds[0]);
    finish(exited(exit_failed));
  }

  pid_t pid = started.pid;

  std::string data;
  bool read_ok = true;
  int read_errno = 0;
  char buf[4096];

  while(true)
  {
    ssize_t n = read(fds[0], buf, sizeof(buf));

    if(n > 0)
    {
      data.append(buf, (size_t)n);
      continue;
    }

    if(n == 0)
      break;

    if(errno == EINTR)
      continue;

    read_ok = false;
    read_errno = errno;
    break;
  }

  // Closed before waiting: after a read error, a compiler still writing gets
  // EPIPE and gives up its link instead of blocking.
  close(fds[0]);
  pony_command_result_t compiled = pony_forward_wait(started);

  // A read error can come after a complete job, which then still names
  // files to remove.
  std::vector<std::string> job;
  bool have_job = !data.empty() && split_job(data, job);

  // Each path below removes the job's files before printing: a write to a
  // closed stderr pipe ends the driver by SIGPIPE.
  if(compiled.kind == pony_command_result_t::NOT_WAITED)
  {
    if(have_job)
      abandon_job(job);

    fprintf(stderr, "Error:\nponyc: unable to wait for the compiler: %s\n",
      strerror(compiled.value));
    finish(exited(exit_failed));
  }

  if(!compiled.succeeded())
  {
    if(have_job)
      abandon_job(job);

    if(compiled.kind == pony_command_result_t::SIGNALED)
    {
      // The driver then dies of the same signal, without a core dump, so this
      // line is the only one that names the process that died.
      if(should_report_signal(compiled.value))
      {
        std::string name = std::string(PONY_COMPILER_NAME) + " (pid " +
          std::to_string((int)pid) + ")";
        fprintf(stderr, "Error:\nponyc: %s; to debug the compiler, run %s with"
          " the same arguments\n", pony_describe_command_status(compiled,
          name.c_str(), compiler_path.c_str()).c_str(), compiler_path.c_str());
      }

      finish(signaled(compiled.value));
    }

    finish(exited(compiled.value));
  }

  if(!read_ok)
  {
    if(have_job)
      abandon_job(job);

    fprintf(stderr, "Error:\nponyc: unable to read the link from the compiler:"
      " %s\n", strerror(read_errno));
    finish(exited(exit_failed));
  }

  if(data.empty())
    finish(exited(0));

  if(!have_job)
  {
    fprintf(stderr, "Error:\nponyc: internal error: the compiler handed over a"
      " malformed link\n");
    finish(exited(exit_failed));
  }

  std::vector<const char*> link_argv = job_argv(job, false);
  pony_forward_spawn_t link_started = pony_forward_spawn(link_argv[0],
    link_argv.data(), PONY_FORWARD_UNLESS_SIGNALED,
    PONY_FORWARD_INHERITED_MASK);
  pony_command_result_t linked;

  switch(link_started.kind)
  {
    case pony_forward_spawn_t::CANCELLED:
      abandon_job(job);
      finish(exited(exit_failed));

    case pony_forward_spawn_t::NOT_STARTED:
      linked = pony_command_result(pony_command_result_t::NOT_STARTED,
        link_started.error);
      break;

    case pony_forward_spawn_t::STARTED:
      linked = pony_forward_wait(link_started);
      break;
  }

  if(linked.succeeded())
    finish(exited(0));

  bool link_exited = (linked.kind == pony_command_result_t::EXITED);
  bool usage = link_exited && (linked.value == PONYLINK_EXIT_USAGE);

  // --abandon removes only the temporary files, never the executable or the
  // C shim objects, so it is safe after any failure, whatever ponylink
  // removed itself. A ponylink that rejected the job's options would reject
  // them with --abandon too.
  if(!usage)
    abandon_job(job);

  // The driver reports the ways ponylink can end without printing why.
  if(usage)
  {
    fprintf(stderr, "Error:\nponyc: internal error: ponylink at %s rejected"
      " its options\n", job[0].c_str());
  } else if(!(link_exited && ponylink_printed_why(linked.value)) &&
    ((linked.kind != pony_command_result_t::SIGNALED) ||
      should_report_signal(linked.value))) {
    fprintf(stderr, "Error:\nponyc: %s\n",
      pony_describe_link_status(linked, job[0].c_str()).c_str());
  }

  finish(link_outcome(linked));
}

#elif defined(PLATFORM_IS_WINDOWS)

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

// The compiler and this process share the console, so a Ctrl-C reaches the
// compiler directly. Ignoring it here keeps this process up to pass on the
// compiler's exit code.
static BOOL WINAPI ctrl_handler(DWORD type)
{
  (void)type;
  return TRUE;
}

static int fail(const wchar_t* msg)
{
  fwprintf(stderr, L"Error:\nponyc: %ls (error %lu)\n", msg, GetLastError());
  return exit_failed;
}

int wmain(void)
{
  wchar_t path[MAX_PATH];
  DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);
  if((n == 0) || (n >= MAX_PATH))
    return fail(L"unable to determine ponyc's own path");

  wchar_t* last = wcsrchr(path, L'\\');
  if(last == NULL)
    return fail(L"ponyc's own path has no directory");

  last[1] = L'\0';
  const wchar_t* name = L"" PONY_COMPILER_NAME;
  if((wcslen(path) + wcslen(name)) >= MAX_PATH)
    return fail(L"the compiler's path is too long");

  wcscat(path, name);

  SetConsoleCtrlHandler(ctrl_handler, TRUE);

  // A job object that kills the compiler when this process ends, however it
  // ends. The compiler starts suspended so it can't run before it is in the
  // job. The job object is best effort: without it the compiler still runs,
  // but outlives a killed driver.
  HANDLE job = CreateJobObjectW(NULL, NULL);
  if(job != NULL)
  {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION ji;
    ZeroMemory(&ji, sizeof(ji));
    ji.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &ji,
      sizeof(ji));
  }

  // The command line is passed on unchanged, program name included, so the
  // compiler parses exactly the arguments the user gave, quoting and all.
  const wchar_t* command_line = GetCommandLineW();
  size_t len = wcslen(command_line) + 1;
  wchar_t* cmd = (wchar_t*)malloc(len * sizeof(wchar_t));
  if(cmd == NULL)
  {
    fwprintf(stderr, L"Error:\nponyc: out of memory\n");
    return exit_failed;
  }
  memcpy(cmd, command_line, len * sizeof(wchar_t));

  STARTUPINFOW si;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi;

  if(!CreateProcessW(path, cmd, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL,
    &si, &pi))
  {
    fwprintf(stderr,
      L"Error:\nponyc: unable to start the ponyc compiler at %ls"
      L" (error %lu)\n", path, GetLastError());
    free(cmd);
    return exit_failed;
  }

  free(cmd);

  if(job != NULL)
    AssignProcessToJobObject(job, pi.hProcess);

  ResumeThread(pi.hThread);
  CloseHandle(pi.hThread);
  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD code = (DWORD)exit_failed;
  if(!GetExitCodeProcess(pi.hProcess, &code))
  {
    int result = fail(L"unable to read the compiler's exit code");
    CloseHandle(pi.hProcess);
    return result;
  }

  CloseHandle(pi.hProcess);
  ExitProcess(code);
}

#endif
