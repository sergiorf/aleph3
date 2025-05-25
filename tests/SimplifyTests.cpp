#include "transforms/Transforms.hpp"
#include "expr/ExprUtils.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aleph3;

TEST_CASE("Simplify relational functions to True/False", "[simplify]") {
    auto expr = make_expr<FunctionCall>("Equal", std::vector<ExprPtr>{
        make_number(2), make_number(2)
    });
    REQUIRE(to_string(simplify(expr)) == "True");

    expr = make_expr<FunctionCall>("NotEqual", std::vector<ExprPtr>{
        make_number(2), make_number(3)
    });
    REQUIRE(to_string(simplify(expr)) == "True");

    expr = make_expr<FunctionCall>("Less", std::vector<ExprPtr>{
        make_number(2), make_number(3)
    });
    REQUIRE(to_string(simplify(expr)) == "True");

    expr = make_expr<FunctionCall>("Greater", std::vector<ExprPtr>{
        make_number(3), make_number(2)
    });
    REQUIRE(to_string(simplify(expr)) == "True");

    expr = make_expr<FunctionCall>("LessEqual", std::vector<ExprPtr>{
        make_number(2), make_number(2)
    });
    REQUIRE(to_string(simplify(expr)) == "True");

    expr = make_expr<FunctionCall>("GreaterEqual", std::vector<ExprPtr>{
        make_number(3), make_number(2)
    });
    REQUIRE(to_string(simplify(expr)) == "True");
}