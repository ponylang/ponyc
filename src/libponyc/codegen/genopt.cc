#ifdef _MSC_VER
#  pragma warning(push)
//because LLVM IR Builder code is broken: e.g. Instructions.h:521-527
#  pragma warning(disable:4244)
#  pragma warning(disable:4800)
#  pragma warning(disable:4267)
#  pragma warning(disable:4624)
#  pragma warning(disable:4141)
#endif

#include "genopt.h"
#include <string.h>

#include <llvm/IR/Module.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/DebugInfo.h>

#if PONY_LLVM >= 307
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#else
#include <llvm/ADT/Triple.h>
#include <llvm/PassManager.h>
#include <llvm/Target/TargetLibraryInfo.h>
#endif

#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/ADT/SmallSet.h>

#include "../../libponyrt/mem/heap.h"

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

using namespace llvm;

#if PONY_LLVM >= 307
using namespace llvm::legacy;
#endif

static __pony_thread_local compile_t* the_compiler;

static void print_transform(compile_t* c, Instruction* i, const char* s)
{
  errors_t* errors = c->opt->check.errors;

  if((c == NULL) || !c->opt->print_stats)
    return;

  /* Starting with LLVM 3.7.0-final getDebugLog may return a
   * DebugLoc without a valid underlying MDNode* for instructions
   * that have no direct source location, instead of returning 0
   * for getLine().
   */
#if PONY_LLVM >= 307
  while(!i->getDebugLoc())
#else
  while(i->getDebugLoc().getLine() == 0)
#endif
  {
    i = i->getNextNode();

    if(i == nullptr)
      return;
  }

  DebugLoc loc = i->getDebugLoc();

#if PONY_LLVM >= 307
  DILocation* location = loc.get();
  DIScope* scope = location->getScope();
  DILocation* at = location->getInlinedAt();
#else
  DIScope scope = DIScope(loc.getScope());
  MDLocation* at = cast_or_null<MDLocation>(loc.getInlinedAt());
#endif

  if(at != NULL)
  {
#if PONY_LLVM >= 307
    DIScope* scope_at = at->getScope();

    errorf(errors, NULL, "[%s] %s:%u:%u@%s:%u:%u: %s",
      i->getParent()->getParent()->getName().str().c_str(),
      scope->getFilename().str().c_str(), loc.getLine(), loc.getCol(),
      scope_at->getFilename().str().c_str(), at->getLine(),
      at->getColumn(), s);
#else
    DIScope scope_at = DIScope(cast_or_null<MDNode>(at->getScope()));

    errorf(errors, NULL, "[%s] %s:%u:%u@%s:%u:%u: %s",
      i->getParent()->getParent()->getName().str().c_str(),
      scope.getFilename().str().c_str(), loc.getLine(), loc.getCol(),
      scope_at.getFilename().str().c_str(), at->getLine(), at->getColumn(), s);
#endif
  }
  else {
#if PONY_LLVM >= 307
    errorf(errors, NULL, "[%s] %s:%u:%u: %s",
      i->getParent()->getParent()->getName().str().c_str(),
      scope->getFilename().str().c_str(), loc.getLine(), loc.getCol(), s);
#else
    errorf(errors, NULL, "[%s] %s:%u:%u: %s",
      i->getParent()->getParent()->getName().str().c_str(),
      scope.getFilename().str().c_str(), loc.getLine(), loc.getCol(), s);
#endif
  }
}

class HeapToStack : public FunctionPass
{
public:
  static char ID;
  compile_t* c;

  HeapToStack() : FunctionPass(ID)
  {
    c = the_compiler;
  }

  bool runOnFunction(Function& f)
  {
    DominatorTree& dt = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    BasicBlock& entry = f.getEntryBlock();
    IRBuilder<> builder(&entry, entry.begin());

    bool changed = false;

    for(auto block = f.begin(), end = f.end(); block != end; ++block)
    {
      for(auto iter = block->begin(), end = block->end(); iter != end; )
      {
        Instruction* inst = &(*(iter++));

        if(runOnInstruction(builder, inst, dt))
          changed = true;
      }
    }

    return changed;
  }

