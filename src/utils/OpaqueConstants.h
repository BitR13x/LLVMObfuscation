#pragma once
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace obfutil {

/// Register a function as a global constructor in @llvm.global_ctors.
/// Preserves any existing constructor entries.
inline void registerCtor(llvm::Module &M, llvm::Function *F,
                          int priority = 65535) {
    using namespace llvm;
    LLVMContext &C = M.getContext();
    Type *i32Ty = Type::getInt32Ty(C);
    auto *ptrTy = PointerType::getUnqual(C);
    StructType *sTy = StructType::get(i32Ty, ptrTy, ptrTy);

    Constant *entry = ConstantStruct::get(
        sTy, ConstantInt::get(i32Ty, priority), F,
        ConstantPointerNull::get(ptrTy));

    SmallVector<Constant *, 8> entries;
    if (auto *g = M.getNamedGlobal("llvm.global_ctors")) {
        if (auto *arr = dyn_cast<ConstantArray>(g->getInitializer()))
            for (unsigned i = 0; i < arr->getNumOperands(); i++)
                entries.push_back(arr->getOperand(i));
        g->eraseFromParent();
    }
    entries.push_back(entry);

    auto *arrTy = ArrayType::get(sTy, entries.size());
    new GlobalVariable(M, arrTy, false, GlobalValue::AppendingLinkage,
                       ConstantArray::get(arrTy, entries),
                       "llvm.global_ctors");
}

/// Create a BSS global array of `n` i64 elements (zeroinitializer).
/// The global has internal linkage so it won't collide across TUs.
inline llvm::GlobalVariable *createBSSGlobal(llvm::Module &M, size_t n,
                                              const std::string &name) {
    using namespace llvm;
    auto *i64Ty = Type::getInt64Ty(M.getContext());
    auto *arrTy = ArrayType::get(i64Ty, n);
    return new GlobalVariable(M, arrTy, /*isConstant=*/false,
                              GlobalValue::InternalLinkage,
                              ConstantAggregateZero::get(arrTy), name);
}

/// Create a constructor function that fills `global` with the real
/// coefficient values, derived through an LCG chain + XOR so the
/// actual constants never appear as plain immediates in the binary.
///
/// At startup the constructor runs:
///   seed = SEED
///   for each i:
///       seed = seed * LCG_MUL + LCG_ADD
///       global[i] = seed ^ ENCODED[i]
///
/// where ENCODED[i] = values[i] ^ lcg_state[i], pre-computed at
/// compile time. The decompiler would need to emulate the full LCG
/// chain to recover the real values.
inline void createInitFunc(llvm::Module &M, llvm::GlobalVariable *global,
                            const std::vector<uint64_t> &values,
                            const std::string &suffix) {
    using namespace llvm;
    LLVMContext &ctx = M.getContext();
    auto *i64Ty = Type::getInt64Ty(ctx);
    auto *i32Ty = Type::getInt32Ty(ctx);

    auto *fn = Function::Create(FunctionType::get(Type::getVoidTy(ctx), false),
                                GlobalValue::InternalLinkage,
                                ".mba.init." + suffix, M);

    auto *bb = BasicBlock::Create(ctx, "e", fn);
    IRBuilder<> b(bb);

    // LCG parameters (Knuth's constants)
    uint64_t seed = ((uint64_t)rand() << 33) ^ ((uint64_t)rand() << 17) ^
                    (uint64_t)rand();
    if (seed == 0) seed = 0xDEADBEEFCAFEBABEULL;
    constexpr uint64_t lcgMul = 6364136223846793005ULL;
    constexpr uint64_t lcgAdd = 1442695040888963407ULL;

    // Pre-compute encoded values: encoded[i] = value[i] ^ lcg_state[i]
    std::vector<uint64_t> encoded(values.size());
    uint64_t s = seed;
    for (size_t i = 0; i < values.size(); i++) {
        s = s * lcgMul + lcgAdd;
        encoded[i] = values[i] ^ s;
    }

    // Emit IR: LCG chain in registers → XOR with encoded → store
    auto *arrTy = cast<ArrayType>(global->getValueType());
    Value *sv = ConstantInt::get(i64Ty, seed);
    Value *mul = ConstantInt::get(i64Ty, lcgMul);
    Value *add = ConstantInt::get(i64Ty, lcgAdd);

    for (size_t i = 0; i < values.size(); i++) {
        sv = b.CreateAdd(b.CreateMul(sv, mul, "l.m"), add, "l.s");
        Value *val =
            b.CreateXor(sv, ConstantInt::get(i64Ty, encoded[i]), "d");
        Value *ptr = b.CreateInBoundsGEP(
            arrTy, global,
            {ConstantInt::get(i32Ty, 0), ConstantInt::get(i32Ty, i)}, "p");
        b.CreateStore(val, ptr);
    }

    b.CreateRetVoid();
    registerCtor(M, fn);
}

} // namespace obfutil
