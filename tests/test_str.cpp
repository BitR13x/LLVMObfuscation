#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <string>

const char* get_secret() {
    return "SuperSecret123456789";
}

const char* get_short() {
    return "Hi";
}

TEST_CASE("String Obfuscation Tests") {
    SUBCASE("Long string decoding") {
        CHECK(std::string(get_secret()) == "SuperSecret123456789");
    }
    SUBCASE("Short string decoding") {
        CHECK(std::string(get_short()) == "Hi");
    }
    SUBCASE("Inline literal decoding") {
        CHECK(std::string("InlineLiteral") == "InlineLiteral");
    }
}