  bool runOnInstruction(IRBuilder<>& builder, Instruction* inst,
    DominatorTree& dt)
  {
    CallSite call(inst);

    if(!call.getInstruction())
      return false;

    Function* fun = call.getCalledFunction();

    if(fun == NULL)
      return false;

    bool small = false;

    if(fun->getName().compare("pony_alloc") == 0)
    {
      // Nothing.
    } else if(fun->getName().compare("pony_alloc_small") == 0) {
      small = true;
    } else {
      return false;
    }

    Value* size = call.getArgument(1);
    c->opt->check.stats.heap_alloc++;
    ConstantInt* int_size = dyn_cast_or_null<ConstantInt>(size);

    if(int_size == NULL)
    {
      print_transform(c, inst, "variable size allocation");
      return false;
    }

    uint64_t alloc_size = int_size->getZExtValue();

    if(small)
    {
      // Convert a heap index to a size.
      int_size = ConstantInt::get(builder.getInt64Ty(),
        ((int64_t)1) << (alloc_size + HEAP_MINBITS));
    } else {
      if(alloc_size > 1024)
      {
        print_transform(c, inst, "large allocation");
        return false;
      }
    }

    SmallVector<CallInst*, 4> tail;

    if(!canStackAlloc(inst, dt, tail))
      return false;

    for(auto iter = tail.begin(), end = tail.end(); iter != end; ++iter)
      (*iter)->setTailCall(false);

    // TODO: for variable size alloca, don't insert at the beginning.
    Instruction* begin = &(*call.getCaller()->getEntryBlock().begin());

    AllocaInst* replace = new AllocaInst(builder.getInt8Ty(), int_size, "",
      begin);

    replace->setDebugLoc(call->getDebugLoc());
    inst->replaceAllUsesWith(replace);
    inst->eraseFromParent();

    print_transform(c, replace, "stack allocation");
    c->opt->check.stats.heap_alloc--;
    c->opt->check.stats.stack_alloc++;

    return true;
  }

  bool canStackAlloc(Instruction* alloc, DominatorTree& dt,
    SmallVector<CallInst*, 4>& tail)
  {
    // This is based on the pass in the LDC compiler.
    SmallVector<Use*, 16> work;
    SmallSet<Use*, 16> visited;

    for(auto iter = alloc->use_begin(), end = alloc->use_end();
      iter != end; ++iter)
    {
      Use* use = &(*iter);
      visited.insert(use);
      work.push_back(use);
    }

    while(!work.empty())
    {
      Use* use = work.pop_back_val();
      Instruction* inst = cast<Instruction>(use->getUser());
      Value* value = use->get();

      switch(inst->getOpcode())
      {
        case Instruction::Call:
        case Instruction::Invoke:
        {
          CallSite call(inst);

          if(call.onlyReadsMemory())
          {
            // If the function is readnone or readonly, and the return value
            // isn't and does not contain a pointer, it isn't captured.
            Type* type = inst->getType();

            if(type->isVoidTy() ||
              type->isFPOrFPVectorTy() ||
              type->isIntOrIntVectorTy()
              )
            {
              // Doesn't return any pointers, so it isn't captured.
              break;
            }
          }

          auto first = call.arg_begin();

          for(auto iter = first, end = call.arg_end(); iter != end; ++iter)
          {
            if(iter->get() == value)
            {
              // If it's not marked as nocapture, it's captured.
              if(!call.doesNotCapture((unsigned)(iter - first)))
              {
                print_transform(c, alloc, "captured allocation");
                print_transform(c, inst, "captured here (call arg)");
                return false;
              }

              if(call.isCall())
              {
                CallInst* ci = cast<CallInst>(inst);

                if(ci->isTailCall())
                  tail.push_back(ci);
              }
            }
          }
          break;
        }

        case Instruction::Load:
          break;

        case Instruction::Store:
        {
          // If we store the pointer, it is captured.
          // TODO: if we store the pointer in something that is stack allocated
          // is it still captured?
          if(value == inst->getOperand(0))
          {
            print_transform(c, alloc, "captured allocation");
            print_transform(c, inst, "captured here (store)");
            return false;
          }
          break;
        }

        case Instruction::BitCast:
        case Instruction::GetElementPtr:
        case Instruction::PHI:
        case Instruction::Select:
        {
          // If a derived pointer can be reused, it can't be stack allocated.
          if(canBeReused(inst, alloc, dt))
            return false;

          // Check that the new value isn't captured.
          for(auto iter = inst->use_begin(), end = inst->use_end();
            iter != end; ++iter)
          {
            Use* use = &(*iter);

            if(visited.insert(use).second)
              work.push_back(use);
          }
          break;
        }

        default:
        {
          // If it's anything else, assume it's captured just in case.
          print_transform(c, alloc, "captured allocation");
          print_transform(c, inst, "captured here (unknown)");
          return false;
        }
      }
    }

    return true;
  }

