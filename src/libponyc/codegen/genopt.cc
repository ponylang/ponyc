#include "genopt.h"
#include <string.h>

#include "llvm_config_begin.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/DebugInfo.h>

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Analysis/Lint.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Analysis/TargetTransformInfo.h>

#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/ADT/SmallSet.h>

#if PONY_LLVM >= 600
#include <llvm-c/DebugInfo.h>
#endif

#include "llvm_config_end.h"

#include "../../libponyrt/mem/heap.h"
#include "ponyassert.h"

using namespace llvm;
using namespace llvm::legacy;

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
  while(!i->getDebugLoc())
  {
    i = i->getNextNode();

    if(i == nullptr)
      return;
  }

  DebugLoc loc = i->getDebugLoc();

  DILocation* location = loc.get();
  DIScope* scope = location->getScope();
  DILocation* at = location->getInlinedAt();

  if(at != NULL)
  {
    DIScope* scope_at = at->getScope();

    errorf(errors, NULL, "[%s] %s:%u:%u@%s:%u:%u: %s",
      i->getParent()->getParent()->getName().str().c_str(),
      scope->getFilename().str().c_str(), loc.getLine(), loc.getCol(),
      scope_at->getFilename().str().c_str(), at->getLine(),
      at->getColumn(), s);
  }
  else {
    errorf(errors, NULL, "[%s] %s:%u:%u: %s",
      i->getParent()->getParent()->getName().str().c_str(),
      scope->getFilename().str().c_str(), loc.getLine(), loc.getCol(), s);
  }
}

// TODO: remove for 6.0.1: https://reviews.llvm.org/D44140
#if PONY_LLVM == 600
PONY_EXTERN_C_BEGIN
void LLVMInitializeInstCombine_Pony(LLVMPassRegistryRef R) {
  initializeInstructionCombiningPassPass(*unwrap(R));
}
PONY_EXTERN_C_END
#endif

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

        if(runOnInstruction(builder, inst, dt, f))
          changed = true;
      }
    }

    return changed;
  }

  bool runOnInstruction(IRBuilder<>& builder, Instruction* inst,
    DominatorTree& dt, Function& f)
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
    SmallVector<Instruction*, 4> new_calls;

    if(!canStackAlloc(inst, dt, tail, new_calls))
      return false;

    for(auto iter = tail.begin(), end = tail.end(); iter != end; ++iter)
      (*iter)->setTailCall(false);

    // TODO: for variable size alloca, don't insert at the beginning.
    Instruction* begin = &(*call.getCaller()->getEntryBlock().begin());

    unsigned int align = target_is_ilp32(c->opt->triple) ? 4 : 8;

#if PONY_LLVM < 500
    AllocaInst* replace = new AllocaInst(builder.getInt8Ty(), int_size, align,
      "", begin);
#else
    AllocaInst* replace = new AllocaInst(builder.getInt8Ty(),
      0, int_size, align, "", begin);
#endif

    replace->setDebugLoc(call->getDebugLoc());
    inst->replaceAllUsesWith(replace);

#if PONY_LLVM < 400
    (void)f;
#else
    if (call.isInvoke())
    {
      InvokeInst *invoke = cast<InvokeInst>(call.getInstruction());
      BranchInst::Create(invoke->getNormalDest(), invoke);
      invoke->getUnwindDest()->removePredecessor(call->getParent());
    }
    CallGraphWrapperPass *cg_pass =
      getAnalysisIfAvailable<CallGraphWrapperPass>();
    CallGraph *cg = cg_pass ? &cg_pass->getCallGraph() : nullptr;
    CallGraphNode *cg_node = cg ? (*cg)[&f] : nullptr;
    if (cg_node)
    {
      cg_node->removeCallEdgeFor(call);
    }
#endif
    inst->eraseFromParent();

    for(auto new_call: new_calls)
    {
      // Force constructor inlining to see if fields can be stack-allocated.
      InlineFunctionInfo ifi{};
      InlineFunction(CallSite(new_call), ifi);
    }

    print_transform(c, replace, "stack allocation");
    c->opt->check.stats.heap_alloc--;
    c->opt->check.stats.stack_alloc++;

    return true;
  }

  bool canStackAlloc(Instruction* alloc, DominatorTree& dt,
    SmallVector<CallInst*, 4>& tail, SmallVector<Instruction*, 4>& new_calls)
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

          if(inst->getMetadata("pony.newcall") != NULL)
            new_calls.push_back(inst);

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

