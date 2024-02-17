#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <deque>
#include <unordered_map>

using namespace llvm;

namespace {

enum class NullCheckType {
  UNDEFINED,
  NOT_A_NULL,
  MIGHT_BE_NULL,
};

struct NullCheck : public FunctionPass {

  static char ID;
  NullCheck() : FunctionPass(ID) {}

  // Data Flow Analysis for checking the nullpointers.
  void performDataFlowAnalysis(Function &F) {

    // Initialize IN and OUT sets.
    for (auto &B : F) {
      for (auto &I : B) {
        // Initialize IN set with conservative values (e.g., all pointers are
        // undefined).
        for (auto &operand : I.operands()) {
          if (isa<PointerType>(operand->getType())) {
            dbgs() << "operand: " << operand->getName().str() << "\n";
            // Push I.getName() also to the set. Make it possible null if
            // operand is not mymalloc.
          }
        }
      }
    }
  }

  bool runOnFunction(Function &F) override {
    dbgs() << "running nullcheck pass on: " << F.getName() << "\n";
    performDataFlowAnalysis(F);
    return false;
  }
}; // end of struct NullCheck

} // end of anonymous namespace

char NullCheck::ID = 0;
static RegisterPass<NullCheck> X("nullcheck", "Null Check Pass",
                                 false /* Only looks at CFG */,
                                 false /* Analysis Pass */);

static RegisterStandardPasses Y(PassManagerBuilder::EP_EarlyAsPossible,
                                [](const PassManagerBuilder &Builder,
                                   legacy::PassManagerBase &PM) {
                                  PM.add(new NullCheck());
                                });