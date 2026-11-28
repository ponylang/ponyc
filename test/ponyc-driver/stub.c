// Stands in for the compiler (ponyc-compiler) and for ponylink, so that
// cmake/RunPonycDriver.cmake can check what ponyc (the driver) does with each
// way they can end. A copy runs as ponylink when its first argument is
// --ponyc-job, and as the compiler otherwise.
//
// Both roles append to the file named by STUB_LOG: their argv, one
// "<role> argv[<i>]=<arg>" line each, then "<role> <SIG>=default|ignore|
// handler" for SIGINT, SIGQUIT, SIGTERM, SIGHUP and SIGPIPE as they found them
// at startup, then "<role> blocked=<SIG>,..." for those of the five and
// SIGUSR1 that were blocked, then "<role> fd<N>=open|closed" for fds 0 to 2,
// then "<role> extra-fds=<fd>,..." for the open fds from 3 to 255, other than
// the compiler's handoff fd.
//
// The compiler then checks that its first argument is --link-handoff=<fd>
// naming a pipe on fd 3 or above (exit 99 otherwise), and does, in order:
//   STUB_COMPILER_JOB=valid|unterminated|truncated|wrong-second|
//                     empty-first|relative-first|short|long
//       write that kind of job to the pipe. A valid one names STUB_PONYLINK
//       as ponylink. The others differ from it in one way: unterminated lacks
//       PONYC_JOB_END; truncated lacks the final NUL; wrong-second has
//       --bogus for PONYLINK_JOB_FLAG; empty-first has an empty ponylink
//       path; relative-first has the relative path "ponylink"; short has only
//       the ponylink path and PONYC_JOB_END; long adds 2000 --remove options
//       naming /nonexistent/stub-long-job-remove-<i>. Unset: write nothing.
//   STUB_COMPILER_LINGER_MS=N
//       wait N milliseconds, then log "compiler exiting".
//   STUB_COMPILER_SIGNAL_PARENT=SIG
//       ignore SIG, then send it to the parent process.
//   STUB_COMPILER_WAIT=1
//       log "compiler received <SIG>" for each of SIGINT, SIGQUIT, SIGTERM,
//       SIGHUP it gets and die of it, create the file named by STUB_READY,
//       and wait for a signal, until 60 seconds after the stub started. With
//       STUB_COMPILER_ON_TERM=SIG, SIGTERM is logged and then the stub raises
//       SIG instead.
//   STUB_COMPILER_KILL=SIG
//       die of SIG.
//   STUB_COMPILER_EXIT=N
//       exit with status N (default 0).
// ponylink run with --abandon right after --ponyc-job exits 0, or 1 if
// STUB_PONYLINK_EXIT is 1: a real ponylink that rejects the job's options
// rejects them with --abandon too. Otherwise it does:
//   STUB_PONYLINK_WAIT=1      as STUB_COMPILER_WAIT, logging "ponylink
//                             received <SIG>".
//   STUB_PONYLINK_KILL=SIG    die of SIG.
//   STUB_PONYLINK_EXIT=N      exit with status N (default 0).

#include "signals.h"
#include "../../src/common/ponylink_job.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define LONG_JOB_REMOVES 2000

static int log_fd = -1;
static int on_term = 0;
static const char* received_prefix = "compiler received ";

static void log_line(const char* format, ...)
{
  if(log_fd == -1)
    return;

  char buf[4096];
  va_list ap;
  va_start(ap, format);
  int n = vsnprintf(buf, sizeof(buf) - 1, format, ap);
  va_end(ap);

  if(n < 0)
    return;

  if((size_t)n > (sizeof(buf) - 2))
    n = (int)(sizeof(buf) - 2);

  buf[n++] = '\n';
  ssize_t ignored = write(log_fd, buf, (size_t)n);
  (void)ignored;
}

static void die_of(int sig)
{
  signal(sig, SIG_DFL);

  sigset_t unblock;
  sigemptyset(&unblock);
  sigaddset(&unblock, sig);
  sigprocmask(SIG_UNBLOCK, &unblock, NULL);

  raise(sig);
  _exit(98);
}

static void received(int sig)
{
  char buf[64];
  const char* name = test_signal_name(sig);
  size_t len = strlen(received_prefix);
  memcpy(buf, received_prefix, len);
  memcpy(buf + len, name, strlen(name));
  len += strlen(name);
  buf[len++] = '\n';
  ssize_t ignored = write(log_fd, buf, len);
  (void)ignored;

  if((sig == SIGTERM) && (on_term != 0))
    die_of(on_term);

  die_of(sig);
}

