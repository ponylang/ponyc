#include <stdint.h>

#ifdef _MSC_VER
#  define EXPORT_SYMBOL __declspec(dllexport)
#else
#  define EXPORT_SYMBOL
#endif

EXPORT_SYMBOL extern uintptr_t ptr_to_int(void* p)
{
  return (uintptr_t)p;
}
