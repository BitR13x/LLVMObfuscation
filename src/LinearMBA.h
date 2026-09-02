#pragma once
#include "llvm/IR/PassManager.h"

namespace llvm {
struct LinearMBAPass : public PassInfoMixin<LinearMBAPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &);
};
} // namespace llvm
