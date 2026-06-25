#pragma once

#include "evaluator/EvaluatorErrors.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "expr/Expr.hpp"

#include <functional>
#include <string>
#include <unordered_map>

namespace aleph3::packs {

using PackFunctionHandler = std::function<ExprPtr(const FunctionCall&, EvaluationContext&)>;

class PackRegistry {
public:
    static PackRegistry& instance() {
        static PackRegistry registry;
        return registry;
    }

    void register_function(const std::string& name, PackFunctionHandler handler) {
        handlers_[name] = std::move(handler);
    }

    [[nodiscard]] bool has_function(const std::string& name) const {
        return handlers_.find(name) != handlers_.end();
    }

    [[nodiscard]] const PackFunctionHandler* find_function(const std::string& name) const {
        auto it = handlers_.find(name);
        return it == handlers_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] PackFunctionHandler get_function(const std::string& name) const {
        if (const auto* handler = find_function(name)) {
            return *handler;
        }
        throw_unsupported_construct("Unknown function: " + name);
    }

private:
    std::unordered_map<std::string, PackFunctionHandler> handlers_;

    PackRegistry() = default;
};

}  // namespace aleph3::packs
