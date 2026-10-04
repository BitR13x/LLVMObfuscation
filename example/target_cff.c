// target_cff.c — Example program for Control Flow Flattening (CFF) obfuscation.
//
// Each function exercises a different control-flow pattern so the CFF pass
// has realistic material to flatten:
//
//   cff_classify    — multi-branch if/else chain
//   cff_fibonacci   — iterative loop with early-exit
//   cff_popcount    — tight bit-manipulation loop
//   cff_grade       — switch statement
//   cff_collatz     — while-loop with non-trivial iteration count
//   cff_nested      — nested if inside a for-loop
//   cff_sign        — ternary-style conditional
//
// After CFF all of these should produce identical outputs to the originals.

#include <stdio.h>

// ---------------------------------------------------------------------------
// 1. Multi-branch if/else — tests flattening of linear condition chains
// ---------------------------------------------------------------------------
int cff_classify(int x) {
    if (x < 0)
        return -1;
    else if (x == 0)
        return 0;
    else if (x < 10)
        return 1;
    else if (x < 100)
        return 2;
    else
        return 3;
}

// ---------------------------------------------------------------------------
// 2. Iterative Fibonacci — tests loop with loop-carried state & early-exit
// ---------------------------------------------------------------------------
int cff_fibonacci(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;

    int a = 0, b = 1;
    for (int i = 2; i <= n; ++i) {
        int tmp = a + b;
        a = b;
        b = tmp;
    }
    return b;
}

// ---------------------------------------------------------------------------
// 3. Population count — tests a tight counting loop over bits
// ---------------------------------------------------------------------------
int cff_popcount(unsigned int x) {
    int count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

// ---------------------------------------------------------------------------
// 4. Grade classifier — tests switch statement flattening
// ---------------------------------------------------------------------------
// Returns: 4='A', 3='B', 2='C', 1='D', 0='F'
int cff_grade(int score) {
    int band = score / 10; // 0–10
    switch (band) {
        case 10:
        case 9:  return 4; // A
        case 8:  return 3; // B
        case 7:  return 2; // C
        case 6:  return 1; // D
        default: return 0; // F
    }
}

// ---------------------------------------------------------------------------
// 5. Collatz step count — tests while-loop with unpredictable iteration count
// ---------------------------------------------------------------------------
int cff_collatz(int n) {
    if (n <= 0) return 0;
    int steps = 0;
    while (n != 1) {
        if (n % 2 == 0)
            n /= 2;
        else
            n = 3 * n + 1;
        ++steps;
    }
    return steps;
}

// ---------------------------------------------------------------------------
// 6. Nested control flow — for-loop with inner if/else
// ---------------------------------------------------------------------------
// Counts integers in [lo, hi) that are divisible by divisor.
int cff_nested(int lo, int hi, int divisor) {
    if (divisor == 0) return -1;
    int count = 0;
    for (int i = lo; i < hi; ++i) {
        if (i % divisor == 0)
            ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
// 7. Sign function — tests simple conditional (ternary-equivalent)
// ---------------------------------------------------------------------------
int cff_sign(int x) {
    if (x > 0) return 1;
    if (x < 0) return -1;
    return 0;
}

// ---------------------------------------------------------------------------
// main — smoke-test print so the binary is self-verifying
// ---------------------------------------------------------------------------
int main(void) {
    printf("classify(-5)   = %d (expect -1)\n", cff_classify(-5));
    printf("classify(0)    = %d (expect  0)\n", cff_classify(0));
    printf("classify(7)    = %d (expect  1)\n", cff_classify(7));
    printf("classify(42)   = %d (expect  2)\n", cff_classify(42));
    printf("classify(200)  = %d (expect  3)\n", cff_classify(200));

    printf("fibonacci(0)   = %d (expect  0)\n", cff_fibonacci(0));
    printf("fibonacci(1)   = %d (expect  1)\n", cff_fibonacci(1));
    printf("fibonacci(10)  = %d (expect 55)\n", cff_fibonacci(10));

    printf("popcount(0)    = %d (expect  0)\n", cff_popcount(0));
    printf("popcount(255)  = %d (expect  8)\n", cff_popcount(255));
    printf("popcount(1023) = %d (expect 10)\n", cff_popcount(1023));

    printf("grade(95)      = %d (expect  4)\n", cff_grade(95));
    printf("grade(83)      = %d (expect  3)\n", cff_grade(83));
    printf("grade(55)      = %d (expect  0)\n", cff_grade(55));

    printf("collatz(1)     = %d (expect  0)\n", cff_collatz(1));
    printf("collatz(6)     = %d (expect  8)\n", cff_collatz(6));
    printf("collatz(27)    = %d (expect111)\n", cff_collatz(27));

    printf("nested(0,10,3) = %d (expect  4)\n", cff_nested(0, 10, 3));
    printf("nested(1,10,2) = %d (expect  4)\n", cff_nested(1, 10, 2));
    printf("nested(0,10,0) = %d (expect -1)\n", cff_nested(0, 10, 0));

    printf("sign(-99)      = %d (expect -1)\n", cff_sign(-99));
    printf("sign(0)        = %d (expect  0)\n", cff_sign(0));
    printf("sign(42)       = %d (expect  1)\n", cff_sign(42));

    return 0;
}
