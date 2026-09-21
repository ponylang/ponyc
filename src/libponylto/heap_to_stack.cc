#include "heap_to_stack.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LazyCallGraph.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/ADT/SmallSet.h>
#include <llvm/ADT/Statistic.h>
#include <llvm/Support/Debug.h>

using namespace llvm;

#define DEBUG_TYPE "pony-heap-to-stack"

STATISTIC(NumHeapAlloc, "Number of heap allocations considered");
STATISTIC(NumStackAlloc, "Number of allocations promoted to stack");

// Pony's heap allocator uses size classes starting at 2^HEAP_MINBITS bytes.
// This must match HEAP_MINBITS in libponyrt/mem/heap.h.
static constexpr unsigned HEAP_MINBITS = 5;

// The alloca's alignment must match the runtime's heap guarantee. The
// `align` attribute on pony_alloc/pony_alloc_small carries that value;
// it is set in init_runtime_decls (codegen.c).

static void printTransform(Instruction *i, const char *s)
{
  LLVM_DEBUG({
    while(i && !i->getDebugLoc())
      i = i->getNextNode();

    if(i == nullptr)
      return;

    DebugLoc loc = i->getDebugLoc();
    DILocation *location = loc.get();
    DIScope *scope = location->getScope();
    DILocation *at = location->getInlinedAt();

    if(at != NULL)
    {
      DIScope *scope_at = at->getScope();
      dbgs() << "[" << i->getParent()->getParent()->getName()
             << "] " << scope->getFilename() << ":" << loc.getLine()
             << ":" << loc.getCol() << "@" << scope_at->getFilename()
             << ":" << at->getLine() << ":" << at->getColumn()
             << ": " << s << "\n";
    }
    else
    {
      dbgs() << "[" << i->getParent()->getParent()->getName()
             << "] " << scope->getFilename() << ":" << loc.getLine()
             << ":" << loc.getCol() << ": " << s << "\n";
    }
  });
}

