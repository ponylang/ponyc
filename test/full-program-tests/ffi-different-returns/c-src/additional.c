#ifdef _MSC_VER
#  define EXPORT_SYMBOL __declspec(dllexport)
#else
#  define EXPORT_SYMBOL
#endif

EXPORT_SYMBOL int ffi_function()
{
    return 0;
}
