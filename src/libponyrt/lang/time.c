#include <platform.h>
#include <pony.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

PONY_EXTERN_C_BEGIN

typedef struct
{
  pony_type_t* type;
  int nsec;
  int sec;
  int min;
  int hour;
  int day_of_month;
  int month;
  int year;
  int day_of_week;
  int day_of_year;
} date_t;

static void date_to_tm(date_t* date, struct tm* tm)
{
  memset(tm, 0, sizeof(struct tm));

  tm->tm_sec = date->sec;
  tm->tm_min = date->min;
  tm->tm_hour = date->hour;
  tm->tm_mday = date->day_of_month;
  tm->tm_mon = date->month - 1;
  tm->tm_year = date->year - 1900;
  tm->tm_wday = date->day_of_week;
  tm->tm_yday = date->day_of_year;
  tm->tm_isdst = -1;
}

static void tm_to_date(struct tm* tm, int nsec, date_t* date)
{
  date->nsec = nsec;
  date->sec = tm->tm_sec;
  date->min = tm->tm_min;
  date->hour = tm->tm_hour;
  date->day_of_month = tm->tm_mday;
  date->month = tm->tm_mon + 1;
  date->year = tm->tm_year + 1900;
  date->day_of_week = tm->tm_wday;
  date->day_of_year = tm->tm_yday;
}

PONY_API int64_t ponyint_timegm(date_t* date)
{
  struct tm tm;
  date_to_tm(date, &tm);

#ifdef PLATFORM_IS_WINDOWS
  return _mkgmtime(&tm);
#else
  return timegm(&tm);
#endif
}

PONY_API void ponyint_gmtime(date_t* date, int64_t sec, int64_t nsec)
{
  time_t overflow_sec = (time_t)(nsec / 1000000000);
  nsec -= (overflow_sec * 1000000000);

  if(nsec < 0)
  {
    nsec += 1000000000;
    overflow_sec--;
  }

  time_t t = (time_t)sec + overflow_sec;

  struct tm tm;

#ifdef PLATFORM_IS_WINDOWS
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif

  tm_to_date(&tm, (int)nsec, date);
}

void format_invalid_parameter_handler(const wchar_t* expression,
  const wchar_t* function,
  const wchar_t* file,
  unsigned int line,
  uintptr_t p_reserved)
{
  // Cast all parameters to void to silence unused parameter warning
  (void) expression;
  (void) function;
  (void) file;
  (void) line;
  (void) p_reserved;

  // Throw a pony error
  pony_error();
}

PONY_API char* ponyint_formattime(date_t* date, const char* fmt)
{
  pony_ctx_t* ctx = pony_ctx();
  char* buffer;

  // Bail out on strftime formats that can produce a zero-length string.
  if((fmt[0] == '\0')
    || !strcmp(fmt, "%p")
    || !strcmp(fmt, "%P")
    || !strcmp(fmt, "%z")
    || !strcmp(fmt, "%Z"))
  {
    char* buffer = (char*)pony_alloc(ctx, 1);
    buffer[0] = '\0';
    return buffer;
  }

  // Check if the format string contains only %p, %P, %z, and/or %Z.
  // This addresses https://github.com/ponylang/ponyc/issues/4446

  bool only_possible_zeros = true;
  const char* p = fmt;

  while (*p && only_possible_zeros) {
      if (*p != '%'
          || (p[1] != 'p' && p[1] != 'P' && p[1] != 'z' && p[1] != 'Z')) {
        only_possible_zeros = false;
      } else {
          p += 2;  // Skip both '%' and 'p'/'P'
      }
  }

  if (only_possible_zeros) {
      char* buffer = (char*)pony_alloc(ctx, 1);
      buffer[0] = '\0';
      return buffer;
  }

  // Our real logic here
  struct tm tm;
  date_to_tm(date, &tm);

  size_t len = 64;
  size_t r = 0;

  while(r == 0)
  {
    buffer = (char*)pony_alloc(ctx, len);

    // Set up an invalid parameter handler on Windows to throw a Pony error
    #ifdef PLATFORM_IS_WINDOWS
      _invalid_parameter_handler old_handler, new_handler;
      new_handler = format_invalid_parameter_handler;
      old_handler = _set_thread_local_invalid_parameter_handler(new_handler);
    #endif

    r = strftime(buffer, len, fmt, &tm);

    // Reset the invalid parameter handler
    #ifdef PLATFORM_IS_WINDOWS
      _set_thread_local_invalid_parameter_handler(old_handler);
    #endif

    len <<= 1;
  }

  return buffer;
}

PONY_EXTERN_C_END
