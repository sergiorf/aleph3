#include "parser/Parser.hpp"
#include "expr/Expr.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace mathix;

TEST_CASE("Basic expressions are parsed correctly", "[parser]") {
    auto expr = parse_expression("2 + 3");
    REQUIRE(to_string(expr) == "2 + 3");
}

TEST_CASE("Parser correctly parses negative numbers in basic expressions", "[parser]") {
    auto expr = parse_expression("-2 + 3");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "-2 + 3");

    expr = parse_expression("2 + -3");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "2 + -3");

    expr = parse_expression("-2 + -3");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "-2 + -3");
}

TEST_CASE("Variables are parsed correctly", "[parser]") {
    auto expr = parse_expression("x + 1");
    REQUIRE(to_string(expr) == "x + 1");
}

TEST_CASE("Function calls are parsed correctly", "[parser]") {
    auto expr = parse_expression("sin[x]");
    REQUIRE(to_string(expr) == "sin[x]");
}

TEST_CASE("Parser correctly parses negative numbers in function calls", "[parser]") {
    auto expr = parse_expression("sin[-x]");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "sin[-x]");

    expr = parse_expression("max[-2, min[-3, -4]]");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "max[-2, min[-3, -4]]");
}

TEST_CASE("Nested function calls are parsed correctly", "[parser]") {
    auto expr = parse_expression("max[2, min[3, 4]]");
    REQUIRE(to_string(expr) == "max[2, min[3, 4]]");
}

TEST_CASE("Parser correctly parses power expressions", "[parser]") {
    auto expr = parse_expression("2^3");

    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "2^3");

    expr = parse_expression("-2^3");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "-2^3");

    expr = parse_expression("2^-3");
    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "2^-3");
}

TEST_CASE("Parser correctly parses exponential function", "[parser][exp]") {
    auto expr = parse_expression("exp[1]");

    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "exp[1]");
}

TEST_CASE("Parser correctly parses floor function", "[parser][floor]") {
    auto expr = parse_expression("floor[3.7]");

    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "floor[3.7]");
}

TEST_CASE("Parser correctly parses ceil function", "[parser][ceil]") {
    auto expr = parse_expression("ceil[3.2]");

    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "ceil[3.2]");
}

TEST_CASE("Parser correctly parses round function", "[parser][round]") {
    auto expr = parse_expression("round[3.5]");

    REQUIRE(expr != nullptr);
    REQUIRE(to_string(expr) == "round[3.5]");
}

TEST_CASE("Parser handles multiplication with number and symbol (2x)") {
    std::string input = "2x";

    ExprPtr expr = parse_expression(input);

    // Check that the parsed expression is a multiplication: "Times(2, x)"
    auto* func_call = std::get_if<FunctionCall>(&(*expr));
    REQUIRE(func_call != nullptr);
    REQUIRE(func_call->head == "Times");
    REQUIRE(func_call->args.size() == 2);

    auto* left = std::get_if<Number>(&(*func_call->args[0]));
    auto* right = std::get_if<Symbol>(&(*func_call->args[1]));

    REQUIRE(left != nullptr);
    REQUIRE(left->value == 2.0);
    REQUIRE(right != nullptr);
    REQUIRE(right->name == "x");
}

TEST_CASE("Parser handles negative numbers with multiplication (2x and -2x)") {
    std::string input = "-2x";

    ExprPtr expr = parse_expression(input);

    // Check that the parsed expression is a Negate followed by Times
    auto* func_call = std::get_if<FunctionCall>(&(*expr));
    REQUIRE(func_call != nullptr);
    REQUIRE(func_call->head == "Negate");

    auto* negate_expr = std::get_if<FunctionCall>(&(*func_call->args[0]));
    REQUIRE(negate_expr != nullptr);
    REQUIRE(negate_expr->head == "Times");
    REQUIRE(negate_expr->args.size() == 2);

    auto* left = std::get_if<Number>(&(*negate_expr->args[0]));
    auto* right = std::get_if<Symbol>(&(*negate_expr->args[1]));

    REQUIRE(left != nullptr);
    REQUIRE(left->value == 2.0);
    REQUIRE(right != nullptr);
    REQUIRE(right->name == "x");
}

TEST_CASE("Parser handles implicit multiplication with parentheses", "[parser]") {
    auto expr = parse_expression("2(3 + x)");
    REQUIRE(expr != nullptr);

    // Check that the parsed expression is a multiplication: "Times(2, Plus(3, x))"
    auto* func_call = std::get_if<FunctionCall>(&(*expr));
    REQUIRE(func_call != nullptr);
    REQUIRE(func_call->head == "Times");
    REQUIRE(func_call->args.size() == 2);

    auto* left = std::get_if<Number>(&(*func_call->args[0]));
    REQUIRE(left != nullptr);
    REQUIRE(left->value == 2.0);

    auto* right = std::get_if<FunctionCall>(&(*func_call->args[1]));
    REQUIRE(right != nullptr);
    REQUIRE(right->head == "Plus");
    REQUIRE(right->args.size() == 2);

    auto* right_left = std::get_if<Number>(&(*right->args[0]));
    auto* right_right = std::get_if<Symbol>(&(*right->args[1]));

    REQUIRE(right_left != nullptr);
    REQUIRE(right_left->value == 3.0);
    REQUIRE(right_right != nullptr);
    REQUIRE(right_right->name == "x");
}

void validate_function_definition(
    const ExprPtr& expr,
    const std::string& expected_name,
    const std::vector<std::string>& expected_params,
    bool expected_delayed,
    const std::string& expected_body_head
) {
    REQUIRE(expr != nullptr);

    // Check that the parsed expression is a function definition
    auto* func_def = std::get_if<FunctionDefinition>(&(*expr));
    REQUIRE(func_def != nullptr);
    REQUIRE(func_def->name == expected_name);
    REQUIRE(func_def->params == expected_params);
    REQUIRE(func_def->delayed == expected_delayed);

    // Check the body of the function
    auto* body = std::get_if<FunctionCall>(&(*func_def->body));
    REQUIRE(body != nullptr);
    REQUIRE(body->head == expected_body_head);
    REQUIRE(body->args.size() == 2);

    auto* left = std::get_if<FunctionCall>(&(*body->args[0]));
    auto* right = std::get_if<Number>(&(*body->args[1]));

    REQUIRE(left != nullptr);
    REQUIRE(left->head == "Pow");
    REQUIRE(left->args.size() == 2);

    auto* base = std::get_if<Symbol>(&(*left->args[0]));
    auto* exponent = std::get_if<Number>(&(*left->args[1]));

    REQUIRE(base != nullptr);
    REQUIRE(base->name == expected_params[0]); // The parameter name
    REQUIRE(exponent != nullptr);
    REQUIRE(exponent->value == 3.0);

    REQUIRE(right != nullptr);
    REQUIRE(right->value == 1.0);
}

TEST_CASE("Parser handles function definitions with delayed and immediate assignment", "[parser]") {
    // Test delayed assignment (:=)
    auto expr = parse_expression("f[a_] := a^3 - 1");
    validate_function_definition(expr, "f", { "a" }, true, "Minus");

    // Test immediate assignment (=)
    expr = parse_expression("f[a_] = a^3 - 1");
    validate_function_definition(expr, "f", { "a" }, false, "Minus");
}

