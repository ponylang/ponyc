#include "gc_calls.h"
#include "heap_to_stack.h"
#include "noalloc_check.h"

#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Plugins/PassPlugin.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/ADT/SmallVector.h>

using namespace llvm;

namespace pony {

// After HeapToStack has run, strip noinline from pony_alloc, pony_alloc_small
// and pony_alloc_small_shared_start and inline the remaining call sites. These
// functions carry noinline (actor.c) so HeapToStack can match them by name at
// call sites.
class InlineRemainingAllocPass
  : public PassInfoMixin<InlineRemainingAllocPass>
{
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &)
  {
    SmallVector<Function*, 3> targets;

    for(auto *name :
      {"pony_alloc", "pony_alloc_small", "pony_alloc_small_shared_start"})
    {
      if(Function *F = M.getFunction(name))
        targets.push_back(F);
    }

    if(targets.empty())
      return PreservedAnalyses::all();

    for(Function *F : targets)
    {
      F->removeFnAttr(Attribute::NoInline);
      F->addFnAttr(Attribute::AlwaysInline);
    }

    bool changed = false;

    for(Function *F : targets)
    {
      SmallVector<CallBase*, 8> calls;

      for(User *U : F->users())
      {
        if(auto *CB = dyn_cast<CallBase>(U))
        {
          if(CB->getCalledFunction() == F)
            calls.push_back(CB);
        }
      }

      for(CallBase *CB : calls)
      {
        InlineFunctionInfo IFI;
        InlineResult IR = InlineFunction(*CB, IFI);
        if(IR.isSuccess())
          changed = true;
      }
    }

    return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
};

} // namespace pony

::llvm::PassPluginLibraryInfo getPonyLTOPluginInfo()
{
  return {
    LLVM_PLUGIN_API_VERSION,
    "PonyLTO",
    LLVM_VERSION_STRING,
    [](PassBuilder &PB)
    {
      PB.registerFullLinkTimeOptimizationEarlyEPCallback(
        [](ModulePassManager &PM, OptimizationLevel Level)
        {
          if(Level != OptimizationLevel::O0)
            PM.addPass(pony::OptimizeGcCalls(true));
        });
      PB.registerPipelineEarlySimplificationEPCallback(
        [](ModulePassManager &PM, OptimizationLevel Level,
          ThinOrFullLTOPhase phase)
        {
          if(Level != OptimizationLevel::O0 &&
            phase == ThinOrFullLTOPhase::ThinLTOPostLink)
            PM.addPass(pony::OptimizeGcCalls(true));
        });

      PB.registerCGSCCOptimizerLateEPCallback(
        [](CGSCCPassManager &PM, OptimizationLevel Level)
        {
          if(Level != OptimizationLevel::O0)
            PM.addPass(pony::HeapToStack());
        });

      PB.registerFullLinkTimeOptimizationLastEPCallback(
        [](ModulePassManager &PM, OptimizationLevel Level)
        {
          if(Level != OptimizationLevel::O0)
          {
            PM.addPass(pony::OptimizeGcCalls());
            PM.addPass(pony::LowerGcMaps());
          }
        });

      PB.registerOptimizerLastEPCallback(
        [](ModulePassManager &PM, OptimizationLevel Level,
           ThinOrFullLTOPhase phase)
        {
          if(Level != OptimizationLevel::O0)
          {
            PM.addPass(pony::OptimizeGcCalls());
            PM.addPass(pony::NoAllocCheck());
            PM.addPass(pony::InlineRemainingAllocPass());
            if(phase == ThinOrFullLTOPhase::ThinLTOPostLink)
              PM.addPass(pony::LowerGcMaps());
          }
        });
    }
  };
}
