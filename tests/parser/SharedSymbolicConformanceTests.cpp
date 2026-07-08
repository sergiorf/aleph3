#include "expr/Expr.hpp"
#include "parser/Parser.hpp"
#include "syntax/SymbolicLowering.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace aleph3;

namespace {

ExprPtr parse_with_legacy_parser(const std::string& source) {
    Parser parser(source);
    return try_make_complex(parser.parse());
}

void require_shared_matches_legacy(const std::string& source) {
    CAPTURE(source);

    const auto legacy = parse_with_legacy_parser(source);
    const auto shared = syntax::parse_symbolic_source(source);

    REQUIRE(shared.ok());
    REQUIRE(to_string(shared.expr) == to_string(legacy));
}

}  // namespace

TEST_CASE("Shared symbolic lowering matches legacy implicit multiplication", "[parser][syntax][conformance]") {
    for (const std::string source : {
             "2x",
             "-2x",
             "2(3 + x)",
             "2x + 3y",
             "f[x]2",
         }) {
        require_shared_matches_legacy(source);
    }
}

TEST_CASE("Shared symbolic lowering matches legacy exact rational forms", "[parser][syntax][conformance]") {
    for (const std::string source : {
             "1/2",
             "-1/2",
             "-2/-3",
             "1/-2",
             "2/3 / 3/4",
             "2/3*x",
             "x/-3x",
             "y/2y",
         }) {
        require_shared_matches_legacy(source);
    }
}

TEST_CASE("Shared symbolic lowering matches legacy complex shorthand", "[parser][syntax][conformance]") {
    for (const std::string source : {
             "I",
             "-I",
             "3 + 4*I",
             "2 - 5*I",
             "Complex[3,4]",
         }) {
        require_shared_matches_legacy(source);
    }
}

TEST_CASE("Shared symbolic lowering matches legacy symbolic syntax", "[parser][syntax][conformance]") {
    for (const std::string source : {
             "x = 2",
             "f[x_] := x^2",
             "f[a_] -> g[a]",
             "{1, {2, 3}, x}",
             "\"Hello\" <> \" \" <> \"World\"",
             "True && False || x",
         }) {
        require_shared_matches_legacy(source);
    }
}

TEST_CASE("Shared symbolic lowering matches legacy function call unary minus", "[parser][syntax][conformance]") {
    for (const std::string source : {
             "Abs[-7]",
             "Tan[-2]",
             "max[-2, min[-3, -4]]",
         }) {
        require_shared_matches_legacy(source);
    }
}
