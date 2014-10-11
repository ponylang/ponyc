#ifdef _MSC_VER
#  pragma warning(disable:4267)
#endif

#include <llvm/Support/Host.h>
#include <llvm/IR/IRBuilder.h>
#include <platform.h>
#include "codegen.h"


char* LLVMGetHostCPUName()
{
  return strdup(llvm::sys::getHostCPUName().str().c_str());
}

void LLVMSetUnsafeAlgebra(LLVMValueRef inst)
{
  llvm::unwrap<llvm::Instruction>(inst)->setHasUnsafeAlgebra(true);
}
