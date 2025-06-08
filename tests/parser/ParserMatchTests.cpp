#include "parser/Parser.hpp"
#include "ParserTestHelper.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_all.hpp>

using namespace aleph3;

TEST_CASE("Parser matches ASCII character") {
    Parser p("abc");
    REQUIRE(ParserTestHelper::match(p, 'a'));
    REQUIRE(ParserTestHelper::match(p, 'b'));
    REQUIRE_FALSE(ParserTestHelper::match(p, 'z'));
    REQUIRE(ParserTestHelper::match(p, 'c'));
    REQUIRE_FALSE(ParserTestHelper::match(p, 'c')); // already advanced
}

TEST_CASE("Parser does not match wrong character") {
    Parser p("xyz");
    REQUIRE_FALSE(ParserTestHelper::match(p, 'a'));
    REQUIRE(ParserTestHelper::match(p, 'x'));
    REQUIRE_FALSE(ParserTestHelper::match(p, 'z'));
}

TEST_CASE("Parser does not match Unicode character with ASCII char") {
    // alpha = U+03B1, UTF-8: CE B1
    std::string alpha = "\xCE\xB1";
    Parser p(alpha + "x");
    REQUIRE_FALSE(ParserTestHelper::match(p, 'a'));      // ASCII 'a'
    REQUIRE_FALSE(ParserTestHelper::match(p, '\xB1'));   // single byte, not correct for alpha
    REQUIRE_FALSE(ParserTestHelper::match(p, '\xCE'));   // single byte, not correct for alpha
}

TEST_CASE("Parser matches after advancing") {
    Parser p("abc");
    REQUIRE(ParserTestHelper::match(p, 'a'));
    REQUIRE(ParserTestHelper::match(p, 'b'));
    REQUIRE(ParserTestHelper::match(p, 'c'));
    REQUIRE_FALSE(ParserTestHelper::match(p, 'd'));
}