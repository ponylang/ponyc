#include "platform.h"

#ifdef PLATFORM_IS_WINDOWS
int getopt(int argc, char* const argv[], const char* optstring)
{
  return 0;
}

int getopt_long(int argc, char* const argv[], const char* optstring,
  const struct option longopts, int* longindex)
{
  return 0;
}
#endif

int32_t pony_snprintf(char* str, size_t size, const char* format, ...)
{
  int32_t count;
  va_list arg;

  va_start(arg, format);
  count = pony_vsnprintf(str, size, format, arg);
  va_end(arg);

  return count;
}

int32_t pony_vsnprintf(char* str, size_t size, const char* format, 
  va_list args)
{
  int32_t count = -1;
  
  if (size != 0)
    count = _vsnprintf_s(str, size, _TRUNCATE, format, args);
  if (count == -1)
    count = _vscprintf(format, args);

  return count;
}

void pony_getenv(const char* s, char** to)
{
#ifdef PLATFORM_IS_WINDOWS
  size_t len = strlen(s);
  _dupenv_s(to, &len, s);
#else
  *to = getenv(s);
#endif
}

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

bool get_terminal_window_size(struct winsize* ws)
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