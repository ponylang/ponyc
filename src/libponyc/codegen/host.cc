#ifdef _MSC_VER
#  pragma warning(push)
//because LLVM IR Builder code is broken: e.g. Instructions.h:521-527
#  pragma warning(disable:4244)
#  pragma warning(disable:4800)
#  pragma warning(disable:4267)
#  pragma warning(disable:4624)
#  pragma warning(disable:4141)
#endif

#include <llvm/IR/Function.h>

#if PONY_LLVM < 307
#include <llvm/Support/Host.h>
#endif

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#include <stdio.h>
#include "codegen.h"

using namespace llvm;

char* LLVMGetHostCPUName()
{
  return strdup(sys::getHostCPUName().str().c_str());
}

void LLVMSetUnsafeAlgebra(LLVMValueRef inst)
{
  unwrap<Instruction>(inst)->setHasUnsafeAlgebra(true);
}

void LLVMSetReturnNoAlias(LLVMValueRef fun)
{
  unwrap<Function>(fun)->setDoesNotAlias(0);
}

void LLVMSetDereferenceable(LLVMValueRef fun, uint32_t i, size_t size)
{
  Function* f = unwrap<Function>(fun);

  AttrBuilder attr;
  attr.addDereferenceableAttr(size);

  f->addAttributes(i, AttributeSet::get(f->getContext(), i, attr));
}
