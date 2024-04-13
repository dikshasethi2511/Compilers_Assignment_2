#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/CodeGen/Analysis.h"
#include "llvm/CodeGen/ValueTypes.h"
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
#include "llvm/Support/LowLevelTypeImpl.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <deque>
#include <map>

using namespace llvm;

namespace {
struct MemSafe : public FunctionPass {
  static char ID;
  const TargetLibraryInfo *TLI = nullptr;
  std::map<AllocaInst *, Value *> allocaMap;
  MemSafe() : FunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetLibraryInfoWrapperPass>();
  }

  bool runOnFunction(Function &F) override;
  void transformAllocaToMyMalloc(Function &F);

}; // end of struct MemSafe
} // end of anonymous namespace

static bool isLibraryCall(const CallInst *CI, const TargetLibraryInfo *TLI) {
  LibFunc Func;
  if (TLI->getLibFunc(ImmutableCallSite(CI), Func)) {
    return true;
  }
  auto Callee = CI->getCalledFunction();
  if (Callee && Callee->getName() == "readArgv") {
    return true;
  }
  if (isa<IntrinsicInst>(CI)) {
    return true;
  }
  return false;
}

void MemSafe::transformAllocaToMyMalloc(Function &F) {
  // Data layout contains all the information about the sizes, types and
  // alignments.
  const DataLayout &DL = F.getParent()->getDataLayout();
  auto Int8PtrTy = Type::getInt8PtrTy(F.getContext());
  // Traverse through all the basic blocks of the functions.
  for (BasicBlock &BB : F) {
    // Collect alloca instructions to erase them later.
    std::vector<AllocaInst *> AllocaInstructions;
    std::vector<CallInst *> MallocCalls;
    // Traverse through all the instructions of the basic block.
    for (Instruction &I : BB) {
      // Whenever an alloca instruction is found, replace it with a call to
      // mymalloc.
      if (AllocaInst *AI = dyn_cast<AllocaInst>(&I)) {
        IRBuilder<> Builder(AI);
        // Get the size and type of the allocated memory.
        // We pass the obtained size to the mymalloc call to allocate the
        // memory.
        Type *Ty = AI->getAllocatedType();
        uint64_t Size = DL.getTypeAllocSize(Ty);

        // Create a call to mymalloc.
        FunctionType *MallocFuncType = FunctionType::get(
            // mymalloc has a return type of i8* and takes an argument of i64.
            Type::getInt8PtrTy(F.getContext()),
            {Type::getInt64Ty(F.getContext())}, false);
        FunctionCallee MallocFunc =
            F.getParent()->getOrInsertFunction("mymalloc", MallocFuncType);
        CallInst *MallocCall = Builder.CreateCall(
            MallocFunc,
            {ConstantInt::get(Type::getInt64Ty(F.getContext()), Size)});

        // Bitcast the result of mymalloc to the desired type.
        // Taken from nullcheck pass.
        //  %call1 = call i8* @mymalloc(i32 4)
        //  %0 = bitcast i8* %call1 to i32*
        // The dynamically allocated memory is bitcasted to the desired type.
        // By bitcasting the pointer to the desired type, you inform LLVM how to
        // interpret the allocated memory.
        Type *PtrTy = PointerType::get(Ty, 0);
        Value *MallocResult = Builder.CreateBitCast(MallocCall, PtrTy);

        // Replace uses of alloca with the result of mymalloc.
        AI->replaceAllUsesWith(MallocResult);
        // Add alloca instruction to the list for later erasure.
        AllocaInstructions.push_back(AI);
        MallocCalls.push_back(MallocCall);
      }
    }

    // Insert myfree calls for each mymalloc call at the end of the basic block.
    if (!MallocCalls.empty()) {
      for (CallInst *MallocCall : MallocCalls) {
        BasicBlock *BB = MallocCall->getParent();
        // The myfree call is inserted at the end of the basic block when the
        // scope gets over.
        Instruction *InsertPoint = BB->getTerminator();
        IRBuilder<> Builder(InsertPoint);
        FunctionType *FreeFuncType =
            FunctionType::get(Type::getVoidTy(F.getContext()),
                              {Type::getInt8PtrTy(F.getContext())}, false);
        FunctionCallee FreeFunc =
            F.getParent()->getOrInsertFunction("myfree", FreeFuncType);
        Builder.CreateCall(FreeFunc, {MallocCall});
      }
    }

    // Erase all collected alloca instructions.
    for (AllocaInst *AI : AllocaInstructions) {
      AI->eraseFromParent();
    }
  }
}

bool MemSafe::runOnFunction(Function &F) {
  TLI = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  transformAllocaToMyMalloc(F);
  return true;
}

char MemSafe::ID = 0;
static RegisterPass<MemSafe> X("memsafe", "Memory Safety Pass",
                               false /* Only looks at CFG */,
                               false /* Analysis Pass */);

static RegisterStandardPasses Y(PassManagerBuilder::EP_EarlyAsPossible,
                                [](const PassManagerBuilder &Builder,
                                   legacy::PassManagerBase &PM) {
                                  PM.add(new MemSafe());
                                });