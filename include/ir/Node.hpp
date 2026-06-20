#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "sdk/Types.hpp"

namespace aleph3::ir {

enum class NodeKind {
    number_literal,
    boolean_literal,
    string_literal,
    variable,
    unary_op,
    binary_op,
    call,
    if_expr
};

enum class UnaryOperator {
    plus,
    minus
};

enum class BinaryOperator {
    add,
    subtract,
    multiply,
    divide,
    power,
    equal,
    not_equal,
    less,
    less_equal,
    greater,
    greater_equal
};

struct Node;
using NodePtr = std::shared_ptr<const Node>;

struct NumberLiteralNode {
    double value = 0.0;
};

struct BooleanLiteralNode {
    bool value = false;
};

struct StringLiteralNode {
    std::string value;
};

struct VariableNode {
    std::string name;
};

struct UnaryOpNode {
    UnaryOperator op = UnaryOperator::plus;
    NodePtr operand;
};

struct BinaryOpNode {
    BinaryOperator op = BinaryOperator::add;
    NodePtr left;
    NodePtr right;
};

struct CallNode {
    std::string callee;
    std::vector<NodePtr> arguments;
};

struct IfNode {
    NodePtr condition;
    NodePtr then_branch;
    NodePtr else_branch;
};

using NodePayload = std::variant<
    NumberLiteralNode,
    BooleanLiteralNode,
    StringLiteralNode,
    VariableNode,
    UnaryOpNode,
    BinaryOpNode,
    CallNode,
    IfNode>;

struct Node {
    NodeKind kind = NodeKind::number_literal;
    aleph3::SourceSpan span;
    NodePayload payload = NumberLiteralNode{};

    Node() = default;

    template <typename Payload>
    Node(NodeKind node_kind, aleph3::SourceSpan source_span, Payload node_payload)
        : kind(node_kind), span(source_span), payload(std::move(node_payload)) {}

    template <typename Payload>
    [[nodiscard]] const Payload* as() const noexcept {
        return std::get_if<Payload>(&payload);
    }
};

template <typename Payload>
[[nodiscard]] NodePtr make_node(aleph3::SourceSpan span, Payload payload) {
    if constexpr (std::is_same_v<Payload, NumberLiteralNode>) {
        return std::make_shared<Node>(NodeKind::number_literal, span, std::move(payload));
    } else if constexpr (std::is_same_v<Payload, BooleanLiteralNode>) {
        return std::make_shared<Node>(NodeKind::boolean_literal, span, std::move(payload));
    } else if constexpr (std::is_same_v<Payload, StringLiteralNode>) {
        return std::make_shared<Node>(NodeKind::string_literal, span, std::move(payload));
    } else if constexpr (std::is_same_v<Payload, VariableNode>) {
        return std::make_shared<Node>(NodeKind::variable, span, std::move(payload));
    } else if constexpr (std::is_same_v<Payload, UnaryOpNode>) {
        return std::make_shared<Node>(NodeKind::unary_op, span, std::move(payload));
    } else if constexpr (std::is_same_v<Payload, BinaryOpNode>) {
        return std::make_shared<Node>(NodeKind::binary_op, span, std::move(payload));
    } else if constexpr (std::is_same_v<Payload, CallNode>) {
        return std::make_shared<Node>(NodeKind::call, span, std::move(payload));
    } else if constexpr (std::is_same_v<Payload, IfNode>) {
        return std::make_shared<Node>(NodeKind::if_expr, span, std::move(payload));
    } else {
        static_assert(sizeof(Payload) == 0, "Unsupported IR payload type");
    }
}

}  // namespace aleph3::ir