  bool canBeReused(Instruction* def, Instruction* alloc, DominatorTree& dt)
  {
    if(def->use_empty() || !dt.dominates(def, alloc))
      return false;

    BasicBlock* def_block = def->getParent();
    BasicBlock* alloc_block = alloc->getParent();

    SmallSet<User*, 16> users;
    SmallSet<BasicBlock*, 16> user_blocks;

    for(auto iter = def->use_begin(), end = def->use_end();
      iter != end; ++iter)
    {
      Use* use = &(*iter);
      Instruction* user = cast<Instruction>(use->getUser());
      BasicBlock* user_block = user->getParent();

      if((alloc_block != user_block) && dt.dominates(alloc_block, user_block))
      {
        print_transform(c, alloc, "captured allocation");
        print_transform(c, def, "captured here (dominated reuse)");
        return true;
      }

      if(!isa<PHINode>(user))
      {
        users.insert(user);
        user_blocks.insert(user_block);
      }
    }

    typedef std::pair<BasicBlock*, Instruction*> Work;
    SmallVector<Work, 16> work;
    SmallSet<BasicBlock*, 16> visited;

    Instruction* start = alloc->getNextNode();
    work.push_back(Work(alloc_block, start));

    while(!work.empty())
    {
      Work w = work.pop_back_val();
      BasicBlock* bb = w.first;
      Instruction* inst = w.second;

      if(user_blocks.count(bb))
      {
        if((bb != def_block) && (bb != alloc_block))
        {
          print_transform(c, alloc, "captured allocation");
          print_transform(c, def,
            "captured here (block contains neither alloc nor def)");
          return true;
        }

        while(inst != nullptr)
        {
          if((inst == def) || (inst == alloc))
            break;

          if(users.count(inst))
          {
            print_transform(c, alloc, "captured allocation");
            print_transform(c, inst, "captured here (reused)");
            return true;
          }

          inst = inst->getNextNode();
        }
      }
      else if((bb == def_block) || ((bb == alloc_block) && (inst != start)))
      {
        continue;
      }

      TerminatorInst* term = bb->getTerminator();
      unsigned count = term->getNumSuccessors();

      for(unsigned i = 0; i < count; i++)
      {
        BasicBlock* successor = term->getSuccessor(i);
        inst = &successor->front();
        bool found = false;

        while(isa<PHINode>(inst))
        {
          if(def == cast<PHINode>(inst)->getIncomingValueForBlock(bb))
          {
            print_transform(c, alloc, "captured allocation");
            print_transform(c, inst, "captured here (phi use)");
            return true;
          }

          if(def == inst)
            found = true;

          inst = inst->getNextNode();
        }

        if(!found &&
          visited.insert(successor).second &&
          dt.dominates(def_block, successor))
        {
          work.push_back(Work(successor, inst));
        }
      }
    }

    return false;
  }

  void getAnalysisUsage(AnalysisUsage& use) const
  {
    use.addRequired<DominatorTreeWrapperPass>();
  }
};

char HeapToStack::ID = 0;

static RegisterPass<HeapToStack>
  H2S("heap2stack", "Move heap allocations to the stack");

static void addHeapToStackPass(const PassManagerBuilder& pmb,
  PassManagerBase& pm)
{
  if(pmb.OptLevel >= 2)
    pm.add(new HeapToStack());
}

