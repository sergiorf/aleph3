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

// Core Expression type: variant of all expression types
using Expr = std::variant<Symbol, Number, FunctionCall>;

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
};

struct FunctionCall {
    std::string head;            // Like "Plus", "Times", "Sin"
    std::vector<ExprPtr> args;    // Arguments

    FunctionCall(std::string h, const std::vector<ExprPtr>& a)
        : head(std::move(h)), args(a) {}
};

// Utility functions

std::string to_string(const Expr& expr);

inline std::string to_string(const ExprPtr& expr_ptr) {
    return to_string(*expr_ptr);
}

} // namespace mathix
