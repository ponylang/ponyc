#include "heap_to_stack.h"

#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Plugins/PassPlugin.h>

using namespace llvm;

::llvm::PassPluginLibraryInfo getPonyLTOPluginInfo()
{
  return {
    LLVM_PLUGIN_API_VERSION,
    "PonyLTO",
    LLVM_VERSION_STRING,
    [](PassBuilder &PB)
    {
      PB.registerCGSCCOptimizerLateEPCallback(
        [](CGSCCPassManager &PM, OptimizationLevel Level)
        {
          if(Level != OptimizationLevel::O0)
            PM.addPass(pony::HeapToStack());
        });
    }
  };
}
