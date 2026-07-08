#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "sdk/Types.hpp"

namespace aleph3::syntax {

enum class NodeKind {
    number_literal,
    boolean_literal,
    string_literal,
    symbol,
    unary_op,
    binary_op,
    call,
    list,
    default_parameter,
    assignment,
    function_definition
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
    greater_equal,
    and_op,
    or_op,
    string_join,
    rule
};

struct Node;
using NodePtr = std::shared_ptr<const Node>;

struct NumberLiteralNode {
    double value = 0.0;
    std::string lexeme;
};

struct BooleanLiteralNode {
    bool value = false;
};

struct StringLiteralNode {
    std::string value;
};

struct SymbolNode {
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
    bool implicit = false;
};

struct CallNode {
    std::string callee;
    std::vector<NodePtr> arguments;
};

struct ListNode {
    std::vector<NodePtr> elements;
};

struct DefaultParameterNode {
    NodePtr parameter;
    NodePtr default_value;
};

struct AssignmentNode {
    std::string name;
    NodePtr value;
};

struct FunctionDefinitionNode {
    std::string name;
    std::vector<NodePtr> parameters;
    NodePtr body;
    bool delayed = true;
};

using NodePayload = std::variant<
    NumberLiteralNode,
    BooleanLiteralNode,
    StringLiteralNode,
    SymbolNode,
    UnaryOpNode,
    BinaryOpNode,
    CallNode,
    ListNode,
    DefaultParameterNode,
    AssignmentNode,
    FunctionDefinitionNode>;

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
    } else if constexpr (std::is_same_v<Payload, SymbolNode>) {
        return std::make_shared<Node>(NodeKind::symbol, span, std::move(payload));
    } else if constexpr (std::is_same_v<Payload, UnaryOpNode>) {
        return std::make_shared<Node>(NodeKind::unary_op, span, std::move(payload));
    } else if constexpr (std::is_same_v<Payload, BinaryOpNode>) {
        return std::make_shared<Node>(NodeKind::binary_op, span, std::move(payload));
    } else if constexpr (std::is_same_v<Payload, CallNode>) {
        return std::make_shared<Node>(NodeKind::call, span, std::move(payload));
    } else if constexpr (std::is_same_v<Payload, ListNode>) {
        return std::make_shared<Node>(NodeKind::list, span, std::move(payload));
    } else if constexpr (std::is_same_v<Payload, DefaultParameterNode>) {
        return std::make_shared<Node>(NodeKind::default_parameter, span, std::move(payload));
    } else if constexpr (std::is_same_v<Payload, AssignmentNode>) {
        return std::make_shared<Node>(NodeKind::assignment, span, std::move(payload));
    } else if constexpr (std::is_same_v<Payload, FunctionDefinitionNode>) {
        return std::make_shared<Node>(NodeKind::function_definition, span, std::move(payload));
    } else {
        static_assert(sizeof(Payload) == 0, "Unsupported syntax payload type");
    }
}

}  // namespace aleph3::syntax
