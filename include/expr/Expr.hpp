#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <iostream>

namespace mathix {

// Forward declarations
struct Symbol;
struct Number;
struct FunctionCall;
struct FunctionDefinition;
struct Assignment;

// Core Expression type: variant of all expression types
using Expr = std::variant < Symbol, Number, FunctionCall, FunctionDefinition, Assignment > ;

// Smart pointer to expressions
using ExprPtr = std::shared_ptr<Expr>;

// Factory function to make an ExprPtr
template <typename T, typename... Args>
ExprPtr make_expr(Args&&... args) {
    return std::make_shared<Expr>(T{std::forward<Args>(args)...});
}

// Expression types

struct Symbol {
    std::string name;

    Symbol(const std::string& n) : name(n) {}
};

struct Number {
    double value;

    Number(double v) : value(v) {}
    Number(int v) : value(static_cast<double>(v)) {}
};

struct FunctionCall {
    std::string head;            // Like "Plus", "Times", "Sin"
    std::vector<ExprPtr> args;    // Arguments

    FunctionCall(std::string h, const std::vector<ExprPtr>& a)
        : head(h), args(a) {}
};

struct FunctionDefinition {
    std::string name;               // Function name
    std::vector<std::string> params; // Parameters
    ExprPtr body;                   // Function body
    bool delayed;                   // True for `:=`, false for `=`

    FunctionDefinition() : name(""), params(), body(nullptr), delayed(true) {}

    FunctionDefinition(const std::string& name, const std::vector<std::string>& params, const ExprPtr& body, bool delayed)
        : name(name), params(params), body(body), delayed(delayed) {
    }
};

struct Assignment {
    std::string name; // Variable name
    ExprPtr value;    // Assigned value

    Assignment(const std::string& name, const ExprPtr& value)
        : name(name), value(value) {
    }
};

// Utility functions

std::string to_string(const Expr& expr);

inline std::string to_string(const ExprPtr& expr_ptr) {
    return to_string(*expr_ptr);
}

} // namespace mathix
