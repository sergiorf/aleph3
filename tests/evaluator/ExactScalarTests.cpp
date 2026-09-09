#include "kernel/ExactScalar.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <stdexcept>

using namespace aleph3::kernel;

TEST_CASE("ExactInteger preserves large decimal values", "[kernel][exact-scalar]") {
    const auto large = ExactInteger::from_decimal_string("123456789012345678901234567890");
    const auto negative = ExactInteger::from_decimal_string("-123456789012345678901234567890");

    REQUIRE(large.to_string() == "123456789012345678901234567890");
    REQUIRE(negative.to_string() == "-123456789012345678901234567890");
    REQUIRE((large + ExactInteger(10)).to_string() == "123456789012345678901234567900");
    REQUIRE((large * ExactInteger(10)).to_string() == "1234567890123456789012345678900");
    REQUIRE((-negative).to_string() == large.to_string());
}

TEST_CASE("ExactInteger reports bounded int64 conversion", "[kernel][exact-scalar]") {
    REQUIRE(ExactInteger(std::numeric_limits<int64_t>::min()).to_int64()
            == std::numeric_limits<int64_t>::min());
    REQUIRE(ExactInteger(std::numeric_limits<int64_t>::max()).to_int64()
            == std::numeric_limits<int64_t>::max());

    REQUIRE_FALSE(ExactInteger::from_decimal_string("-9223372036854775809").to_int64().has_value());
    REQUIRE_FALSE(ExactInteger::from_decimal_string("9223372036854775808").to_int64().has_value());
}

TEST_CASE("ExactInteger rejects malformed decimal strings", "[kernel][exact-scalar]") {
    REQUIRE_THROWS_AS(ExactInteger::from_decimal_string(""), std::invalid_argument);
    REQUIRE_THROWS_AS(ExactInteger::from_decimal_string("-"), std::invalid_argument);
    REQUIRE_THROWS_AS(ExactInteger::from_decimal_string("12.0"), std::invalid_argument);
}

TEST_CASE("ExactRational normalizes arbitrary precision values", "[kernel][exact-scalar]") {
    const ExactRational rational(
        ExactInteger::from_decimal_string("100000000000000000000000000000"),
        ExactInteger::from_decimal_string("-25000000000000000000000000000"));

    REQUIRE(rational.numerator().to_string() == "-4");
    REQUIRE(rational.denominator().to_string() == "1");
    REQUIRE(rational.to_string() == "-4");
    REQUIRE(rational.to_bounded().value() == std::pair<int64_t, int64_t>{-4, 1});
}

TEST_CASE("ExactRational preserves large rational arithmetic", "[kernel][exact-scalar]") {
    const ExactRational left(
        ExactInteger::from_decimal_string("9223372036854775808"),
        ExactInteger(2));
    const ExactRational right(
        ExactInteger::from_decimal_string("9223372036854775808"),
        ExactInteger(2));

    const ExactRational sum = left + right;
    REQUIRE(sum.to_string() == "9223372036854775808");
    REQUIRE_FALSE(sum.to_bounded().has_value());

    const ExactRational product = ExactRational(
        ExactInteger::from_decimal_string("100000000000000000000"),
        ExactInteger::from_decimal_string("300000000000000000000"))
        * ExactRational(ExactInteger(9), ExactInteger(10));
    REQUIRE(product.to_string() == "3/10");
}

TEST_CASE("ExactRational compares without narrowing", "[kernel][exact-scalar]") {
    const ExactRational left(
        ExactInteger::from_decimal_string("9223372036854775808"),
        ExactInteger::from_decimal_string("9223372036854775809"));
    const ExactRational right(
        ExactInteger::from_decimal_string("9223372036854775809"),
        ExactInteger::from_decimal_string("9223372036854775810"));

    REQUIRE(compare(left, right) < 0);
    REQUIRE(left < right);
}

TEST_CASE("ExactRational rejects zero denominators", "[kernel][exact-scalar]") {
    REQUIRE_THROWS_AS(ExactRational(ExactInteger(1), ExactInteger(0)), std::domain_error);
    REQUIRE_THROWS_AS(ExactRational(ExactInteger(1), ExactInteger(2)) / ExactRational(), std::domain_error);
}
