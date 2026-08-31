#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

int calculate_sum(int a, int b) {
    return a + b;
}

int calculate_diff(int a, int b) {
    return a - b;
}

int calculate_remainder(int a, int b) {
    return a % b;
}

TEST_CASE("Testing MBA Obfuscated Arithmetic") {
    SUBCASE("Addition works") {
        CHECK(calculate_sum(15, 27) == 42);
        CHECK(calculate_sum(-5, 5) == 0);
        CHECK(calculate_sum(0, 0) == 0);
    }
    
    SUBCASE("Subtraction works") {
        CHECK(calculate_diff(15, 27) == -12);
        CHECK(calculate_diff(10, 5) == 5);
        CHECK(calculate_diff(0, 0) == 0);
    }

    SUBCASE("Remainder works") {
        CHECK(calculate_remainder(42, 5) == 2);
        CHECK(calculate_remainder(10, 3) == 1);
        CHECK(calculate_remainder(7, 7) == 0);
    }
}
