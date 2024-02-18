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
#include <set>
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

  std::unordered_map<Instruction *,
                     std::unordered_map<std::string, NullCheckType>>
      IN, OUT;

  // TODO: Make this unordered map
  std::vector<std::string> pointerOperands;

  // Data Flow Analysis for checking the nullpointers.
  void performDataFlowAnalysis(Function &F) {
    // Collect all pointer operands in an array
    for (auto &B : F) {
      for (auto &I : B) {
        for (auto &operand : I.operands()) {
          if (isa<PointerType>(operand->getType())) {
            pointerOperands.push_back(operand->getName().str());
          }
          if (isa<PointerType>(I.getType())) {
            pointerOperands.push_back(I.getName().str());
          }
        }
      }
    }

    // Initialize IN and OUT sets of all instructions for all pointer operands
    // as UNDEFINED
    for (auto &B : F) {
      for (auto &I : B) {
        // Iterate over the pointerOperands vector
        for (std::string operandName : pointerOperands) {
          IN[&I][operandName] = NullCheckType::UNDEFINED;
          OUT[&I][operandName] = NullCheckType::UNDEFINED;

          // dbgs() << "operand name: " << operandName << "\n";
        }
      }
    }

    // Walk across all instructions and apply meet and transfer functions
    for (auto &B : F) {
      dbgs() << "block name: " << B.getName() << "\n";
      dbgs() << "first instruction: " << B.front().getName().str() << "\n";
      dbgs() << "last instruction: " << B.back().getName().str() << "\n";
      // For all Basic Blocks
      for (BasicBlock *PredBB : predecessors(&B)) {

        // dbgs() << "predecessor name: " << PredBB->getName().str() << "\n";
        // for (auto &I : B) {
        //   dbgs() << "predecessor name: " << I.getPrevNode()->getName().str()
        //          << "\n";
        // }
      }
      // for (auto &I : B) {
      //   for (Instruction *PredI : I.prede) {
      //   }

      //   // Initialize IN set with conservative values (e.g., all pointers
      //   // are undefined).
      //   for (auto &operand : I.operands()) {
      //     if (isa<PointerType>(operand->getType())) {
      //       dbgs() << "instruction name: " << I.getName().str() << "\n";

      //       IN[&I][operand->getName().str()] = NullCheckType::UNDEFINED;
      //       OUT[&I][operand->getName().str()] = NullCheckType::UNDEFINED;
      //       // Push I.getName() also to the set. Make it possible null if
      //       // operand is not mymalloc.
      //     }
      //     if (isa<PointerType>(I.getType())) {
      //       dbgs() << "instruction type: " << I.getType()->getTypeID()
      //              << "\n";
      //       IN[&I][I.getName().str()] = NullCheckType::UNDEFINED;
      //       OUT[&I][I.getName().str()] = NullCheckType::UNDEFINED;
      //     }
      //   }
      // }
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