class DispatchPonyCtx : public FunctionPass
{
public:
  static char ID;

  DispatchPonyCtx() : FunctionPass(ID)
  {}

  bool runOnFunction(Function& f)
  {
    // Check if we're in a Dispatch function.
    StringRef name = f.getName();
    if(name.size() < 10 || name.rfind("_Dispatch") != name.size() - 9)
      return false;

    assert(f.arg_size() > 0);

    Value* ctx = &(*f.arg_begin());

    bool changed = false;

    for(auto block = f.begin(), end = f.end(); block != end; ++block)
    {
      for(auto iter = block->begin(), end = block->end(); iter != end; ++iter)
      {
        Instruction* inst = &(*iter);

        if(runOnInstruction(inst, ctx))
          changed = true;
      }
    }

    return changed;
  }

  bool runOnInstruction(Instruction* inst, Value* ctx)
  {
    CallSite call(inst);

    if(!call.getInstruction())
      return false;

    Function* fun = call.getCalledFunction();

    if(fun == NULL)
      return false;

    if(fun->getName().compare("pony_ctx") != 0)
      return false;

    inst->replaceAllUsesWith(ctx);

    return true;
  }
};

char DispatchPonyCtx::ID = 0;

static RegisterPass<DispatchPonyCtx>
  DPC("dispatchponyctx", "Replace pony_ctx calls in a dispatch function by the\
                          context passed to the function");

static void addDispatchPonyCtxPass(const PassManagerBuilder& pmb,
  PassManagerBase& pm)
{
  if(pmb.OptLevel >= 2)
    pm.add(new DispatchPonyCtx());
}

class MergeRealloc : public FunctionPass
{
public:
  static char ID;
  Module* module;

  MergeRealloc() : FunctionPass(ID)
  {
    module = NULL;
  }

  bool doInitialization(Module& m)
  {
    module = &m;
    return false;
  }

  bool runOnFunction(Function& f)
  {
    BasicBlock& entry = f.getEntryBlock();
    IRBuilder<> builder(&entry, entry.begin());

    bool changed = false;
    SmallVector<Instruction*, 16> new_allocs;
    SmallVector<Instruction*, 16> removed;

    for(auto block = f.begin(), end = f.end(); block != end; ++block)
    {
      for(auto iter = block->begin(), end = block->end(); iter != end; ++iter)
      {
        Instruction* inst = &(*iter);

        if(runOnInstruction(builder, inst, new_allocs, removed))
          changed = true;
      }
    }

    while(!new_allocs.empty())
    {
      // If we get here, changed is already true
      Instruction* inst = new_allocs.pop_back_val();
      runOnInstruction(builder, inst, new_allocs, removed);
    }

    for(auto elt : removed)
      elt->eraseFromParent();

    return changed;
  }

