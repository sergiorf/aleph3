#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "expr/Expr.hpp"

namespace aleph3::kernel {

struct SymbolAssumptionFacts {
    std::optional<bool> boolean_value;
    bool nonnegative = false;
    bool positive = false;
    bool nonpositive = false;
    bool negative = false;
    bool zero = false;
    bool nonzero = false;
};

class AssumptionStore {
public:
    void assume(const ExprPtr& expr);

    [[nodiscard]] std::optional<bool> find_boolean_value(std::string_view symbol_name) const;
    [[nodiscard]] std::optional<bool> evaluate_comparison(
        const std::string& head,
        const ExprPtr& left,
        const ExprPtr& right) const;
    [[nodiscard]] const SymbolAssumptionFacts* find_symbol_facts(std::string_view symbol_name) const;

private:
    void assume_boolean_symbol(std::string name, bool value);
    void assume_comparison(const FunctionCall& comparison);
    void assume_sign_fact(std::string symbol_name, const std::string& head);

    std::unordered_map<std::string, SymbolAssumptionFacts> symbol_facts_;
    std::unordered_set<std::string> exact_true_forms_;
};

ExprPtr refine_expr_with_assumptions(const ExprPtr& expr, const AssumptionStore& assumptions);

}  // namespace aleph3::kernel
