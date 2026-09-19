#ifndef LIBPONYLTO_HEAP_TO_STACK_H
#define LIBPONYLTO_HEAP_TO_STACK_H

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LazyCallGraph.h>

namespace pony {

class HeapToStack : public llvm::PassInfoMixin<HeapToStack>
{
public:
  llvm::PreservedAnalyses run(llvm::LazyCallGraph::SCC &C,
    llvm::CGSCCAnalysisManager &AM, llvm::LazyCallGraph &CG,
    llvm::CGSCCUpdateResult &UR);

private:
  bool runOnFunction(llvm::Function &f, llvm::FunctionAnalysisManager &FAM,
    llvm::LazyCallGraph &CG, llvm::CGSCCAnalysisManager &AM,
    llvm::CGSCCUpdateResult &UR);

  bool runOnInstruction(llvm::IRBuilder<> &builder, llvm::Instruction *inst,
    llvm::DominatorTree &dt, llvm::Function &f);

  bool canStackAlloc(llvm::Instruction *alloc, llvm::DominatorTree &dt,
    llvm::SmallVector<llvm::CallInst*, 4> &tail,
    llvm::SmallVector<llvm::Instruction*, 4> &new_calls);

  bool canBeReused(llvm::Instruction *def, llvm::Instruction *alloc,
    llvm::DominatorTree &dt);
};

} // namespace pony

#endif
