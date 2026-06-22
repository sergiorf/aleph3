#include "frontend/Parser.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace aleph3;

TEST_CASE("Parser respects arithmetic precedence and grouping", "[frontend][parser]") {
    frontend::Parser parser("2 + 3 * (4 + x)");
    const auto result = parser.parse();

    REQUIRE(result.ok());
    REQUIRE(result.root->kind == ir::NodeKind::binary_op);

    const auto* plus = result.root->as<ir::BinaryOpNode>();
    REQUIRE(plus != nullptr);
    REQUIRE(plus->op == ir::BinaryOperator::add);
    REQUIRE(plus->left->as<ir::NumberLiteralNode>()->value == 2.0);

    const auto* multiply = plus->right->as<ir::BinaryOpNode>();
    REQUIRE(multiply != nullptr);
    REQUIRE(multiply->op == ir::BinaryOperator::multiply);
    REQUIRE(multiply->left->as<ir::NumberLiteralNode>()->value == 3.0);

    const auto* grouped = multiply->right->as<ir::BinaryOpNode>();
    REQUIRE(grouped != nullptr);
    REQUIRE(grouped->op == ir::BinaryOperator::add);
    REQUIRE(grouped->left->as<ir::NumberLiteralNode>()->value == 4.0);
    REQUIRE(grouped->right->as<ir::VariableNode>()->name == "x");
}

TEST_CASE("Parser keeps power right-associative", "[frontend][parser]") {
    frontend::Parser parser("2 ^ 3 ^ 4");
    const auto result = parser.parse();

    REQUIRE(result.ok());
    const auto* root = result.root->as<ir::BinaryOpNode>();
    REQUIRE(root != nullptr);
    REQUIRE(root->op == ir::BinaryOperator::power);
    REQUIRE(root->left->as<ir::NumberLiteralNode>()->value == 2.0);

    const auto* right = root->right->as<ir::BinaryOpNode>();
    REQUIRE(right != nullptr);
    REQUIRE(right->op == ir::BinaryOperator::power);
    REQUIRE(right->left->as<ir::NumberLiteralNode>()->value == 3.0);
    REQUIRE(right->right->as<ir::NumberLiteralNode>()->value == 4.0);
}

TEST_CASE("Parser builds unary nodes before binary operations", "[frontend][parser]") {
    frontend::Parser parser("-x + +1");
    const auto result = parser.parse();

    REQUIRE(result.ok());
    const auto* root = result.root->as<ir::BinaryOpNode>();
    REQUIRE(root != nullptr);
    REQUIRE(root->op == ir::BinaryOperator::add);

    const auto* left = root->left->as<ir::UnaryOpNode>();
    REQUIRE(left != nullptr);
    REQUIRE(left->op == ir::UnaryOperator::minus);
    REQUIRE(left->operand->as<ir::VariableNode>()->name == "x");

    const auto* right = root->right->as<ir::UnaryOpNode>();
    REQUIRE(right != nullptr);
    REQUIRE(right->op == ir::UnaryOperator::plus);
    REQUIRE(right->operand->as<ir::NumberLiteralNode>()->value == 1.0);
}

TEST_CASE("Parser builds function call and If nodes", "[frontend][parser]") {
    frontend::Parser call_parser("Clamp[x, 0, 1]");
    const auto call_result = call_parser.parse();

    REQUIRE(call_result.ok());
    REQUIRE(call_result.root->kind == ir::NodeKind::call);
    const auto* call = call_result.root->as<ir::CallNode>();
    REQUIRE(call != nullptr);
    REQUIRE(call->callee == "Clamp");
    REQUIRE(call->arguments.size() == 3);

    frontend::Parser if_parser("If[x > 0, x, 0]");
    const auto if_result = if_parser.parse();

    REQUIRE(if_result.ok());
    REQUIRE(if_result.root->kind == ir::NodeKind::if_expr);
    const auto* if_node = if_result.root->as<ir::IfNode>();
    REQUIRE(if_node != nullptr);
    REQUIRE(if_node->condition->as<ir::BinaryOpNode>()->op == ir::BinaryOperator::greater);
    REQUIRE(if_node->then_branch->as<ir::VariableNode>()->name == "x");
    REQUIRE(if_node->else_branch->as<ir::NumberLiteralNode>()->value == 0.0);
}

