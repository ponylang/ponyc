#ifdef PLATFORM_IS_VISUAL_STUDIO
#  define EXPORT_SYMBOL __declspec(dllexport)
#else
#  define EXPORT_SYMBOL
#endif

typedef int (*codegentest_ccallback_fn)(void* self, int value);

EXPORT_SYMBOL int codegentest_ccallback(void* self, codegentest_ccallback_fn cb,
  int value)
{
  return cb(self, value);
}
