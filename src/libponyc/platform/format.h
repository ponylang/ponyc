#ifndef PLATFORM_FORMAT_H
#define PLATFORM_FORMAT_H

/** Format specifiers.
*
*  SALs are only supported on Visual Studio >= Professional Editions. If we
*  cannot support FORMAT_STRING, we do not validate printf-like functions.
*/
#ifndef PLATFORM_IS_VISUAL_STUDIO
#  define __pony_format__(X, Y, Z) __attribute__((format \
            (X, Y, Z)))
#  define FORMAT_STRING(X) X
#  define __pony_format_zu "%zu"
#else
#  if _MSC_VER >= 1400
#    include <sal.h>
#    if _MSC_VER > 1400
#      define FORMAT_STRING(p) _Printf_format_string_ p
#    else
#      define FORMAT_STRING(p) __format_string p
#    endif
#  else
#    define FORMAT_STRING(p) p
#  endif
#  define __pony_format__(X, Y, Z)
// %zu appeared in C99 (not supported by MS)
#  define __pony_format_zu "%Iu"
#endif

int32_t pony_snprintf(char* str, size_t size, 
  FORMAT_STRING(const char* format), ...);

int32_t pony_vsnprintf(char* str, size_t size, 
  FORMAT_STRING(const char* format), va_list args);

#endif