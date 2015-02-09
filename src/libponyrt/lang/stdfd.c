#include <platform.h>
#include <stdio.h>

#ifndef PLATFORM_IS_WINDOWS
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#endif

PONY_EXTERN_C_BEGIN

FILE* os_stdout()
{
  return stdout;
}

FILE* os_stderr()
{
  return stderr;
}

#ifndef PLATFORM_IS_WINDOWS
static struct termios orig_termios;

typedef enum
{
  FD_TYPE_NONE = 0,
  FD_TYPE_DEVICE,
  FD_TYPE_TTY,
  FD_TYPE_PIPE,
  FD_TYPE_FILE
} fd_type_t;

static fd_type_t fd_type(int fd)
{
  fd_type_t type = FD_TYPE_NONE;
  struct stat st;

  if(fstat(fd, &st) != -1)
  {
    switch(st.st_mode & S_IFMT)
    {
      case S_IFIFO:
      case S_IFSOCK:
        // A pipe or a socket.
        type = FD_TYPE_PIPE;
        break;

      case S_IFCHR:
        // A tty or a character device.
        if(isatty(fd))
          type = FD_TYPE_TTY;
        else
          type = FD_TYPE_DEVICE;
        break;

      case S_IFREG:
        // A redirected file.
        type = FD_TYPE_FILE;
        break;

      default:
        // A directory or a block device.
        break;
    }
  }

  return type;
}

static void stdin_tty_restore()
{
  tcsetattr(0, TCSAFLUSH, &orig_termios);
}

static void fd_tty(int fd)
{
  // Turn off canonical mode if we're reading from a tty.
  if(tcgetattr(fd, &orig_termios) != -1)
  {
    struct termios io = orig_termios;

    io.c_iflag &= ~(BRKINT | INPCK | ISTRIP | IXON);
    io.c_cflag |= (CS8);
    io.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    io.c_cc[VMIN] = 1;
    io.c_cc[VTIME] = 0;

    tcsetattr(fd, TCSAFLUSH, &io);

    if(fd == 0)
      atexit(stdin_tty_restore);
  }
}

static void fd_nonblocking(int fd)
{
  // Set to non-blocking.
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
#endif

void os_stdout_setup()
{
#ifdef PLATFORM_IS_WINDOWS
  // TODO:
#else
  fd_type_t type = fd_type(STDOUT_FILENO);

  // Use unbuffered output if we're writing to a tty.
  if(type == FD_TYPE_TTY)
    setvbuf(stdout, NULL, _IONBF, 0);
#endif
}

bool os_stdin_setup()
{
  // Return true if reading stdin should be event based.
#ifdef PLATFORM_IS_WINDOWS
  // TODO:
  return true;
#else
  int fd = STDIN_FILENO;
  fd_type_t type = fd_type(fd);

  switch(type)
  {
    case FD_TYPE_TTY:
      fd_nonblocking(fd);
      fd_tty(fd);
      return true;

    case FD_TYPE_PIPE:
    case FD_TYPE_DEVICE:
      fd_nonblocking(fd);
      return true;

    default: {}
  }

  // For a file, directory, or block device, do nothing.
  return false;
#endif
}

PONY_EXTERN_C_END
