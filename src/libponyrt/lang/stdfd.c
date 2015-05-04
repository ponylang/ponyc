#include <platform.h>
#include <stdio.h>
#include <string.h>
#include "../asio/asio.h"

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

static bool is_stdout_tty = false;
static bool is_stderr_tty = false;
static bool is_term_color = false;
static FILE* changed_stream_color = NULL;

#ifdef PLATFORM_IS_WINDOWS

static HANDLE stdinHandle;
static bool is_stdin_tty = false;
static WORD prev_term_color;

#else
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
  DWORD type = GetFileType(GetStdHandle(STD_INPUT_HANDLE));
  is_stdout_tty = (type == FILE_TYPE_CHAR);

  type = GetFileType(GetStdHandle(STD_ERROR_HANDLE));
  is_stderr_tty = (type == FILE_TYPE_CHAR);

  is_term_color = true;
#else
  fd_type_t type = fd_type(STDOUT_FILENO);
  is_stdout_tty = (type == FD_TYPE_TTY);

  // Use unbuffered output if we're writing to a tty.
  if(type == FD_TYPE_TTY)
    setvbuf(stdout, NULL, _IONBF, 0);

  is_stderr_tty = (fd_type(STDERR_FILENO) == FD_TYPE_TTY);

  const char* term = getenv("TERM");

  if(term != NULL &&
    (strcmp(term, "xterm") == 0 ||
     strcmp(term, "xterm-color") == 0 ||
     strcmp(term, "xterm-256color") == 0 ||
     strcmp(term, "screen") == 0 ||
     strcmp(term, "screen-256color") == 0 ||
     strcmp(term, "linux") == 0 ||
     strcmp(term, "cygwin")))
    is_term_color = true;
#endif
}

bool os_stdin_setup()
{
  // Return true if reading stdin should be event based.
#ifdef PLATFORM_IS_WINDOWS
  stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
  DWORD type = GetFileType(stdinHandle);

  if(type == FILE_TYPE_CHAR)
  {
    // TTY
    DWORD mode;
    GetConsoleMode(stdinHandle, &mode);
    SetConsoleMode(stdinHandle, mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
    is_stdin_tty = true;
  }

  // Always use events
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

uint64_t os_stdin_read(void* buffer, uint64_t space, bool* out_again)
{
#ifdef PLATFORM_IS_WINDOWS
  uint64_t len = 0;

  if(is_stdin_tty)
  {
    // TTY. Read from console input.

    /* Note:
     * We can only call ReadConsoleInput() once as the second time it might
     * block. We only get useful data from key downs, which are less than half
     * the events we get. However, due to copy and paste we may get many key
     * down events in a row. Furthermore, <Enter> key downs have to expand to
     * 2 characters in the buffer (an 0xD and an 0xA). This means we can only
     * read (space / 2) events and guarantee that the data they produce will
     * fit in the provided buffer. In general this means the buffer will only
     * be a quarter full, even if there are more events waiting.
     * AMc, 10/4/15
     */
    INPUT_RECORD record[64];
    DWORD readCount = 32;
    char* buf = (char*)buffer;
    uint64_t max_events = space / 2;

    if(max_events < readCount)
      // Limit events read to buffer size, in case they're all key down events
      readCount = (DWORD)max_events;

    BOOL r = ReadConsoleInput(stdinHandle, record, readCount, &readCount);

    if(r == TRUE)
    {
      for(DWORD i = 0; i < readCount; i++)
      {
        INPUT_RECORD* rec = &record[i];

        if(rec->EventType == KEY_EVENT &&
          rec->Event.KeyEvent.bKeyDown == TRUE &&
          rec->Event.KeyEvent.uChar.AsciiChar != 0)
        {
          // This is a key down event
          buf[len++] = rec->Event.KeyEvent.uChar.AsciiChar;

          if(rec->Event.KeyEvent.uChar.AsciiChar == 0xD)
            buf[len++] = 0xA;
        }
      }
    }

    if(len == 0)
      // We have no data, but 0 means EOF, so we return -1 which is try again
      len = -1;
  }
  else
  {
    // Not TTY, ie file or pipe. Just use ReadFile.
    DWORD buf_size = (space <= 0xFFFFFFFF) ? (DWORD)space : 0xFFFFFFFF;
    DWORD actual_len = 0;

    BOOL r = ReadFile(stdinHandle, buffer, buf_size, &actual_len, NULL);

    len = actual_len;

    if(r == FALSE && GetLastError() == ERROR_BROKEN_PIPE)  // Broken pipe
      len = 0;
  }

  if(len != 0)  // Start listening to stdin notifications again
    iocp_resume_stdin();

  *out_again = false;
  return len;
#else
  *out_again = true;
  return read(0, buffer, pony_downcast(size_t, space));
#endif
}

#ifdef PLATFORM_IS_WINDOWS
// Unsurprisingly Windows uses a different order for colors. Map them here.
// We use the bright versions here, hence the additional 8 on each value.
static const uint8_t map_color[8] =
{
   0, // black
  12, // red
  10, // green
  14, // yellow
   9, // blue
  13, // magenta
  11, // cyan
  15  // white
};
#endif


// os_set_color() and os_reset_color() MUST be called in pairs, on the same
// stream, without any interleaving.

void os_set_color(FILE* stream, uint8_t color)
{
  // If stream doesn't support color do nothing
  changed_stream_color = NULL;

  if(stream == stdout && !is_stdout_tty) return;
  if(stream == stderr && !is_stderr_tty) return;
  if(!is_term_color) return;

  if(color > 7) // Invalid color, do nothing
    return;

  changed_stream_color = stream;

#ifdef PLATFORM_IS_WINDOWS
  HANDLE stdxxx_handle;

  if(stream == stdout)
    stdxxx_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  else
    stdxxx_handle = GetStdHandle(STD_ERROR_HANDLE);

  CONSOLE_SCREEN_BUFFER_INFO buffer_info;
  GetConsoleScreenBufferInfo(stdxxx_handle, &buffer_info);
  prev_term_color = buffer_info.wAttributes;

  // The color setting is console wide, so if we're writing to both stdout and
  // stderr they can interfere with each other. We flush both in an attempt to
  // avoid this.
  fflush(stdout);
  fflush(stderr);
  SetConsoleTextAttribute(stdxxx_handle, map_color[color]);
#else
  uint8_t t[] = {'\033', '[', '0', ';', '3', '#', 'm'};
  t[5] = (uint8_t)(color + '0');
#ifdef PLATFORM_IS_LINUX
  fwrite_unlocked(t, 1, 7, stream);
#else
  fwrite(t, 1, 7, stream);
#endif
#endif
}

void os_reset_color()
{
  // If we haven't set the colour stream do nothing
  if(changed_stream_color == NULL)
    return;

#ifdef PLATFORM_IS_WINDOWS
  HANDLE stdxxx_handle;

  if(changed_stream_color == stdout)
    stdxxx_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  else
    stdxxx_handle = GetStdHandle(STD_ERROR_HANDLE);

  fflush(stdout);
  fflush(stderr);
  SetConsoleTextAttribute(stdxxx_handle, prev_term_color);
#elif defined PLATFORM_IS_LINUX
  fwrite_unlocked("\033[m", 1, 3, changed_stream_color);
#else
  fwrite("\033[m", 1, 3, changed_stream_color);
#endif

  changed_stream_color = NULL;
}

PONY_EXTERN_C_END
