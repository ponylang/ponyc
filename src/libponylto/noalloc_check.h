#ifndef LIBPONYLTO_NOALLOC_CHECK_H
#define LIBPONYLTO_NOALLOC_CHECK_H

#include <llvm/IR/PassManager.h>

namespace pony {

class NoAllocCheck : public llvm::PassInfoMixin<NoAllocCheck>
{
public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &);
};

} // namespace pony

#endif
