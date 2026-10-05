#!/bin/bash
# test.sh — Run doctest suites for MBA and/or CFF obfuscation passes.
#
# Usage:
#   ./test.sh              — run MBA tests (default, passes="mba")
#   ./test.sh mba          — explicit MBA tests
#   ./test.sh mba-linear   — run LinearMBA module-pass tests
#   ./test.sh cff          — run CFF tests
#   ./test.sh mba,cff      — run both passes together on MBA tests
#
# The CFF test suite is compiled from tests/test_cff.cpp and exercises
# all control-flow patterns in example/target_cff.c.

set -e

# ---------------------------------------------------------------------------
# 1. Build the pass plugin if not already built
# ---------------------------------------------------------------------------
if [ ! -f ./build/libObfuscationPass.so ]; then
    echo "[BUILD] Pass plugin not found — building..."
    cmake -B build
    cmake --build build -j$(nproc)
fi

# ---------------------------------------------------------------------------
# 2. Determine which passes to apply
# ---------------------------------------------------------------------------
if [[ -n "${1}" ]]; then
    PASSES=$1
else
    PASSES="mba"
fi

echo "=== Running test suite with passes: ${PASSES} ==="
mkdir -p build/test
mkdir -p build/test/IR

# ---------------------------------------------------------------------------
# 3. Generic test runner function
# ---------------------------------------------------------------------------
run_suite() {
    local NAME=$1
    local PREFIX=$(echo "$NAME" | tr 'a-z' 'A-Z')
    
    echo ""
    echo "[${PREFIX}] Compiling tests/test_${NAME}.cpp to LLVM Bitcode..."
    clang++ -std=c++11 -O0 -Xclang -disable-O0-optnone \
        -emit-llvm -c "tests/test_${NAME}.cpp" \
        -o "build/test/test_${NAME}.bc"

    echo "[${PREFIX}] Running obfuscation pass (${PASSES})..."
    opt -load-pass-plugin=./build/libObfuscationPass.so \
        -passes="${PASSES}" \
        "build/test/test_${NAME}.bc" \
        -o "build/test/test_${NAME}_obf.bc"

    llvm-dis "build/test/test_${NAME}_obf.bc" -o "build/test/IR/test_${NAME}_obf.ll" 2>/dev/null || true

    echo "[${PREFIX}] Compiling obfuscated bitcode to executable..."
    clang++ "build/test/test_${NAME}_obf.bc" -o "build/test/test_${NAME}_obf_exe"

    echo ""
    echo "--- RUNNING ${PREFIX} DOCTESTS ---"
    "./build/test/test_${NAME}_obf_exe"
    echo "----------------------------"
    echo "[${PREFIX}] All tests passed."
}

# ---------------------------------------------------------------------------
# 4. Dispatch: run the appropriate suite(s) based on the pass name
# ---------------------------------------------------------------------------
case "${PASSES}" in
    str*)
        run_suite "str"
        ;;
    cff*)
        run_suite "cff"
        ;;
    mba*|linear*)
        run_suite "target"
        ;;
    *)
        # Unknown pass — run all suites so we always have coverage
        run_suite "target"
        run_suite "cff"
        run_suite "str"
        ;;
esac

echo ""
echo "=== All selected test suites passed for passes: ${PASSES} ==="
