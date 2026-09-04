// test_cff.cpp — Correctness tests for Control Flow Flattening (CFF) obfuscation.
//
// These tests verify that every function in example/target_cff.c produces
// exactly the same outputs before and after the CFF pass is applied.
//
// The doctest framework is header-only (doctest.h in this directory).
// Build & run via test.sh with the "cff" pass name, e.g.:
//
//   ./test.sh cff
//
// The test binary returns 0 on success and non-zero on any failure,
// so the CI script will catch regressions automatically.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// ---------------------------------------------------------------------------
// Functions under test — mirrors example/target_cff.c exactly so that
// after CFF obfuscation the same definitions are compiled and tested.
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

int cff_popcount(unsigned int x) {
    int count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

int cff_grade(int score) {
    int band = score / 10;
    switch (band) {
        case 10:
        case 9:  return 4;
        case 8:  return 3;
        case 7:  return 2;
        case 6:  return 1;
        default: return 0;
    }
}

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

int cff_nested(int lo, int hi, int divisor) {
    if (divisor == 0) return -1;
    int count = 0;
    for (int i = lo; i < hi; ++i) {
        if (i % divisor == 0)
            ++count;
    }
    return count;
}

int cff_sign(int x) {
    if (x > 0) return 1;
    if (x < 0) return -1;
    return 0;
}

// ===========================================================================
// TEST SUITE 1 — cff_classify
// ===========================================================================
TEST_CASE("CFF: if/else chain — cff_classify") {
    // Boundary on the negative side
    SUBCASE("negative inputs always return -1") {
        CHECK(cff_classify(-1)    == -1);
        CHECK(cff_classify(-100)  == -1);
        CHECK(cff_classify(-9999) == -1);
    }

    // Exact zero
    SUBCASE("zero returns 0") {
        CHECK(cff_classify(0) == 0);
    }

    // [1, 9] — single-digit positives
    SUBCASE("single-digit positives return 1") {
        CHECK(cff_classify(1)  == 1);
        CHECK(cff_classify(5)  == 1);
        CHECK(cff_classify(9)  == 1);
    }

    // [10, 99] — two-digit range
    SUBCASE("two-digit range returns 2") {
        CHECK(cff_classify(10)  == 2);
        CHECK(cff_classify(42)  == 2);
        CHECK(cff_classify(99)  == 2);
    }

    // [100, …) — three+ digits
    SUBCASE("large values return 3") {
        CHECK(cff_classify(100)   == 3);
        CHECK(cff_classify(200)   == 3);
        CHECK(cff_classify(99999) == 3);
    }

    // Branch boundaries — CFF must not collapse adjacent branches
    SUBCASE("boundary values hit the correct branch") {
        CHECK(cff_classify(-1) == -1); // last negative
        CHECK(cff_classify(0)  ==  0); // exact zero
        CHECK(cff_classify(1)  ==  1); // first positive < 10
        CHECK(cff_classify(9)  ==  1); // last < 10
        CHECK(cff_classify(10) ==  2); // first >= 10
        CHECK(cff_classify(99) ==  2); // last < 100
        CHECK(cff_classify(100)==  3); // first >= 100
    }
}

// ===========================================================================
// TEST SUITE 2 — cff_fibonacci
// ===========================================================================
TEST_CASE("CFF: iterative loop — cff_fibonacci") {
    SUBCASE("base cases") {
        CHECK(cff_fibonacci(0)  == 0);
        CHECK(cff_fibonacci(1)  == 1);
        CHECK(cff_fibonacci(-1) == 0); // non-positive guard
        CHECK(cff_fibonacci(-5) == 0);
    }

    SUBCASE("small values") {
        CHECK(cff_fibonacci(2)  == 1);
        CHECK(cff_fibonacci(3)  == 2);
        CHECK(cff_fibonacci(4)  == 3);
        CHECK(cff_fibonacci(5)  == 5);
        CHECK(cff_fibonacci(6)  == 8);
        CHECK(cff_fibonacci(7)  == 13);
    }

    SUBCASE("larger values — loop executes many iterations") {
        CHECK(cff_fibonacci(10) == 55);
        CHECK(cff_fibonacci(15) == 610);
        CHECK(cff_fibonacci(20) == 6765);
    }

    SUBCASE("sequential correctness — fib(n) == fib(n-1) + fib(n-2)") {
        for (int n = 2; n <= 18; ++n) {
            CHECK(cff_fibonacci(n) == cff_fibonacci(n - 1) + cff_fibonacci(n - 2));
        }
    }
}

// ===========================================================================
// TEST SUITE 3 — cff_popcount
// ===========================================================================
TEST_CASE("CFF: bit-manipulation while-loop — cff_popcount") {
    SUBCASE("edge cases") {
        CHECK(cff_popcount(0) == 0); // zero bits set
        CHECK(cff_popcount(1) == 1); // single bit
    }

    SUBCASE("powers of two — exactly one bit set") {
        CHECK(cff_popcount(1u   << 0)  == 1);
        CHECK(cff_popcount(1u   << 7)  == 1);
        CHECK(cff_popcount(1u   << 15) == 1);
        CHECK(cff_popcount(1u   << 31) == 1);
    }

    SUBCASE("all-ones patterns") {
        CHECK(cff_popcount(0xFFu)       == 8);   // 8 bits
        CHECK(cff_popcount(0xFFFFu)     == 16);  // 16 bits
        CHECK(cff_popcount(0xFFFFFFFFu) == 32);  // 32 bits
    }

    SUBCASE("alternating patterns") {
        CHECK(cff_popcount(0xAAAAAAAAu) == 16); // 1010…
        CHECK(cff_popcount(0x55555555u) == 16); // 0101…
    }

    SUBCASE("specific known values") {
        CHECK(cff_popcount(255u)  == 8);
        CHECK(cff_popcount(1023u) == 10);
        CHECK(cff_popcount(7u)    == 3);
        CHECK(cff_popcount(6u)    == 2);
    }
}

// ===========================================================================
// TEST SUITE 4 — cff_grade
// ===========================================================================
TEST_CASE("CFF: switch statement — cff_grade") {
    SUBCASE("A (90-100)") {
        CHECK(cff_grade(90)  == 4);
        CHECK(cff_grade(95)  == 4);
        CHECK(cff_grade(100) == 4);
    }

    SUBCASE("B (80-89)") {
        CHECK(cff_grade(80) == 3);
        CHECK(cff_grade(83) == 3);
        CHECK(cff_grade(89) == 3);
    }

    SUBCASE("C (70-79)") {
        CHECK(cff_grade(70) == 2);
        CHECK(cff_grade(75) == 2);
        CHECK(cff_grade(79) == 2);
    }

    SUBCASE("D (60-69)") {
        CHECK(cff_grade(60) == 1);
        CHECK(cff_grade(65) == 1);
        CHECK(cff_grade(69) == 1);
    }

    SUBCASE("F (below 60 and invalid)") {
        CHECK(cff_grade(59) == 0);
        CHECK(cff_grade(50) == 0);
        CHECK(cff_grade(0)  == 0);
        CHECK(cff_grade(-1) == 0); // negative scores hit default
    }

    SUBCASE("switch fall-through: 100 and 90 both map to A") {
        CHECK(cff_grade(100) == cff_grade(90));
    }
}

// ===========================================================================
// TEST SUITE 5 — cff_collatz
// ===========================================================================
TEST_CASE("CFF: Collatz conjecture — while-loop with data-dependent steps") {
    SUBCASE("guard: non-positive inputs return 0") {
        CHECK(cff_collatz(0)  == 0);
        CHECK(cff_collatz(-1) == 0);
        CHECK(cff_collatz(-7) == 0);
    }

    SUBCASE("n=1 halts immediately (0 steps)") {
        CHECK(cff_collatz(1) == 0);
    }

    SUBCASE("small known sequences") {
        // 2 -> 1  (1 step)
        CHECK(cff_collatz(2)  == 1);
        // 4 -> 2 -> 1  (2 steps)
        CHECK(cff_collatz(4)  == 2);
        // 3 -> 10 -> 5 -> 16 -> 8 -> 4 -> 2 -> 1  (7 steps)
        CHECK(cff_collatz(3)  == 7);
        // 6 -> 3 -> 10 -> 5 -> 16 -> 8 -> 4 -> 2 -> 1  (8 steps)
        CHECK(cff_collatz(6)  == 8);
        // 16 -> 8 -> 4 -> 2 -> 1  (4 steps)
        CHECK(cff_collatz(16) == 4);
    }

    SUBCASE("longer sequences with many iterations") {
        // 27 is famous for taking 111 steps
        CHECK(cff_collatz(27) == 111);
    }

    SUBCASE("even numbers halve quickly") {
        // Powers of two: n = 2^k takes exactly k steps
        CHECK(cff_collatz(2)  == 1);
        CHECK(cff_collatz(4)  == 2);
        CHECK(cff_collatz(8)  == 3);
        CHECK(cff_collatz(32) == 5);
    }
}

// ===========================================================================
// TEST SUITE 6 — cff_nested
// ===========================================================================
TEST_CASE("CFF: nested if inside for-loop — cff_nested") {
    SUBCASE("zero divisor returns -1 (guard branch)") {
        CHECK(cff_nested(0,  10, 0) == -1);
        CHECK(cff_nested(-5, 20, 0) == -1);
    }

    SUBCASE("divisor larger than range — no multiples") {
        CHECK(cff_nested(1, 5, 100) == 0);
    }

    SUBCASE("divisor = 1 — every integer is a multiple") {
        CHECK(cff_nested(0, 10, 1) == 10);
        CHECK(cff_nested(5, 15, 1) == 10);
    }

    SUBCASE("divisor = 2 — half the integers in even-length range") {
        // [0, 10): 0,2,4,6,8 → 5
        CHECK(cff_nested(0, 10, 2) == 5);
        // [1, 10): 2,4,6,8 → 4
        CHECK(cff_nested(1, 10, 2) == 4);
    }

    SUBCASE("divisor = 3") {
        // [0, 10): 0,3,6,9 → 4
        CHECK(cff_nested(0, 10, 3) == 4);
        // [1, 12): 3,6,9 → 3
        CHECK(cff_nested(1, 12, 3) == 3);
    }

    SUBCASE("empty range — loop body never executes") {
        CHECK(cff_nested(5, 5, 2)  == 0);
        CHECK(cff_nested(10, 5, 3) == 0); // lo > hi
    }

    SUBCASE("negative range") {
        // [-6, 0): -6,-3 → 2
        CHECK(cff_nested(-6, 0, 3) == 2);
    }
}

// ===========================================================================
// TEST SUITE 7 — cff_sign
// ===========================================================================
TEST_CASE("CFF: simple conditional — cff_sign") {
    SUBCASE("positive inputs return +1") {
        CHECK(cff_sign(1)     ==  1);
        CHECK(cff_sign(42)    ==  1);
        CHECK(cff_sign(99999) ==  1);
    }

    SUBCASE("zero returns 0") {
        CHECK(cff_sign(0) == 0);
    }

    SUBCASE("negative inputs return -1") {
        CHECK(cff_sign(-1)     == -1);
        CHECK(cff_sign(-42)    == -1);
        CHECK(cff_sign(-99999) == -1);
    }

    // sign(x) * sign(-x) == -1 for all non-zero x
    SUBCASE("antisymmetry: sign(-x) == -sign(x)") {
        for (int x : {1, 5, 100, 1000}) {
            CHECK(cff_sign(-x) == -cff_sign(x));
        }
    }
}

// ===========================================================================
// TEST SUITE 8 — cross-function integration smoke tests
// ===========================================================================
TEST_CASE("CFF: integration — combined function calls are consistent") {
    // Fibonacci popcount: popcount of fib(n) should be stable across
    // obfuscation passes because both functions must be correct.
    SUBCASE("popcount(fibonacci(n)) is deterministic") {
        CHECK(cff_popcount((unsigned)cff_fibonacci(10)) == cff_popcount(55));
        CHECK(cff_popcount((unsigned)cff_fibonacci(8))  == cff_popcount(21));
    }

    // Grade of a score computed from classify output
    SUBCASE("classify then grade are independently correct") {
        // classify(95) == 2, grade(95) == 4 — independent branches
        CHECK(cff_classify(95) == 2);
        CHECK(cff_grade(95)    == 4);
    }

    // sign(collatz(n)) is always +1 for n > 1 (Collatz always > 0)
    SUBCASE("sign(collatz(n)) == 1 for n > 1") {
        for (int n : {2, 3, 6, 10, 27}) {
            CHECK(cff_sign(cff_collatz(n)) == 1);
        }
    }
}
