#ifdef _MSC_VER
#  pragma warning(disable:4267)
#endif

#include <llvm/Support/Host.h>
#include <platform/platform.h>

#ifdef PLATFORM_IS_POSIX_BASED
extern "C"
#endif
char* LLVMGetHostCPUName()
{
  return strdup(llvm::sys::getHostCPUName().str().c_str());
}
