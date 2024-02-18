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

  std::unordered_map<std::string, NullCheckType>
  meet(const std::unordered_map<std::string, NullCheckType> &currentInstruction,
       const std::unordered_map<std::string, NullCheckType>
           &prevBlockTerminatorInstruction) {
    // The meet operation computes the intersection of two sets
    std::unordered_map<std::string, NullCheckType> result;

    // Iterate over all strings in currentInstruction
    for (const auto &entry : currentInstruction) {
      const std::string &name = entry.first;

      // Find the corresponding value in prevBlockTerminatorInstruction
      auto it = prevBlockTerminatorInstruction.find(name);

      // If the string is present in both sets
      if (it != prevBlockTerminatorInstruction.end()) {
        // Compare values and update result accordingly
        result[name] = (entry.second == NullCheckType::NOT_A_NULL &&
                        it->second == NullCheckType::NOT_A_NULL)
                           ? NullCheckType::NOT_A_NULL
                           : NullCheckType::MIGHT_BE_NULL;
      } else {
        // If the string is not present in prevBlockTerminatorInstruction, keep
        // the value from currentInstruction
        result[name] = entry.second;
      }
    }

    return result;
  }

  // Define the transfer function
  // @dikshu complete transfer function
  std::unordered_map<std::string, NullCheckType>
  transfer(Instruction *I,
           const std::unordered_map<std::string, NullCheckType> &inSet,
           const std::unordered_map<std::string, NullCheckType> &outSet) {
    // The transfer function logic based on your requirements
    std::unordered_map<std::string, NullCheckType> result = outSet;

    // Check if the instruction is Alloca, Call, Load, Store, GetElementPtr, or
    // Cast
    AllocaInst *AI = dyn_cast<AllocaInst>(I);
    CallInst *CI = dyn_cast<CallInst>(I);
    LoadInst *LI = dyn_cast<LoadInst>(I);
    StoreInst *SI = dyn_cast<StoreInst>(I);
    GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(I);
    CastInst *CAI = dyn_cast<CastInst>(I);

    if (AI) {
      // If the instruction is an Alloca instruction

    }

    if (AI || CI || LI || SI || GEP || CAI) {
      // If the instruction is one of the specified types

      // For all elements inside the in set, make them MIGHT_BE_NULL
      for (const auto &entry : inSet) {
        const std::string &operandName = entry.first;
        result[operandName] = NullCheckType::MIGHT_BE_NULL;
      }

      // If the operand for a Call instruction is mymalloc, set the element to
      // NOT_A_NULL
      if (CI && isMallocCall(CI)) {
        for (const auto &entry : inSet) {
          const std::string &operandName = entry.first;
          result[operandName] = NullCheckType::NOT_A_NULL;
        }
      }
    } else {
      // If the instruction is not one of the specified types, OUT = IN
      result = inSet;
    }

    return result;
  }

  // Helper function to check if a Call instruction is a malloc call
  bool isMallocCall(CallInst *CI) {
    Function *Callee = CI->getCalledFunction();
    if (Callee && Callee->getName() == "mymalloc") {
      return true;
    }
    return false;
  }

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

    bool changeFlag;
    while (changeFlag == true) {
      changeFlag = false;

      // Walk across all basic blocks
      for (auto &B : F) {
        // 1. B.front() which is the first instruction, apply meet operator on
        // the terminator of all  predecessors. ( IN[first intruction] = for all
        // pred meet(ins, out(terminator inst of pred))

        // Find the IN Of the first instruction of the basic block
        for (BasicBlock *PredBB : predecessors(&B)) {
          // You can also use PredBB.back() here
          IN[&B.front()] = meet(IN[&B.front()], OUT[PredBB->getTerminator()]);
        }

        // 2. Walk through the rest of the instructions, and apply
        for (auto &I : B) {
          // TODO: Make sure that this skips the first instruction, and nothing
          // else. This is a way to identify what is not the first instruction
          if (I.getPrevNode() != NULL) {
            IN[&I] = OUT[I.getPrevNode()];
          }

          // TODO: Check if OUT[I] has changed or not, if it has changed then
          // set changeFlag = true

          OUT[&I] = transfer(&I, IN[&I], OUT[&I]);
        }
      }

      // dbgs() << "block name: " << B.getName() << "\n";
      // dbgs() << "first instruction: " << B.front().getName().str() << "\n";
      // dbgs() << "last instruction: " << current->getParent.str() << "\n";
      // dbgs() << "predecessor name: " << PredBB->getName().str() << "\n";
      // for (auto &I : B) {
      //   dbgs() << "predecessor name: " << I.getPrevNode()->getName().str()
      //          << "\n";
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