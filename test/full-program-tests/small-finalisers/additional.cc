#include <stdint.h>
#include <pony.h>

#ifdef _MSC_VER
#  define EXPORT_SYMBOL __declspec(dllexport)
#else
#  define EXPORT_SYMBOL
#endif

extern "C"
{

static uint32_t num_objects = 0;

EXPORT_SYMBOL extern void codegentest_small_finalisers_increment_num_objects()
{
  num_objects++;
  pony_exitcode((int)num_objects);
}

#ifndef _MSC_VER
void Main_runtime_override_defaults_oo(void* opt)
{
  (void)opt;
  return;
}
#endif

}
