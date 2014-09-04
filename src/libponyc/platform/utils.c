#include "platform.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef PLATFORM_IS_WINDOWS
int opterr, optind, optopt, optreset = 0;
char* optarg = NULL;

int getopt(int argc, char* const argv[], const char* optstring)
{
  return 0;
}

int getopt_long(int argc, char* const argv[], const char* optstring,
  const struct option* longopts, int* longindex)
{
  return 0;
}
#endif

bool pony_get_term_winsize(struct winsize* ws)
{
#ifdef PLATFORM_IS_WINDOWS
  CONSOLE_SCREEN_BUFFER_INFO info;

  int ret = GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);

  if (!ret)
    return false;

  ws->ws_col = info.srWindow.Right - info.srWindow.Left + 1;
  ws->ws_row = info.srWindow.Bottom - info.srWindow.Top + 1;
  ws->ws_xpixel = info.dwSize.X;
  ws->ws_ypixel = info.dwSize.Y;

  return true;
#elif defined(PLATFORM_IS_POSIX_BASED)
  return ioctl(STDOUT_FILENO, TIOCGWINSZ, ws) != -1;
#endif
}
