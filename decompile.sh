#!/bin/bash
# Decompile calculate_sum from the obfuscated binary using r2ghidra
# without loading ghidra everytime

BINARY="./build/bin/target_obf_exe"

if [[ ! -f "$BINARY" ]]; then
    echo "Binary not found: $BINARY — run ./run.sh first"
    exit 1
fi

echo "=== r2ghidra decompilation of calculate_sum in $BINARY ==="
r2 -A -q \
    -e log.level=0 \
    -c "axt @ sym.calculate_sum~[0]; s sym.calculate_sum; pdg" \
    "$BINARY"