  bool runOnInstruction(IRBuilder<>& builder, Instruction* inst,
    SmallVector<Instruction*, 16>& new_allocs,
    SmallVector<Instruction*, 16>& removed)
  {
    if(std::find(removed.begin(), removed.end(), inst) != removed.end())
      return false;

    CallSite call(inst);

    if(!call.getInstruction())
      return false;

    Function* fun = call.getCalledFunction();

    if(fun == NULL)
      return false;

    int alloc_type;

    if(fun->getName().compare("pony_alloc") == 0)
    {
      alloc_type = 0;
    } else if(fun->getName().compare("pony_alloc_small") == 0) {
      alloc_type = 1;
    } else if(fun->getName().compare("pony_alloc_large") == 0) {
      alloc_type = -1;
    } else if(fun->getName().compare("pony_realloc") == 0) {
      Value* old_ptr = call.getArgument(1);
      if(dyn_cast_or_null<ConstantPointerNull>(old_ptr) == NULL)
        return false;

      Value* new_size = call.getArgument(2);

      builder.SetInsertPoint(inst);
      Value* replace = mergeNoOp(builder, call.getArgument(0), new_size);
      new_allocs.push_back(reinterpret_cast<Instruction*>(replace));
      inst->replaceAllUsesWith(replace);
      removed.push_back(inst);

      return true;
    } else {
      return false;
    }

    Value* old_size = call.getArgument(1);
    ConstantInt* old_int_size = dyn_cast_or_null<ConstantInt>(old_size);

    CallInst* realloc = findRealloc(inst);
    Value* replace;

    if(old_int_size == NULL)
    {
      if(realloc == NULL || !isZeroRealloc(realloc))
        return false;

      do
      {
        realloc->replaceAllUsesWith(inst);
        realloc->eraseFromParent();
        realloc = findRealloc(inst);
      } while(realloc != NULL && isZeroRealloc(realloc));

      return true;
    } else {
      uint64_t old_alloc_size = old_int_size->getZExtValue();

      if(realloc == NULL)
      {
        if(alloc_type != 0)
          return false;

        // Not realloc'd, but we can still turn a generic allocation into a
        // small/large one.
        builder.SetInsertPoint(inst);
        replace = mergeConstant(builder, call.getArgument(0), old_alloc_size);
      } else {
        Value* new_size = realloc->getArgOperand(2);

        if(alloc_type > 0) // Small allocation.
          old_alloc_size = ((int64_t)1) << (old_alloc_size + HEAP_MINBITS);

        ConstantInt* new_int_size = dyn_cast_or_null<ConstantInt>(new_size);

        if(new_int_size == NULL)
        {
          if(old_alloc_size != 0)
            return false;

          builder.SetInsertPoint(realloc);
          replace = mergeNoOp(builder, call.getArgument(0), new_size);
          new_allocs.push_back(reinterpret_cast<Instruction*>(replace));
        } else {
          uint64_t new_alloc_size = new_int_size->getZExtValue();
          new_alloc_size = std::max(old_alloc_size, new_alloc_size);

          replace = mergeReallocChain(builder, call, &realloc, new_alloc_size,
            new_allocs);
        }

        realloc->replaceAllUsesWith(replace);
        realloc->eraseFromParent();
      }
    }

    inst->replaceAllUsesWith(replace);
    removed.push_back(inst);

    return true;
  }

  CallInst* findRealloc(Instruction* alloc)
  {
    CallInst* realloc = NULL;
    for(auto iter = alloc->use_begin(), end = alloc->use_end();
      iter != end; ++iter)
    {
      Use* use = &(*iter);
      CallInst* call = dyn_cast_or_null<CallInst>(use->getUser());

      if(call == NULL)
        continue;

      Function* fun = call->getCalledFunction();

      if(fun == NULL)
        continue;

      if(fun->getName().compare("pony_realloc") == 0)
      {
        if(realloc != NULL)
        {
          // TODO: Handle more than one realloc path (caused by conditionals).
          return NULL;
        }
        realloc = call;
      }
    }

    return realloc;
  }

  Value* mergeReallocChain(IRBuilder<>& builder, CallSite alloc,
    CallInst** last_realloc, uint64_t alloc_size,
    SmallVector<Instruction*, 16>& new_allocs)
  {
    builder.SetInsertPoint(alloc.getInstruction());
    Value* replace = mergeConstant(builder, alloc.getArgument(0), alloc_size);

    while(alloc_size == 0)
    {
      // replace is a LLVM null pointer here. We have to handle any realloc
      // chain before losing call use informations.

      CallInst* next_realloc = findRealloc(*last_realloc);
      if(next_realloc == NULL)
        break;

      Value* new_size = next_realloc->getArgOperand(2);
      ConstantInt* new_int_size = dyn_cast_or_null<ConstantInt>(new_size);

      if(new_int_size == NULL)
      {
        builder.SetInsertPoint(next_realloc);
        replace = mergeNoOp(builder, alloc.getArgument(0), new_size);
        alloc_size = 1;
      } else {
        alloc_size = new_int_size->getZExtValue();

        builder.SetInsertPoint(*last_realloc);
        replace = mergeConstant(builder, alloc.getArgument(0), alloc_size);
      }

      new_allocs.push_back(reinterpret_cast<Instruction*>(replace));
      (*last_realloc)->replaceAllUsesWith(replace);
      (*last_realloc)->eraseFromParent();
      *last_realloc = next_realloc;
    }

    return replace;
  }

