// Linear MBA Obfuscation Pass (Module Pass)
//
// Replaces integer Add/Sub with mathematically equivalent MBA (Mixed
// Boolean-Arithmetic) expressions that resist decompiler simplification.
//
// Uses reusable utilities from utils/ for matrix math, opaque constants,
// and weighted random selection.

#include "LinearMBA.h"
#include "utils/MBAMatrix.h"
#include "utils/OpaqueConstants.h"
#include "utils/WeightedRandom.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

using namespace llvm;
using obfutil::Matrix;

namespace {

static constexpr int DIM = 5;
static constexpr int COEFFS_PER_INST = DIM * DIM + DIM; // 30

// =================================================================
// SIGNAL RULES — MBA identities for Add/Sub
// =================================================================
// Each rule defines a pair of bitwise expressions whose weighted sum
// equals the original arithmetic operation.

enum SignalRule { SR_XOR_AND, SR_OR_AND, SR_2OR_XOR, SR_DEMORGAN, SR_COUNT };
static constexpr int kSignalWeights[SR_COUNT] = {35, 30, 20, 15};

static std::vector<uint64_t> signalTruth(SignalRule rule, bool isAdd) {
    if (isAdd) {
        switch (rule) {
            case SR_XOR_AND:  return {1, 2};              // (a^b) + 2*(a&b)
            case SR_OR_AND:   return {1, 1};              // (a|b) + (a&b)
            case SR_2OR_XOR:  return {2, (uint64_t)-1};   // 2*(a|b) - (a^b)
            case SR_DEMORGAN: return {1, 2};              // DeMorgan(a^b) + 2*(a&b)
            default:          return {1, 2};
        }
    } else {
        switch (rule) {
            case SR_XOR_AND:  return {1, (uint64_t)-2};   // (a^b) - 2*(~a&b)
            case SR_OR_AND:   return {1, (uint64_t)-1};   // (a&~b) - (~a&b)
            case SR_2OR_XOR:  return {2, (uint64_t)-1};   // 2*(a&~b) - (a^b)
            case SR_DEMORGAN: return {1, (uint64_t)-2};   // DeMorgan(a^b) - 2*(~a&b)
            default:          return {1, (uint64_t)-2};
        }
    }
}

static std::vector<Value *> emitSignal(IRBuilder<> &b, SignalRule rule,
                                        bool isAdd, Value *x, Value *y) {
    if (isAdd) {
        switch (rule) {
            case SR_XOR_AND:
                return {b.CreateXor(x, y, "s.xor"),
                        b.CreateAnd(x, y, "s.and")};
            case SR_OR_AND:
                return {b.CreateOr(x, y, "s.or"),
                        b.CreateAnd(x, y, "s.and")};
            case SR_2OR_XOR:
                return {b.CreateOr(x, y, "s.or"),
                        b.CreateXor(x, y, "s.xor")};
            case SR_DEMORGAN: {
                Value *o = b.CreateOr(x, y, "s.or");
                Value *n = b.CreateOr(b.CreateNot(x, "nx"),
                                      b.CreateNot(y, "ny"), "s.on");
                return {b.CreateAnd(o, n, "s.dm"),
                        b.CreateAnd(x, y, "s.and")};
            }
            default:
                return {b.CreateXor(x, y), b.CreateAnd(x, y)};
        }
    } else {
        switch (rule) {
            case SR_XOR_AND: {
                Value *nx = b.CreateNot(x, "nx");
                return {b.CreateXor(x, y, "s.xor"),
                        b.CreateAnd(nx, y, "s.na")};
            }
            case SR_OR_AND: {
                Value *nx = b.CreateNot(x, "nx");
                Value *ny = b.CreateNot(y, "ny");
                return {b.CreateAnd(x, ny, "s.an"),
                        b.CreateAnd(nx, y, "s.na")};
            }
            case SR_2OR_XOR: {
                Value *ny = b.CreateNot(y, "ny");
                return {b.CreateAnd(x, ny, "s.an"),
                        b.CreateXor(x, y, "s.xor")};
            }
            case SR_DEMORGAN: {
                Value *nx = b.CreateNot(x, "nx");
                Value *ny = b.CreateNot(y, "ny");
                Value *o = b.CreateOr(x, y, "s.or");
                Value *n = b.CreateOr(nx, ny, "s.on");
                return {b.CreateAnd(o, n, "s.dm"),
                        b.CreateAnd(nx, y, "s.na")};
            }
            default: {
                Value *nx = b.CreateNot(x, "nx");
                return {b.CreateXor(x, y), b.CreateAnd(nx, y)};
            }
        }
    }
}

// =================================================================
// JUNK STRATEGIES — noise terms that cancel to zero
// =================================================================
// Each strategy produces 3 IR values + 3 truth coefficients whose
// weighted sum is identically zero for all inputs.

enum JunkStrat { JS_SELF_CANCEL, JS_TRIPLE, JS_XOR_AND_OR, JS_IDENTITY,
                 JS_COUNT };
static constexpr int kJunkWeights[JS_COUNT] = {30, 25, 25, 20};

static std::vector<uint64_t> junkTruthVec(JunkStrat strat) {
    uint64_t k = (uint64_t)rand() | 1ULL;
    uint64_t m = (uint64_t)rand() | 1ULL;
    switch (strat) {
        case JS_SELF_CANCEL:
            return {k, (uint64_t)-k, 0};
        case JS_TRIPLE:
            return {k, m, (uint64_t)(-(int64_t)(k + m))};
        case JS_XOR_AND_OR:
            // (a^b) + (a&b) - (a|b) = 0  (disjoint-bit identity)
            return {k, k, (uint64_t)-k};
        case JS_IDENTITY:
            return {k, m, (uint64_t)(-(int64_t)(k + m))};
        default:
            return {k, (uint64_t)-k, 0};
    }
}

static std::vector<Value *> emitJunk(IRBuilder<> &b, JunkStrat strat,
                                      Value *rdtscV, Type *ty) {
    switch (strat) {
        case JS_SELF_CANCEL: {
            // j1, j1^0 (=j1), j1^CONST — first two cancel, third has T=0
            Value *j1 = rdtscV;
            Value *j2 = b.CreateXor(j1, ConstantInt::get(ty, 0xDEADBEEFULL),
                                    "j2");
            Value *j3 = b.CreateXor(j1, ConstantInt::get(ty, 0), "j3");
            return {j1, j3, j2};
        }
        case JS_TRIPLE: {
            // j, j|j (=j), j&j (=j) — all equal, coefficients sum to 0
            Value *j = rdtscV;
            return {j, b.CreateOr(j, j, "j2"),
                    b.CreateAnd(j, j, "j3")};
        }
        case JS_XOR_AND_OR: {
            // (j1^j2) + (j1&j2) = (j1|j2) — MBA identity on junk
            Value *j1 = rdtscV;
            Value *j2 = b.CreateXor(j1,
                                    ConstantInt::get(ty, 0xCAFEBABEULL), "j2");
            return {b.CreateXor(j1, j2, "jx"),
                    b.CreateAnd(j1, j2, "ja"),
                    b.CreateOr(j1, j2, "jo")};
        }
        case JS_IDENTITY: {
            // j, j+0 (=j), j^0 (=j) — all equal, coefficients sum to 0
            Value *j = rdtscV;
            return {j, b.CreateAdd(j, ConstantInt::get(ty, 0), "j2"),
                    b.CreateXor(j, ConstantInt::get(ty, 0), "j3")};
        }
        default:
            return {rdtscV, rdtscV, rdtscV};
    }
}

// =================================================================
// PER-INSTRUCTION CONFIG
// =================================================================

struct InstInfo {
    Instruction *I;
    Matrix A;
    std::vector<uint64_t> C;
    SignalRule sigRule;
    JunkStrat junkStrat;
    bool isAdd;
};

// =================================================================
// MBA IR EMISSION
// =================================================================

static void emitMBA(InstInfo &info, GlobalVariable *global, int offset) {
    Instruction *I = info.I;
    IRBuilder<> b(I);
    Value *x = I->getOperand(0), *y = I->getOperand(1);
    Type *ty = I->getType();
    Type *i64Ty = Type::getInt64Ty(b.getContext());
    Type *i32Ty = Type::getInt32Ty(b.getContext());
    auto *arrTy = cast<ArrayType>(global->getValueType());

    // Helper: load one opaque coefficient from the global array
    auto loadC = [&](int idx) -> Value * {
        Value *ptr = b.CreateInBoundsGEP(
            arrTy, global,
            {ConstantInt::get(i32Ty, 0), ConstantInt::get(i32Ty, idx)},
            "cp");
        Value *v = b.CreateLoad(i64Ty, ptr, "cl");
        return (ty != i64Ty) ? b.CreateTrunc(v, ty, "ct") : v;
    };

    // Emit rdtsc for junk entropy
    Function *rdtscFn = Intrinsic::getOrInsertDeclaration(
        I->getModule(), Intrinsic::readcyclecounter);
    Value *rdtscV =
        b.CreateTruncOrBitCast(b.CreateCall(rdtscFn), ty, "ent");

    // Build the 5-element base vector V
    auto sig = emitSignal(b, info.sigRule, info.isAdd, x, y);
    auto jnk = emitJunk(b, info.junkStrat, rdtscV, ty);
    Value *V[DIM] = {sig[0], sig[1], jnk[0], jnk[1], jnk[2]};

    // E = V × A  (all 25 terms — no skipping so decompiler can't
    //             infer which matrix entries are zero)
    Value *E[DIM];
    for (int col = 0; col < DIM; col++) {
        Value *sum = ConstantInt::get(ty, 0);
        for (int row = 0; row < DIM; row++) {
            Value *coeff = loadC(offset + row * DIM + col);
            sum = b.CreateAdd(
                sum, b.CreateMul(V[row], coeff, "Et"), "Es");
        }
        E[col] = sum;
    }

    // result = E · C  (dot product — all 5 terms)
    Value *result = ConstantInt::get(ty, 0);
    for (int i = 0; i < DIM; i++) {
        Value *coeff = loadC(offset + DIM * DIM + i);
        result = b.CreateAdd(
            result, b.CreateMul(E[i], coeff, "Ft"), "Fs");
    }

    I->replaceAllUsesWith(result);
}

// =================================================================
// PER-FUNCTION PROCESSING
// =================================================================

static bool processFunction(Function &F) {
    // Seed rand() per-function so every function + compilation gets
    // a different matrix / rule selection.
    std::size_t nameHash = std::hash<std::string>{}(F.getName().str());
    auto now = std::chrono::high_resolution_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    srand((unsigned)(nanos ^ nameHash));

    // Phase 1: Collect all Add/Sub instructions and compute configs
    std::vector<InstInfo> infos;
    for (auto &BB : F) {
        for (auto &I : BB) {
            if (!I.getType()->isIntegerTy()) continue;
            unsigned opc = I.getOpcode();
            if (opc != Instruction::Add && opc != Instruction::Sub) continue;

            InstInfo info;
            info.I = &I;
            info.isAdd = (opc == Instruction::Add);
            info.sigRule = (SignalRule)obfutil::weightedPick(
                kSignalWeights, SR_COUNT);
            info.junkStrat = (JunkStrat)obfutil::weightedPick(
                kJunkWeights, JS_COUNT);

            // Assemble truth vector T = [signal..., junk...]
            auto sigT = signalTruth(info.sigRule, info.isAdd);
            auto jnkT = junkTruthVec(info.junkStrat);
            std::vector<uint64_t> T = {sigT[0], sigT[1],
                                       jnkT[0], jnkT[1], jnkT[2]};

            // Generate matrix pair + constants C = A^-1 * T
            Matrix A_inv;
            obfutil::createMatrixPair(info.A, A_inv, DIM);
            info.C = obfutil::computeConstants(A_inv, T);

            infos.push_back(std::move(info));
        }
    }

    if (infos.empty()) return false;

    // Phase 2: Create opaque global + constructor
    std::vector<uint64_t> allValues;
    for (auto &info : infos) {
        for (int r = 0; r < DIM; r++)
            for (int c = 0; c < DIM; c++)
                allValues.push_back(info.A[r][c]);
        for (int i = 0; i < DIM; i++)
            allValues.push_back(info.C[i]);
    }

    Module &M = *F.getParent();
    std::string safeName = F.getName().str();
    for (char &ch : safeName)
        if (!isalnum(ch) && ch != '_') ch = '_';

    auto *global = obfutil::createBSSGlobal(
        M, allValues.size(), ".mba.d." + safeName);
    obfutil::createInitFunc(M, global, allValues, safeName);

    // Phase 3: Emit MBA IR for each instruction
    for (size_t i = 0; i < infos.size(); i++)
        emitMBA(infos[i], global, i * COEFFS_PER_INST);

    // Phase 4: Remove original instructions
    for (auto &info : infos)
        info.I->eraseFromParent();

    return true;
}

} // end anonymous namespace

// =================================================================
// MODULE PASS ENTRY POINT
// =================================================================

PreservedAnalyses LinearMBAPass::run(Module &M, ModuleAnalysisManager &) {
    bool Modified = false;
    for (Function &F : M) {
        if (F.isDeclaration()) continue;
        Modified |= processFunction(F);
    }
    return Modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
