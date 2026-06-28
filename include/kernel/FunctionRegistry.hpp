/*
 * Kernel Function Registry
 * ------------------------
 * Shared registration catalog for symbolic handlers, builtin execution specs,
 * and rewrite handlers for one engine, session, or test environment.
 */

#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "evaluator/EvaluatorErrors.hpp"
#include "expr/Expr.hpp"
#include "sdk/Types.hpp"

namespace aleph3::kernel {

class EvaluationContext;

using SymbolicFunctionHandler = std::function<ExprPtr(const FunctionCall&, EvaluationContext&)>;
using BuiltinFunctionHandler = SymbolicFunctionHandler;
using HeadRewriteHandler = std::function<std::optional<ExprPtr>(const FunctionCall&, EvaluationContext&)>;
using HostFunctionRegistry = std::unordered_map<std::string, HostFunctionSpec>;

enum class RegistrationSource {
    builtin,
    pack,
    host_bridge
};

struct SymbolicFunctionMetadata {
    std::string name;
    std::string owning_package;
    std::string documentation;
    RegistrationSource source = RegistrationSource::builtin;
    bool rewrite_safe = false;
};

struct SymbolicFunctionSpec {
    SymbolicFunctionMetadata metadata;
    SymbolicFunctionHandler handler;
};

struct BuiltinFunctionSpec {
    SymbolicFunctionMetadata metadata;
    BuiltinFunctionHandler handler;
};

enum class RewriteStage {
    normalized_head
};

struct HeadRewriteSpec {
    SymbolicFunctionMetadata metadata;
    std::string rewrite_name;
    RewriteStage stage = RewriteStage::normalized_head;
    int priority = 0;
    HeadRewriteHandler handler;
};

class FunctionRegistry {
public:
    static FunctionRegistry& instance() {
        static FunctionRegistry registry;
        return registry;
    }

    void register_symbolic_function(std::string name, SymbolicFunctionHandler handler) {
        SymbolicFunctionSpec spec;
        spec.metadata.name = name;
        spec.metadata.source = RegistrationSource::builtin;
        spec.handler = std::move(handler);
        register_symbolic_function(std::move(spec));
    }

    void register_symbolic_function(SymbolicFunctionSpec spec) {
        symbolic_functions_[spec.metadata.name] = std::move(spec);
    }

    void register_pack_function(
        std::string package_name,
        std::string symbol_name,
        SymbolicFunctionHandler handler,
        std::string documentation = {},
        bool rewrite_safe = false) {
        SymbolicFunctionSpec spec;
        spec.metadata.name = std::move(symbol_name);
        spec.metadata.owning_package = std::move(package_name);
        spec.metadata.documentation = std::move(documentation);
        spec.metadata.source = RegistrationSource::pack;
        spec.metadata.rewrite_safe = rewrite_safe;
        spec.handler = std::move(handler);
        register_symbolic_function(std::move(spec));
    }

    void register_function(std::string name, SymbolicFunctionHandler handler) {
        register_symbolic_function(std::move(name), std::move(handler));
    }

    void register_builtin_function(std::string name, BuiltinFunctionHandler handler) {
        BuiltinFunctionSpec spec;
        spec.metadata.name = std::move(name);
        spec.metadata.source = RegistrationSource::builtin;
        spec.handler = std::move(handler);
        register_builtin_function(std::move(spec));
    }

    void register_builtin_function(BuiltinFunctionSpec spec) {
        builtin_functions_[spec.metadata.name] = std::move(spec);
    }

    void register_head_rewrite(
        std::string head_name,
        std::string rewrite_name,
        HeadRewriteHandler handler,
        int priority) {
        HeadRewriteSpec spec;
        spec.metadata.name = std::move(head_name);
        spec.metadata.source = RegistrationSource::builtin;
        spec.rewrite_name = std::move(rewrite_name);
        spec.priority = priority;
        spec.handler = std::move(handler);
        register_head_rewrite(std::move(spec));
    }

