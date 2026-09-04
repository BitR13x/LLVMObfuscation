# LLVM Obfuscation Passes

This repository contains an LLVM pass framework designed to obfuscate C/C++ programs, making them significantly harder to analyze using decompilers (like Ghidra/IDA) and reverse engineering tools.

Currently, the framework includes two obfuscation techniques: **Mixed Boolean-Arithmetic (MBA)** and **Control Flow Flattening (CFF)**.

## Obfuscation Passes

### 1. Linear MBA Pass (`mba-linear`)
Mixed Boolean Arithmetic (MBA) obfuscation transforms simple mathematical operations into complex, mathematically equivalent expressions by combining standard arithmetic (`+`, `-`, `*`) with bitwise operations (`^`, `&`, `|`, `~`).

**Features:**
* **Dynamic Rule Generation:** Dynamically builds transformation identities with varying probabilities instead of using hardcoded rules.
* **Opaque Constants (LCG):** Injects random transformation coefficients into a `.bss` array initialized via a Linear Congruential Generator at runtime. This prevents reverse-engineering tools from statically evaluating and simplifying the expressions.

### 2. Control Flow Flattening (`flattening`)
Control Flow Flattening completely destroys the structural control flow graph (CFG) of a function (like `if/else` trees, `for` loops, and `switch` statements).

**Features:**
* **Polymorphic State Machine:** Converts basic blocks into cases within a massive `switch` statement inside an infinite loop.
* **Compile-Time Polymorphism:** Uses a high-resolution time seed to generate completely random, unique 32-bit state IDs every time the binary is compiled, defeating static un-flattening scripts that rely on hardcoded state numbers.
* **SSA Demotion:** Demotes PHI nodes and cross-block registers to memory (`alloca`/`load`/`store`) to bypass strict LLVM SSA dominance rules.

---

## Project Structure

* `src/` — Contains the LLVM pass plugin (`ObfuscationPass.cpp`) and pass implementations (`LinearMBA`, `Flattening`).
* `src/utils/` — Reusable math utilities and the Opaque Constants generator.
* `example/` — Sample C/C++ target programs (`target.c`, `target_cff.c`) to test obfuscation against.
* `tests/` — Automated test suite using `doctest` to ensure binaries still execute correctly after obfuscation.
* `run.sh` — The main wrapper script to build the pass, compile a target, obfuscate it, and execute it.
* `test.sh` — The test runner script.
* `decompile.sh` — A helper script that uses `radare2`/`r2ghidra` to decompile the resulting binary and verify the obfuscation visually.

---

## How to Run

This project uses the modern **LLVM New Pass Manager** and includes helper bash scripts that automate the CMake build, LLVM Bitcode compilation (`clang -emit-llvm`), optimization (`opt`), and final linking stages.

### Obfuscating a File

You can use the `run.sh` script to obfuscate any target file with a chosen set of passes.

```bash
# Usage: ./run.sh <source_file> <pass_names>

# Apply Linear MBA obfuscation
./run.sh example/target.c mba-linear

# Apply Control Flow Flattening
./run.sh example/target_cff.c cff

# Chain passes together (Flattening first, then MBA)
./run.sh example/target_cff.c cff,mba-linear
```

### Running Tests

We use `doctest` to assert that obfuscation does not break the runtime execution of the binaries (e.g., verifying `x + y` still equals the right value after MBA).

```bash
# Run tests for a specific pass
./test.sh cff
```

### Decompilation Analysis

To view the results of the obfuscation, you can run the decompilation helper (requires `radare2` and `r2ghidra`).

```bash
# Decompile the main function of the most recently built obfuscated binary
./decompile.sh
```
