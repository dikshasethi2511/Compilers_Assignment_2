# Null Check Pass

## Overview

The Null Check Pass is an LLVM pass designed to detect array accesses with NULL bases at runtime. It uses data flow analysis to identify array accesses where the array base is guaranteed not to be NULL, ensuring that null pointer dereferences are caught and handled appropriately. The analysis and checks are performed on LLVM Intermediate Representation (IR).

## Detailed Explanation

### Introduction

In many programming scenarios, dereferencing a NULL pointer can lead to crashes or undefined behavior. To prevent this, the Null Check Pass performs a comprehensive analysis of the program's pointer variables to ensure their validity before use. This pass not only detects potential NULL pointers but also inserts runtime checks to handle these cases gracefully.

### Data Flow Analysis

The core of the Null Check Pass is its data flow analysis, which operates as follows:

1. **Initialization**:
   - **Pointer Collection**: The pass collects all pointer operands used in the function, including those in function arguments.
   - **IN and OUT Sets**: Each instruction is associated with IN and OUT sets, which map pointer operands to their nullability state (`UNDEFINED`, `NOT_A_NULL`, or `MIGHT_BE_NULL`). Initially, all pointers are set to `UNDEFINED` except for function arguments, which are set to `MIGHT_BE_NULL`.

2. **Propagation**:
   - **Transfer Function**: This function updates the OUT set based on the IN set and the type of instruction. For example, an `alloca` instruction (which allocates memory) will set its pointer operand to `NOT_A_NULL`.
   - **Meet Operator**: The meet operator combines the IN sets of all predecessor instructions to compute the IN set for a basic block's first instruction.

3. **Iteration**:
   - The analysis iterates over the instructions, updating the IN and OUT sets until they stabilize (i.e., no further changes occur). This fixed-point iteration ensures that the nullability information is accurately propagated throughout the function.

### Null Check Insertion

After the data flow analysis, the pass identifies instructions that might dereference a NULL pointer and inserts runtime checks to prevent this:

1. **Basic Block Splitting**:
   - When a potentially NULL pointer is detected, the basic block is split before the instruction that uses the pointer. This allows the insertion of conditional branches based on the nullability of the pointer.

2. **Null Check Blocks**:
   - **CheckBlock**: A new block is created to perform the null check. If the pointer is NULL, control is transferred to an exit block.
   - **ExitBlock**: This block contains logic to handle the NULL case, such as calling the `exit` function to terminate the program.

3. **Control Flow Modification**:
   - The original basic block is modified to branch to the CheckBlock instead of continuing directly to the instruction. The CheckBlock then conditionally branches to either the ExitBlock (if NULL) or the continuation block (if not NULL).

### Detailed Steps

- **Pointer Operand Collection**: The pass iterates over all instructions and their operands, collecting those that are pointers.
- **Initialization**: For each instruction, the IN and OUT sets are initialized. Function arguments are initially assumed to be `MIGHT_BE_NULL`, while other pointers are `UNDEFINED`.
- **Data Flow Analysis Execution**: The pass performs iterative analysis, applying the transfer function and meet operator until the OUT sets converge.
- **Null Check Logic Insertion**: When a `MIGHT_BE_NULL` pointer is identified, the pass splits the basic block and inserts the appropriate null check logic.

### Conclusion

The Null Check Pass provides a robust mechanism to ensure pointer safety in LLVM-based programs. By performing detailed data flow analysis and inserting runtime checks, it helps prevent null pointer dereferences, enhancing program stability and reliability.

## Installation and Usage

### Prerequisites

Ensure you have `cmake` and `ninja-build` installed. You can install them using the following command:

```sh
sudo apt install cmake ninja-build
```

# Building the Project

## Clone the repository:

```sh
git clone https://github.com/dikshasethi2511/Compilers_Assignment_2.git
```

## Navigate to the project directory:

```sh
cd Compilers_Assignment_2
```

## Create and navigate to the build directory:

```sh
mkdir build
cd build
```

## Copy the build script and execute it:

```sh
cp ../scripts/build.sh .
sh build.sh
```

## Build the project using ninja:

```sh
ninja
```
**Note:** This may take some time

# After the build completes, navigate to the tests directory and run the tests:
```sh
cd ../tests
make
```
The make command generates an executable for each test case.
