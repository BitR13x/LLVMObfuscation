# LLVM Mixed Boolean Arithmetic (MBA) Obfuscation Pass

This repository contains a skeleton for an LLVM pass designed to perform Mixed Boolean Arithmetic (MBA) obfuscation. 

## What is Mixed Boolean Arithmetic (MBA) Obfuscation?

Mixed Boolean Arithmetic (MBA) is an obfuscation technique that transforms simple mathematical operations into complex, mathematically equivalent expressions. It does this by combining standard integer arithmetic (like `+`, `-`, `*`) with bitwise operations (`^`, `&`, `|`, `~`).

The goal is to make the binary harder to understand for reverse engineers and analysis tools, as simple operations are hidden behind dense mathematical formulas.

### Example

A classic, simple MBA identity for addition is:
`x + y == (x ^ y) + 2 * (x & y)`

An obfuscator would take an instruction like `add %a, %b` and replace it with a series of LLVM IR instructions that compute `(%a ^ %b) + 2 * (%a & %b)`. More complex MBA identities are often generated dynamically.

## Project Structure

- `CMakeLists.txt`: Build configuration for the LLVM pass.
- `src/MBAPass.cpp`: The C++ source code containing the skeleton of our LLVM `FunctionPass`.
- `example/target.c`: A simple C program with basic arithmetic operations to test the pass on.

## Setup & Implementation

The `MBAPass.cpp` currently contains a skeleton of the pass with the following setup functions:

1. `isObfuscationTarget(Instruction *Inst)`: Should be implemented to identify which instructions to obfuscate (e.g., looking for `Instruction::Add`).
2. `applyMBASubstitution(Instruction *Inst)`: Should be implemented to construct the new MBA expression using `IRBuilder` and replace the original instruction.

## How to Build

To build this pass, you will need LLVM installed on your system (e.g., via your package manager or built from source).

```bash
# Configure the project to use the 'build' directory
cmake -B build

# Compile the project inside the 'build' directory
cmake --build build
```

This will produce a shared library (e.g., `MBAPass.so` or `MBAPass.dylib` depending on your OS) in the `build` directory.

## How to Run

You can run this pass using the `opt` tool provided by LLVM.

First, compile your target program to LLVM IR (Bitcode):

```bash
clang -emit-llvm -c ../example/target.c -o target.bc
```

Next, run the pass using `opt`:

```bash
opt -load ./MBAPass.so -legacy-pass-manager -mba-obfuscation < target.bc > target_obfuscated.bc
```
*(Note: Use `-enable-new-pm=0` or `-legacy-pass-manager` depending on your LLVM version if you are using the legacy pass manager).*

Finally, compile the obfuscated bitcode to an executable:

```bash
clang target_obfuscated.bc -o target_obfuscated
```
