#include "parser/Parser.hpp"
#include "expr/Expr.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace mathix;

TEST_CASE("Parser handles simple function definition", "[parser][functions]") {
    auto expr = parse_expression("f[x_] := x^2");
    REQUIRE(expr != nullptr);

    auto* def = std::get_if<FunctionDefinition>(&(*expr));
    REQUIRE(def != nullptr);
    REQUIRE(def->name == "f");
    REQUIRE(def->params.size() == 1);
    REQUIRE(def->params[0].name == "x");
    REQUIRE(def->delayed == true);
}

TEST_CASE("Parser handles function definition with multiple parameters", "[parser][functions]") {
    auto expr = parse_expression("g[x_, y_] := x + y");
    REQUIRE(expr != nullptr);

    auto* def = std::get_if<FunctionDefinition>(&(*expr));
    REQUIRE(def != nullptr);
    REQUIRE(def->name == "g");
    REQUIRE(def->params.size() == 2);
    REQUIRE(def->params[0].name == "x");
    REQUIRE(def->params[1].name == "y");
}

TEST_CASE("Parser handles function definition with default argument", "[parser][functions]") {
    auto expr = parse_expression("h[x_, y_:2] := x + y");
    REQUIRE(expr != nullptr);

    auto* def = std::get_if<FunctionDefinition>(&(*expr));
    REQUIRE(def != nullptr);
    REQUIRE(def->name == "h");
    REQUIRE(def->params.size() == 2);
    REQUIRE(def->params[0].name == "x");
    REQUIRE(def->params[1].name == "y");
    // You may want to check for default values if your AST supports it
}

TEST_CASE("Parser handles function call with all arguments", "[parser][functions]") {
    auto expr = parse_expression("f[3]");
    REQUIRE(expr != nullptr);

    auto* call = std::get_if<FunctionCall>(&(*expr));
    REQUIRE(call != nullptr);
    REQUIRE(call->head == "f");
    REQUIRE(call->args.size() == 1);
    REQUIRE(std::holds_alternative<Number>(*call->args[0]));
    REQUIRE(std::get<Number>(*call->args[0]).value == 3.0);
}

TEST_CASE("Parser handles function call with missing optional argument", "[parser][functions]") {
    auto expr = parse_expression("h[5]");
    REQUIRE(expr != nullptr);

    auto* call = std::get_if<FunctionCall>(&(*expr));
    REQUIRE(call != nullptr);
    REQUIRE(call->head == "h");
    REQUIRE(call->args.size() == 1);
    REQUIRE(std::holds_alternative<Number>(*call->args[0]));
    REQUIRE(std::get<Number>(*call->args[0]).value == 5.0);
}

TEST_CASE("Parser handles function call with all arguments including optional", "[parser][functions]") {
    auto expr = parse_expression("h[5, 10]");
    REQUIRE(expr != nullptr);

    auto* call = std::get_if<FunctionCall>(&(*expr));
    REQUIRE(call != nullptr);
    REQUIRE(call->head == "h");
    REQUIRE(call->args.size() == 2);
    REQUIRE(std::holds_alternative<Number>(*call->args[0]));
    REQUIRE(std::get<Number>(*call->args[0]).value == 5.0);
    REQUIRE(std::holds_alternative<Number>(*call->args[1]));
    REQUIRE(std::get<Number>(*call->args[1]).value == 10.0);
}

TEST_CASE("Parser handles recursive function definitions", "[parser]") {
    std::string input = "factorial[n_] := If[n == 0, 1, n * factorial[n - 1]]";
    auto expr = mathix::parse_expression(input);

    REQUIRE(std::holds_alternative<FunctionDefinition>(*expr));
    auto func_def = std::get<FunctionDefinition>(*expr);

    REQUIRE(func_def.name == "factorial");
    REQUIRE(func_def.params.size() == 1);
    REQUIRE(func_def.params[0].name == "n");

    REQUIRE(std::holds_alternative<FunctionCall>(*func_def.body));
    auto if_call = std::get<FunctionCall>(*func_def.body);
    REQUIRE(if_call.head == "If");
    REQUIRE(if_call.args.size() == 3);

    REQUIRE(std::holds_alternative<FunctionCall>(*if_call.args[0]));
    auto condition = std::get<FunctionCall>(*if_call.args[0]);
    REQUIRE(condition.head == "Equal");
    REQUIRE(condition.args.size() == 2);
    REQUIRE(std::get<Symbol>(*condition.args[0]).name == "n");
    REQUIRE(std::get<Number>(*condition.args[1]).value == 0.0);

    REQUIRE(std::holds_alternative<Number>(*if_call.args[1]));
    REQUIRE(std::get<Number>(*if_call.args[1]).value == 1.0);

    REQUIRE(std::holds_alternative<FunctionCall>(*if_call.args[2]));
    auto false_branch = std::get<FunctionCall>(*if_call.args[2]);
    REQUIRE(false_branch.head == "Times");
    REQUIRE(false_branch.args.size() == 2);
    REQUIRE(std::get<Symbol>(*false_branch.args[0]).name == "n");

    auto recursive_call = std::get<FunctionCall>(*false_branch.args[1]);
    REQUIRE(recursive_call.head == "factorial");
    REQUIRE(recursive_call.args.size() == 1);

    auto recursive_arg = std::get<FunctionCall>(*recursive_call.args[0]);
    REQUIRE(recursive_arg.head == "Minus");
    REQUIRE(recursive_arg.args.size() == 2);
    REQUIRE(std::get<Symbol>(*recursive_arg.args[0]).name == "n");
    REQUIRE(std::get<Number>(*recursive_arg.args[1]).value == 1.0);
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
    REQUIRE(func_def->params.size() == expected_params.size());
    for (size_t i = 0; i < expected_params.size(); ++i) {
        REQUIRE(func_def->params[i].name == expected_params[i]);
        // Optionally, check default values here if needed
        // Example: REQUIRE((func_def->params[i].default_value != nullptr) == expected_has_default[i]);
    }
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

TEST_CASE("Parser handles function definitions with delayed and immediate assignment", "[parser][functions]") {
    // Test delayed assignment (:=)
    auto expr = parse_expression("f[a_] := a^3 - 1");
    validate_function_definition(expr, "f", { "a" }, true, "Minus");

    // Test immediate assignment (=)
    expr = parse_expression("f[a_] = a^3 - 1");
    validate_function_definition(expr, "f", { "a" }, false, "Minus");
}