  Value* mergeNoOp(IRBuilder<>& builder, Value* ctx, Value* size)
  {
    Function* alloc_fn = module->getFunction("pony_alloc");
    Value* args[2];
    args[0] = ctx;
    args[1] = size;

    CallInst* inst = builder.CreateCall(alloc_fn, ArrayRef<Value*>(args, 2));
    inst->setTailCall();
    return inst;
  }

  Value* mergeConstant(IRBuilder<>& builder, Value* ctx, uint64_t size)
  {
    Function* alloc_fn;
    ConstantInt* int_size;

    if(size == 0)
    {
      return ConstantPointerNull::get(builder.getInt8PtrTy());
    } else if(size <= HEAP_MAX) {
      alloc_fn = module->getFunction("pony_alloc_small");
      size = ponyint_heap_index(size);
      int_size = ConstantInt::get(builder.getInt32Ty(), size);
    } else {
      alloc_fn = module->getFunction("pony_alloc_large");
#ifdef PLATFORM_IS_ILP32
      int_size = ConstantInt::get(builder.getInt32Ty(), size);
#else
      int_size = ConstantInt::get(builder.getInt64Ty(), size);
#endif
    }

    Value* args[2];
    args[0] = ctx;
    args[1] = int_size;

    CallInst* inst = builder.CreateCall(alloc_fn, ArrayRef<Value*>(args, 2));
    inst->setTailCall();
    return inst;
  }

  bool isZeroRealloc(CallInst* realloc)
  {
    Value* new_size = realloc->getArgOperand(2);
    ConstantInt* new_int_size = dyn_cast_or_null<ConstantInt>(new_size);
    if(new_int_size == NULL)
      return false;

    uint64_t new_alloc_size = new_int_size->getZExtValue();
    if(new_alloc_size != 0)
      return false;
    return true;
  }
};

char MergeRealloc::ID = 0;

static RegisterPass<MergeRealloc>
  MR("mergerealloc", "Merge successive reallocations of the same variable");

static void addMergeReallocPass(const PassManagerBuilder& pmb,
  PassManagerBase& pm)
{
  if(pmb.OptLevel >= 2)
    pm.add(new MergeRealloc());
}

static void optimise(compile_t* c)
{
  the_compiler = c;

  Module* m = unwrap(c->module);
  TargetMachine* machine = reinterpret_cast<TargetMachine*>(c->machine);

  PassManager lpm;
  PassManager mpm;
  FunctionPassManager fpm(m);

#if PONY_LLVM >= 307
  TargetLibraryInfoImpl *tl = new TargetLibraryInfoImpl(
    Triple(m->getTargetTriple()));

  lpm.add(createTargetTransformInfoWrapperPass(
    machine->getTargetIRAnalysis()));

  mpm.add(new TargetLibraryInfoWrapperPass(*tl));
  mpm.add(createTargetTransformInfoWrapperPass(
    machine->getTargetIRAnalysis()));

  fpm.add(createTargetTransformInfoWrapperPass(
    machine->getTargetIRAnalysis()));
#else
  lpm.add(new DataLayoutPass());
  machine->addAnalysisPasses(lpm);

  TargetLibraryInfo *tl = new TargetLibraryInfo(Triple(m->getTargetTriple()));
  mpm.add(tl);
  mpm.add(new DataLayoutPass());
  machine->addAnalysisPasses(mpm);

  fpm.add(new DataLayoutPass());
  machine->addAnalysisPasses(fpm);
#endif

  PassManagerBuilder pmb;

  if(c->opt->release)
  {
    if(c->opt->verbosity >= VERBOSITY_MINIMAL)
      fprintf(stderr, "Optimising\n");

    pmb.OptLevel = 3;
    pmb.Inliner = createFunctionInliningPass(275);
  } else {
    pmb.OptLevel = 0;
  }

  pmb.LoopVectorize = true;
  pmb.SLPVectorize = true;
  pmb.RerollLoops = true;
  pmb.LoadCombine = true;
  pmb.MergeFunctions = true;

  pmb.addExtension(PassManagerBuilder::EP_Peephole,
    addMergeReallocPass);
  pmb.addExtension(PassManagerBuilder::EP_Peephole,
    addHeapToStackPass);
  pmb.addExtension(PassManagerBuilder::EP_ScalarOptimizerLate,
    addDispatchPonyCtxPass);

  pmb.populateFunctionPassManager(fpm);

  if(target_is_arm(c->opt->triple))
  {
    // On ARM, without this, trace functions are being loaded with a double
    // indirection with a debug binary. An ldr r0, [LABEL] is done, loading
    // the trace function address, but then ldr r2, [r0] is done to move the
    // address into the 3rd arg to pony_traceobject. This double indirection
    // gives a garbage trace function. In release mode, a different path is
    // used and this error doesn't happen. Forcing an OptLevel of 1 for the MPM
    // results in the alternate (working) asm being used for a debug build.
    if(!c->opt->release)
      pmb.OptLevel = 1;
  }

  pmb.populateModulePassManager(mpm);

  if(target_is_arm(c->opt->triple))
  {
    if(!c->opt->release)
      pmb.OptLevel = 0;
  }

  pmb.populateLTOPassManager(lpm);

  // There is a problem with optimised debug info in certain cases. This is
  // due to unknown bugs in the way ponyc is generating debug info. When they
  // are found and fixed, an optimised build should not always strip debug
  // info.
  if(c->opt->release)
    c->opt->strip_debug = true;

  if(c->opt->strip_debug)
    lpm.add(createStripSymbolsPass());

  fpm.doInitialization();

  for(Module::iterator f = m->begin(), end = m->end(); f != end; ++f)
    fpm.run(*f);

  fpm.doFinalization();
  mpm.run(*m);

  if(!c->opt->library)
    lpm.run(*m);
}

