#include <exception>

#ifdef _MSC_VER
#  define EXPORT_SYMBOL __declspec(dllexport)
#else
#  define EXPORT_SYMBOL
#endif

extern "C"
{

EXPORT_SYMBOL int codegen_test_tryblock_catch(void (*callback)())
{
  try
  {
    callback();
    return 0;
  } catch(std::exception const&) {
    return 1;
  }
}

EXPORT_SYMBOL void codegen_test_tryblock_throw()
{
  throw std::exception();
}

}
