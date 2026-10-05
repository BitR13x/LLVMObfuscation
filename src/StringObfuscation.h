#pragma once
#include "llvm/IR/PassManager.h"

namespace llvm {
struct StringObfuscationPass : public PassInfoMixin<StringObfuscationPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &);
};
} // namespace llvm
