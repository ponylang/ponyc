#include "gc_calls.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/Cloning.h>

using namespace llvm;

namespace pony {

bool isGcRootBorrow(const CallBase& call)
{
  if(!call.getMetadata("pony.gc.borrow") || call.arg_size() != 1 ||
    !call.getType()->isPointerTy() ||
    call.getType() != call.getArgOperand(0)->getType())
    return false;
  auto* identity = dyn_cast<InlineAsm>(call.getCalledOperand());
  return identity != nullptr && identity->hasSideEffects() &&
    identity->getAsmString().empty() &&
    identity->getConstraintString() == "=r,0";
}

static bool can_allocate(Function* root, bool early = false)
{
  SmallPtrSet<Function*, 32> seen;
  SmallVector<Function*, 16> work{root};
  while(!work.empty())
  {
    Function* f = work.pop_back_val();
    if(!seen.insert(f).second)
      continue;
    for(BasicBlock &bb : *f)
    {
      for(Instruction &inst : bb)
      {
        auto* call = dyn_cast<CallBase>(&inst);
        if(call == nullptr)
        {
          if(inst.mayWriteToMemory())
          {
            auto* store = dyn_cast<StoreInst>(&inst);
            if(store == nullptr)
              return true;
            bool pony_store = early && f->getMetadata("pony.abi") != nullptr;
            if(!pony_store &&
              !isa<AllocaInst>(getUnderlyingObject(store->getPointerOperand())))
              return true;
          }
          continue;
        }
        Function* callee = call->getCalledFunction();
        if(callee == nullptr)
          return true;
        if(callee->getName() == "ponyint_assert_fail" ||
          callee->getName() == "pony_ctx")
          continue;
        if(callee->isIntrinsic())
        {
          if(call->mayWriteToMemory() && !isa<LifetimeIntrinsic>(call))
            return true;
          continue;
        }
        if(callee->isDeclaration())
          return true;
        work.push_back(callee);
      }
    }
  }
  return false;
}

static bool small_allocation(Value* value)
{
  auto* call = dyn_cast<CallBase>(value);
  return call != nullptr && call->getCalledFunction() != nullptr &&
    call->getCalledFunction()->getName() == "pony_alloc_small";
}

// Count a recursive call as one small allocation. Its path must contain
// no other allocations, including after the call returns.
static bool at_most_one_small_allocation(Function &f)
{
  DenseMap<BasicBlock*, unsigned> reached;
  SmallVector<std::pair<BasicBlock*, unsigned>, 16> work{{&f.getEntryBlock(), 0}};
  while(!work.empty())
  {
    auto [bb, count] = work.pop_back_val();
    if(reached.lookup(bb) > count)
      continue;
    reached[bb] = count + 1;
    for(Instruction &inst : *bb)
    {
      if(auto* call = dyn_cast<CallBase>(&inst))
      {
        Function* callee = call->getCalledFunction();
        if(callee == &f || small_allocation(call))
        {
          if(++count > 1)
            return false;
        } else if(callee == nullptr ||
          (callee->getName() != "pony_ctx" &&
           callee->getName() != "ponyint_assert_fail" &&
           !(callee->isIntrinsic() &&
             (!call->mayWriteToMemory() || isa<LifetimeIntrinsic>(call))) &&
           (callee->isDeclaration() || can_allocate(callee)))) {
          return false;
        }
      } else if(inst.mayWriteToMemory()) {
        auto* store = dyn_cast<StoreInst>(&inst);
        if(store == nullptr)
          return false;
        Value* dest = getUnderlyingObject(store->getPointerOperand());
        if(!isa<AllocaInst>(dest) && !small_allocation(dest) &&
          !small_allocation(store->getValueOperand()))
          return false;
      }
    }
    for(BasicBlock* next : successors(bb))
      work.emplace_back(next, count);
  }
  return true;
}

static bool guard_bounded_call(Function &collecting, Function &ordinary)
{
  Function* room = ordinary.getParent()->getFunction("ponyint_gc_has_small_room");
  if(room == nullptr || room->isDeclaration() ||
    collecting.hasFnAttribute("pony-gc-bounded") ||
    !at_most_one_small_allocation(ordinary))
    return false;

  LLVMContext &ctx = ordinary.getContext();
  BasicBlock* slow = &collecting.getEntryBlock();
  BasicBlock* entry = BasicBlock::Create(ctx, "gc_check_room", &collecting, slow);
  BasicBlock* fast = BasicBlock::Create(ctx, "gc_has_room", &collecting, slow);
  IRBuilder<> builder(entry);
  if(auto* scope = collecting.getSubprogram())
    builder.SetCurrentDebugLocation(DILocation::get(ctx, scope->getLine(), 0,
      scope));
  CallInst* check = builder.CreateCall(room,
    {collecting.getArg(ordinary.arg_size())});
  check->setCallingConv(room->getCallingConv());
  builder.CreateCondBr(check, fast, slow);
  builder.SetInsertPoint(fast);
  SmallVector<Value*, 8> args;
  for(Argument &param : ordinary.args())
    args.push_back(collecting.getArg(param.getArgNo()));
  CallInst* call = builder.CreateCall(&ordinary, args);
  call->setCallingConv(ordinary.getCallingConv());
  call->setTailCallKind(CallInst::TCK_Tail);
  if(ordinary.getReturnType()->isVoidTy())
    builder.CreateRetVoid();
  else
    builder.CreateRet(call);
  InlineFunctionInfo info;
  InlineFunction(*check, info);
  collecting.addFnAttr("pony-gc-bounded");
  return true;
}

static bool optimize_pair(Function &collecting, Function* ordinary, bool early)
{
  if(ordinary->isDeclaration() ||
    ordinary->arg_size() + 2 != collecting.arg_size() ||
    ordinary->getReturnType() != collecting.getReturnType() ||
    ordinary->getCallingConv() != collecting.getCallingConv())
    return false;
  bool same_params = true;
  for(Argument &param : ordinary->args())
    same_params &= param.getType() ==
      collecting.getArg(param.getArgNo())->getType();
  if(!same_params || collecting.isDeclaration() ||
    !collecting.getArg(ordinary->arg_size())->getType()->isPointerTy())
    return false;
  if(can_allocate(ordinary, early))
  {
    return !early && guard_bounded_call(collecting, *ordinary);
  }

  SmallVector<CallInst*, 8> calls;
  for(User* user : collecting.users())
  {
    auto* call = dyn_cast<CallInst>(user);
    if(call != nullptr && call->getCalledFunction() == &collecting &&
      !call->hasOperandBundles() && !call->isMustTailCall())
      calls.push_back(call);
  }

  for(CallInst* call : calls)
  {
    SmallVector<Value*, 8> args;
    for(Argument &param : ordinary->args())
      args.push_back(call->getArgOperand(param.getArgNo()));
    IRBuilder<> builder(call);
    builder.SetCurrentDebugLocation(call->getDebugLoc());
    CallInst* replacement = builder.CreateCall(ordinary, args);
    replacement->setCallingConv(ordinary->getCallingConv());
    call->replaceAllUsesWith(replacement);
    call->eraseFromParent();
  }
  return !calls.empty();
}

PreservedAnalyses OptimizeGcCalls::run(Module &module, ModuleAnalysisManager &)
{
  bool changed = false;
  if(!early && module.getModuleFlag("pony.midbehavior.gc") != nullptr)
  {
    for(Function& f : module)
    {
      for(BasicBlock& bb : f)
      {
        for(Instruction& inst : make_early_inc_range(bb))
        {
          auto* call = dyn_cast<CallInst>(&inst);
          if(call == nullptr || !isGcRootBorrow(*call))
            continue;
          call->replaceAllUsesWith(call->getArgOperand(0));
          call->eraseFromParent();
          changed = true;
        }
      }
    }
  }
  for(GlobalVariable &global : module.globals())
  {
    if(global.getMetadata("pony.gc.pairs") == nullptr ||
      !global.hasInitializer())
      continue;
    auto* pairs = dyn_cast<ConstantArray>(global.getInitializer());
    if(pairs == nullptr)
      continue;
    for(unsigned i = 0; i + 1 < pairs->getNumOperands(); i += 2)
    {
      auto* ordinary = dyn_cast<Function>(pairs->getOperand(i));
      auto* collecting = dyn_cast<Function>(pairs->getOperand(i + 1));
      if(ordinary != nullptr && collecting != nullptr)
        changed |= optimize_pair(*collecting, ordinary, early);
    }
  }
  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

} // namespace pony
