#include "expr/Expr.hpp"
#include "expr/FullForm.hpp"
#include <catch2/catch_all.hpp>

using namespace aleph3;

TEST_CASE("to_fullform basic types") {
    REQUIRE(to_fullform(make_expr<Number>(42.0)) == "42");
    REQUIRE(to_fullform(make_expr<Symbol>("x")) == "x");
    REQUIRE(to_fullform(make_expr<String>("hello")) == "\"hello\"");
    REQUIRE(to_fullform(make_expr<Boolean>(true)) == "True");
    REQUIRE(to_fullform(make_expr<Boolean>(false)) == "False");
}

TEST_CASE("to_fullform rational") {
    REQUIRE(to_fullform(make_expr<Rational>(3, 4)) == "Rational[3, 4]");
}

TEST_CASE("to_fullform function calls") {
    ExprPtr expr = make_expr<FunctionCall>("Plus", std::vector<ExprPtr>{
        make_expr<Number>(2),
        make_expr<FunctionCall>("Times", std::vector<ExprPtr>{
            make_expr<Number>(3),
            make_expr<Symbol>("x")
        })
    });
    REQUIRE(to_fullform(expr) == "Plus[2, Times[3, x]]");
}

TEST_CASE("to_fullform assignment") {
    ExprPtr expr = make_expr<Assignment>("y", make_expr<Number>(5));
    REQUIRE(to_fullform(expr) == "Set[y, 5]");
}

TEST_CASE("to_fullform rule") {
    ExprPtr expr = make_expr<Rule>(make_expr<Symbol>("x"), make_expr<Number>(1));
    REQUIRE(to_fullform(expr) == "Rule[x, 1]");
}

TEST_CASE("to_fullform nested") {
    ExprPtr expr = make_expr<FunctionCall>("Divide", std::vector<ExprPtr>{
        make_expr<FunctionCall>("Times", std::vector<ExprPtr>{
            make_expr<Number>(-1),
            make_expr<Symbol>("a")
        }),
        make_expr<Symbol>("bC")
    });
    REQUIRE(to_fullform(expr) == "Divide[Times[-1, a], bC]");
}

TEST_CASE("to_fullform function definition") {
    // g[x_, y_] := x + y
    std::vector<Parameter> params = {
        Parameter("x", nullptr),
        Parameter("y", nullptr)
    };
    ExprPtr body = make_expr<FunctionCall>("Plus", std::vector<ExprPtr>{
        make_expr<Symbol>("x"),
            make_expr<Symbol>("y")
    });
    ExprPtr expr = make_expr<FunctionDefinition>("g", params, body, true);
    REQUIRE(
        to_fullform(expr) ==
        "FunctionDefinition[g, List[Parameter[x], Parameter[y]], Plus[x, y], True]"
    );
}