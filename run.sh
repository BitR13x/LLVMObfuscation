#!/bin/bash
# run.sh — Build, obfuscate, and run an example source file.
#
# Usage:
#   ./run.sh <file> <passes>
#
# Arguments:
#   file    — path to the source file (default: example/target.c)
#   passes  — comma-separated opt pass names (default: mba-linear)
#
# Examples:
#   ./run.sh example/target.c mba-linear
#   ./run.sh example/target_cff.c cff
#   ./run.sh example/target.c mba,mba-linear

set -e

# ---------------------------------------------------------------------------
# Arguments
# ---------------------------------------------------------------------------
FILE="${1:-example/target.c}"
PASSES="${2:-mba-linear}"

# Derive a clean basename for output files (e.g. "target_cff")
BASENAME=$(basename "${FILE}" | sed 's/\.[^.]*$//')

echo "=== Obfuscation Run ==="
echo "    Source : ${FILE}"
echo "    Passes : ${PASSES}"
echo "    Base   : ${BASENAME}"
echo ""

# ---------------------------------------------------------------------------
# [1/4] Build the pass plugin
# ---------------------------------------------------------------------------
echo "[1/4] Building the Obfuscation Pass..."
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
mkdir -p build/IR
mkdir -p build/bin

# ---------------------------------------------------------------------------
# [2/4] Compile source to LLVM Bitcode
# ---------------------------------------------------------------------------
echo "[2/4] Compiling ${FILE} to LLVM Bitcode..."
# -O0        — no optimisations, keeps all target instructions alive
# -disable-O0-optnone — lets opt run passes on the -O0 output

# Detect C vs C++ by extension
if [[ "${FILE}" == *.cpp || "${FILE}" == *.cxx || "${FILE}" == *.cc ]]; then
    COMPILER=clang++
    STD_FLAG="-std=c++17"
else
    COMPILER=clang
    STD_FLAG=""
fi

${COMPILER} ${STD_FLAG} -O0 -Xclang -disable-O0-optnone \
    -emit-llvm -c "${FILE}" \
    -o "build/${BASENAME}.bc"

${COMPILER} ${STD_FLAG} -S -emit-llvm "${FILE}" \
    -o "build/IR/${BASENAME}.ll"

# ---------------------------------------------------------------------------
# [3/4] Run the obfuscation pass via opt
# ---------------------------------------------------------------------------
echo "[3/4] Running obfuscation passes: ${PASSES}..."
opt -load-pass-plugin=./build/libObfuscationPass.so \
    -passes="${PASSES}" \
    "build/${BASENAME}.bc" \
    -o "build/${BASENAME}_obf.bc"

llvm-dis "build/${BASENAME}_obf.bc" -o "build/IR/${BASENAME}_obf.ll"

# ---------------------------------------------------------------------------
# [4/4] Compile obfuscated bitcode to executable and run it
# ---------------------------------------------------------------------------
echo "[4/4] Compiling obfuscated bitcode to executable..."
${COMPILER} "build/${BASENAME}_obf.bc" -o "build/bin/${BASENAME}_obf_exe"
echo "- Compiling original bitcode to executable..."
${COMPILER} "build/${BASENAME}.bc" -o "build/bin/${BASENAME}_exe"

echo ""
echo "--- RUNNING EXECUTABLE ---"
"./build/bin/${BASENAME}_obf_exe"
echo "--------------------------"
echo "Success! ${FILE} was obfuscated with [${PASSES}] and ran correctly."

# ---------------------------------------------------------------------------
# Run the matching test suite
# ---------------------------------------------------------------------------
echo ""
echo "--- RUNNING TESTS ---"
./test.sh "${PASSES}"
