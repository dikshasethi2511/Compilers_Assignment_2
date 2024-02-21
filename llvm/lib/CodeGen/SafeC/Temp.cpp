
  //   void addAddInstruction(Function &F) {
  //     for (auto &B : F) {
  //       // Create an IRBuilder and set it to the beginning of the basic block

  //       for (auto &I : B) {
  //         // Check if the instruction is not the terminator instruction
  //         IRBuilder<> Builder(&I);
  //         if (!I.isTerminator()) {
  //           // Check if the type of the instruction is IntegerType
  //           if (IntegerType *IntTy = dyn_cast<IntegerType>(I.getType())) {
  //             // Check if the type is Int32
  //             if (IntTy->getBitWidth() == 32) {
  //               // Create an "add" instruction adding 1 to the result of the
  //               // original instruction
  //               Value *Op1 = Builder.getInt32(
  //                   1); // Use the result of the original instruction as
  //               Value *Op2 = Builder.getInt32(1); // Constant operand 1
  //               Instruction *AddInst =
  //                   dyn_cast<Instruction>(Builder.CreateAdd(Op1, Op2));
  //             }
  //           }
  //         }
  //       }
  //     }
  //   }

  // #include "llvm/Support/raw_ostream.h"

  //   int addDynamicCheck(Function &F) {
  //     int dynamicCheckID = 0;
  //     int dynamicCheckSum = 0;
  //     FunctionType *EmptyFnTy =
  //         FunctionType::get(Type::getInt32Ty(F.getContext()), false);
  //     Function *EmptyFn =
  //         Function::Create(EmptyFnTy, GlobalValue::InternalLinkage,
  //                          "emptyFunction", F.getParent());

  //     for (auto &B : F) {
  //       errs() << "Basic Block: " << B.getName() << "\n";

  //       Instruction *I = &(B.front());

  //       IRBuilder<> Builder(I);

  //       // Call before secodn instruction of BB.

  //       CallInst *DynamicCheck = Builder.CreateCall(EmptyFn, {});
  //       DynamicCheck->setDoesNotReturn();
  //       DynamicCheck->setName("dynamicCheck_" +
  //       std::to_string(dynamicCheckID)); dbgs() << "Created Dynamic Check: "
  //       << *DynamicCheck << "\n";

  //       // for (auto &I : B) {
  //       //   errs() << "Instruction: " << I << "\n";

  //       //   // for (auto &operand : I.operands()) {
  //       //   if (LoadInst *LI = dyn_cast<LoadInst>(&I)) {
  //       //     CallInst *DynamicCheck = Builder.CreateCall(EmptyFn, {});
  //       //     DynamicCheck->setDoesNotReturn();
  //       //     DynamicCheck->setName("dynamicCheck_" +
  //       //                           std::to_string(dynamicCheckID));
  //       //     errs() << "Created Dynamic Check: " << *DynamicCheck << "\n";
  //       //     dynamicCheckSum += dynamicCheckID;
  //       //     dynamicCheckID++;
  //       //   }

  //       // }
  //     }

  //     errs() << "Total Dynamic Check Sum: " << dynamicCheckSum << "\n";

  //     return dynamicCheckSum;
  //   }

  // void addNullChecks(Function &F) {
  //   for (auto &B : F) {
  //     for (auto &I : B) {
  //       // If the instruction is a Load instruction, then add a null check.
  //       if (LoadInst *LI = dyn_cast<LoadInst>(&I)) {
  //         if (isa<PointerType>(LI->getType())) {
  //           // If the left hand side of the Load instruction is
  //           MIGHT_BE_NULL,
  //               // then add a null check.
  //               if (OUT[&I][LI->getName().str()] ==
  //                   NullCheckType::MIGHT_BE_NULL) {
  //             // Create a new basic block for the null check.
  //             BasicBlock *nullCheckBB = BasicBlock::Create(
  //                 F.getContext(), "nullCheckBB",
  //                 F.getEntryBlock().getParent());
  //             // Create a new basic block for the original basic block.
  //             BasicBlock *originalBB = B.splitBasicBlock(LI, "originalBB");
  //             // Remove the unconditional branch instruction from the
  //             original
  //                 // basic block.
  //                 B.getTerminator()
  //                     ->eraseFromParent();
  //             // Add a conditional branch instruction to the original basic
  //             block
  //                 // to the null check basic block.
  //                 IRBuilder<>
  //                     builder(&B);
  //             builder.CreateCondBr(
  //                 builder.CreateICmpNE(LI,
  //                                      ConstantPointerNull::get(LI->getType())),
  //                 originalBB, nullCheckBB);
  //             // Add the null check to the null check basic block.
  //             IRBuilder<> builder2(nullCheckBB);
  //             builder2.CreateCall(
  //                 Intrinsic::getDeclaration(F.getParent(), Intrinsic::trap));
  //             builder2.CreateBr(originalBB);
  //           }
  //         }
  //       }
  //     }
  //   }
  // }