#pragma once
#include "llvm/IR/PassManager.h"

namespace llvm {
struct RulesMBAPass : public PassInfoMixin<RulesMBAPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &);
};
} // namespace llvm
