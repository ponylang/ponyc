// Starts a command with controlled signal dispositions and file descriptors,
// acts on it while it runs, and prints how it ended.
//
// launcher [options] -- command [args...]
// launcher --fill-fifo PATH
//
// The second form fills the FIFO at PATH, which something must have open for
// reading, so that the next write to it waits until it is read or nothing has
// it open for reading.
//
// Set up before the command starts (every signal in signals.h starts with its
// default action and unblocked, core dumps are off, and only fds 0 to 2 and the
// handoff fd are open, unless an option says otherwise):
//   --ignore SIG          start the command with SIG ignored
//   --block SIG           start the command with SIG blocked
//   --close FD            start the command with FD closed
//   --handoff-closed      insert --link-handoff=<fd> as the command's first
//                         argument, naming the write end of a pipe whose read
//                         end is closed
//   --handoff-pipe        insert --link-handoff=<fd>, naming the write end of a
//                         pipe whose read end the launcher holds open until
//                         --close-handoff, and print "handoff fd <fd>"
//   --handoff-file PATH   insert --link-handoff=<fd>, naming PATH opened for
//                         writing
//   --stderr-no-reader    start the command with stderr on a pipe whose read
//                         end is closed
//   --fill-handoff        fill the --handoff-pipe pipe before the command
//                         starts, so that the command's first write to it
//                         waits until the pipe is read or its read end closed
// Done in order once the command has started:
//   --wait-file PATH      wait until PATH exists
//   --fifo-open PATH      open the FIFO at PATH for writing, waiting until
//                         something opens it for reading, and keep it open
//   --signal SIG          send SIG to the command
//   --sleep-ms N          wait N milliseconds
//   --check-alive         print "alive" if the command hasn't ended
//   --close-handoff       close the read end of the --handoff-pipe pipe
// Once the command has ended, prints "exited N" or "signaled SIG"; a command
// still running 60 seconds after the last action is killed first. After
// --fifo-open, also writes to the FIFO and prints "fifo reader gone" if
// nothing has it open for reading any more, "fifo reader alive" otherwise.

#include "signals.h"
#include "../../src/common/ponylink_job.h"

#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_ITEMS 64
#define TIMEOUT_MS 60000

static void sleep_ms(long ms)
{
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000L;
  while((nanosleep(&ts, &ts) == -1) && (errno == EINTR))
    ;
}

static int signal_arg(const char* name)
{
  int sig = test_signal_number(name);

  if(sig == -1)
  {
    fprintf(stderr, "launcher: unknown signal %s\n", name);
    exit(2);
  }

  return sig;
}

// Write to fd until it would block, leaving fd blocking. Returns false on any
// other error.
static bool fill(int fd)
{
  int flags = fcntl(fd, F_GETFL);
  if((flags == -1) || (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1))
    return false;

  static char buf[65536];
  size_t size = sizeof(buf);

  while(size > 0)
  {
    if(write(fd, buf, size) == -1)
    {
      if(errno != EAGAIN)
        return false;

      size /= 2;
    }
  }

  return fcntl(fd, F_SETFL, flags) != -1;
}

static void give_up(pid_t pid, const char* why)
{
  printf("launcher: %s\n", why);
  fflush(stdout);
  kill(pid, SIGKILL);
  waitpid(pid, NULL, 0);
  exit(2);
}

