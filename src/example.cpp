#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {
  struct MBAObfuscation : public PassInfoMixin<MBAObfuscation> {
    [[nodiscard]] PreservedAnalyses run(Function &F, FunctionAnalysisManager &) const {
      bool Modified = false;
      std::vector<Instruction*> InstsToRemove;

      for (auto &BB : F) {
        for (auto &I : BB) {
          if (isObfuscationTarget(&I)) {
            applyMBASubstitution(&I);
            InstsToRemove.push_back(&I);
            Modified = true;
          }
        }
      }

      for (auto *I : InstsToRemove) {
        I->eraseFromParent();
      }

      return Modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }

  private:
    bool isObfuscationTarget(Instruction *Inst) const {
      if (Inst->getOpcode() == Instruction::Add) {
        return Inst->getType()->isIntegerTy();
      }
      return false;
    }

    void applyMBASubstitution(Instruction *Inst) const {
      Value *Op0 = Inst->getOperand(0);
      Value *Op1 = Inst->getOperand(1);

      IRBuilder<> Builder(Inst);

      Value *Xor = Builder.CreateXor(Op0, Op1, "mba.xor");
      Value *And = Builder.CreateAnd(Op0, Op1, "mba.and");
      Value *Two = ConstantInt::get(Inst->getType(), 2);
      Value *Mul = Builder.CreateMul(Two, And, "mba.mul");
      Value *NewAdd = Builder.CreateAdd(Xor, Mul, "mba.add");

      Inst->replaceAllUsesWith(NewAdd);
    }
  };

  bool applyLinearMBA(Instruction *I) {
      /* Small example how to generate a linear MBA expression for an Add instruction from matrix */

      IRBuilder<> builder(I);
      Value *x = I->getOperand(0);
      Value *y = I->getOperand(1);
      Type *ty = I->getType();

      Value *c2 = ConstantInt::get(ty, 2);
      Value *c3 = ConstantInt::get(ty, 3);

      // Step 1: V' = A * [x, y]
      // v1 = 2*x + 3*y
      Value *x2 = builder.CreateMul(c2, x, "mba.x2");
      Value *y3 = builder.CreateMul(c3, y, "mba.y3");
      Value *v1 = builder.CreateAdd(x2, y3, "mba.v1");

      // v2 = x + 2*y
      Value *y2 = builder.CreateMul(c2, y, "mba.y2");
      Value *v2 = builder.CreateAdd(x, y2, "mba.v2");

      // Step 2: [x, y] = A^-1 * V'
      // We want to recover (x + y).
      // Mathematically: v1 - v2 = (2x + 3y) - (x + 2y) = x + y.
      Value *newAdd = builder.CreateSub(v1, v2, "mba.matrix_add");

      I->replaceAllUsesWith(newAdd);
      return true;
  }
} // end anonymous namespace

// Modern Plugin Registration
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "MBAObfuscation", LLVM_VERSION_STRING,
        [](PassBuilder& PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager& FPM, ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "obfuscate") {
                        FPM.addPass(MBAObfuscation());
                        return true;
                    }
                    return false;
                });
        }};
}