bool genopt(compile_t* c)
{
  errors_t* errors = c->opt->check.errors;

  // Finalise the debug info.
  LLVMDIBuilderFinalize(c->di);
  optimise(c);

  if(c->opt->verify)
  {
    if(c->opt->verbosity >= VERBOSITY_MINIMAL)
      fprintf(stderr, "Verifying\n");

    char* msg = NULL;

    if(LLVMVerifyModule(c->module, LLVMPrintMessageAction, &msg) != 0)
    {
      errorf(errors, NULL, "Module verification failed: %s", msg);
      LLVMDisposeMessage(msg);
      return false;
    }

    if(msg != NULL)
      LLVMDisposeMessage(msg);
  }

  return true;
}

bool target_is_linux(char* t)
{
  Triple triple = Triple(t);

  return triple.isOSLinux();
}

bool target_is_freebsd(char* t)
{
  Triple triple = Triple(t);

  return triple.isOSFreeBSD();
}

bool target_is_macosx(char* t)
{
  Triple triple = Triple(t);

  return triple.isMacOSX();
}

bool target_is_windows(char* t)
{
  Triple triple = Triple(t);

  return triple.isOSWindows();
}

bool target_is_posix(char* t)
{
  Triple triple = Triple(t);

  return triple.isMacOSX() || triple.isOSFreeBSD() || triple.isOSLinux();
}

bool target_is_x86(char* t)
{
  Triple triple = Triple(t);

  const char* arch = Triple::getArchTypePrefix(triple.getArch());

  return !strcmp("x86", arch);
}

bool target_is_arm(char* t)
{
  Triple triple = Triple(t);

  const char* arch = Triple::getArchTypePrefix(triple.getArch());

  return !strcmp("arm", arch);
}

bool target_is_lp64(char* t)
{
  Triple triple = Triple(t);

  return triple.isArch64Bit() && !triple.isOSWindows();
}

bool target_is_llp64(char* t)
{
  Triple triple = Triple(t);

  return triple.isArch64Bit() && triple.isOSWindows();
}

bool target_is_ilp32(char* t)
{
  Triple triple = Triple(t);

  return triple.isArch32Bit();
}

bool target_is_native128(char* t)
{
  Triple triple = Triple(t);

  return !triple.isArch32Bit() && !triple.isKnownWindowsMSVCEnvironment();
}
