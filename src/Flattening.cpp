// Control Flow Flattening Pass
//
// Transforms a function's natural control flow (if/else, loops, etc.)
// into a single dispatcher loop with a switch statement. All original
// basic blocks become cases of the switch, and transitions between
// blocks are controlled by updating a state variable.
//
// Before:  entry → BB1 → BB3 → return
//            ↘ BB2 ↗
//
// After:   entry → while(1) { switch(state) {
//              case R1: BB1_code; state = R3; break;
//              case R2: BB2_code; state = R3; break;
//              case R3: return;
//          }}
//
// State IDs are randomized so the case order doesn't leak the original
// control flow structure.

#include "Flattening.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace llvm;

namespace {

// =================================================================
// PHI NODE AND REGISTER DEMOTION
// =================================================================
// After flattening, sibling switch cases don't dominate each other.
// Values defined in one case can't be directly used in another.
// We solve this by demoting cross-block values to stack (alloca).

/// Replace all PHI nodes with alloca + store/load.
/// PHI nodes reference predecessor blocks, which change after flattening.
static void demotePhiNodes(Function &F) {
    std::vector<PHINode *> phis;
    for (auto &BB : F)
        for (auto &I : BB)
            if (auto *phi = dyn_cast<PHINode>(&I))
                phis.push_back(phi);

    if (phis.empty()) return;

    BasicBlock &entry = F.getEntryBlock();

    for (PHINode *phi : phis) {
        // Alloca at top of entry block
        IRBuilder<> allocaB(&*entry.begin());
        AllocaInst *slot = allocaB.CreateAlloca(
            phi->getType(), nullptr, phi->getName() + ".dm");

        // Store each incoming value at end of its predecessor
        for (unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
            IRBuilder<> b(phi->getIncomingBlock(i)->getTerminator());
            b.CreateStore(phi->getIncomingValue(i), slot);
        }

        // Replace PHI with load
        IRBuilder<> b(&*phi->getIterator());
        Value *load = b.CreateLoad(phi->getType(), slot,
                                    phi->getName() + ".ld");
        phi->replaceAllUsesWith(load);
        phi->eraseFromParent();
    }
}

/// Demote instructions whose results are used in a different block.
/// Creates alloca + store after definition + load before each cross-block use.
static void demoteRegisters(Function &F) {
    BasicBlock &entry = F.getEntryBlock();

    // Collect instructions with cross-block uses
    std::vector<Instruction *> toDemote;
    for (auto &BB : F) {
        for (auto &I : BB) {
            if (I.isTerminator() || isa<AllocaInst>(&I) ||
                I.getType()->isVoidTy())
                continue;
            for (auto *U : I.users()) {
                if (auto *UI = dyn_cast<Instruction>(U)) {
                    if (UI->getParent() != I.getParent()) {
                        toDemote.push_back(&I);
                        break;
                    }
                }
            }
        }
    }

    for (Instruction *I : toDemote) {
        // Alloca at top of entry
        IRBuilder<> allocaB(&*entry.begin());
        AllocaInst *slot = allocaB.CreateAlloca(
            I->getType(), nullptr, I->getName() + ".reg");

        // Store after the defining instruction
        IRBuilder<> storeB(&*I->getNextNode()->getIterator());
        storeB.CreateStore(I, slot);

        // Replace cross-block uses with loads
        SmallVector<Use *, 8> xUses;
        for (auto &U : I->uses())
            if (auto *UI = dyn_cast<Instruction>(U.getUser()))
                if (UI->getParent() != I->getParent())
                    xUses.push_back(&U);

        for (Use *U : xUses) {
            auto *user = cast<Instruction>(U->getUser());
            IRBuilder<> b(&*user->getIterator());
            Value *ld = b.CreateLoad(I->getType(), slot,
                                      I->getName() + ".rld");
            U->set(ld);
        }
    }
}

// =================================================================
// CONTROL FLOW FLATTENING
// =================================================================

static bool flattenFunction(Function &F) {
    // Need at least 2 blocks to flatten
    if (F.size() <= 1) return false;

    // Skip functions with exception handling (landing pads, invokes)
    for (auto &BB : F) {
        if (BB.isLandingPad()) return false;
        if (isa<InvokeInst>(BB.getTerminator())) return false;
    }

    // --- Step 1: Demote PHIs and cross-block registers ---
    demotePhiNodes(F);
    demoteRegisters(F);

    // --- Step 2: Split entry block at first non-alloca ---
    // Allocas must remain in the entry block (LLVM requirement for
    // efficient stack frame allocation). Everything else moves to a
    // new "entry.body" block that becomes a switch case.
    BasicBlock *entry = &F.getEntryBlock();
    auto splitIt = entry->begin();
    while (splitIt != entry->end() && isa<AllocaInst>(&*splitIt))
        ++splitIt;

    if (splitIt == entry->end()) return false;

    BasicBlock *firstBlock = entry->splitBasicBlock(splitIt, "entry.body");

    // --- Step 3: Collect all blocks to flatten ---
    // Everything except the entry block (which now only has allocas).
    std::vector<BasicBlock *> blocks;
    for (auto &BB : F)
        if (&BB != entry) blocks.push_back(&BB);

    if (blocks.empty()) return false;

    // --- Step 4: Assign random state IDs ---
    Type *i32Ty = Type::getInt32Ty(F.getContext());
    std::set<uint32_t> usedIDs;
    std::map<BasicBlock *, ConstantInt *> stateMap;

    for (BasicBlock *BB : blocks) {
        uint32_t id;
        do { id = (uint32_t)rand(); } while (!id || usedIDs.count(id));
        usedIDs.insert(id);
        stateMap[BB] = cast<ConstantInt>(ConstantInt::get(i32Ty, id));
    }

    // --- Step 5: Setup entry block ---
    // Remove the auto-generated branch from splitBasicBlock and
    // replace with: alloca state, store initial ID, br dispatcher
    entry->getTerminator()->eraseFromParent();
    IRBuilder<> entryB(entry);
    AllocaInst *stateVar = entryB.CreateAlloca(i32Ty, nullptr, "flat.state");
    entryB.CreateStore(stateMap[firstBlock], stateVar);

    // --- Step 6: Create dispatcher (loop header + switch) ---
    BasicBlock *dispatcher = BasicBlock::Create(
        F.getContext(), "flat.head", &F, firstBlock);
    BasicBlock *defaultBB = BasicBlock::Create(
        F.getContext(), "flat.def", &F);

    entryB.CreateBr(dispatcher);

    IRBuilder<> dispB(dispatcher);
    LoadInst *sv = dispB.CreateLoad(i32Ty, stateVar, "flat.sv");
    SwitchInst *sw = dispB.CreateSwitch(sv, defaultBB, blocks.size());

    for (BasicBlock *BB : blocks)
        sw->addCase(stateMap[BB], BB);

    // Default: loop back (unreachable in correct execution)
    IRBuilder<>(defaultBB).CreateBr(dispatcher);

    // --- Step 7: Redirect all terminators through the dispatcher ---
    for (BasicBlock *BB : blocks) {
        Instruction *term = BB->getTerminator();

        if (auto *br = dyn_cast<BranchInst>(term)) {
            if (br->isUnconditional()) {
                BasicBlock *target = br->getSuccessor(0);
                if (stateMap.count(target)) {
                    IRBuilder<> b(term);
                    b.CreateStore(stateMap[target], stateVar);
                    b.CreateBr(dispatcher);
                    term->eraseFromParent();
                }
            } else {
                BasicBlock *trueBB = br->getSuccessor(0);
                BasicBlock *falseBB = br->getSuccessor(1);
                if (stateMap.count(trueBB) && stateMap.count(falseBB)) {
                    IRBuilder<> b(term);
                    Value *next = b.CreateSelect(
                        br->getCondition(),
                        stateMap[trueBB], stateMap[falseBB],
                        "flat.next");
                    b.CreateStore(next, stateVar);
                    b.CreateBr(dispatcher);
                    term->eraseFromParent();
                }
            }
        } else if (auto *swI = dyn_cast<SwitchInst>(term)) {
            // Convert switch to cascaded selects + dispatch
            IRBuilder<> b(term);
            BasicBlock *defTarget = swI->getDefaultDest();
            Value *next = stateMap.count(defTarget)
                              ? (Value *)stateMap[defTarget]
                              : (Value *)ConstantInt::get(i32Ty, 0);

            for (auto caseIt : swI->cases()) {
                if (stateMap.count(caseIt.getCaseSuccessor())) {
                    Value *cond = b.CreateICmpEQ(
                        swI->getCondition(), caseIt.getCaseValue());
                    next = b.CreateSelect(
                        cond, stateMap[caseIt.getCaseSuccessor()],
                        next, "flat.sw");
                }
            }

            b.CreateStore(next, stateVar);
            b.CreateBr(dispatcher);
            term->eraseFromParent();
        }
        // ReturnInst, UnreachableInst: leave as-is (they exit the function)
    }

    return true;
}

} // end anonymous namespace

PreservedAnalyses FlatteningPass::run(Function &F, FunctionAnalysisManager &) {
    std::size_t nameHash = std::hash<std::string>{}(F.getName().str());
    auto now = std::chrono::high_resolution_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    srand((unsigned)(nanos ^ nameHash));

    if (flattenFunction(F))
        return PreservedAnalyses::none();
    return PreservedAnalyses::all();
}
