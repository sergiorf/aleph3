#pragma once

#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include <unordered_map>
#include <functional>
#include <stdexcept>

namespace mathix {

using FunctionHandler = std::function<ExprPtr(const FunctionCall&, EvaluationContext&)>;

class FunctionRegistry {
public:
    static FunctionRegistry& instance() {
        static FunctionRegistry registry;
        return registry;
    }

    void register_function(const std::string& name, FunctionHandler handler) {
        handlers[name] = handler;
    }

    FunctionHandler get_function(const std::string& name) const {
        auto it = handlers.find(name);
        if (it != handlers.end()) {
            return it->second;
        }
        throw std::runtime_error("Unknown function: " + name);
    }

    bool has_function(const std::string& name) const {
        return handlers.find(name) != handlers.end();
    }

private:
    std::unordered_map<std::string, FunctionHandler> handlers;

    // Private constructor for singleton pattern
    FunctionRegistry() = default;
};

}