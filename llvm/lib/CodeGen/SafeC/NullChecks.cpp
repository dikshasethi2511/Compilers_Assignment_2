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

  // IN and OUT sets for all instructions.
  std::unordered_map<Instruction *,
                     std::unordered_map<std::string, NullCheckType>>
      IN, OUT;

  int count = 0;

  // Vector to store all pointer operands in the function.
  std::vector<std::string> pointerOperands;

  std::unordered_map<std::string, NullCheckType>
  meet(const std::unordered_map<std::string, NullCheckType> &currentInstruction,
       const std::unordered_map<std::string, NullCheckType>
           &prevBlockTerminatorInstruction) {

    std::unordered_map<std::string, NullCheckType> result;

    // Iterate over all operands in currentInstruction.
    for (const auto &entry : currentInstruction) {
      const std::string &name = entry.first;

      // Find the corresponding value in prevBlockTerminatorInstruction.
      auto it = prevBlockTerminatorInstruction.find(name);

      // If the string is present in both sets.
      if (it != prevBlockTerminatorInstruction.end()) {
        // If even one of them is MIGHT_BE_NULL, the result is MIGHT_BE_NULL.
        result[name] = (entry.second == NullCheckType::NOT_A_NULL &&
                        it->second == NullCheckType::NOT_A_NULL)
                           ? NullCheckType::NOT_A_NULL
                           : NullCheckType::MIGHT_BE_NULL;
      } else {
        // If the variable is not present in prevBlockTerminatorInstruction,
        // keep the value from currentInstruction.
        result[name] = entry.second;
      }
    }
    return result;
  }

  std::unordered_map<std::string, NullCheckType>
  transfer(Instruction *I,
           const std::unordered_map<std::string, NullCheckType> &inSet,
           const std::unordered_map<std::string, NullCheckType> &outSet) {
    std::unordered_map<std::string, NullCheckType> result = outSet;

    // Check if the instruction is Alloca, Call, Load, Store, GetElementPtr, or
    // Cast.
    AllocaInst *AI = dyn_cast<AllocaInst>(I);
    CallInst *CI = dyn_cast<CallInst>(I);
    LoadInst *LI = dyn_cast<LoadInst>(I);
    StoreInst *SI = dyn_cast<StoreInst>(I);
    GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(I);
    CastInst *CAI = dyn_cast<CastInst>(I);

    if (AI) {
      // If the instruction is an Alloca instruction then the pointer operand is
      // definitely not NULL.
      if (isa<PointerType>(AI->getAllocatedType())) {
        result[AI->getName().str()] = NullCheckType::NOT_A_NULL;
      }
    }

    // If the instruction is a Load instruction, then the left hand side is set
    // to might be NULL.
    else if (LI) {
      if (isa<PointerType>(LI->getType())) {
        result[LI->getName().str()] = NullCheckType::MIGHT_BE_NULL;
      }
    }

    // If the instruction is a Store instruction, then the pointer operand is
    // set to might be NULL. Note: We only consider the pointer operand of the
    // Store instruction.
    else if (SI) {
      if (isa<PointerType>(SI->getPointerOperand()->getType())) {
        result[SI->getPointerOperand()->getName().str()] =
            NullCheckType::MIGHT_BE_NULL;
      }
    }

    // If the instruction is a Call instruction and calls the mymalloc function,
    // then the left hand side can never be NULL.
    else if (CI && isMallocCall(CI)) {
      if (isa<PointerType>(CI->getType())) {
        result[CI->getName().str()] = NullCheckType::NOT_A_NULL;
      }
    }

    // Otherwise the pointer argument is set to might be NULL.
    else if (CI) {
      if (isa<PointerType>(CI->getType())) {
        result[CI->getName().str()] = NullCheckType::MIGHT_BE_NULL;
      }
    }

    // If the instruction is a GetElementPtr instruction, then the left hand
    // side is set to might be NULL.
    else if (GEP) {
      if (isa<PointerType>(GEP->getType())) {
        result[GEP->getName().str()] = NullCheckType::MIGHT_BE_NULL;
      }
    }

    // If the instruction is a Cast instruction, then the left hand side is set
    // to might be NULL.
    else if (CAI) {
      if (isa<PointerType>(CAI->getType())) {
        result[CAI->getName().str()] = NullCheckType::MIGHT_BE_NULL;
      }
    } else {
      // If the instruction is not one of the specified types, OUT = IN.
      result = inSet;
    }

    return result;
  }

  // Checks if a Call instruction is a malloc call.
  bool isMallocCall(CallInst *CI) {
    Function *Callee = CI->getCalledFunction();
    if (Callee && Callee->getName() == "mymalloc") {
      return true;
    }
    return false;
  }

  // Data Flow Analysis for checking the nullpointers.
  // performDataFlowAnalysis performs a forward data flow analysis to check for
  // null pointers in the program.
  // Steps:
  // 1. Initialization: The IN and OUT sets of all instructions contain a map of
  // all pointer operands in the function, with their values set to UNDEFINED.
  // 2. Iteration: The IN and OUT sets of all instructions are updated using the
  // transfer function and the meet operator until the OUT sets of all the
  // instructions stop changing.
  void performDataFlowAnalysis(Function &F) {
    // Storing all pointer operands in the function in the pointerOperands
    // vector.
    for (auto &B : F) {
      for (auto &I : B) {
        for (auto &operand : I.operands()) {
          // Stores the operands on the right hand side.
          if (isa<PointerType>(operand->getType())) {
            pointerOperands.push_back(operand->getName().str());
          }
          // Stores the variables on the right hand side.
          if (isa<PointerType>(I.getType())) {
            pointerOperands.push_back(I.getName().str());
          }
        }
      }
    }

    // Initialize IN and OUT sets of all instructions for all pointer operands
    // as UNDEFINED.
    for (auto &B : F) {
      for (auto &I : B) {
        // Iterate over the pointerOperands vector.
        for (std::string operandName : pointerOperands) {
          IN[&I][operandName] = NullCheckType::UNDEFINED;
          OUT[&I][operandName] = NullCheckType::UNDEFINED;
        }
      }
    }

    bool hasOutChanged = true;

    while (hasOutChanged == true) {
      hasOutChanged = false;
      // Walk across all basic blocks.
      for (auto &B : F) {
        // The IN set for the first instruction of the basic block is computed
        // by applying the meet operator on the terminator of all predecessors.
        // B.front() fetches the first instruction of the basic block.
        // IN[First_Instruction] = For all Preds => meet(IN, OUT(Last
        // instruction of Preds))

        for (BasicBlock *PredBB : predecessors(&B)) {
          IN[&B.front()] = meet(IN[&B.front()], OUT[PredBB->getTerminator()]);
        }

        // Start the iteration from the first instruction of the basic block.
        for (auto &I : B) {
          // Doesn't update the IN set for the first instruction of the basic
          // block.
          if (I.getPrevNode() != nullptr) {
            IN[&I] = OUT[I.getPrevNode()];
          }

          auto currentOld = OUT[&I];
          OUT[&I] = transfer(&I, IN[&I], OUT[&I]);

          if (currentOld != OUT[&I]) {
            hasOutChanged = true;
          }
        }
      }
    }
  }

  bool runOnFunction(Function &F) override {
    dbgs() << "running nullcheck pass on: " << F.getName() << "\n";
    performDataFlowAnalysis(F);

    // Map of instructions already processed.
    std::unordered_map<Instruction *, bool> processedInstructions;

    for (auto B_it = F.begin(); B_it != F.end(); ++B_it) {
      BasicBlock *B = &*B_it;
      for (auto I_it = B->begin(); I_it != B->end(); ++I_it) {
        bool nullcheckFound = false;

        // Check if processedInstructions contains the current instruction.
        if (processedInstructions.find(&*I_it) != processedInstructions.end()) {
          break;
        }

        // Print all the basic blocks of the function.
        dbgs() << "Function: " << F.getName() << "\n";
        dbgs() << "Basic Blocks: \n";
        for (auto &B : F) {
          dbgs() << B.getName() << "\n";
        }
        Instruction *currentInst = &*I_it;
        Value *operand;

        dbgs() << "currentInst: " << *currentInst << "\n";

        // Determine the operand based on the instruction type
        if (LoadInst *LI = dyn_cast<LoadInst>(currentInst)) {
          operand = LI->getPointerOperand();
        } else if (StoreInst *SI = dyn_cast<StoreInst>(currentInst)) {
          operand = SI->getPointerOperand();
        } else if (GetElementPtrInst *GEP =
                       dyn_cast<GetElementPtrInst>(currentInst)) {
          operand = GEP->getPointerOperand();
        } else if (CastInst *CI = dyn_cast<CastInst>(currentInst)) {
          operand = CI->getOperand(0);
        } else {
          continue;
        }

        if (OUT[currentInst][operand->getName().str()] ==
            NullCheckType::MIGHT_BE_NULL) {
          dbgs() << "Found a null check: " << *currentInst << "\n";
          dbgs() << "Current basic block: " << (*B_it).getName() << "\n";

          // Add to processedInstructions map.
          processedInstructions[currentInst] = true;

          // Split the basic block before this instruction I of this basic block
          std::string nameBB = "nullCheckSplit" + std::to_string(count);
          BasicBlock *NewBB = (*B_it).splitBasicBlock(currentInst, nameBB);

          count++;

          // Print the last instruction of NewBB.
          dbgs() << "NewBB: " << NewBB->getName() << "\n";
          dbgs() << "NewBB Last Instruction: " << NewBB->back() << "\n";
          // Print the first instruction of newBB.
          dbgs() << "NewBB First Instruction: " << NewBB->front() << "\n";

          // Create the blocks for null check logic.
          BasicBlock *CheckBlock =
              BasicBlock::Create(F.getContext(), "NullCheck", &F);
          BasicBlock *ExitBlock =
              BasicBlock::Create(F.getContext(), "NullCheckExit", &F);

          // Add the null check logic in the CheckBlock.
          IRBuilder<> builder(CheckBlock);
          Value *isNull = builder.CreateICmpEQ(
              operand,
              ConstantPointerNull::get(cast<PointerType>(operand->getType())));
          builder.CreateCondBr(isNull, ExitBlock, NewBB);

          // Terminate the ExitBlock.
          IRBuilder<> exitBuilder(ExitBlock);
          exitBuilder.CreateUnreachable();

          // Print all the instructions of the CheckBlock and the ExitBlock.
          dbgs() << "CheckBlock: " << CheckBlock->getName() << "\n";
          dbgs() << "CheckBlock Instructions: \n";
          for (auto &I : *CheckBlock) {
            dbgs() << I << "\n";
          }
          dbgs() << "ExitBlock: " << ExitBlock->getName() << "\n";
          dbgs() << "ExitBlock Instructions: \n";
          for (auto &I : *ExitBlock) {
            dbgs() << I << "\n";
          }

          // Before deleting the original instruction from the basic block.
          // Print the last instruction of the original basic block.
          dbgs() << "OriginalBB: " << (*B_it).getName() << "\n";
          dbgs() << "OriginalBB Last Instruction: " << (*B_it).back() << "\n";

          dbgs() << "OriginalBB Second Last Instruction: ";
          Instruction *secondLastInst = (*B_it).back().getPrevNode();
          secondLastInst->print(dbgs()); // or use dump() for more details
          dbgs() << "\n";

          // Remove the unconditional branch instruction from the original basic
          // block.
          (*B_it).getTerminator()->eraseFromParent();
          // Insert a branch instruction to the checkBlock.
          IRBuilder<> originalBlockBuilder(&*B_it);
          originalBlockBuilder.CreateBr(CheckBlock);
          nullcheckFound = true;
        }
        if (nullcheckFound) {
          break;
        }
      }
    }
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