TEST_CASE("Parser handles nested and mixed function-call expressions", "[frontend][parser]") {
    frontend::Parser nested_parser("Clamp[ScaleAdd[x, 2, 1], 0, 10]");
    const auto nested_result = nested_parser.parse();

    REQUIRE(nested_result.ok());
    const auto* outer_call = nested_result.root->as<ir::CallNode>();
    REQUIRE(outer_call != nullptr);
    REQUIRE(outer_call->callee == "Clamp");
    REQUIRE(outer_call->arguments.size() == 3);

    const auto* inner_call = outer_call->arguments[0]->as<ir::CallNode>();
    REQUIRE(inner_call != nullptr);
    REQUIRE(inner_call->callee == "ScaleAdd");
    REQUIRE(inner_call->arguments.size() == 3);
    REQUIRE(inner_call->arguments[0]->as<ir::VariableNode>()->name == "x");
    REQUIRE(inner_call->arguments[1]->as<ir::NumberLiteralNode>()->value == 2.0);
    REQUIRE(inner_call->arguments[2]->as<ir::NumberLiteralNode>()->value == 1.0);

    frontend::Parser mixed_parser("f[x] + g[y, 2] * 3");
    const auto mixed_result = mixed_parser.parse();

    REQUIRE(mixed_result.ok());
    const auto* root = mixed_result.root->as<ir::BinaryOpNode>();
    REQUIRE(root != nullptr);
    REQUIRE(root->op == ir::BinaryOperator::add);

    const auto* left_call = root->left->as<ir::CallNode>();
    REQUIRE(left_call != nullptr);
    REQUIRE(left_call->callee == "f");
    REQUIRE(left_call->arguments.size() == 1);
    REQUIRE(left_call->arguments[0]->as<ir::VariableNode>()->name == "x");

    const auto* multiply = root->right->as<ir::BinaryOpNode>();
    REQUIRE(multiply != nullptr);
    REQUIRE(multiply->op == ir::BinaryOperator::multiply);

    const auto* right_call = multiply->left->as<ir::CallNode>();
    REQUIRE(right_call != nullptr);
    REQUIRE(right_call->callee == "g");
    REQUIRE(right_call->arguments.size() == 2);
    REQUIRE(right_call->arguments[0]->as<ir::VariableNode>()->name == "y");
    REQUIRE(right_call->arguments[1]->as<ir::NumberLiteralNode>()->value == 2.0);
    REQUIRE(multiply->right->as<ir::NumberLiteralNode>()->value == 3.0);
}

TEST_CASE("Parser handles function-call arguments with booleans, strings, and If branches", "[frontend][parser]") {
    frontend::Parser call_parser(R"(PickLabel[flag, "ok", "fail"])");
    const auto call_result = call_parser.parse();

    REQUIRE(call_result.ok());
    const auto* call = call_result.root->as<ir::CallNode>();
    REQUIRE(call != nullptr);
    REQUIRE(call->callee == "PickLabel");
    REQUIRE(call->arguments.size() == 3);
    REQUIRE(call->arguments[0]->as<ir::VariableNode>()->name == "flag");
    REQUIRE(call->arguments[1]->as<ir::StringLiteralNode>()->value == "ok");
    REQUIRE(call->arguments[2]->as<ir::StringLiteralNode>()->value == "fail");

    frontend::Parser if_parser(R"(If[IsReady[x], Clamp[x, 0, 10], ScaleAdd[x, 1.5, 2]])");
    const auto if_result = if_parser.parse();

    REQUIRE(if_result.ok());
    const auto* if_node = if_result.root->as<ir::IfNode>();
    REQUIRE(if_node != nullptr);
    REQUIRE(if_node->condition->as<ir::CallNode>() != nullptr);
    REQUIRE(if_node->then_branch->as<ir::CallNode>() != nullptr);
    REQUIRE(if_node->else_branch->as<ir::CallNode>() != nullptr);
}

