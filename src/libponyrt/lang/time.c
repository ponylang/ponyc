#include <platform.h>
#include <pony.h>
#include <time.h>
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

static void tm_to_date(struct tm* tm, int64_t nsec, date_t* date)
{
  date->nsec = (int)nsec;
  date->sec = tm->tm_sec;
  date->min = tm->tm_min;
  date->hour = tm->tm_hour;
  date->day_of_month = tm->tm_mday;
  date->month = tm->tm_mon + 1;
  date->year = tm->tm_year + 1900;
  date->day_of_week = tm->tm_wday;
  date->day_of_year = tm->tm_yday;
}

int64_t os_timegm(date_t* date)
{
  struct tm tm;
  date_to_tm(date, &tm);

#ifdef PLATFORM_IS_WINDOWS
  return _mkgmtime(&tm);
#else
  return timegm(&tm);
#endif
}

void os_gmtime(date_t* date, int64_t sec, int64_t nsec)
{
  int64_t overflow_sec = nsec / 1000000000;
  nsec -= (overflow_sec * 1000000000);

  if(nsec < 0)
  {
    nsec += 1000000000;
    overflow_sec--;
  }

  time_t t = cast_checked(time_t, sec + overflow_sec);

  struct tm tm;

#ifdef PLATFORM_IS_WINDOWS
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif

  tm_to_date(&tm, nsec, date);
}

char* os_formattime(date_t* date, const char* fmt)
{
  char* buffer;

  // Bail out on strftime formats that can produce a zero-length string.
  if((fmt[0] == '\0') || !strcmp(fmt, "%p") || !strcmp(fmt, "%P"))
  {
    buffer = (char*)pony_alloc(1);
    buffer[0] = '\0';
    return buffer;
  }

  struct tm tm;
  date_to_tm(date, &tm);

  size_t len = 64;
  size_t r = 0;

  while(r == 0)
  {
    buffer = (char*)pony_alloc(len);
    r = strftime(buffer, len, fmt, &tm);
    len <<= 1;
  }

  return buffer;
}

PONY_EXTERN_C_END
