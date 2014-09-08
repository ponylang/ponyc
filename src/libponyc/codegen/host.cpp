#include <llvm/Support/Host.h>

extern "C" char* LLVMGetHostCPUName()
{
  return strdup(llvm::sys::getHostCPUName().c_str());
}
