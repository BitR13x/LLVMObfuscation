#pragma once
#include "llvm/IR/PassManager.h"

namespace llvm {
struct FlatteningPass : public PassInfoMixin<FlatteningPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &);
};
} // namespace llvm
