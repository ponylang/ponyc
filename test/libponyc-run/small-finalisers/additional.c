#include <stdint.h>

#ifdef PLATFORM_IS_VISUAL_STUDIO
#  define EXPORT_SYMBOL __declspec(dllexport)
#else
#  define EXPORT_SYMBOL
#endif

extern void pony_exitcode(int code);

static uint32_t num_objects = 0;

EXPORT_SYMBOL extern void codegentest_small_finalisers_increment_num_objects() {
  num_objects++;
  pony_exitcode((int)num_objects);
}
