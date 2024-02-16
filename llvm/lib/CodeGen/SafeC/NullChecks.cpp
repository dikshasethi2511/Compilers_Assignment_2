#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
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
  };

struct NullCheck : public FunctionPass {

  static char ID;
  NullCheck() : FunctionPass(ID) {}

  // Data Flow Analysis for checking the nullpointers.
  // Prints the IN and OUT sets for each instruction.
  // Initialiases the IN and OUT sets for each instruction.
  void printInOutSetsForInstructions(Function &F) {
  ::std::unordered_map<Instruction *, NullCheckType> inSet;
  ::std::unordered_map<Instruction *, NullCheckType> outSet;

  // Initialize IN and OUT sets.
  for (auto &B : F) {
    for (auto &I : B) {
      inSet[&I] = NullCheckType::UNDEFINED;
      outSet[&I] = NullCheckType::UNDEFINED;
    }
  }

  // Print IN and OUT sets for each instruction.
  for (auto &B : F) {
    for (auto &I : B) {
      dbgs() << "Instruction: " << I << "\n";

      // Print IN set.
      dbgs() << "  IN Set: ";
      switch (inSet[&I]) {
      case NullCheckType::UNDEFINED:
        dbgs() << "UNDEFINED";
        break;
      case NullCheckType::NOT_A_NULL:
        dbgs() << "NOT_A_NULL";
        break;
      }
      dbgs() << "\n";

      // Print OUT set.
      dbgs() << "  OUT Set: ";
      switch (outSet[&I]) {
      case NullCheckType::UNDEFINED:
        dbgs() << "UNDEFINED";
        break;
      case NullCheckType::NOT_A_NULL:
        dbgs() << "NOT_A_NULL";
        break;
      }
      dbgs() << "\n\n";
    }
  }
}



  bool runOnFunction(Function &F) override {
		dbgs() << "running nullcheck pass on: " << F.getName() << "\n";
    printInOutSetsForInstructions(F);
    return false;
  }
}; // end of struct NullCheck

}  // end of anonymous namespace

char NullCheck::ID = 0;
static RegisterPass<NullCheck> X("nullcheck", "Null Check Pass",
                                 false /* Only looks at CFG */,
                                 false /* Analysis Pass */);

static RegisterStandardPasses Y(
    PassManagerBuilder::EP_EarlyAsPossible,
    [](const PassManagerBuilder &Builder,
       legacy::PassManagerBase &PM) { PM.add(new NullCheck()); });