    void register_head_rewrite(HeadRewriteSpec spec) {
        auto& specs = head_rewrites_[spec.metadata.name];
        for (auto& existing : specs) {
            if (existing.rewrite_name == spec.rewrite_name &&
                existing.stage == spec.stage) {
                existing = std::move(spec);
                sort_head_rewrites(specs);
                return;
            }
        }
        specs.push_back(std::move(spec));
        sort_head_rewrites(specs);
    }

    void register_pack_head_rewrite(
        std::string package_name,
        std::string head_name,
        std::string rewrite_name,
        HeadRewriteHandler handler,
        int priority,
        std::string documentation = {}) {
        HeadRewriteSpec spec;
        spec.metadata.name = std::move(head_name);
        spec.metadata.owning_package = std::move(package_name);
        spec.metadata.documentation = std::move(documentation);
        spec.metadata.source = RegistrationSource::pack;
        spec.rewrite_name = std::move(rewrite_name);
        spec.priority = priority;
        spec.handler = std::move(handler);
        register_head_rewrite(std::move(spec));
    }

    [[nodiscard]] bool has_symbolic_function(const std::string& name) const {
        return symbolic_functions_.find(name) != symbolic_functions_.end();
    }

    [[nodiscard]] bool has_function(const std::string& name) const {
        return has_symbolic_function(name);
    }

    [[nodiscard]] bool has_builtin_function(const std::string& name) const {
        return builtin_functions_.find(name) != builtin_functions_.end();
    }

    [[nodiscard]] bool has_head_rewrites(const std::string& name) const {
        auto it = head_rewrites_.find(name);
        return it != head_rewrites_.end() && !it->second.empty();
    }

    [[nodiscard]] const SymbolicFunctionHandler* find_symbolic_function(const std::string& name) const {
        const auto* spec = find_symbolic_function_spec(name);
        return spec == nullptr ? nullptr : &spec->handler;
    }

    [[nodiscard]] const SymbolicFunctionHandler* find_function(const std::string& name) const {
        return find_symbolic_function(name);
    }

    [[nodiscard]] const BuiltinFunctionHandler* find_builtin_function(const std::string& name) const {
        const auto* spec = find_builtin_function_spec(name);
        return spec == nullptr ? nullptr : &spec->handler;
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

    [[nodiscard]] BuiltinFunctionHandler get_builtin_function(const std::string& name) const {
        if (const auto* handler = find_builtin_function(name)) {
            return *handler;
        }
        throw_unsupported_construct("Unknown builtin function: " + name);
    }

    [[nodiscard]] const SymbolicFunctionSpec* find_symbolic_function_spec(const std::string& name) const {
        auto it = symbolic_functions_.find(name);
        return it == symbolic_functions_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] const BuiltinFunctionSpec* find_builtin_function_spec(const std::string& name) const {
        auto it = builtin_functions_.find(name);
        return it == builtin_functions_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] const std::vector<HeadRewriteSpec>* find_head_rewrites(const std::string& name) const {
        auto it = head_rewrites_.find(name);
        return it == head_rewrites_.end() ? nullptr : &it->second;
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
    static void sort_head_rewrites(std::vector<HeadRewriteSpec>& specs) {
        std::stable_sort(
            specs.begin(),
            specs.end(),
            [](const HeadRewriteSpec& left, const HeadRewriteSpec& right) {
                return left.priority < right.priority;
            });
    }

    std::unordered_map<std::string, SymbolicFunctionSpec> symbolic_functions_;
    std::unordered_map<std::string, BuiltinFunctionSpec> builtin_functions_;
    std::unordered_map<std::string, std::vector<HeadRewriteSpec>> head_rewrites_;
};

[[nodiscard]] FunctionRegistry create_default_function_registry();
[[nodiscard]] const FunctionRegistry& default_function_registry();

}  // namespace aleph3::kernel