// Log and die of SIGINT, SIGQUIT, SIGTERM or SIGHUP, create the file named by
// STUB_READY, and wait for one of them.
static void wait_for_signal(void)
{
  const int caught[] = {SIGINT, SIGQUIT, SIGTERM, SIGHUP};
  for(size_t i = 0; i < (sizeof(caught) / sizeof(caught[0])); i++)
    signal(caught[i], received);

  const char* ready = getenv("STUB_READY");
  if(ready != NULL)
    close(open(ready, O_WRONLY | O_CREAT, 0600));

  while(true)
    pause();
}

static int env_signal(const char* name)
{
  const char* value = getenv(name);

  if((value == NULL) || (*value == '\0'))
    return 0;

  return test_signal_number(value);
}

static int env_int(const char* name, int fallback)
{
  const char* value = getenv(name);

  if((value == NULL) || (*value == '\0'))
    return fallback;

  return atoi(value);
}

static void log_startup(const char* role, int argc, char* argv[])
{
  for(int i = 0; i < argc; i++)
    log_line("%s argv[%d]=%s", role, i, argv[i]);

  const int checked[] = {SIGINT, SIGQUIT, SIGTERM, SIGHUP, SIGPIPE};
  const int blockable[] = {SIGINT, SIGQUIT, SIGTERM, SIGHUP, SIGPIPE, SIGUSR1};
  sigset_t mask;
  sigprocmask(SIG_BLOCK, NULL, &mask);
  char blocked[128] = "";

  for(size_t i = 0; i < (sizeof(checked) / sizeof(checked[0])); i++)
  {
    int sig = checked[i];
    struct sigaction sa;
    sigaction(sig, NULL, &sa);

    const char* disposition = "handler";
    if(sa.sa_handler == SIG_DFL)
      disposition = "default";
    else if(sa.sa_handler == SIG_IGN)
      disposition = "ignore";

    log_line("%s %s=%s", role, test_signal_name(sig), disposition);
  }

  for(size_t i = 0; i < (sizeof(blockable) / sizeof(blockable[0])); i++)
  {
    int sig = blockable[i];

    if(sigismember(&mask, sig))
    {
      if(blocked[0] != '\0')
        strcat(blocked, ",");
      strcat(blocked, test_signal_name(sig));
    }
  }

  log_line("%s blocked=%s", role, blocked);
}

static void write_all(int fd, const char* data, size_t len)
{
  while(len > 0)
  {
    ssize_t n = write(fd, data, len);

    if(n < 0)
    {
      if(errno == EINTR)
        continue;

      log_line("compiler write-failed");
      exit(97);
    }

    data += n;
    len -= (size_t)n;
  }
}

