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

#include <cstdlib>
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
  void findBasePointers(Value *V, std::set<Value *> &BasePointers, Function &F);
  bool checkOutOfBounds(GetElementPtrInst *GEP, Function &F,
                        const DataLayout &DL);
  bool reachedAnArgument(Value *V, bool result, Function &F);
  void insertWriteBarrier(Value *Base, Value *Ptr, Value *ObjSize,
                          IRBuilder<> &IRB, Function *F);

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
        // Get the allocated type and calculate the total size.
        Type *Ty = AI->getAllocatedType();

        // Calculate the total size.
        Value *totalSizeValue = nullptr;

        if (Ty->isArrayTy() || AI->isArrayAllocation()) {
          uint64_t elemSize;
          Type *Int64Ty = Type::getInt64Ty(F.getContext());

          if (AI->isArrayAllocation()) {
            // For array allocations, the size is the second operand of the
            // alloca instruction.
            totalSizeValue = AI->getOperand(0);
            elemSize = DL.getTypeAllocSize(Ty);
            totalSizeValue = Builder.CreateMul(
                totalSizeValue, ConstantInt::get(Int64Ty, elemSize));
          } else {
            // If the type is an array, calculate the total size using the array
            // size.
            uint64_t Size = 1;
            Size = cast<ArrayType>(Ty)->getNumElements();
            totalSizeValue = Builder.CreateMul(
                ConstantInt::get(Int64Ty, Size),
                ConstantInt::get(
                    Int64Ty,
                    AI->getModule()->getDataLayout().getTypeAllocSize(Ty)));
          }
        } else {
          // If it's not an array, calculate the size of the type directly.
          totalSizeValue = Builder.getInt64(DL.getTypeAllocSize(Ty));
        }

        // Create a call to mymalloc.
        FunctionType *MallocFuncType = FunctionType::get(
            // mymalloc has a return type of i8* and takes an argument of i64.
            Type::getInt8PtrTy(F.getContext()),
            {Type::getInt64Ty(F.getContext())}, false);
        FunctionCallee MallocFunc =
            F.getParent()->getOrInsertFunction("mymalloc", MallocFuncType);
        CallInst *MallocCall = Builder.CreateCall(MallocFunc, {totalSizeValue});

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

void MemSafe::findBasePointers(Value *V, std::set<Value *> &BasePointers,
                               Function &F) {

  // If the instruction is a call instruction and the function being called is
  // mymalloc, then add the call instruction to the set of base pointers.
  // This means that we have reached the base pointer of the variable and found
  // the first instruction where it was allocated.
  if (CallInst *CI = dyn_cast<CallInst>(V)) {
    Function *Callee = CI->getCalledFunction();
    if (isLibraryCall(CI, TLI)) {
      return;
    }
    BasePointers.insert(CI);
    return;
  }

  // If the instruction is a GEP instruction, then backtrack to find the base
  // pointer of the variable.
  else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V)) {
    for (Value *Idx : GEP->indices()) {
      if (Instruction *I = dyn_cast<Instruction>(Idx)) {
        findBasePointers(I, BasePointers, F);
      }
    }
    return;
  }

  // If the instruction is a load instruction, then backtrack to find the base
  // pointer of the variable.
  else if (LoadInst *LI = dyn_cast<LoadInst>(V)) {
    findBasePointers(LI->getPointerOperand(), BasePointers, F);
    return;
  }

  // If the instruction is a bitcast instruction, then backtrack to find the
  // base pointer of the variable.
  else if (BitCastInst *BCI = dyn_cast<BitCastInst>(V)) {
    findBasePointers(BCI->getOperand(0), BasePointers, F);
    return;
  } else {
    // Check if the variable we are looking for matches any of the function
    // arguments.
    for (Argument &Arg : F.args()) {
      if (V == &Arg) {
        BasePointers.insert(V);
        return;
      }
    }
  }
}

bool MemSafe::reachedAnArgument(Value *V, bool result, Function &F) {
  // If the instruction is a call instruction and the function being called is
  // mymalloc, then add the call instruction to the set of base pointers.
  // This means that we have reached the base pointer of the variable and found
  // the first instruction where it was allocated.
  if (CallInst *CI = dyn_cast<CallInst>(V)) {
    Function *Callee = CI->getCalledFunction();
    if (isLibraryCall(CI, TLI)) {
      return false;
    }
    return false;
  }

  // If the instruction is a GEP instruction, then backtrack to find the base
  // pointer of the variable.
  else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V)) {
    for (Value *Idx : GEP->indices()) {
      if (Instruction *I = dyn_cast<Instruction>(Idx)) {
        return result || reachedAnArgument(I, false, F);
      }
    };
  }

  // If the instruction is a load instruction, then backtrack to find the base
  // pointer of the variable.
  else if (LoadInst *LI = dyn_cast<LoadInst>(V)) {
    return result || reachedAnArgument(LI->getPointerOperand(), false, F);
  }

  // If the instruction is a bitcast instruction, then backtrack to find the
  // base pointer of the variable.
  else if (BitCastInst *BCI = dyn_cast<BitCastInst>(V)) {
    return result || reachedAnArgument(BCI->getOperand(0), false, F);
  } else {
    // Check if the variable we are looking for matches any of the function
    // arguments.
    for (Argument &Arg : F.args()) {
      if (V == &Arg) {
        return true;
      }
    }
  }
  return false;
}

