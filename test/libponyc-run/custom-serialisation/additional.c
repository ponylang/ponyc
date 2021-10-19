#include <stddef.h>
#include <stdint.h>
#include <../libponyrt/mem/pool.h>

#ifdef _MSC_VER
#  define EXPORT_SYMBOL __declspec(dllexport)
#else
#  define EXPORT_SYMBOL
#endif

EXPORT_SYMBOL void* test_custom_serialisation_get_object()
{
  uint64_t* i = POOL_ALLOC(uint64_t);
  *i = 0xDEADBEEF10ADBEE5;
  return i;
}

EXPORT_SYMBOL void test_custom_serialisation_free_object(uint64_t* p)
{
  POOL_FREE(uint64_t, p);
}

EXPORT_SYMBOL void test_custom_serialisation_serialise(uint64_t* p,
  unsigned char* bytes)
{
  *(uint64_t*)(bytes) = *p;
}

EXPORT_SYMBOL void* test_custom_serialisation_deserialise(unsigned char* bytes)
{
  uint64_t* p = POOL_ALLOC(uint64_t);
  *p = *(uint64_t*)(bytes);
  return p;
}

EXPORT_SYMBOL char test_custom_serialisation_compare(uint64_t* p1, uint64_t* p2)
{
  return *p1 == *p2;
}
