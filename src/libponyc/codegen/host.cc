#ifdef _MSC_VER
#  pragma warning(push) 
//because LLVM IR Builder code is broken: e.g. Instructions.h:521-527
#  pragma warning(disable:4244)
#  pragma warning(disable:4800)
#  pragma warning(disable:4267)
#endif
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/Host.h>
#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#include "codegen.h"

char* LLVMGetHostCPUName()
{
  return strdup(llvm::sys::getHostCPUName().str().c_str());
}

void LLVMSetUnsafeAlgebra(LLVMValueRef inst)
{
  llvm::unwrap<llvm::Instruction>(inst)->setHasUnsafeAlgebra(true);
}
