#ifndef PLATFORM_UTILS_H
#define PLATFORM_UTILS_H

#ifdef PLATFORM_IS_WINDOWS
#  include <malloc.h>
#  define PONY_VL_ARRAY(TYPE, NAME, SIZE) TYPE* NAME = (TYPE*) alloca(\
            (SIZE)*sizeof(TYPE))
#  define no_argument 0x00
#  define required_argument 0x01
#  define optional_argument 0x02
struct option
{
  const char* name;
  int has_arg;
  int *flag;
  int val;
};

struct winsize
{
  unsigned short ws_row;
  unsigned short ws_col;
  unsigned short ws_xpixel;
  unsigned short ws_ypixel;
};
#else
#  define PONY_VL_ARRAY(TYPE, NAME, SIZE) TYPE NAME[(SIZE)]
#endif

#ifdef PLATFORM_IS_WINDOWS
#  define snprintf pony_snprintf
#endif

int32_t pony_snprintf(char* str, size_t size, 
  FORMAT_STRING(const char* format), ...);

int32_t pony_vsnprintf(char* str, size_t size, const char* format,
  va_list args);

bool get_window_size(struct winsize* ws);

#endif