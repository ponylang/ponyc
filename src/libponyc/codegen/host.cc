#ifdef _MSC_VER
#  pragma warning(push)
//because LLVM IR Builder code is broken: e.g. Instructions.h:521-527
#  pragma warning(disable:4244)
#  pragma warning(disable:4800)
#  pragma warning(disable:4267)
#endif

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/DebugInfo.h>
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

void LLVMSetReturnNoAlias(LLVMValueRef fun)
{
  unwrap<Function>(fun)->setDoesNotAlias(0);
}

static void print_transform(compile_t* c, Instruction* inst, const char* s)
{
  if(!c->opt->print_stats)
    return;

  Instruction* i = inst;

  while(i->getDebugLoc().getLine() == 0)
  {
    BasicBlock::iterator iter = i;

    if(++iter == i->getParent()->end())
    {
      return;
      // i = inst;
      // break;
    }

    i = iter;
  }

  DebugLoc loc = i->getDebugLoc();
  DIScope scope = DIScope(loc.getScope());
  MDLocation* at = cast_or_null<MDLocation>(loc.getInlinedAt());

  if(at != NULL)
  {
    DIScope scope_at = DIScope((MDNode*)at->getScope());

    errorf(NULL, "[%s] %s:%u:%u@%s:%u:%u: %s",
      i->getParent()->getParent()->getName().str().c_str(),
      scope.getFilename().str().c_str(), loc.getLine(), loc.getCol(),
      scope_at.getFilename().str().c_str(), at->getLine(), at->getColumn(), s);
  } else {
    errorf(NULL, "[%s] %s:%u:%u: %s",
      i->getParent()->getParent()->getName().str().c_str(),
      scope.getFilename().str().c_str(), loc.getLine(), loc.getCol(), s);
  }
}

static LLVMValueRef stack_alloc_inst(compile_t* c, LLVMValueRef inst)
{
  CallInst* call = dyn_cast_or_null<CallInst>(unwrap(inst));

  if(call == NULL)
    return inst;

  Function* fun = call->getCalledFunction();

  if(fun == NULL)
    return inst;

  if(fun->getName().compare("pony_alloc") != 0)
    return inst;

  c->opt->check.stats.heap_alloc++;

  if(PointerMayBeCaptured(call, true, false))
  {
    print_transform(c, call, "captured allocation");
    return inst;
  }

  // TODO: what if it's not constant? could we still alloca?
  // https://github.com/ldc-developers/ldc/blob/master/gen/passes/
  // GarbageCollect2Stack.cpp
  Value* size = call->getArgOperand(0);
  ConstantInt* int_size = dyn_cast_or_null<ConstantInt>(size);

  if(int_size == NULL)
  {
    print_transform(c, call, "variable size allocation");
    return inst;
  }

  size_t alloc_size = int_size->getZExtValue();

  // Limit stack allocations to 1 kb each.
  if(alloc_size > 1024)
  {
    print_transform(c, call, "large allocation");
    return inst;
  }

  // All alloca should happen in the entry block of a function.
  LLVMBasicBlockRef block = LLVMGetInstructionParent(inst);
  LLVMValueRef func = LLVMGetBasicBlockParent(block);
  block = LLVMGetFirstBasicBlock(func);
  LLVMValueRef first_inst = LLVMGetFirstInstruction(block);
  LLVMPositionBuilderBefore(c->builder, first_inst);

  LLVMValueRef len = LLVMConstInt(c->i64, alloc_size, false);
  LLVMValueRef alloca = LLVMBuildArrayAlloca(c->builder, c->i8, len, "");

  Instruction* alloca_value = unwrap<Instruction>(alloca);
  alloca_value->setDebugLoc(call->getDebugLoc());

  BasicBlock::iterator iter(call);
  ReplaceInstWithValue(call->getParent()->getInstList(), iter, alloca_value);

  c->opt->check.stats.heap_alloc--;
  c->opt->check.stats.stack_alloc++;

  print_transform(c, alloca_value, "stack allocation");

  return wrap(alloca_value);
}

static void stack_alloc_block(compile_t* c, LLVMBasicBlockRef block)
{
  LLVMValueRef inst = LLVMGetFirstInstruction(block);

  while(inst != NULL)
  {
    inst = stack_alloc_inst(c, inst);
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
