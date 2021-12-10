#include <stddef.h>

#ifdef _MSC_VER
#  define EXPORT_SYMBOL __declspec(dllexport)
#else
#  define EXPORT_SYMBOL
#endif

extern "C"
{

typedef void (*baretest_callback_fn)(size_t value);

EXPORT_SYMBOL extern void baretest_callback(baretest_callback_fn cb, size_t value)
{
  cb(value);
}

}
