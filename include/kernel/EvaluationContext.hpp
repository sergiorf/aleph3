/*
 * Kernel Evaluation Context
 * -------------------------
 * Shared execution context for symbolic kernel state and embedded runtime
 * state during the kernel-unification refactor.
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "kernel/Diagnostics.hpp"
#include "expr/Expr.hpp"
#include "sdk/Policy.hpp"
#include "sdk/Types.hpp"
#include "symbols/SymbolState.hpp"

namespace aleph3::kernel {

class EvaluationContext {
public:
    EvaluationContext()
        : runtime_state_(std::make_shared<RuntimeSemanticsState>()),
          variables(symbol_values.entries()),
          user_functions(function_definitions.entries()) {}

    EvaluationContext(
        const Bindings& bindings,
        const Bindings& constants,
        const std::unordered_map<std::string, HostFunctionSpec>& host_functions,
        const Policy& policy)
        : runtime_state_(std::make_shared<RuntimeSemanticsState>()),
          variables(symbol_values.entries()),
          user_functions(function_definitions.entries()),
          bindings_ptr_(&bindings),
          constants_ptr_(&constants),
          host_functions_ptr_(&host_functions),
          policy_ptr_(&policy) {}

    EvaluationContext(const EvaluationContext& other)
        : symbol_values(other.symbol_values),
          symbol_metadata(other.symbol_metadata),
          definition_records(other.definition_records),
          function_definitions(other.function_definitions),
          owned_bindings_(other.owned_bindings_),
          owned_constants_(other.owned_constants_),
          owned_host_functions_(other.owned_host_functions_),
          owned_policy_(other.owned_policy_),
          runtime_state_(other.runtime_state_),
          variables(symbol_values.entries()),
          user_functions(function_definitions.entries()) {
        copy_runtime_sources(other);
    }

    EvaluationContext(EvaluationContext&& other) noexcept
        : symbol_values(std::move(other.symbol_values)),
          symbol_metadata(std::move(other.symbol_metadata)),
          definition_records(std::move(other.definition_records)),
          function_definitions(std::move(other.function_definitions)),
          owned_bindings_(std::move(other.owned_bindings_)),
          owned_constants_(std::move(other.owned_constants_)),
          owned_host_functions_(std::move(other.owned_host_functions_)),
          owned_policy_(std::move(other.owned_policy_)),
          runtime_state_(std::move(other.runtime_state_)),
          variables(symbol_values.entries()),
          user_functions(function_definitions.entries()) {
        move_runtime_sources(other);
    }

    EvaluationContext& operator=(const EvaluationContext& other) {
        if (this != &other) {
            symbol_values = other.symbol_values;
            symbol_metadata = other.symbol_metadata;
            definition_records = other.definition_records;
            function_definitions = other.function_definitions;
            owned_bindings_ = other.owned_bindings_;
            owned_constants_ = other.owned_constants_;
            owned_host_functions_ = other.owned_host_functions_;
            owned_policy_ = other.owned_policy_;
            runtime_state_ = other.runtime_state_;
            copy_runtime_sources(other);
        }
        return *this;
    }

    EvaluationContext& operator=(EvaluationContext&& other) noexcept {
        if (this != &other) {
            symbol_values = std::move(other.symbol_values);
            symbol_metadata = std::move(other.symbol_metadata);
            definition_records = std::move(other.definition_records);
            function_definitions = std::move(other.function_definitions);
            owned_bindings_ = std::move(other.owned_bindings_);
            owned_constants_ = std::move(other.owned_constants_);
            owned_host_functions_ = std::move(other.owned_host_functions_);
            owned_policy_ = std::move(other.owned_policy_);
            runtime_state_ = std::move(other.runtime_state_);
            move_runtime_sources(other);
        }
        return *this;
    }

    [[nodiscard]] const Bindings& bindings() const noexcept {
        return *bindings_ptr_;
    }

    [[nodiscard]] const Bindings& constants() const noexcept {
        return *constants_ptr_;
    }

    [[nodiscard]] const std::unordered_map<std::string, HostFunctionSpec>& host_functions() const noexcept {
        return *host_functions_ptr_;
    }

    [[nodiscard]] const Policy& policy() const noexcept {
        return *policy_ptr_;
    }

    void enable_runtime_strict_semantics(bool enabled = true) {
        runtime_state_->strict_runtime_semantics = enabled;
    }

    [[nodiscard]] bool strict_runtime_semantics() const noexcept {
        return runtime_state_->strict_runtime_semantics;
    }

    void reset_runtime_step_counter() noexcept {
        runtime_state_->evaluation_steps_used = 0;
    }

    void consume_evaluation_step() {
        if (!runtime_state_->strict_runtime_semantics) {
            return;
        }
        ++runtime_state_->evaluation_steps_used;
        if (runtime_state_->evaluation_steps_used > policy().budget().max_evaluation_steps) {
            throw_runtime_error(
                ErrorCode::step_budget_exhausted,
                "Evaluation exceeded the configured step budget.");
        }
    }

    symbols::SymbolValueTable symbol_values;
    symbols::SymbolMetadataTable symbol_metadata;
    symbols::SymbolDefinitionTable definition_records;
    symbols::FunctionDefinitionTable function_definitions;

    std::unordered_map<std::string, ExprPtr>& variables;
    std::unordered_map<std::string, FunctionDefinition>& user_functions;

private:
    struct RuntimeSemanticsState {
        bool strict_runtime_semantics = false;
        std::size_t evaluation_steps_used = 0;
    };

    void copy_runtime_sources(const EvaluationContext& other) noexcept {
        bindings_ptr_ = other.bindings_ptr_ == &other.owned_bindings_ ? &owned_bindings_ : other.bindings_ptr_;
        constants_ptr_ = other.constants_ptr_ == &other.owned_constants_ ? &owned_constants_ : other.constants_ptr_;
        host_functions_ptr_ =
            other.host_functions_ptr_ == &other.owned_host_functions_ ? &owned_host_functions_ : other.host_functions_ptr_;
        policy_ptr_ = other.policy_ptr_ == &other.owned_policy_ ? &owned_policy_ : other.policy_ptr_;
    }

    void move_runtime_sources(EvaluationContext& other) noexcept {
        bindings_ptr_ = other.bindings_ptr_ == &other.owned_bindings_ ? &owned_bindings_ : other.bindings_ptr_;
        constants_ptr_ = other.constants_ptr_ == &other.owned_constants_ ? &owned_constants_ : other.constants_ptr_;
        host_functions_ptr_ =
            other.host_functions_ptr_ == &other.owned_host_functions_ ? &owned_host_functions_ : other.host_functions_ptr_;
        policy_ptr_ = other.policy_ptr_ == &other.owned_policy_ ? &owned_policy_ : other.policy_ptr_;
    }

    Bindings owned_bindings_;
    Bindings owned_constants_;
    std::unordered_map<std::string, HostFunctionSpec> owned_host_functions_;
    Policy owned_policy_ = Policy::default_policy();
    std::shared_ptr<RuntimeSemanticsState> runtime_state_;

    const Bindings* bindings_ptr_ = &owned_bindings_;
    const Bindings* constants_ptr_ = &owned_constants_;
    const std::unordered_map<std::string, HostFunctionSpec>* host_functions_ptr_ = &owned_host_functions_;
    const Policy* policy_ptr_ = &owned_policy_;
};

}  // namespace aleph3::kernel