static int compiler(int argc, char* argv[])
{
  const char* prefix = PONYC_LINK_HANDOFF_OPT;
  int fd = -1;

  if((argc >= 2) && (strncmp(argv[1], prefix, strlen(prefix)) == 0))
    fd = atoi(argv[1] + strlen(prefix));

  struct stat st;
  if((fd <= 2) || (fstat(fd, &st) != 0) || !S_ISFIFO(st.st_mode))
  {
    log_line("compiler bad-handoff");
    return 99;
  }

  const char* job = getenv("STUB_COMPILER_JOB");
  const char* ponylink = getenv("STUB_PONYLINK");

  if(ponylink == NULL)
    ponylink = "ponylink";

  if(job != NULL)
  {
    // The strings of a job, NUL-terminated: first, second, the options,
    // then PONYC_JOB_END.
    const char* first = ponylink;
    const char* second = PONYLINK_JOB_FLAG;
    const char* options = PONYLINK_OPT_ARGS "\0stub.lldargs";
    size_t options_len = strlen(PONYLINK_OPT_ARGS) + 1 + strlen("stub.lldargs");

    if(strcmp(job, "wrong-second") == 0)
      second = "--bogus";
    else if(strcmp(job, "empty-first") == 0)
      first = "";
    else if(strcmp(job, "relative-first") == 0)
      first = "ponylink";

    // A long job adds LONG_JOB_REMOVES --remove options, making the job
    // larger than a pipe holds.
    bool long_job = (strcmp(job, "long") == 0);
    size_t size = long_job ? (LONG_JOB_REMOVES * 64) + 4096 : 4096;
    char* data = (char*)malloc(size);
    size_t len = 0;
    len += (size_t)snprintf(data + len, size - len, "%s", first) + 1;

    if(strcmp(job, "short") != 0)
    {
      len += (size_t)snprintf(data + len, size - len, "%s", second) + 1;
      memcpy(data + len, options, options_len);
      len += options_len + 1;
      data[len - 1] = '\0';
    }

    for(int i = 0; long_job && (i < LONG_JOB_REMOVES); i++)
    {
      len += (size_t)snprintf(data + len, size - len, "%s",
        PONYLINK_OPT_REMOVE) + 1;
      len += (size_t)snprintf(data + len, size - len,
        "/nonexistent/stub-long-job-remove-%d", i) + 1;
    }

    size_t end_len = strlen(PONYC_JOB_END) + 1;
    memcpy(data + len, PONYC_JOB_END, end_len);
    len += end_len;

    if(strcmp(job, "unterminated") == 0)
      write_all(fd, data, len - end_len);
    else if(strcmp(job, "truncated") == 0)
      write_all(fd, data, len - 1);
    else
      write_all(fd, data, len);

    free(data);
  }

  close(fd);

  int linger = env_int("STUB_COMPILER_LINGER_MS", 0);
  if(linger > 0)
  {
    struct timespec ts;
    ts.tv_sec = linger / 1000;
    ts.tv_nsec = (long)(linger % 1000) * 1000000L;
    while((nanosleep(&ts, &ts) == -1) && (errno == EINTR))
      ;
    log_line("compiler exiting");
  }

  int parent_signal = env_signal("STUB_COMPILER_SIGNAL_PARENT");
  if(parent_signal != 0)
  {
    signal(parent_signal, SIG_IGN);
    kill(getppid(), parent_signal);
  }

  if(env_int("STUB_COMPILER_WAIT", 0) != 0)
  {
    on_term = env_signal("STUB_COMPILER_ON_TERM");
    wait_for_signal();
  }

  int kill_signal = env_signal("STUB_COMPILER_KILL");
  if(kill_signal != 0)
    die_of(kill_signal);

  return env_int("STUB_COMPILER_EXIT", 0);
}

static int ponylink(int argc, char* argv[])
{
  if((argc >= 3) && (strcmp(argv[2], PONYLINK_OPT_ABANDON) == 0))
    return (env_int("STUB_PONYLINK_EXIT", 0) == PONYLINK_EXIT_USAGE) ?
      PONYLINK_EXIT_USAGE : 0;

  if(env_int("STUB_PONYLINK_WAIT", 0) != 0)
  {
    received_prefix = "ponylink received ";
    wait_for_signal();
  }

  int kill_signal = env_signal("STUB_PONYLINK_KILL");
  if(kill_signal != 0)
    die_of(kill_signal);

  return env_int("STUB_PONYLINK_EXIT", 0);
}

int main(int argc, char* argv[])
{
  struct rlimit no_core = {0, 0};
  setrlimit(RLIMIT_CORE, &no_core);

  // A stub left waiting for a signal that never comes dies of SIGALRM rather
  // than hanging the test.
  alarm(60);

  // Checked before the log is opened, which could take a closed fd's number
  // or add an fd.
  bool std_open[3];
  for(int fd = 0; fd <= 2; fd++)
    std_open[fd] = (fcntl(fd, F_GETFD) != -1);

  int handoff = -1;
  const char* prefix = PONYC_LINK_HANDOFF_OPT;
  if((argc >= 2) && (strncmp(argv[1], prefix, strlen(prefix)) == 0))
    handoff = atoi(argv[1] + strlen(prefix));

  char extra[1024] = "";
  for(int fd = 3; fd < 256; fd++)
  {
    if((fd != handoff) && (fcntl(fd, F_GETFD) != -1))
    {
      size_t used = strlen(extra);
      snprintf(extra + used, sizeof(extra) - used, "%s%d",
        (used > 0) ? "," : "", fd);
    }
  }

  const char* log_path = getenv("STUB_LOG");
  if(log_path != NULL)
    log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0600);

  bool is_ponylink = (argc >= 2) && (strcmp(argv[1], PONYLINK_JOB_FLAG) == 0);
  const char* role = is_ponylink ? "ponylink" : "compiler";
  log_startup(role, argc, argv);

  for(int fd = 0; fd <= 2; fd++)
    log_line("%s fd%d=%s", role, fd, std_open[fd] ? "open" : "closed");

  log_line("%s extra-fds=%s", role, extra);

  if(is_ponylink)
    return ponylink(argc, argv);

  return compiler(argc, argv);
}
