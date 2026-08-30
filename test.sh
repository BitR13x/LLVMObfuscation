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
# 3. MBA tests — tests/test_target.cpp
# ---------------------------------------------------------------------------
run_mba_tests() {
    echo ""
    echo "[MBA] Compiling tests/test_target.cpp to LLVM Bitcode..."
    clang++ -std=c++11 -O0 -Xclang -disable-O0-optnone \
        -emit-llvm -c tests/test_target.cpp \
        -o build/test/test_target.bc

    echo "[MBA] Running obfuscation pass (${PASSES})..."
    opt -load-pass-plugin=./build/libObfuscationPass.so \
        -passes="${PASSES}" \
        build/test/test_target.bc \
        -o build/test/test_target_obf.bc

    llvm-dis build/test/test_target_obf.bc -o build/test/IR/test_target_obf.ll 2>/dev/null || true

    echo "[MBA] Compiling obfuscated bitcode to executable..."
    clang++ build/test/test_target_obf.bc -o build/test/test_target_obf_exe

    echo ""
    echo "--- RUNNING MBA DOCTESTS ---"
    ./build/test/test_target_obf_exe
    echo "----------------------------"
    echo "[MBA] All tests passed."
}

# ---------------------------------------------------------------------------
# 4. CFF tests — tests/test_cff.cpp
# ---------------------------------------------------------------------------
run_cff_tests() {
    echo ""
    echo "[CFF] Compiling tests/test_cff.cpp to LLVM Bitcode..."
    clang++ -std=c++11 -O0 -Xclang -disable-O0-optnone \
        -emit-llvm -c tests/test_cff.cpp \
        -o build/test/test_cff.bc

    echo "[CFF] Running obfuscation pass (${PASSES})..."
    opt -load-pass-plugin=./build/libObfuscationPass.so \
        -passes="${PASSES}" \
        build/test/test_cff.bc \
        -o build/test/test_cff_obf.bc

    llvm-dis build/test/test_cff_obf.bc -o build/test/IR/test_cff_obf.ll 2>/dev/null || true

    echo "[CFF] Compiling obfuscated bitcode to executable..."
    clang++ build/test/test_cff_obf.bc -o build/test/test_cff_obf_exe

    echo ""
    echo "--- RUNNING CFF DOCTESTS ---"
    ./build/test/test_cff_obf_exe
    echo "----------------------------"
    echo "[CFF] All tests passed."
}

# ---------------------------------------------------------------------------
# 5. Dispatch: run the appropriate suite(s) based on the pass name
# ---------------------------------------------------------------------------
case "${PASSES}" in
    cff*)
        run_cff_tests
        ;;
    mba*|linear*)
        run_mba_tests
        ;;
    *)
        # Unknown pass — run both suites so we always have coverage
        run_mba_tests
        run_cff_tests
        ;;
esac

echo ""
echo "=== All selected test suites passed for passes: ${PASSES} ==="
