#include "platform.h"

#include <stdio.h>
#include <stdarg.h>

#ifdef PLATFORM_IS_WINDOWS
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

void pony_strcpy(char* dest, const char* source)
{
#ifdef PLATFORM_IS_WINDOWS
  strcpy_s(dest, strlen(dest), source);
#else
  strcpy(dest, source);
#endif
}

void pony_strncpy(char* dest,  const char* source, size_t len)
{
#ifdef PLATFORM_IS_WINDOWS
  strncpy_s(dest, strlen(dest), source, len);
#else
  strncpy(dest, source, len);
#endif
}

void pony_strcat(char* dest, const char* appendix)
{
#ifdef PLATFORM_IS_WINDOWS
  strcat_s(dest, strlen(dest), appendix);
#else
  strcat(dest, appendix);
#endif
}

char* pony_strdup(const char* source)
{
#ifdef PLATFORM_IS_WINDOWS
  return _strdup(source);
#else
  return strdup(source);
#endif
}

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
#elif PLATFORM_IS_POSIX_BASED
  return ioctl(STDOUT_FILENO, TIOCGWINSZ, ws) != -1
#endif
}