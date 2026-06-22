#include "ir/Node.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace aleph3;

TEST_CASE("IR node factory maps payloads to the trusted subset node kinds", "[ir]") {
    const SourceSpan span{3, 7, 1, 4};

    const auto number = ir::make_node(span, ir::NumberLiteralNode{42.0});
    const auto boolean = ir::make_node(span, ir::BooleanLiteralNode{true});
    const auto string = ir::make_node(span, ir::StringLiteralNode{"hello"});
    const auto variable = ir::make_node(span, ir::VariableNode{"x"});

    REQUIRE(number->kind == ir::NodeKind::number_literal);
    REQUIRE(boolean->kind == ir::NodeKind::boolean_literal);
    REQUIRE(string->kind == ir::NodeKind::string_literal);
    REQUIRE(variable->kind == ir::NodeKind::variable);
    REQUIRE(number->span.start_offset == 3);
}

TEST_CASE("IR nodes preserve payload access for structural SDK operations", "[ir]") {
    const auto left = ir::make_node({}, ir::VariableNode{"x"});
    const auto right = ir::make_node({}, ir::NumberLiteralNode{1.0});
    const auto binary = ir::make_node(
        {},
        ir::BinaryOpNode{ir::BinaryOperator::add, left, right});

    REQUIRE(binary->kind == ir::NodeKind::binary_op);
    REQUIRE(binary->as<ir::BinaryOpNode>() != nullptr);
    REQUIRE(binary->as<ir::BinaryOpNode>()->op == ir::BinaryOperator::add);
    REQUIRE(binary->as<ir::BinaryOpNode>()->left->as<ir::VariableNode>()->name == "x");
    REQUIRE(binary->as<ir::BinaryOpNode>()->right->as<ir::NumberLiteralNode>()->value == 1.0);
}

TEST_CASE("IR supports call and If nodes without legacy expression features", "[ir]") {
    const auto condition = ir::make_node({}, ir::BooleanLiteralNode{true});
    const auto then_branch = ir::make_node({}, ir::NumberLiteralNode{1.0});
    const auto else_branch = ir::make_node({}, ir::NumberLiteralNode{0.0});

    const auto call = ir::make_node(
        {},
        ir::CallNode{"Clamp", {then_branch, else_branch}});
    const auto if_node = ir::make_node(
        {},
        ir::IfNode{condition, then_branch, else_branch});

    REQUIRE(call->kind == ir::NodeKind::call);
    REQUIRE(call->as<ir::CallNode>() != nullptr);
    REQUIRE(call->as<ir::CallNode>()->callee == "Clamp");
    REQUIRE(call->as<ir::CallNode>()->arguments.size() == 2);

    REQUIRE(if_node->kind == ir::NodeKind::if_expr);
    REQUIRE(if_node->as<ir::IfNode>() != nullptr);
    REQUIRE(if_node->as<ir::IfNode>()->condition->as<ir::BooleanLiteralNode>()->value);
}
