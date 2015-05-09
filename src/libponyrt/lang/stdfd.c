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

static void add_modifier(char* buffer, int* len, DWORD mod)
{
  bool alt = ((mod & LEFT_ALT_PRESSED) > 0) ||
    ((mod & RIGHT_ALT_PRESSED) > 0);

  bool ctrl = ((mod & LEFT_CTRL_PRESSED) > 0) ||
    ((mod & RIGHT_CTRL_PRESSED) > 0);

  bool shift = (mod & SHIFT_PRESSED) > 0;

  if(!alt && !ctrl && !shift)
    return;

  int next_len = *len;
  buffer[next_len++] = ';';

  if(shift)
  {
    if(ctrl)
    {
      if(alt)
        buffer[next_len++] = '8';
      else
        buffer[next_len++] = '6';
    } else if(alt) {
      buffer[next_len++] = '4';
    } else {
      buffer[next_len++] = '2';
    }
  } else if(alt) {
    if(ctrl)
      buffer[next_len++] = '7';
    else
      buffer[next_len++] = '3';
  } else if(ctrl) {
    buffer[next_len++] = '5';
  }

  *len = next_len;
}

static int add_ansi_code(char* buffer, const char* pre, const char* post,
  DWORD mod)
{
  int prelen = (int)strlen(pre);
  memcpy(buffer, pre, prelen);
  add_modifier(buffer, &prelen, mod);

  int postlen = (int)strlen(post);
  memcpy(&buffer[prelen], post, postlen);

  return prelen + postlen;
}

static bool add_input_record(char* buffer, uint64_t space, uint64_t *len,
  INPUT_RECORD* rec)
{
  // Returns true if the record can be consumed.
  if(rec->EventType != KEY_EVENT ||
    rec->Event.KeyEvent.bKeyDown == FALSE)
  {
    return true;
  }

  // This is a key down event. Handle key repeats.
  DWORD mod = rec->Event.KeyEvent.dwControlKeyState;
  uint64_t next_len = *len;
  char out[8];
  int outlen;

  switch(rec->Event.KeyEvent.wVirtualKeyCode)
  {
    case VK_PRIOR: // page up
      outlen = add_ansi_code(out, "\x1B[5", "~", mod);
      break;

    case VK_NEXT: // page down
      outlen = add_ansi_code(out, "\x1B[6", "~", mod);
      break;

    case VK_END:
      outlen = add_ansi_code(out, "\x1B[4", "~", mod);
      break;

    case VK_HOME:
      outlen = add_ansi_code(out, "\x1B[1", "~", mod);
      break;

    case VK_LEFT:
      outlen = add_ansi_code(out, "\x1B[1", "D", mod);
      break;

    case VK_RIGHT:
      outlen = add_ansi_code(out, "\x1B[1", "C", mod);
      break;

    case VK_UP:
      outlen = add_ansi_code(out, "\x1B[1", "A", mod);
      break;

    case VK_DOWN:
      outlen = add_ansi_code(out, "\x1B[1", "B", mod);
      break;

    case VK_INSERT:
      outlen = add_ansi_code(out, "\x1B[2", "~", mod);
      break;

    case VK_DELETE:
      outlen = add_ansi_code(out, "\x1B[3", "~", mod);
      break;

    case VK_F1:
      outlen = add_ansi_code(out, "\x1B[11", "~", mod);
      break;

    case VK_F2:
      outlen = add_ansi_code(out, "\x1B[12", "~", mod);
      break;

    case VK_F3:
      outlen = add_ansi_code(out, "\x1B[13", "~", mod);
      break;

    case VK_F4:
      outlen = add_ansi_code(out, "\x1B[14", "~", mod);
      break;

    case VK_F5:
      outlen = add_ansi_code(out, "\x1B[15", "~", mod);
      break;

    case VK_F6:
      outlen = add_ansi_code(out, "\x1B[17", "~", mod);
      break;

    case VK_F7:
      outlen = add_ansi_code(out, "\x1B[18", "~", mod);
      break;

    case VK_F8:
      outlen = add_ansi_code(out, "\x1B[19", "~", mod);
      break;

    case VK_F9:
      outlen = add_ansi_code(out, "\x1B[20", "~", mod);
      break;

    case VK_F10:
      outlen = add_ansi_code(out, "\x1B[21", "~", mod);
      break;

    case VK_F11:
      outlen = add_ansi_code(out, "\x1B[23", "~", mod);
      break;

    case VK_F12:
      outlen = add_ansi_code(out, "\x1B[24", "~", mod);
      break;

    default:
    {
      // WCHAR to UTF-8
      if(rec->Event.KeyEvent.uChar.UnicodeChar != 0)
      {
        outlen = WideCharToMultibyte(CP_UTF8, 0,
          &rec->Event.KeyEvent.uChar.UnicodeChar, 1, out, 4,
          NULL, NULL);
      }
    }
  }

  WORD count = rec->Event.KeyEvent.wRepeatCount;

  if((next_len + (count * outlen)) > space)
    return false;

  for(WORD i = 0; i < count; i++)
  {
    memcpy(&buffer[next_len], out, outlen);
    next_len += outlen;
  }

  *len = next_len;
  return true;
}

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
    SetConsoleMode(stdinHandle,
      mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
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

uint64_t os_stdin_read(char* buffer, uint64_t space, bool* out_again)
{
#ifdef PLATFORM_IS_WINDOWS
  uint64_t len = 0;

  if(is_stdin_tty)
  {
    // TTY. Peel at the console input.
    INPUT_RECORD record[64];
    DWORD readCount = 64;
    BOOL r = PeekConsoleInput(stdinHandle, record, readCount, &readCount);

    if(r == TRUE)
    {
      DWORD consumed = 0;

      while(consumed < readCount)
      {
        if(!add_input_record(buffer, space, &len, &record[consumed]))
          break;

        consumed++;
      }

      // Pull as many records as we were able to handle.
      ReadConsoleInput(stdinHandle, record, consumed, &readCount);
    }

    // We have no data, but 0 means EOF, so we return -1 which is try again
    if(len == 0)
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
  return read(0, buffer, space);
#endif
}

static bool is_fp_tty(FILE* fp)
{
  return
    ((fp == stdout) && is_stdout_tty) ||
    ((fp == stderr) && is_stderr_tty);
}

void os_std_write(FILE* fp, char* buffer, uint64_t len)
{
  // TODO: strip ANSI if not a tty
  // can't strip in place
#ifdef PLATFORM_IS_WINDOWS
  // TODO: ANSI to API
  if(is_fp_tty(fp))
  {
    uint64_t last = 0;
    uint64_t pos = 0;

    while(pos < len)
    {
      if(buffer[pos] == '\x1B')
      {
        if(pos > last)
        {
          fwrite(&buffer[last], pos - last, 1, fp);
          last = pos;
        }

        // TODO:
      }
    }

    if(pos > last)
    {
      fwrite(&buffer[last], pos - last, 1, fp);
      last = pos;
    }
  } else {
    fwrite(buffer, len, 1, fp);
  }
#elif defined PLATFORM_IS_LINUX
  fwrite_unlocked(buffer, len, 1, fp);
#else
  fwrite(buffer, len, 1, fp);
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

  if(!is_fp_tty(stream) || !is_term_color)
    return;

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
