#include <stddef.h>

#ifdef PLATFORM_IS_VISUAL_STUDIO
#  define EXPORT_SYMBOL __declspec(dllexport)
#else
#  define EXPORT_SYMBOL
#endif

typedef void (*baretest_callback_fn)(size_t value);

EXPORT_SYMBOL extern void baretest_callback(baretest_callback_fn cb, size_t value)
{
  cb(value);
}

