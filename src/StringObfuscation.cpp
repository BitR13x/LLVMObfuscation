// String Obfuscation Pass
//
// Converts all static constant strings in the module into zero-initialized 
// .bss buffers. A global constructor is injected to decode the strings at 
// runtime. The decoding packs up to 8 characters (bytes) into a 64-bit 
// integer and computes the value via A + B, A - B, or A * B, completely 
// hiding the string from static analysis.

#include "StringObfuscation.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include <chrono>
#include <cstdlib>
#include <string>
#include <vector>

using namespace llvm;

namespace {

// Generate a random 64-bit integer
static uint64_t rand64() {
    uint64_t r = 0;
    for (int i = 0; i < 4; ++i) {
        r = (r << 16) | (rand() & 0xFFFF);
    }
    return r;
}

// Modular multiplicative inverse modulo 2^64 (Newton's method)
// 'a' must be odd
static uint64_t modInverse(uint64_t a) {
    uint64_t x = a;         // 3 bits
    x *= 2 - a * x;         // 6 bits
    x *= 2 - a * x;         // 12 bits
    x *= 2 - a * x;         // 24 bits
    x *= 2 - a * x;         // 48 bits
    x *= 2 - a * x;         // 96 bits (accurate for 64)
    return x;
}

static bool obfuscateStrings(Module &M) {
    LLVMContext &Ctx = M.getContext();
    Type *Int64Ty = Type::getInt64Ty(Ctx);
    Type *Int8Ty = Type::getInt8Ty(Ctx);

    std::vector<GlobalVariable *> stringsToObf;

    // 1. Identify all string literals
    for (GlobalVariable &GV : M.globals()) {
        if (!GV.isConstant() || !GV.hasInitializer()) continue;
        if (GV.getName().starts_with("llvm.")) continue; // skip metadata

        auto *CDS = dyn_cast<ConstantDataSequential>(GV.getInitializer());
        if (!CDS || !CDS->isString()) continue;

        stringsToObf.push_back(&GV);
    }

    if (stringsToObf.empty()) return false;

    // 2. Prepare the Global Constructor
    FunctionType *InitFuncTy = FunctionType::get(Type::getVoidTy(Ctx), false);
    Function *InitFunc = Function::Create(InitFuncTy, GlobalValue::PrivateLinkage, ".str.obf.init", &M);
    BasicBlock *InitBB = BasicBlock::Create(Ctx, "entry", InitFunc);
    IRBuilder<> B(InitBB);

    // 3. Process each string
    for (GlobalVariable *GV : stringsToObf) {
        auto *CDS = cast<ConstantDataSequential>(GV->getInitializer());
        StringRef raw = CDS->getRawDataValues();

        // Calculate padded length (multiple of 8)
        size_t padLen = (raw.size() + 7) & ~7;

        // Create the BSS buffer
        ArrayType *NewTy = ArrayType::get(Int8Ty, padLen);
        GlobalVariable *NewGV = new GlobalVariable(
            M, NewTy, false, GlobalValue::PrivateLinkage,
            ConstantAggregateZero::get(NewTy), GV->getName() + ".obf");
        NewGV->setAlignment(Align(8));
        
        // Replace uses of the original string with the new buffer.
        // Thanks to LLVM opaque pointers, ptr types match exactly.
        GV->replaceAllUsesWith(NewGV);

        // Build the runtime decoding logic
        for (size_t i = 0; i < padLen; i += 8) {
            // Pack 8 bytes into a 64-bit integer (little endian)
            uint64_t V = 0;
            for (size_t j = 0; j < 8; ++j) {
                if (i + j < raw.size()) {
                    V |= ((uint64_t)(unsigned char)raw[i + j]) << (j * 8);
                }
            }

            // Decide on an arithmetic split (0 = Add, 1 = Sub, 2 = Mul)
            int op = rand() % 3;
            uint64_t valA, valB;
            Value *Res = nullptr;

            if (op == 0) {
                valA = rand64();
                valB = V - valA;
                Res = B.CreateAdd(ConstantInt::get(Int64Ty, valA), 
                                  ConstantInt::get(Int64Ty, valB));
            } else if (op == 1) {
                valA = rand64();
                valB = valA - V;
                Res = B.CreateSub(ConstantInt::get(Int64Ty, valA), 
                                  ConstantInt::get(Int64Ty, valB));
            } else {
                valB = rand64() | 1; // Force odd
                valA = V * modInverse(valB);
                Res = B.CreateMul(ConstantInt::get(Int64Ty, valA), 
                                  ConstantInt::get(Int64Ty, valB));
            }

            // Store the 64-bit chunk directly into the buffer at offset i
            Value *Gep = B.CreateConstGEP1_32(Int8Ty, NewGV, i);
            B.CreateStore(Res, Gep);
        }

        // Erase original string
        GV->eraseFromParent();
    }

    B.CreateRetVoid();

    // 4. Register the global constructor with high priority (0)
    appendToGlobalCtors(M, InitFunc, 0);

    return true;
}

} // end anonymous namespace

PreservedAnalyses StringObfuscationPass::run(Module &M, ModuleAnalysisManager &) {
    auto now = std::chrono::high_resolution_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    srand((unsigned)nanos);

    if (obfuscateStrings(M))
        return PreservedAnalyses::none();
    return PreservedAnalyses::all();
}