namespace pony {

PreservedAnalyses HeapToStack::run(LazyCallGraph::SCC &C,
  CGSCCAnalysisManager &AM, LazyCallGraph &CG, CGSCCUpdateResult &UR)
{
  auto &FAM =
    AM.getResult<FunctionAnalysisManagerCGSCCProxy>(C, CG).getManager();

  bool changed = false;

  SmallVector<Function*, 4> functions;
  for(LazyCallGraph::Node &N : C)
    functions.push_back(&N.getFunction());

  for(Function *fp : functions)
  {
    if(fp->isDeclaration())
      continue;

    if(runOnFunction(*fp, FAM, CG, AM, UR))
      changed = true;
  }

  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

bool HeapToStack::runOnFunction(Function &f, FunctionAnalysisManager &FAM,
  LazyCallGraph &CG, CGSCCAnalysisManager &AM, CGSCCUpdateResult &UR)
{
  bool changed = false;
  bool restart;

  do
  {
    restart = false;
    DominatorTree &dt = FAM.getResult<DominatorTreeAnalysis>(f);
    BasicBlock &entry = f.getEntryBlock();
    IRBuilder<> builder(&entry, entry.begin());

    for(auto block = f.begin(); block != f.end(); ++block)
    {
      for(auto iter = block->begin(); iter != block->end();)
      {
        Instruction *inst = &(*iter);

        if(runOnInstruction(builder, inst, dt, f))
        {
          changed = restart = true;

          FAM.invalidate(f, PreservedAnalyses::none());

          auto *FN = CG.lookup(f);
          if(FN)
          {
            auto *FC = CG.lookupSCC(*FN);
            if(FC)
            {
              updateCGAndAnalysisManagerForCGSCCPass(
                CG, *FC, *FN, AM, UR, FAM);
            }
          }

          break;
        }

        ++iter;
      }

      if(restart)
        break;
    }
  } while(restart);

  return changed;
}

bool HeapToStack::runOnInstruction(IRBuilder<> &builder, Instruction *inst,
  DominatorTree &dt, Function &f)
{
  auto call_base = dyn_cast<CallBase>(inst);
  if(!call_base)
    return false;

  CallBase &call = *call_base;
  Function *fun = call.getCalledFunction();

  if(fun == NULL)
    return false;

  bool small = false;

  if(fun->getName().compare("pony_alloc") == 0)
  {
  }
  else if(fun->getName().compare("pony_alloc_small") == 0)
  {
    small = true;
  }
  else
  {
    return false;
  }

  Value *size = call.getArgOperand(1);
  ++NumHeapAlloc;
  ConstantInt *int_size = dyn_cast_or_null<ConstantInt>(size);

  if(int_size == NULL)
  {
    printTransform(inst, "variable size allocation");
    return false;
  }

  uint64_t alloc_size = int_size->getZExtValue();

  if(small)
  {
    int_size = ConstantInt::get(builder.getInt64Ty(),
      ((int64_t)1) << (alloc_size + HEAP_MINBITS));
  }
  else
  {
    if(alloc_size > 1024)
    {
      printTransform(inst, "large allocation");
      return false;
    }
  }

  SmallVector<CallInst*, 4> tail;
  SmallVector<Instruction*, 4> new_calls;

  if(!canStackAlloc(inst, dt, tail, new_calls))
    return false;

  for(auto iter = tail.begin(), end = tail.end(); iter != end; ++iter)
    (*iter)->setTailCall(false);

  BasicBlock::iterator begin = call.getCaller()->getEntryBlock().begin();

  const DataLayout &DL = inst->getModule()->getDataLayout();
  AllocaInst *replace = new AllocaInst(builder.getInt8Ty(), 0, int_size,
    inst->getPointerAlignment(DL), "", begin);

  replace->setDebugLoc(inst->getDebugLoc());

  inst->replaceAllUsesWith(replace);

  auto invoke = dyn_cast<InvokeInst>(static_cast<Instruction*>(&call));
  if(invoke)
  {
    BranchInst::Create(invoke->getNormalDest(), invoke->getIterator());
    invoke->getUnwindDest()->removePredecessor(call.getParent());
  }

  inst->eraseFromParent();

  for(auto new_call: new_calls)
  {
    InlineFunctionInfo ifi{};

    auto new_call_base = dyn_cast<CallBase>(new_call);
    if(new_call_base)
    {
      InlineFunction(*new_call_base, ifi);
    }
  }

  printTransform(replace, "stack allocation");
  --NumHeapAlloc;
  ++NumStackAlloc;

  return true;
}

bool HeapToStack::canStackAlloc(Instruction *alloc, DominatorTree &dt,
  SmallVector<CallInst*, 4> &tail, SmallVector<Instruction*, 4> &new_calls)
{
  SmallVector<Use*, 16> work;
  SmallSet<Use*, 16> visited;

  for(auto iter = alloc->use_begin(), end = alloc->use_end();
    iter != end; ++iter)
  {
    Use *use = &(*iter);
    visited.insert(use);
    work.push_back(use);
  }

  while(!work.empty())
  {
    Use *use = work.pop_back_val();
    Instruction *inst = cast<Instruction>(use->getUser());
    Value *value = use->get();

    switch(inst->getOpcode())
    {
      case Instruction::Call:
      case Instruction::Invoke:
      {
        auto ci = dyn_cast<CallInst>(inst);
        if(ci && ci->isTailCall())
        {
          tail.push_back(ci);
        }

        auto call_base = dyn_cast<CallBase>(inst);
        if(!call_base)
        {
          return false;
        }

        CallBase &call = *call_base;

        if(call.onlyReadsMemory())
        {
          Type *type = inst->getType();

          if(type->isVoidTy() ||
            type->isFPOrFPVectorTy() ||
            type->isIntOrIntVectorTy()
            )
          {
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
            if(!call.doesNotCapture((unsigned)(iter - first)))
            {
              printTransform(alloc, "captured allocation");
              printTransform(inst, "captured here (call arg)");
              return false;
            }
          }
        }
        break;
      }

      case Instruction::Load:
        break;

      case Instruction::Store:
      {
        if(value == inst->getOperand(0))
        {
          Value *dest = inst->getOperand(1);
          const DataLayout &dl =
            inst->getModule()->getDataLayout();

          APInt store_offset(64, 0);
          Value *store_base =
            dest->stripAndAccumulateConstantOffsets(dl, store_offset,
              true);

          if(!isa<AllocaInst>(store_base))
          {
            printTransform(alloc, "captured allocation");
            printTransform(inst, "captured here (store to non-stack)");
            return false;
          }

          AllocaInst *slot = cast<AllocaInst>(store_base);
          SmallVector<Value*, 8> slot_ptrs;
          SmallSet<Value*, 8> slot_seen;
          slot_ptrs.push_back(slot);
          slot_seen.insert(slot);

          while(!slot_ptrs.empty())
          {
            Value *ptr = slot_ptrs.pop_back_val();

            for(auto ui = ptr->user_begin(), ue = ptr->user_end();
              ui != ue; ++ui)
            {
              Instruction *user = dyn_cast<Instruction>(*ui);
              if(user == nullptr)
                continue;

              if(isa<GetElementPtrInst>(user) ||
                isa<BitCastInst>(user))
              {
                if(slot_seen.insert(user).second)
                  slot_ptrs.push_back(user);
              }
              else if(auto *li = dyn_cast<LoadInst>(user))
              {
                Value *load_addr = li->getPointerOperand();
                APInt load_offset(64, 0);
                Value *load_base =
                  load_addr->stripAndAccumulateConstantOffsets(
                    dl, load_offset, true);

                if(load_base == store_base &&
                  load_offset != store_offset)
                  continue;

                if(canBeReused(li, alloc, dt))
                  return false;

                for(auto lui = li->use_begin(), lue = li->use_end();
                  lui != lue; ++lui)
                {
                  Use *load_use = &(*lui);

                  if(visited.insert(load_use).second)
                    work.push_back(load_use);
                }
              }
              else if(auto *si = dyn_cast<StoreInst>(user))
              {
                if(si->getValueOperand() == ptr)
                {
                  printTransform(alloc, "captured allocation");
                  printTransform(inst,
                    "captured here (alloca address stored)");
                  return false;
                }
              }
              else
              {
                printTransform(alloc, "captured allocation");
                printTransform(inst,
                  "captured here (alloca use in call/unknown)");
                return false;
              }
            }
          }
        }
        break;
      }

      case Instruction::BitCast:
      case Instruction::GetElementPtr:
      case Instruction::PHI:
      case Instruction::Select:
      {
        if(canBeReused(inst, alloc, dt))
          return false;

        for(auto iter = inst->use_begin(), end = inst->use_end();
          iter != end; ++iter)
        {
          Use *use = &(*iter);

          if(visited.insert(use).second)
            work.push_back(use);
        }
        break;
      }

      default:
      {
        printTransform(alloc, "captured allocation");
        printTransform(inst, "captured here (unknown)");
        return false;
      }
    }
  }

  return true;
}

bool HeapToStack::canBeReused(Instruction *def, Instruction *alloc,
  DominatorTree &dt)
{
  if(def->use_empty() || !dt.dominates(def, alloc))
    return false;

  BasicBlock *def_block = def->getParent();
  BasicBlock *alloc_block = alloc->getParent();

  SmallSet<User*, 16> users;
  SmallSet<BasicBlock*, 16> user_blocks;

  for(auto iter = def->use_begin(), end = def->use_end();
    iter != end; ++iter)
  {
    Use *use = &(*iter);
    Instruction *user = cast<Instruction>(use->getUser());
    BasicBlock *user_block = user->getParent();

    if((alloc_block != user_block) && dt.dominates(alloc_block, user_block))
    {
      printTransform(alloc, "captured allocation");
      printTransform(def, "captured here (dominated reuse)");
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

  Instruction *start = alloc->getNextNode();
  work.push_back(Work(alloc_block, start));

  while(!work.empty())
  {
    Work w = work.pop_back_val();
    BasicBlock *bb = w.first;
    Instruction *inst = w.second;

    if(user_blocks.count(bb))
    {
      if((bb != def_block) && (bb != alloc_block))
      {
        printTransform(alloc, "captured allocation");
        printTransform(def,
          "captured here (block contains neither alloc nor def)");
        return true;
      }

      while(inst != nullptr)
      {
        if((inst == def) || (inst == alloc))
          break;

        if(users.count(inst))
        {
          printTransform(alloc, "captured allocation");
          printTransform(inst, "captured here (reused)");
          return true;
        }

        inst = inst->getNextNode();
      }
    }
    else if((bb == def_block) || ((bb == alloc_block) && (inst != start)))
    {
      continue;
    }

    Instruction *term = bb->getTerminator();
    unsigned count = term->getNumSuccessors();

    for(unsigned i = 0; i < count; i++)
    {
      BasicBlock *successor = term->getSuccessor(i);
      inst = &successor->front();
      bool found = false;

      while(isa<PHINode>(inst))
      {
        if(def == cast<PHINode>(inst)->getIncomingValueForBlock(bb))
        {
          printTransform(alloc, "captured allocation");
          printTransform(inst, "captured here (phi use)");
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

} // namespace pony
