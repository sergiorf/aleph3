#pragma once

#include <cstddef>

namespace aleph3 {

struct EvaluationBudget {
    std::size_t max_ast_depth = 128;
    std::size_t max_node_count = 2048;
    std::size_t max_call_depth = 64;
    std::size_t max_evaluation_steps = 10000;
    std::size_t max_string_bytes = 4096;
    std::size_t max_list_elements = 1024;
};

class Policy {
public:
    Policy() = default;

    [[nodiscard]] static constexpr Policy default_policy() noexcept {
        return Policy{};
    }

    [[nodiscard]] static constexpr Policy strict_numeric() noexcept {
        Policy policy;
        policy.enable_strings_ = false;
        policy.enable_lists_ = false;
        policy.enable_optional_builtins_ = false;
        return policy;
    }

    [[nodiscard]] const EvaluationBudget& budget() const noexcept { return budget_; }
    [[nodiscard]] EvaluationBudget& budget() noexcept { return budget_; }

    [[nodiscard]] bool enable_arithmetic() const noexcept { return enable_arithmetic_; }
    [[nodiscard]] bool enable_comparisons() const noexcept { return enable_comparisons_; }
    [[nodiscard]] bool enable_conditionals() const noexcept { return enable_conditionals_; }
    [[nodiscard]] bool enable_host_functions() const noexcept { return enable_host_functions_; }
    [[nodiscard]] bool enable_strings() const noexcept { return enable_strings_; }
    [[nodiscard]] bool enable_lists() const noexcept { return enable_lists_; }
    [[nodiscard]] bool enable_optional_builtins() const noexcept { return enable_optional_builtins_; }
    [[nodiscard]] bool allow_assignments() const noexcept { return allow_assignments_; }
    [[nodiscard]] bool allow_user_defined_functions() const noexcept { return allow_user_defined_functions_; }
    [[nodiscard]] bool allow_implicit_multiplication() const noexcept { return allow_implicit_multiplication_; }
    [[nodiscard]] bool allow_chained_comparisons() const noexcept { return allow_chained_comparisons_; }

    void set_enable_arithmetic(bool enable) noexcept { enable_arithmetic_ = enable; }
    void set_enable_comparisons(bool enable) noexcept { enable_comparisons_ = enable; }
    void set_enable_conditionals(bool enable) noexcept { enable_conditionals_ = enable; }
    void set_enable_host_functions(bool enable) noexcept { enable_host_functions_ = enable; }
    void set_enable_strings(bool enable) noexcept { enable_strings_ = enable; }
    void set_enable_lists(bool enable) noexcept { enable_lists_ = enable; }
    void set_enable_optional_builtins(bool enable) noexcept { enable_optional_builtins_ = enable; }
    void set_allow_assignments(bool allow) noexcept { allow_assignments_ = allow; }
    void set_allow_user_defined_functions(bool allow) noexcept { allow_user_defined_functions_ = allow; }
    void set_allow_implicit_multiplication(bool allow) noexcept { allow_implicit_multiplication_ = allow; }
    void set_allow_chained_comparisons(bool allow) noexcept { allow_chained_comparisons_ = allow; }

private:
    EvaluationBudget budget_;
    bool enable_arithmetic_ = true;
    bool enable_comparisons_ = true;
    bool enable_conditionals_ = true;
    bool enable_host_functions_ = true;
    bool enable_strings_ = true;
    bool enable_lists_ = false;
    bool enable_optional_builtins_ = false;
    bool allow_assignments_ = false;
    bool allow_user_defined_functions_ = false;
    bool allow_implicit_multiplication_ = false;
    bool allow_chained_comparisons_ = false;
};

}  // namespace aleph3