bool MemSafe::checkOutOfBounds(GetElementPtrInst *GEP, Function &F,
                               const DataLayout &DL) {

  // Find the base pointer of the variable which is being accessed.
  std::set<Value *> basePointers;
  Value *startPoint = GEP->getPointerOperand();
  Value *checkReachedArg = GEP->getPointerOperand();
  if (reachedAnArgument(checkReachedArg, false, F)) {
    IRBuilder<> IRB(GEP);
    FunctionType *CallingExitType =
        FunctionType::get(Type::getVoidTy(F.getContext()), false);
    FunctionCallee CallingExitFunc =
        F.getParent()->getOrInsertFunction("CallingExit", CallingExitType);
    IRB.SetInsertPoint(GEP);
    IRB.CreateCall(CallingExitFunc);
    return true;
  }
  // Backtrack all the GEP and Bitcast instructions to find the base pointer.
  findBasePointers(startPoint, basePointers, F);

  // If the basePointers set is empty, then the base pointer of the variable
  // could not be found. This means that the variable is not allocated using
  // mymalloc.
  if (basePointers.empty()) {
    return false;
  }

  IRBuilder<> IRB(GEP);

  for (Value *Base : basePointers) {
    if (Instruction *BaseI = dyn_cast<Instruction>(Base)) {
      // Get the size of the access.
      Value *AccessSize =
          IRB.getInt64(DL.getTypeAllocSize(GEP->getResultElementType()));
      // Directly use the index from the GEP instruction.
      Value *Index = GEP->getOperand(1);
      Index = IRB.CreateMul(Index, AccessSize);
      Value *realBase = IRB.CreateGEP(BaseI, {Index});

      // Check if the access is within bounds.
      // Call CheckBounds functions with base, realbase, size, and access size.
      FunctionType *BoundsCheckFuncType =
          FunctionType::get(Type::getVoidTy(F.getContext()),
                            {Type::getInt8PtrTy(F.getContext()),
                             Type::getInt8PtrTy(F.getContext()),
                             Type::getInt64Ty(F.getContext())},
                            false);

      FunctionCallee BoundsCheckFunc = F.getParent()->getOrInsertFunction(
          "BoundsCheck", BoundsCheckFuncType);

      IRB.SetInsertPoint(GEP);
      IRB.CreateCall(
          BoundsCheckFunc,
          {IRB.CreateBitCast(Base, Type::getInt8PtrTy(F.getContext())),
           IRB.CreateBitCast(realBase, Type::getInt8PtrTy(F.getContext())),
           AccessSize});
    }
  }

  return true;
}

void MemSafe::insertWriteBarrier(Value *Base, Value *Ptr, Value *ObjSize,
                                 IRBuilder<> &IRB, Function *F) {
  FunctionType *WriteBarrierFuncType =
      FunctionType::get(Type::getVoidTy(Base->getContext()),
                        {Type::getInt8PtrTy(Base->getContext()),
                         Type::getInt8PtrTy(Base->getContext()),
                         Type::getInt64Ty(Base->getContext())},
                        false);

  FunctionCallee WriteBarrierFunc =
      F->getParent()->getOrInsertFunction("WriteBarrier", WriteBarrierFuncType);

  IRB.CreateCall(WriteBarrierFunc, {Base, Ptr, ObjSize});
}

bool MemSafe::runOnFunction(Function &F) {
  TLI = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  transformAllocaToMyMalloc(F);
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      // Check if the instruction is a GetElementPtr instruction.
      // If it is, check if the instruction is out of bounds.
      if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(&I)) {
        bool isOutOfBounds =
            checkOutOfBounds(GEP, F, F.getParent()->getDataLayout());
      }
    }
  }

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      // Check if the instruction is a store instruction.
      if (StoreInst *SI = dyn_cast<StoreInst>(&I)) {
        Value *StoredValue = SI->getValueOperand();
        Value *StoredAddress = SI->getPointerOperand();
        Type *StoredType = StoredValue->getType();

        // Check if the stored value is a pointer.
        if (StoredType->isPointerTy()) {
          IRBuilder<> IRB(SI);
          IRB.SetInsertPoint(SI);
          // Get the size of the stored object.
          Value *ObjSize =
              IRB.getInt64(F.getParent()->getDataLayout().getTypeAllocSize(
                  StoredType->getPointerElementType()));

          // Insert the write barrier.
          // insertWriteBarrier(StoredAddress, StoredValue, ObjSize, IRB, &F);
        }
      }
    }
  }

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