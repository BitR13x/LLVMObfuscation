#include "Flattening.h"
#include "LinearMBA.h"
#include "RulesMBA.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "GeneralObfuscationPass",
            LLVM_VERSION_STRING, [](PassBuilder &PB) {
                // Function-level passes
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, FunctionPassManager &FPM,
                       ArrayRef<PassBuilder::PipelineElement>) {
                        if (Name == "mba") {
                            FPM.addPass(RulesMBAPass());
                            return true;
                        }
                        if (Name == "cff") {
                            FPM.addPass(FlatteningPass());
                            return true;
                        }
                        return false;
                    });

                // Module-level passes
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, ModulePassManager &MPM,
                       ArrayRef<PassBuilder::PipelineElement>) {
                        if (Name == "mba-linear") {
                            MPM.addPass(LinearMBAPass());
                            return true;
                        }
                        return false;
                    });
            }};
}
