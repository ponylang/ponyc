#include "platform.h"

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
    count = _vsnprintf(str, size, _TRUNCATE, format, args);
  if (count == -1)
    count = _vcsprintf(format, args);

  return count;
}

bool get_window_size(struct winsize* ws)
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