TEST_CASE("Parser accepts zero-argument and single-argument calls", "[frontend][parser]") {
    frontend::Parser zero_arg_parser("Now[]");
    const auto zero_arg_result = zero_arg_parser.parse();

    REQUIRE(zero_arg_result.ok());
    const auto* zero_arg_call = zero_arg_result.root->as<ir::CallNode>();
    REQUIRE(zero_arg_call != nullptr);
    REQUIRE(zero_arg_call->callee == "Now");
    REQUIRE(zero_arg_call->arguments.empty());

    frontend::Parser single_arg_parser("Abs[x]");
    const auto single_arg_result = single_arg_parser.parse();

    REQUIRE(single_arg_result.ok());
    const auto* single_arg_call = single_arg_result.root->as<ir::CallNode>();
    REQUIRE(single_arg_call != nullptr);
    REQUIRE(single_arg_call->callee == "Abs");
    REQUIRE(single_arg_call->arguments.size() == 1);
    REQUIRE(single_arg_call->arguments[0]->as<ir::VariableNode>()->name == "x");
}

TEST_CASE("Parser reports syntax failures with structured diagnostics", "[frontend][parser]") {
    frontend::Parser missing_paren("1 + (2 * 3");
    const auto missing_paren_result = missing_paren.parse();

    REQUIRE_FALSE(missing_paren_result.ok());
    REQUIRE(missing_paren_result.diagnostics.size() == 1);
    REQUIRE(missing_paren_result.diagnostics[0].code == "frontend.parser.expected_right_paren");

    frontend::Parser trailing("x 1");
    const auto trailing_result = trailing.parse();

    REQUIRE_FALSE(trailing_result.ok());
    REQUIRE(trailing_result.diagnostics.size() == 1);
    REQUIRE(trailing_result.diagnostics[0].code == "frontend.parser.trailing_tokens");

    frontend::Parser bad_if("If[x, 1]");
    const auto bad_if_result = bad_if.parse();

    REQUIRE_FALSE(bad_if_result.ok());
    REQUIRE(bad_if_result.diagnostics.size() == 1);
    REQUIRE(bad_if_result.diagnostics[0].code == "frontend.parser.invalid_if_arity");
}

TEST_CASE("Parser reports malformed function-call syntax", "[frontend][parser]") {
    frontend::Parser missing_bracket("Clamp[x, 0, 1");
    const auto missing_bracket_result = missing_bracket.parse();

    REQUIRE_FALSE(missing_bracket_result.ok());
    REQUIRE(missing_bracket_result.diagnostics.size() == 1);
    REQUIRE(missing_bracket_result.diagnostics[0].code == "frontend.parser.expected_right_bracket");

    frontend::Parser trailing_comma("Clamp[x, 0, ]");
    const auto trailing_comma_result = trailing_comma.parse();

    REQUIRE_FALSE(trailing_comma_result.ok());
    REQUIRE(trailing_comma_result.diagnostics.size() == 1);
    REQUIRE(trailing_comma_result.diagnostics[0].code == "frontend.parser.expected_expression");

    frontend::Parser empty_argument("f[x,,y]");
    const auto empty_argument_result = empty_argument.parse();

    REQUIRE_FALSE(empty_argument_result.ok());
    REQUIRE(empty_argument_result.diagnostics.size() == 1);
    REQUIRE(empty_argument_result.diagnostics[0].code == "frontend.parser.expected_expression");

    frontend::Parser missing_comma("f[x y]");
    const auto missing_comma_result = missing_comma.parse();

    REQUIRE_FALSE(missing_comma_result.ok());
    REQUIRE(missing_comma_result.diagnostics.size() == 1);
    REQUIRE(missing_comma_result.diagnostics[0].code == "frontend.parser.expected_right_bracket");
}
