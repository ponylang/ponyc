#ifdef _MSC_VER
#  pragma warning(push)
//because LLVM IR Builder code is broken: e.g. Instructions.h:521-527
#  pragma warning(disable:4244)
#  pragma warning(disable:4800)
#  pragma warning(disable:4267)
#endif

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/Analysis/CaptureTracking.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Support/Host.h>

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

static void stack_alloc_inst(compile_t* c, LLVMValueRef inst)
{
  CallInst* call = dyn_cast_or_null<CallInst>(unwrap(inst));

  if(call == NULL)
    return;

  Function* fun = call->getCalledFunction();

  if(fun == NULL)
    return;

  if(fun->getName().compare("pony_alloc") != 0)
    return;

  if(PointerMayBeCaptured(call, true, true))
    return;

  Type* type = unwrap(c->i8);
  Value* size = call->getArgOperand(0);

  ConstantInt* int_size = dyn_cast_or_null<ConstantInt>(size);
  size_t alloc_size = int_size->getZExtValue();

  // Limit stack allocations to 1 kb each.
  if(alloc_size > 1024)
    return;

  AllocaInst* alloca = new AllocaInst(type, size);
  ReplaceInstWithInst(call, alloca);
}

static void stack_alloc_block(compile_t* c, LLVMBasicBlockRef block)
{
  LLVMValueRef inst = LLVMGetFirstInstruction(block);

  while(inst != NULL)
  {
    stack_alloc_inst(c, inst);
    inst = LLVMGetNextInstruction(inst);
  }
}

static void stack_alloc_fun(compile_t* c, LLVMValueRef fun)
{
  LLVMBasicBlockRef block = LLVMGetFirstBasicBlock(fun);

  while(block != NULL)
  {
    stack_alloc_block(c, block);
    block = LLVMGetNextBasicBlock(block);
  }
}

void stack_alloc(compile_t* c)
{
  LLVMValueRef fun = LLVMGetFirstFunction(c->module);

  while(fun != NULL)
  {
    stack_alloc_fun(c, fun);
    fun = LLVMGetNextFunction(fun);
  }
}