#if PONY_LLVM >= 800
      Instruction* term = bb->getTerminator();
#else
      TerminatorInst* term = bb->getTerminator();
#endif
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
  if(pmb.OptLevel >= 2) {
    pm.add(new DominatorTreeWrapperPass());
    pm.add(new HeapToStack());
  }
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

    pony_assert(f.arg_size() > 0);

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

char DispatchPonyCtx::ID = 1;

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
  compile_t* c;
  Function* alloc_fn;
  Function* alloc_small_fn;
  Function* alloc_large_fn;

  MergeRealloc() : FunctionPass(ID)
  {
    c = the_compiler;
    alloc_fn = NULL;
    alloc_small_fn = NULL;
    alloc_large_fn = NULL;
  }

  bool doInitialization(Module& m)
  {
    alloc_fn = m.getFunction("pony_alloc");
    alloc_small_fn = m.getFunction("pony_alloc_small");
    alloc_large_fn = m.getFunction("pony_alloc_large");

    if(alloc_fn == NULL)
      alloc_fn = declareAllocFunction(m, "pony_alloc", unwrap(c->intptr),
        HEAP_MIN, true);

    if(alloc_small_fn == NULL)
      alloc_small_fn = declareAllocFunction(m, "pony_alloc_small",
        unwrap(c->i32), HEAP_MIN, false);

    if(alloc_large_fn == NULL)
      alloc_large_fn = declareAllocFunction(m, "pony_alloc_large",
        unwrap(c->intptr), HEAP_MAX << 1, false);

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
      // Alloc sizes always fit in size_t. We can safely cast.
      size_t old_alloc_size = (size_t)old_int_size->getZExtValue();

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
          old_alloc_size = ((size_t)1) << (old_alloc_size + HEAP_MINBITS);

        ConstantInt* new_int_size = dyn_cast_or_null<ConstantInt>(new_size);

        if(new_int_size == NULL)
        {
          if(old_alloc_size != 0)
            return false;

          builder.SetInsertPoint(realloc);
          replace = mergeNoOp(builder, call.getArgument(0), new_size);
          new_allocs.push_back(reinterpret_cast<Instruction*>(replace));
        } else {
          size_t new_alloc_size = (size_t)new_int_size->getZExtValue();
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
    CallInst** last_realloc, size_t alloc_size,
    SmallVector<Instruction*, 16>& new_allocs)
  {
    builder.SetInsertPoint(alloc.getInstruction());
    Value* replace = mergeConstant(builder, alloc.getArgument(0), alloc_size);

    while(alloc_size == 0)
    {
      // replace is a LLVM null pointer here. We have to handle any realloc
      // chain before losing call use information.

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
        alloc_size = (size_t)new_int_size->getZExtValue();

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
    Value* args[2];
    args[0] = ctx;
    args[1] = size;

    CallInst* inst = builder.CreateCall(alloc_fn, ArrayRef<Value*>(args, 2));
    inst->setTailCall();
    return inst;
  }

  Value* mergeConstant(IRBuilder<>& builder, Value* ctx, size_t size)
  {
    Function* used_alloc_fn;
    IntegerType* int_type;

    if(size == 0)
    {
      return ConstantPointerNull::get(builder.getInt8PtrTy());
    } else if(size <= HEAP_MAX) {
      used_alloc_fn = alloc_small_fn;
      int_type = unwrap<IntegerType>(c->i32);
      size = ponyint_heap_index(size);
    } else {
      used_alloc_fn = alloc_large_fn;
      int_type = unwrap<IntegerType>(c->intptr);
    }

    Value* args[2];
    args[0] = ctx;
    args[1] = ConstantInt::get(int_type, size);

    CallInst* inst = builder.CreateCall(used_alloc_fn,
      ArrayRef<Value*>(args, 2));
    inst->setTailCall();
    return inst;
  }

  Function* declareAllocFunction(Module& m, std::string const& name,
    Type* size_type, size_t min_size, bool can_be_null)
  {
    Type* params[2];
    params[0] = unwrap(c->void_ptr);
    params[1] = size_type;

    FunctionType* fn_type = FunctionType::get(unwrap(c->void_ptr),
      ArrayRef<Type*>(params, 2), false);
    Function* fn = Function::Create(fn_type, Function::ExternalLinkage, name,
      &m);

#if PONY_LLVM < 500
    unsigned functionIndex = AttributeSet::FunctionIndex;
    unsigned returnIndex = AttributeSet::ReturnIndex;
#else
    unsigned functionIndex = AttributeList::FunctionIndex;
    unsigned returnIndex = AttributeList::ReturnIndex;
#endif

    fn->addAttribute(functionIndex, Attribute::NoUnwind);
    fn->addAttribute(functionIndex,
      Attribute::InaccessibleMemOrArgMemOnly);

#if PONY_LLVM < 500
    fn->setDoesNotAlias(0);
#endif

    if(can_be_null)
      fn->addDereferenceableOrNullAttr(returnIndex, min_size);
    else
      fn->addDereferenceableAttr(returnIndex, min_size);

    AttrBuilder attr;
    attr.addAlignmentAttr(target_is_ilp32(c->opt->triple) ? 4 : 8);
#if PONY_LLVM < 500
    fn->addAttributes(returnIndex, AttributeSet::get(m.getContext(),
      returnIndex, attr));
#else
    fn->addAttributes(returnIndex, AttributeSet::get(m.getContext(), attr));
#endif
    return fn;
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

char MergeRealloc::ID = 2;

static RegisterPass<MergeRealloc>
  MR("mergerealloc", "Merge successive reallocations of the same variable");

static void addMergeReallocPass(const PassManagerBuilder& pmb,
  PassManagerBase& pm)
{
  if(pmb.OptLevel >= 2)
    pm.add(new MergeRealloc());
}

class MergeMessageSend : public BasicBlockPass
{
public:
  struct MsgFnGroup
  {
    BasicBlock::iterator alloc, trace, done, send;
  };

  enum MsgKind
  {
    MsgNone,
    MsgNoTrace,
    MsgTrace
  };

  static char ID;
  compile_t* c;
  Function* send_next_fn;
  Function* msg_chain_fn;
  Function* sendv_single_fn;

  MergeMessageSend() : BasicBlockPass(ID)
  {
    c = the_compiler;
    send_next_fn = nullptr;
    msg_chain_fn = nullptr;
    sendv_single_fn = nullptr;
  }

  bool doInitialization(Module& m)
  {
    send_next_fn = m.getFunction("pony_send_next");
    msg_chain_fn = m.getFunction("pony_chain");
    sendv_single_fn = m.getFunction("pony_sendv_single");

    if(send_next_fn == NULL)
      send_next_fn = declareTraceNextFn(m);

    if(msg_chain_fn == NULL)
      msg_chain_fn = declareMsgChainFn(m);

    return false;
  }

  bool doInitialization(Function& f)
  {
    (void)f;
    return false;
  }

  bool runOnBasicBlock(BasicBlock& b)
  {
    auto start = b.begin();
    auto end = b.end();
    bool changed = false;
    SmallVector<CallInst*, 16> sends;

    while(start != end)
    {
      MsgFnGroup first;
      MsgKind first_kind = findMsgSend(start, end, true, first);

      switch(first_kind)
      {
        case MsgNone:
          start = end;
          break;

        default:
        {
          sends.push_back(cast<CallInst>(&(*first.send)));
          bool stop = false;

          while(!stop)
          {
            MsgFnGroup next;
            MsgKind next_kind = findMsgSend(std::next(first.send), end, false,
              next);

            switch(next_kind)
            {
              case MsgNone:
                stop = true;
                break;

              default:
                sends.push_back(cast<CallInst>(&(*next.send)));
                mergeMsgSend(first, next, first_kind, next_kind);
                first = next;
                first_kind = next_kind;
                changed = true;
                break;
            }
          }

          sends.push_back(nullptr);
          start = std::next(first.send);
          break;
        }
      }
    }

    if(!sends.empty())
      makeMsgChains(sends);

    return changed;
  }

  MsgKind findMsgSend(BasicBlock::iterator start, BasicBlock::iterator end,
    bool can_pass_writes, MsgFnGroup& out_calls)
  {
    auto alloc = findCallTo("pony_alloc_msg", start, end, can_pass_writes);

    if(alloc == end)
      return MsgNone;

    decltype(alloc) trace;
    size_t fn_index;

    std::tie(trace, fn_index) =
      findCallTo(std::vector<StringRef>{"pony_gc_send", "pony_sendv",
        "pony_sendv_single"}, std::next(alloc), end, true);

    switch(fn_index)
    {
      case (size_t)-1:
        return MsgNone;

      case 0:
        break;

      default:
        out_calls.alloc = alloc;
        out_calls.send = trace;
        return MsgNoTrace;
    }

    auto done = findCallTo("pony_send_done", std::next(trace), end, true);

    if(done == end)
      return MsgNone;

    auto send = findCallTo(std::vector<StringRef>{"pony_sendv",
      "pony_sendv_single"}, std::next(done), end, true).first;

    if(send == end)
      return MsgNone;

    out_calls.alloc = alloc;
    out_calls.trace = trace;
    out_calls.done = done;
    out_calls.send = send;
    return MsgTrace;
  }

  BasicBlock::iterator findCallTo(StringRef name,
    BasicBlock::iterator start, BasicBlock::iterator end, bool can_pass_writes)
  {
    return findCallTo(std::vector<StringRef>{name}, start, end, can_pass_writes).first;
  }

  std::pair<BasicBlock::iterator, size_t> findCallTo(
      std::vector<StringRef> const& names, BasicBlock::iterator start,
    BasicBlock::iterator end, bool can_pass_writes)
  {
    for(auto iter = start; iter != end; ++iter)
    {
      if(iter->getMetadata("pony.msgsend") != NULL)
      {
        CallSite call(&(*iter));
        Function* fun = call.getCalledFunction();
        pony_assert(fun != NULL);

        for(size_t i = 0; i < names.size(); i++)
        {
          if(fun->getName().compare(names[i]) == 0)
            return {iter, i};
        }
      }

      if(!can_pass_writes && iter->mayWriteToMemory())
        break;
    }

    return {end, -1};
  }

  void mergeMsgSend(MsgFnGroup& first, MsgFnGroup& next, MsgKind first_kind,
    MsgKind& next_kind)
  {
    pony_assert((first_kind != MsgNone) && (next_kind != MsgNone));

    if(first_kind == MsgNoTrace)
    {
      auto src = first.send;
      auto dst = next.send;

      while(1)
      {
        auto prev = std::prev(src);
        src->moveBefore(&(*dst));

        if(prev->getMetadata("pony.msgsend") == NULL)
          break;

        CallSite call(&(*prev));
        auto fun = call.getCalledFunction();
        pony_assert(fun != NULL);

        if(fun->getName().compare("pony_alloc_msg") == 0)
          break;

        dst = src;
        src = prev;
      }
    } else {
      auto iter = first.alloc;

      while(iter != first.trace)
      {
        auto& inst = (*iter++);
        inst.moveBefore(&(*next.alloc));
      }

      if(next_kind == MsgNoTrace)
      {
        auto first_send_post = std::next(first.send);

        while(iter != first_send_post)
        {
          auto& inst = *(iter++);
          if(inst.getOpcode() == Instruction::Call)
            inst.moveBefore(&(*next.send));
        }

        next.trace = first.trace;
        next.done = first.done;
        next_kind = MsgTrace;
      } else {
        while(iter != first.done)
        {
          auto& inst = *(iter++);
          if(inst.getOpcode() == Instruction::Call)
            inst.moveBefore(&(*next.trace));
        }

        iter++;
        first.done->eraseFromParent();
        CallSite call(&(*next.trace));
        call.setCalledFunction(send_next_fn);

        auto first_send_post = std::next(first.send);
        auto next_done_post = std::next(next.done);

        while(iter != first_send_post)
        {
          auto& inst = *(iter++);
          if(inst.getOpcode() == Instruction::Call)
            inst.moveBefore(&(*next_done_post));
        }

        next.trace = first.trace;
      }
    }
  }

  bool makeMsgChains(SmallVector<CallInst*, 16> const& sends)
  {
    if(sends.size() < 2)
      return false;

    auto iter = std::begin(sends);
    auto next = std::next(iter);
    Value* chain_start = nullptr;
    bool is_single = false;
    bool changed = false;

    while(true)
    {
      pony_assert(*iter != nullptr);

      if((next == std::end(sends)) || (*next == nullptr))
      {
        if(chain_start != nullptr)
        {
          (*iter)->setArgOperand(2, chain_start);
          chain_start = nullptr;

          if(is_single)
          {
            pony_assert(sendv_single_fn != nullptr);
            (*iter)->setCalledFunction(sendv_single_fn);
            is_single = false;
          }
        }

        if(next == std::end(sends))
          break;

        iter = std::next(next);

        if(iter == std::end(sends))
          break;

        next = std::next(iter);
        continue;
      }

      auto prev_msg = (*iter)->getArgOperand(2);
      auto next_msg = (*next)->getArgOperand(2);

      pony_assert(prev_msg == (*iter)->getArgOperand(3));
      pony_assert(next_msg == (*next)->getArgOperand(3));

      if((*iter)->getArgOperand(1) == (*next)->getArgOperand(1))
      {
        if((*iter)->getCalledFunction() == sendv_single_fn)
          is_single = true;

        auto chain_call = CallInst::Create(msg_chain_fn, {prev_msg, next_msg},
          "", *iter);
        chain_call->setTailCall();
        (*iter)->eraseFromParent();

        if(chain_start == nullptr)
        {
          chain_start = prev_msg;
          changed = true;
        }
      } else if(chain_start != nullptr) {
        (*iter)->setArgOperand(2, chain_start);
        chain_start = nullptr;

        if(is_single)
        {
          pony_assert(sendv_single_fn != nullptr);
          (*iter)->setCalledFunction(sendv_single_fn);
          is_single = false;
        }
      }

      iter = next;
      next = std::next(iter);
    }

    return changed;
  }

  Function* declareTraceNextFn(Module& m)
  {
    FunctionType* fn_type = FunctionType::get(unwrap(c->void_type),
      {unwrap(c->void_ptr)}, false);
    Function* fn = Function::Create(fn_type, Function::ExternalLinkage,
      "pony_send_next", &m);

#if PONY_LLVM < 500
    unsigned functionIndex = AttributeSet::FunctionIndex;
#else
    unsigned functionIndex = AttributeList::FunctionIndex;
#endif

    fn->addAttribute(functionIndex, Attribute::NoUnwind);
    return fn;
  }

  Function* declareMsgChainFn(Module& m)
  {
    FunctionType* fn_type = FunctionType::get(unwrap(c->void_type),
      {unwrap(c->msg_ptr), unwrap(c->msg_ptr)}, false);
    Function* fn = Function::Create(fn_type, Function::ExternalLinkage,
      "pony_chain", &m);

#if PONY_LLVM < 500
    unsigned functionIndex = AttributeSet::FunctionIndex;
#else
    unsigned functionIndex = AttributeList::FunctionIndex;
#endif

    fn->addAttribute(functionIndex, Attribute::NoUnwind);
    fn->addAttribute(functionIndex, Attribute::ArgMemOnly);
    fn->addAttribute(1, Attribute::NoCapture);
    fn->addAttribute(2, Attribute::ReadNone);
    return fn;
  }
};

char MergeMessageSend::ID = 3;

static RegisterPass<MergeMessageSend>
  MMS("mergemessagesend", "Group message sends in the same BasicBlock");

static void addMergeMessageSendPass(const PassManagerBuilder& pmb,
  PassManagerBase& pm)
{
  if(pmb.OptLevel >= 2)
    pm.add(new MergeMessageSend());
}

static void optimise(compile_t* c, bool pony_specific)
{
  the_compiler = c;

  Module* m = unwrap(c->module);
  TargetMachine* machine = reinterpret_cast<TargetMachine*>(c->machine);

  llvm::legacy::PassManager lpm;
  llvm::legacy::PassManager mpm;
  llvm::legacy::FunctionPassManager fpm(m);

  TargetLibraryInfoImpl tl = TargetLibraryInfoImpl(
    Triple(m->getTargetTriple()));

  lpm.add(createTargetTransformInfoWrapperPass(
    machine->getTargetIRAnalysis()));

  mpm.add(new TargetLibraryInfoWrapperPass(tl));
  mpm.add(createTargetTransformInfoWrapperPass(
    machine->getTargetIRAnalysis()));

  fpm.add(createTargetTransformInfoWrapperPass(
    machine->getTargetIRAnalysis()));

  PassManagerBuilder pmb;

  if(c->opt->release)
  {
    if(c->opt->verbosity >= VERBOSITY_MINIMAL)
      fprintf(stderr, "Optimising\n");

    pmb.OptLevel = 3;
    pmb.Inliner = createFunctionInliningPass(275);
    pmb.MergeFunctions = true;
  } else {
    pmb.OptLevel = 0;
  }

  pmb.LoopVectorize = true;
  pmb.SLPVectorize = true;
  pmb.RerollLoops = true;
#if PONY_LLVM < 500
  pmb.LoadCombine = true;
#endif

  if(pony_specific)
  {
    pmb.addExtension(PassManagerBuilder::EP_Peephole,
      addMergeReallocPass);
    pmb.addExtension(PassManagerBuilder::EP_Peephole,
      addHeapToStackPass);
    pmb.addExtension(PassManagerBuilder::EP_ScalarOptimizerLate,
      addDispatchPonyCtxPass);
    pmb.addExtension(PassManagerBuilder::EP_ScalarOptimizerLate,
      addMergeMessageSendPass);
  }

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

#if PONY_LLVM >= 700
  // LLVM 7 has a bug where running MergeFunctions more than once causes an
  // assert fail saying "Invalid RAUW on key of ValueMap". We can avoid that
  // by setting to false before populating lpm, after mpm was already populated.
  pmb.MergeFunctions = false;
#endif
  pmb.populateLTOPassManager(lpm);

  // There is a problem with optimised debug info in certain cases. This is
  // due to unknown bugs in the way ponyc is generating debug info. When they
  // are found and fixed, an optimised build should not always strip debug
  // info.
  if(c->opt->release)
    c->opt->strip_debug = true;

  if(c->opt->strip_debug)
    lpm.add(createStripSymbolsPass());

  if(c->opt->lint_llvm)
    fpm.add(createLintPass());

  fpm.doInitialization();

  for(Module::iterator f = m->begin(), end = m->end(); f != end; ++f)
    fpm.run(*f);

  fpm.doFinalization();
  mpm.run(*m);

  if(!c->opt->library)
    lpm.run(*m);
}

bool genopt(compile_t* c, bool pony_specific)
{
  errors_t* errors = c->opt->check.errors;

  // Finalise the debug info.
  LLVMDIBuilderFinalize(c->di);
  optimise(c, pony_specific);

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

bool target_is_bsd(char* t)
{
  Triple triple = Triple(t);

  return triple.isOSDragonFly() || triple.isOSFreeBSD() || triple.isOSOpenBSD();
}

bool target_is_freebsd(char* t)
{
  Triple triple = Triple(t);

  return triple.isOSFreeBSD();
}

bool target_is_dragonfly(char* t)
{
  Triple triple = Triple(t);

  return triple.isOSDragonFly();
}

bool target_is_openbsd(char* t)
{
  Triple triple = Triple(t);

  return triple.isOSOpenBSD();
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

  return triple.isMacOSX() || triple.isOSFreeBSD() || triple.isOSLinux()
    || triple.isOSDragonFly() || triple.isOSOpenBSD();
}

bool target_is_x86(char* t)
{
  Triple triple = Triple(t);

#if PONY_LLVM >= 400
  const char* arch = Triple::getArchTypePrefix(triple.getArch()).data();
#else
  const char* arch = Triple::getArchTypePrefix(triple.getArch());
#endif

  return !strcmp("x86", arch);
}

bool target_is_arm(char* t)
{
  Triple triple = Triple(t);

#if PONY_LLVM >= 400
  const char* arch = Triple::getArchTypePrefix(triple.getArch()).data();
#else
  const char* arch = Triple::getArchTypePrefix(triple.getArch());
#endif

  return (!strcmp("arm", arch) || !strcmp("aarch64", arch));
}

// This function is used to safeguard against potential oversights on the size
// of Bool on any future port to PPC32.
// We do not currently support compilation to PPC. It could work, but no
// guarantees.
bool target_is_ppc(char* t)
{
  Triple triple = Triple(t);

#if PONY_LLVM >= 400
  const char* arch = Triple::getArchTypePrefix(triple.getArch()).data();
#else
  const char* arch = Triple::getArchTypePrefix(triple.getArch());
#endif

  return !strcmp("ppc", arch);
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

bool target_is_bigendian(char* t)
{
  Triple triple = Triple(t);

  return !triple.isLittleEndian();
}

bool target_is_littleendian(char* t)
{
  Triple triple = Triple(t);

  return triple.isLittleEndian();
}
