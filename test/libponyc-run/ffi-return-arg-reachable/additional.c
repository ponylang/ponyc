#ifdef _MSC_VER
#  define EXPORT_SYMBOL __declspec(dllexport)
#else
#  define EXPORT_SYMBOL
#endif

int identity(int a) {
  return a;
}

int add(int a, int b) {
  return a + b;
}

EXPORT_SYMBOL void* get_function(int type)
{
  if(type == 0)
    return identity;

  return add;
}
