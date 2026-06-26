/*
 * Kernel Function Registry
 * ------------------------
 * Shared registration surface for symbolic handlers and SDK/runtime host
 * function adapters during the registry unification phase.
 */

#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

#include "evaluator/EvaluatorErrors.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "expr/Expr.hpp"
#include "sdk/Types.hpp"

namespace aleph3::kernel {

using SymbolicFunctionHandler = std::function<ExprPtr(const FunctionCall&, EvaluationContext&)>;
using HostFunctionRegistry = std::unordered_map<std::string, HostFunctionSpec>;

class FunctionRegistry {
public:
    static FunctionRegistry& instance() {
        static FunctionRegistry registry;
        return registry;
    }

    void register_symbolic_function(std::string name, SymbolicFunctionHandler handler) {
        symbolic_handlers_[std::move(name)] = std::move(handler);
    }

    void register_function(std::string name, SymbolicFunctionHandler handler) {
        register_symbolic_function(std::move(name), std::move(handler));
    }

    [[nodiscard]] bool has_symbolic_function(const std::string& name) const {
        return symbolic_handlers_.find(name) != symbolic_handlers_.end();
    }

    [[nodiscard]] bool has_function(const std::string& name) const {
        return has_symbolic_function(name);
    }

    [[nodiscard]] const SymbolicFunctionHandler* find_symbolic_function(const std::string& name) const {
        auto it = symbolic_handlers_.find(name);
        return it == symbolic_handlers_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] const SymbolicFunctionHandler* find_function(const std::string& name) const {
        return find_symbolic_function(name);
    }

    [[nodiscard]] SymbolicFunctionHandler get_symbolic_function(const std::string& name) const {
        if (const auto* handler = find_symbolic_function(name)) {
            return *handler;
        }
        throw_unsupported_construct("Unknown function: " + name);
    }

    [[nodiscard]] SymbolicFunctionHandler get_function(const std::string& name) const {
        return get_symbolic_function(name);
    }

    static void register_host_function(HostFunctionRegistry& registry, HostFunctionSpec spec) {
        registry[spec.name] = std::move(spec);
    }

    [[nodiscard]] static const HostFunctionSpec* find_host_function(
        const HostFunctionRegistry& registry,
        const std::string& name) {
        auto it = registry.find(name);
        return it == registry.end() ? nullptr : &it->second;
    }

private:
    std::unordered_map<std::string, SymbolicFunctionHandler> symbolic_handlers_;
};

}  // namespace aleph3::kernel