int main(int argc, char* argv[])
{
  if((argc == 3) && (strcmp(argv[1], "--fill-fifo") == 0))
  {
    int fd = open(argv[2], O_WRONLY | O_NONBLOCK);
    if((fd == -1) || !fill(fd))
    {
      perror("launcher: fill FIFO");
      return 2;
    }

    close(fd);
    return 0;
  }

  int ignore[MAX_ITEMS];
  int ignore_count = 0;
  int block[MAX_ITEMS];
  int block_count = 0;
  int close_fd[MAX_ITEMS];
  int close_count = 0;
  bool handoff_closed = false;
  bool handoff_open = false;
  const char* handoff_file = NULL;
  bool stderr_no_reader = false;
  bool fill_handoff = false;
  int command = -1;

  for(int i = 1; i < argc; i++)
  {
    if(strcmp(argv[i], "--") == 0)
    {
      command = i + 1;
      break;
    }

    if((strcmp(argv[i], "--handoff-closed") == 0))
    {
      handoff_closed = true;
      continue;
    }

    if((strcmp(argv[i], "--handoff-pipe") == 0))
    {
      handoff_open = true;
      continue;
    }

    if((strcmp(argv[i], "--stderr-no-reader") == 0))
    {
      stderr_no_reader = true;
      continue;
    }

    if((strcmp(argv[i], "--fill-handoff") == 0))
    {
      fill_handoff = true;
      continue;
    }

    if((strcmp(argv[i], "--check-alive") == 0) ||
      (strcmp(argv[i], "--close-handoff") == 0))
      continue;

    if((i + 1) >= argc)
    {
      fprintf(stderr, "launcher: %s needs a value\n", argv[i]);
      return 2;
    }

    if(strcmp(argv[i], "--ignore") == 0)
      ignore[ignore_count++] = signal_arg(argv[i + 1]);
    else if(strcmp(argv[i], "--block") == 0)
      block[block_count++] = signal_arg(argv[i + 1]);
    else if(strcmp(argv[i], "--close") == 0)
      close_fd[close_count++] = atoi(argv[i + 1]);
    else if(strcmp(argv[i], "--handoff-file") == 0)
      handoff_file = argv[i + 1];
    else if((strcmp(argv[i], "--wait-file") == 0) ||
      (strcmp(argv[i], "--fifo-open") == 0) ||
      (strcmp(argv[i], "--signal") == 0) ||
      (strcmp(argv[i], "--sleep-ms") == 0))
    {
      // Done once the command has started.
    } else {
      fprintf(stderr, "launcher: unknown option %s\n", argv[i]);
      return 2;
    }

    i++;
  }

  if((command < 0) || (command >= argc))
  {
    fprintf(stderr, "usage: launcher [options] -- command [args...]\n");
    return 2;
  }

  int handoff_pipe[2] = {-1, -1};
  if((handoff_closed || handoff_open) && (pipe(handoff_pipe) != 0))
  {
    perror("launcher: pipe");
    return 2;
  }

  if(fill_handoff && (!handoff_open || !fill(handoff_pipe[1])))
  {
    fprintf(stderr, "launcher: unable to fill the handoff pipe\n");
    return 2;
  }

  fflush(stdout);
  pid_t pid = fork();

  if(pid == -1)
  {
    perror("launcher: fork");
    return 2;
  }

  if(pid == 0)
  {
    for(size_t i = 0; i < TEST_SIGNAL_COUNT; i++)
    {
      if(test_signals[i].number != SIGKILL)
        signal(test_signals[i].number, SIG_DFL);
    }

    for(int i = 0; i < ignore_count; i++)
      signal(ignore[i], SIG_IGN);

    sigset_t mask;
    sigemptyset(&mask);
    for(int i = 0; i < block_count; i++)
      sigaddset(&mask, block[i]);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    struct rlimit no_core = {0, 0};
    setrlimit(RLIMIT_CORE, &no_core);

    // The pipe is made here so that no other process holds its read end.
    int stderr_pipe[2];
    if(stderr_no_reader)
    {
      if(pipe(stderr_pipe) != 0)
      {
        perror("launcher: pipe");
        _exit(127);
      }

      close(stderr_pipe[0]);
      dup2(stderr_pipe[1], STDERR_FILENO);
      close(stderr_pipe[1]);
    }

    int handoff_fd = -1;

    if(handoff_closed || handoff_open)
    {
      close(handoff_pipe[0]);
      handoff_fd = handoff_pipe[1];
    }

    if(handoff_file != NULL)
    {
      handoff_fd = open(handoff_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
      if(handoff_fd == -1)
      {
        perror("launcher: open handoff file");
        _exit(127);
      }
    }

    for(int i = 0; i < close_count; i++)
      close(close_fd[i]);

    // No fd above 2 inherited from whatever started the launcher reaches the
    // command, except the handoff fd.
    for(int fd = 3; fd < 256; fd++)
    {
      if(fd != handoff_fd)
        close(fd);
    }

    int count = argc - command;
    char** args = (char**)calloc((size_t)count + 2, sizeof(char*));
    char handoff[64];
    int n = 0;
    args[n++] = argv[command];

    if(handoff_fd != -1)
    {
      snprintf(handoff, sizeof(handoff), PONYC_LINK_HANDOFF_OPT "%d",
        handoff_fd);
      args[n++] = handoff;
    }

    for(int i = command + 1; i < argc; i++)
      args[n++] = argv[i];

    args[n] = NULL;
    execvp(args[0], args);
    _exit(127);
  }

  if(handoff_closed)
    close(handoff_pipe[0]);

  if(handoff_open)
    printf("handoff fd %d\n", handoff_pipe[1]);

  if(handoff_closed || handoff_open)
    close(handoff_pipe[1]);

  signal(SIGPIPE, SIG_IGN);
  int fifo = -1;
  int status;
  bool reaped = false;

  for(int i = 1; i < (command - 1); i++)
  {
    const char* action = argv[i];

    if((strcmp(action, "--handoff-closed") == 0) ||
      (strcmp(action, "--handoff-pipe") == 0) ||
      (strcmp(action, "--stderr-no-reader") == 0) ||
      (strcmp(action, "--fill-handoff") == 0))
      continue;

    if(strcmp(action, "--close-handoff") == 0)
    {
      close(handoff_pipe[0]);
      continue;
    }

    if(strcmp(action, "--check-alive") == 0)
    {
      if(waitpid(pid, &status, WNOHANG) == pid)
        reaped = true;
      else
        printf("alive\n");
      continue;
    }

    const char* value = argv[++i];

    if(strcmp(action, "--wait-file") == 0)
    {
      struct stat st;
      long waited = 0;
      while(stat(value, &st) != 0)
      {
        if(waitpid(pid, &status, WNOHANG) == pid)
        {
          if(WIFSIGNALED(status))
            printf("launcher: the command ended (signaled %s) before the file"
              " appeared\n", test_signal_name(WTERMSIG(status)));
          else
            printf("launcher: the command ended (exited %d) before the file"
              " appeared\n", WEXITSTATUS(status));
          return 2;
        }

        if(waited >= TIMEOUT_MS)
          give_up(pid, "the file never appeared");
        sleep_ms(10);
        waited += 10;
      }
    } else if(strcmp(action, "--fifo-open") == 0) {
      long waited = 0;
      while((fifo = open(value, O_WRONLY | O_NONBLOCK)) == -1)
      {
        if((errno != ENXIO) || (waited >= TIMEOUT_MS))
          give_up(pid, "nothing opened the FIFO for reading");
        sleep_ms(10);
        waited += 10;
      }
    } else if(strcmp(action, "--signal") == 0) {
      kill(pid, signal_arg(value));
    } else if(strcmp(action, "--sleep-ms") == 0) {
      sleep_ms(atol(value));
    }
    // Anything else was set up before the command started.
  }

  // A command that doesn't end in time is killed, so a test fails instead
  // of hanging.
  long waited = 0;
  bool killed = false;
  while(!reaped)
  {
    pid_t r = waitpid(pid, &status, WNOHANG);

    if(r == pid)
      break;

    if((r == -1) && (errno != EINTR))
    {
      perror("launcher: waitpid");
      return 2;
    }

    if((waited >= TIMEOUT_MS) && !killed)
    {
      printf("launcher: the command didn't end; killing it\n");
      kill(pid, SIGKILL);
      killed = true;
    }

    sleep_ms(10);
    waited += 10;
  }

  if(WIFSIGNALED(status))
    printf("signaled %s\n", test_signal_name(WTERMSIG(status)));
  else
    printf("exited %d\n", WEXITSTATUS(status));

  if(fifo != -1)
  {
    if((write(fifo, "x", 1) == -1) && (errno == EPIPE))
      printf("fifo reader gone\n");
    else
      printf("fifo reader alive\n");
  }

  return 0;
}
