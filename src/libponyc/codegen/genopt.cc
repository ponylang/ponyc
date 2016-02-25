#ifdef _MSC_VER
#  pragma warning(push)
//because LLVM IR Builder code is broken: e.g. Instructions.h:521-527
#  pragma warning(disable:4244)
#  pragma warning(disable:4800)
#  pragma warning(disable:4267)
#endif

#include "genopt.h"
#include <string.h>

#include <llvm/IR/Module.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/DebugInfo.h>

#if PONY_LLVM >= 307
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#else
#include <llvm/PassManager.h>
#include <llvm/Target/TargetLibraryInfo.h>
#endif

#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/ADT/Triple.h>
#include <llvm/ADT/SmallSet.h>
#include <llvm/ADT/SmallVector.h>

#include "../../libponyrt/mem/heap.h"

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

using namespace llvm;

#if PONY_LLVM >= 307
using namespace llvm::legacy;
#endif

static __pony_thread_local compile_t* the_compiler;

static void print_transform(compile_t* c, Instruction* inst, const char* s)
{
  if((c == NULL) || !c->opt->print_stats)
    return;

  Instruction* i = inst;

  while(i->getDebugLoc().getLine() == 0)
  {
    BasicBlock::iterator iter = i;

    if(++iter == i->getParent()->end())
      return;

    i = iter;
  }

  DebugLoc loc = i->getDebugLoc();

#if PONY_LLVM >= 307
  DIScope scope = DIScope(cast_or_null<MDScope>(loc.getScope()));
#else
  DIScope scope = DIScope(loc.getScope());
#endif

  MDLocation* at = cast_or_null<MDLocation>(loc.getInlinedAt());

  if(at != NULL)
  {
#if PONY_LLVM >= 307
    DIScope scope_at = DIScope(cast_or_null<MDScope>(at->getScope()));
#else
    DIScope scope_at = DIScope(cast_or_null<MDNode>(at->getScope()));
#endif

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

class HeapToStack : public FunctionPass
{
public:
  static char ID;
  compile_t* c;
  Module* module;

  HeapToStack() : FunctionPass(ID)
  {
    c = the_compiler;
    module = NULL;
  }

  bool doInitialization(Module& m)
  {
    module = &m;
    return false;
  }

  bool runOnFunction(Function& f)
  {
    DominatorTree& dt = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    BasicBlock& entry = f.getEntryBlock();
    IRBuilder<> builder(&entry, entry.begin());

    bool changed = false;

    for(auto block = f.begin(), end = f.end(); block != end; ++block)
    {
      for(auto iter = block->begin(), end = block->end(); iter != end; ++iter)
      {
        Instruction* inst = iter;

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

    Value* size;
    bool small = false;

    if(fun->getName().compare("pony_alloc") == 0)
    {
      // Nothing.
    } else if(fun->getName().compare("pony_alloc_small") == 0) {
      small = true;
    } else {
      return false;
    }

    size = call.getArgument(1);
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
        1 << (alloc_size + HEAP_MINBITS));
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
    Instruction* begin = call.getCaller()->getEntryBlock().begin();
    AllocaInst* replace = new AllocaInst(builder.getInt8Ty(), int_size, "",
      begin);

    replace->setDebugLoc(call->getDebugLoc());
    inst->replaceAllUsesWith(replace);

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

    typedef std::pair<BasicBlock*, BasicBlock::iterator> Work;
    SmallVector<Work, 16> work;
    SmallSet<BasicBlock*, 16> visited;

    BasicBlock::iterator start = alloc;
    ++start;
    work.push_back(Work(alloc_block, start));

    while(!work.empty())
    {
      Work w = work.pop_back_val();
      BasicBlock* bb = w.first;
      BasicBlock::iterator iter = w.second;

      if(user_blocks.count(bb))
      {
        if((bb != def_block) && (bb != alloc_block))
        {
          print_transform(c, alloc, "captured allocation");
          print_transform(c, def,
            "captured here (block contains neither alloc nor def)");
          return true;
        }

        for(auto end = bb->end(); iter != end; ++iter)
        {
          if((&(*iter) == def) || (&(*iter) == alloc))
            break;

          if(users.count(iter))
          {
            print_transform(c, alloc, "captured allocation");
            print_transform(c, &(*iter), "captured here (reused)");
            return true;
          }
        }
      }
      else if((bb == def_block) || ((bb == alloc_block) && (iter != start)))
      {
        continue;
      }

      TerminatorInst* term = bb->getTerminator();
      unsigned count = term->getNumSuccessors();

      for(unsigned i = 0; i < count; i++)
      {
        BasicBlock* successor = term->getSuccessor(i);
        iter = successor->begin();
        bool found = false;

        while(isa<PHINode>(iter))
        {
          if(def == cast<PHINode>(iter)->getIncomingValueForBlock(bb))
          {
            print_transform(c, alloc, "captured allocation");
            print_transform(c, &(*iter), "captured here (phi use)");
            return true;
          }

          if(def == &(*iter))
            found = true;

          ++iter;
        }

        if(!found &&
          visited.insert(successor).second &&
          dt.dominates(def_block, successor))
        {
          work.push_back(Work(successor, iter));
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
  X("heap2stack", "Move heap allocations to the stack");

static void addHeapToStackPass(const PassManagerBuilder& pmb,
  PassManagerBase& pm)
{
  if(pmb.OptLevel >= 2)
    pm.add(new HeapToStack());
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
    printf("Optimising\n");
    pmb.OptLevel = 3;
    pmb.Inliner = createFunctionInliningPass(275);
  } else {
    pmb.OptLevel = 0;
  }

  pmb.BBVectorize = true;
  pmb.LoopVectorize = true;
  pmb.SLPVectorize = true;
  pmb.RerollLoops = true;
  pmb.LoadCombine = true;

  pmb.addExtension(PassManagerBuilder::EP_LoopOptimizerEnd,
    addHeapToStackPass);

  pmb.populateFunctionPassManager(fpm);

  if(target_is_arm(c->opt->triple))
  {
  // On ARM, without this, trace functions are being loaded with a double
  // indirection with a debug binary. An ldr r0, [LABEL] is done, loading
  // the trace function address, but then ldr r2, [r0] is done to move the
  // address into the 3rd arg to pony_traceobject. This double indirection
  // gives a garbage trace function. In release mode, a different path is used
  // and this error doesn't happen. Forcing an OptLevel of 1 for the MPM
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
  // Finalise the DWARF info.
  dwarf_finalise(&c->dwarf);
  optimise(c);

  if(c->opt->verify)
  {
    printf("Verifying\n");
    char* msg = NULL;

    if(LLVMVerifyModule(c->module, LLVMPrintMessageAction, &msg) != 0)
    {
      errorf(NULL, "Module verification failed: %s", msg);
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

