#ifdef _MSC_VER
#  pragma warning(disable:4267)
#endif

#include <llvm/Support/Host.h>
#pragma warning(push) 
//because LLVM IR Builder code is broken: e.g. Instructions.h:521-527
#pragma warning(disable:4244)
#pragma warning(disable:4800) 
#include <llvm/IR/IRBuilder.h>
#pragma warning(pop)
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
