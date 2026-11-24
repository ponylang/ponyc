#ifndef LIBPONYLTO_GC_CALLS_H
#define LIBPONYLTO_GC_CALLS_H

#include <llvm/IR/PassManager.h>

namespace llvm { class CallBase; }

namespace pony {

bool isGcRootBorrow(const llvm::CallBase& call);

class OptimizeGcCalls
  : public llvm::PassInfoMixin<OptimizeGcCalls>
{
public:
  explicit OptimizeGcCalls(bool before_inlining = false)
    : early(before_inlining) {}
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &);
private:
  bool early;
};

class LowerGcMaps : public llvm::PassInfoMixin<LowerGcMaps>
{
public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &);
};

} // namespace pony

#endif
