#include "RulesMBA.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstVisitor.h"

using namespace llvm;

namespace {

struct MBAVisitor : public InstVisitor<MBAVisitor> {
    std::vector<Instruction *> toRemove;
    bool modified = false;

    template <typename MathGenerator>
    void applyMath(BinaryOperator &I, MathGenerator gen) {
        if (!I.getType()->isIntegerTy())
            return;

        IRBuilder<> builder(&I);
        Value *x = I.getOperand(0);
        Value *y = I.getOperand(1);

        Value *replacement = gen(builder, x, y);

        I.replaceAllUsesWith(replacement);
        toRemove.push_back(&I);
        modified = true;
    }

    /*
    x XOR y  =  (x OR y) - (x AND y)           -- classic
    x XOR y  =  (x | y) & ~(x & y)             -- bitwise
    x AND y  =  (x + y - (x XOR y)) / 2        -- arithmetic form
    x OR y   =  x + y - (x AND y)              -- inclusion-exclusion
    NOT x    =  -x - 1                          -- two's complement
    (x - y)  = a + (~b) + 1
    (x + y)  = (a XOR ~b) + 2*(a AND ~b) + 1
    */

    void visitAdd(BinaryOperator &I) {
        applyMath(I, [&](IRBuilder<> &b, Value *x, Value *y) {
            Value *xorVal = b.CreateXor(x, y);
            Value *andVal = b.CreateAnd(x, y);
            Value *two = ConstantInt::get(I.getType(), 2);
            Value *mulVal = b.CreateMul(two, andVal);
            return b.CreateAdd(xorVal, mulVal);
        });
    }

    void visitSub(BinaryOperator &I) {
        applyMath(I, [&](IRBuilder<> &b, Value *x, Value *y) {
            Value *notY = b.CreateNot(y);
            Value *add1 = b.CreateAdd(x, notY);
            Value *one = ConstantInt::get(I.getType(), 1);
            return b.CreateAdd(add1, one);
        });
    }

    void visitSRem(BinaryOperator &I) {
        applyMath(I, [&](IRBuilder<> &b, Value *x, Value *y) {
            Value *div = b.CreateSDiv(x, y);
            Value *negY = b.CreateNeg(y);
            Value *mul = b.CreateMul(div, negY);
            return b.CreateAdd(mul, x);
        });
    }

    void visitXor(BinaryOperator &I) {
        applyMath(I, [&](IRBuilder<> &b, Value *x, Value *y) {
            Value *orVal = b.CreateOr(x, y);
            Value *andVal = b.CreateAnd(x, y);
            return b.CreateSub(orVal, andVal);
        });
    }

    void visitAnd(BinaryOperator &I) {
        applyMath(I, [&](IRBuilder<> &b, Value *x, Value *y) {
            Value *xorVal = b.CreateXor(x, y);
            Value *addVal = b.CreateAdd(x, y);
            Value *subVal = b.CreateSub(addVal, xorVal);
            Value *two = ConstantInt::get(I.getType(), 2);
            return b.CreateSDiv(subVal, two);
        });
    }

    void visitOr(BinaryOperator &I) {
        applyMath(I, [&](IRBuilder<> &b, Value *x, Value *y) {
            Value *andVal = b.CreateAnd(x, y);
            Value *addVal = b.CreateAdd(x, y);
            return b.CreateSub(addVal, andVal);
        });
    }
};

} // anonymous namespace

PreservedAnalyses RulesMBAPass::run(Function &F, FunctionAnalysisManager &) {
    MBAVisitor visitor;
    visitor.visit(F);

    for (auto *I : visitor.toRemove) {
        I->eraseFromParent();
    }

    return visitor.modified ? PreservedAnalyses::none()
                            : PreservedAnalyses::all();
}
