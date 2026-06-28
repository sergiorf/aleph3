#include "kernel/Rewrite.hpp"

#include "kernel/EvaluationContext.hpp"
#include "kernel/FunctionRegistry.hpp"
#include "expr/ExprUtils.hpp"
#include "normalizer/Normalizer.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <unordered_map>
#include <type_traits>
#include <utility>

namespace aleph3::kernel {

namespace {

using PatternBindings = std::unordered_map<std::string, ExprPtr>;

bool is_integral(double value) {
    return std::floor(value) == value;
}

ExprPtr clone_expr(const ExprPtr& expr) {
    return expr == nullptr ? nullptr : std::make_shared<Expr>(*expr);
}

struct ScalarCoefficient {
    bool exact = true;
    int64_t numerator = 0;
    int64_t denominator = 1;
    double approximate = 0.0;

    void add_number(double value) {
        if (exact && is_integral(value)) {
            auto [nn, dd] = normalize_rational(
                numerator + static_cast<int64_t>(value) * denominator,
                denominator);
            numerator = nn;
            denominator = dd;
            return;
        }

        if (exact) {
            approximate = static_cast<double>(numerator) / denominator;
            exact = false;
        }
        approximate += value;
    }

    void add_rational(int64_t num, int64_t den) {
        if (exact) {
            auto [nn, dd] = normalize_rational(
                numerator * den + num * denominator,
                denominator * den);
            numerator = nn;
            denominator = dd;
            return;
        }
        approximate += static_cast<double>(num) / den;
    }

    void add(const ScalarCoefficient& other) {
        if (other.exact) {
            add_rational(other.numerator, other.denominator);
            return;
        }
        add_number(other.approximate);
    }

    [[nodiscard]] bool is_zero() const {
        return exact ? numerator == 0 : approximate == 0.0;
    }

    [[nodiscard]] bool is_one() const {
        if (!exact) {
            return approximate == 1.0;
        }
        return numerator == denominator;
    }

    [[nodiscard]] ExprPtr to_expr() const {
        if (!exact) {
            return make_expr<Number>(approximate);
        }
        if (denominator == 1) {
            return make_expr<Number>(static_cast<double>(numerator));
        }
        return make_expr<Rational>(numerator, denominator);
    }
};

struct SupportedMonomialTerm {
    ExprPtr basis;
    ScalarCoefficient coefficient;
};

bool contains_list_argument(const std::vector<ExprPtr>& args) {
    for (const auto& arg : args) {
        if (std::holds_alternative<List>(*arg)) {
            return true;
        }
        if (const auto* call = std::get_if<FunctionCall>(arg.get());
            call != nullptr && call->head == "List") {
            return true;
        }
    }
    return false;
}

ExprPtr build_plus_bucket_expr(
    const std::vector<ExprPtr>& args,
    bool& changed) {
    double numeric_result = 0.0;
    bool has_rational_result = false;
    int64_t rational_num = 0;
    int64_t rational_den = 1;
    std::vector<ExprPtr> symbolic_terms;

    for (const auto& arg : args) {
        if (std::holds_alternative<Number>(*arg)) {
            const double value = get_number_value(arg);
            if (value == 0.0) {
                changed = true;
                continue;
            }
            numeric_result += value;
            continue;
        }
        if (std::holds_alternative<Rational>(*arg)) {
            const auto& rational = std::get<Rational>(*arg);
            if (!has_rational_result) {
                rational_num = rational.numerator;
                rational_den = rational.denominator;
                has_rational_result = true;
            } else {
                auto [nn, dd] = normalize_rational(
                    rational_num * rational.denominator + rational.numerator * rational_den,
                    rational_den * rational.denominator);
                rational_num = nn;
                rational_den = dd;
            }
            continue;
        }
        symbolic_terms.push_back(arg);
    }

    std::vector<ExprPtr> rebuilt_terms;
    rebuilt_terms.reserve(symbolic_terms.size() + 1);
    rebuilt_terms.insert(rebuilt_terms.end(), symbolic_terms.begin(), symbolic_terms.end());

    if (has_rational_result) {
        if (is_integral(numeric_result)) {
            auto [nn, dd] = normalize_rational(
                rational_num + static_cast<int64_t>(numeric_result) * rational_den,
                rational_den);
            if (nn != 0) {
                rebuilt_terms.push_back(
                    dd == 1 ? make_expr<Number>(static_cast<double>(nn))
                            : make_expr<Rational>(nn, dd));
            } else {
                changed = true;
            }
        } else {
            const double rational_value = static_cast<double>(rational_num) / rational_den;
            const double combined = rational_value + numeric_result;
            if (combined != 0.0) {
                rebuilt_terms.push_back(make_expr<Number>(combined));
            } else {
                changed = true;
            }
        }
    } else if (numeric_result != 0.0) {
        rebuilt_terms.push_back(make_expr<Number>(numeric_result));
    } else if (args.size() != symbolic_terms.size()) {
        changed = true;
    }

    if (rebuilt_terms.empty()) {
        return make_expr<Number>(0.0);
    }
    if (rebuilt_terms.size() == 1) {
        return rebuilt_terms.front();
    }
    return normalize_expr(make_fcall("Plus", rebuilt_terms));
}

ExprPtr build_times_bucket_expr(
    const std::vector<ExprPtr>& args,
    bool& changed) {
    double numeric_result = 1.0;
    bool has_rational_result = false;
    int64_t rational_num = 1;
    int64_t rational_den = 1;
    std::vector<ExprPtr> symbolic_terms;

    for (const auto& arg : args) {
        if (std::holds_alternative<Number>(*arg)) {
            const double value = get_number_value(arg);
            if (value == 0.0) {
                changed = true;
                return make_expr<Number>(0.0);
            }
            if (value == 1.0) {
                changed = true;
                continue;
            }
            numeric_result *= value;
            continue;
        }
        if (std::holds_alternative<Rational>(*arg)) {
            const auto& rational = std::get<Rational>(*arg);
            if (rational.numerator == 0) {
                changed = true;
                return make_expr<Number>(0.0);
            }
            if (!has_rational_result) {
                rational_num = rational.numerator;
                rational_den = rational.denominator;
                has_rational_result = true;
            } else {
                auto [nn, dd] = normalize_rational(
                    rational_num * rational.numerator,
                    rational_den * rational.denominator);
                rational_num = nn;
                rational_den = dd;
            }
            if (rational.numerator == rational.denominator) {
                changed = true;
            }
            continue;
        }
        symbolic_terms.push_back(arg);
    }

    std::vector<ExprPtr> rebuilt_terms;
    rebuilt_terms.reserve(symbolic_terms.size() + 1);

    if (has_rational_result) {
        if (is_integral(numeric_result)) {
            auto [nn, dd] = normalize_rational(
                rational_num * static_cast<int64_t>(numeric_result),
                rational_den);
            if (!(nn == 1 && dd == 1)) {
                rebuilt_terms.push_back(
                    dd == 1 ? make_expr<Number>(static_cast<double>(nn))
                            : make_expr<Rational>(nn, dd));
            } else if (!symbolic_terms.empty()) {
                changed = true;
            }
        } else {
            const double combined = numeric_result * (static_cast<double>(rational_num) / rational_den);
            if (combined == 0.0) {
                changed = true;
                return make_expr<Number>(0.0);
            }
            if (combined != 1.0 || symbolic_terms.empty()) {
                rebuilt_terms.push_back(make_expr<Number>(combined));
            } else {
                changed = true;
            }
        }
    } else if (numeric_result != 1.0 || symbolic_terms.empty()) {
        rebuilt_terms.push_back(make_expr<Number>(numeric_result));
    } else if (args.size() != symbolic_terms.size()) {
        changed = true;
    }

    rebuilt_terms.insert(rebuilt_terms.end(), symbolic_terms.begin(), symbolic_terms.end());

    if (rebuilt_terms.empty()) {
        return make_expr<Number>(1.0);
    }
    if (rebuilt_terms.size() == 1) {
        return rebuilt_terms.front();
    }
    return normalize_expr(make_fcall("Times", rebuilt_terms));
}

bool extract_supported_symbolic_basis(const ExprPtr& expr, ExprPtr& basis) {
    if (std::holds_alternative<Symbol>(*expr)) {
        basis = expr;
        return true;
    }

    const auto* power = std::get_if<FunctionCall>(expr.get());
    if (power == nullptr || power->head != "Power" || power->args.size() != 2) {
        return false;
    }
    if (!std::holds_alternative<Symbol>(*power->args[0]) ||
        !std::holds_alternative<Number>(*power->args[1])) {
        return false;
    }

    basis = expr;
    return true;
}

bool extract_symbol_power_term(const ExprPtr& expr, std::string& symbol_name, double& exponent) {
    if (const auto* symbol = std::get_if<Symbol>(expr.get())) {
        symbol_name = symbol->name;
        exponent = 1.0;
        return true;
    }

    const auto* power = std::get_if<FunctionCall>(expr.get());
    if (power == nullptr || power->head != "Power" || power->args.size() != 2) {
        return false;
    }

    if (std::holds_alternative<Symbol>(*power->args[0]) && std::holds_alternative<Number>(*power->args[1])) {
        symbol_name = std::get<Symbol>(*power->args[0]).name;
        exponent = get_number_value(power->args[1]);
        return true;
    }

    return false;
}

bool extract_supported_monomial_term(
    const ExprPtr& expr,
    SupportedMonomialTerm& term) {
    ExprPtr basis;
    if (extract_supported_symbolic_basis(expr, basis)) {
        term.basis = basis;
        term.coefficient.numerator = 1;
        term.coefficient.denominator = 1;
        return true;
    }

    const auto* times = std::get_if<FunctionCall>(expr.get());
    if (times == nullptr || times->head != "Times" || times->args.size() != 2) {
        return false;
    }

    const ExprPtr* coefficient_expr = nullptr;
    const ExprPtr* basis_expr = nullptr;
    if (std::holds_alternative<Number>(*times->args[0]) ||
        std::holds_alternative<Rational>(*times->args[0])) {
        coefficient_expr = &times->args[0];
        basis_expr = &times->args[1];
    } else if (std::holds_alternative<Number>(*times->args[1]) ||
               std::holds_alternative<Rational>(*times->args[1])) {
        coefficient_expr = &times->args[1];
        basis_expr = &times->args[0];
    } else {
        return false;
    }

    if (!extract_supported_symbolic_basis(*basis_expr, basis)) {
        return false;
    }

    term.basis = basis;
    if (std::holds_alternative<Number>(**coefficient_expr)) {
        term.coefficient.add_number(std::get<Number>(**coefficient_expr).value);
    } else {
        const auto& rational = std::get<Rational>(**coefficient_expr);
        term.coefficient.add_rational(rational.numerator, rational.denominator);
    }
    return true;
}

ExprPtr rebuild_supported_monomial_term(const SupportedMonomialTerm& term) {
    if (term.coefficient.is_zero()) {
        return nullptr;
    }
    if (term.coefficient.is_one()) {
        return clone_expr(term.basis);
    }
    return normalize_expr(make_fcall("Times", {term.coefficient.to_expr(), term.basis}));
}

bool is_pattern_symbol_name(const std::string& name) {
    return name == "_" || (!name.empty() && name.back() == '_');
}

std::string pattern_binding_name(const std::string& name) {
    if (name == "_") {
        return {};
    }
    return name.substr(0, name.size() - 1);
}

bool structurally_equal_list(
    const std::vector<ExprPtr>& left,
    const std::vector<ExprPtr>& right);

bool structurally_equal_impl(const Expr& left, const Expr& right) {
    if (left.index() != right.index()) {
        return false;
    }

    return std::visit(
        [&](const auto& lhs) -> bool {
            using T = std::decay_t<decltype(lhs)>;
            const auto& rhs = std::get<T>(right);

            if constexpr (std::is_same_v<T, Symbol>) {
                return lhs.name == rhs.name;
            } else if constexpr (std::is_same_v<T, Number>) {
                return lhs.value == rhs.value;
            } else if constexpr (std::is_same_v<T, Complex>) {
                return lhs.real == rhs.real && lhs.imag == rhs.imag;
            } else if constexpr (std::is_same_v<T, Rational>) {
                return lhs.numerator == rhs.numerator &&
                       lhs.denominator == rhs.denominator;
            } else if constexpr (std::is_same_v<T, Boolean>) {
                return lhs.value == rhs.value;
            } else if constexpr (std::is_same_v<T, String>) {
                return lhs.value == rhs.value;
            } else if constexpr (std::is_same_v<T, FunctionCall>) {
                return lhs.head == rhs.head &&
                       structurally_equal_list(lhs.args, rhs.args);
            } else if constexpr (std::is_same_v<T, FunctionDefinition>) {
                if (lhs.name != rhs.name ||
                    lhs.delayed != rhs.delayed ||
                    lhs.params.size() != rhs.params.size()) {
                    return false;
                }
                for (std::size_t index = 0; index < lhs.params.size(); ++index) {
                    if (lhs.params[index].name != rhs.params[index].name) {
                        return false;
                    }
                    if (!structurally_equal(lhs.params[index].default_value,
                                            rhs.params[index].default_value)) {
                        return false;
                    }
                }
                return structurally_equal(lhs.body, rhs.body);
            } else if constexpr (std::is_same_v<T, Assignment>) {
                return lhs.name == rhs.name &&
                       structurally_equal(lhs.value, rhs.value);
            } else if constexpr (std::is_same_v<T, Rule>) {
                return structurally_equal(lhs.lhs, rhs.lhs) &&
                       structurally_equal(lhs.rhs, rhs.rhs);
            } else if constexpr (std::is_same_v<T, List>) {
                return structurally_equal_list(lhs.elements, rhs.elements);
            } else if constexpr (std::is_same_v<T, Infinity> ||
                                 std::is_same_v<T, ComplexInfinity> ||
                                 std::is_same_v<T, Indeterminate>) {
                return true;
            } else {
                return false;
            }
        },
        left);
}

bool structurally_equal_list(
    const std::vector<ExprPtr>& left,
    const std::vector<ExprPtr>& right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (!structurally_equal(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

bool match_list(
    const std::vector<ExprPtr>& pattern,
    const std::vector<ExprPtr>& expr,
    PatternBindings& bindings);

bool match_pattern(
    const ExprPtr& pattern,
    const ExprPtr& expr,
    PatternBindings& bindings) {
    if (pattern == nullptr || expr == nullptr) {
        return pattern == nullptr && expr == nullptr;
    }

    if (const auto* symbol = std::get_if<Symbol>(&*pattern)) {
        if (is_pattern_symbol_name(symbol->name)) {
            const auto binding_name = pattern_binding_name(symbol->name);
            if (binding_name.empty()) {
                return true;
            }
            auto it = bindings.find(binding_name);
            if (it == bindings.end()) {
                bindings.emplace(binding_name, expr);
                return true;
            }
            return structurally_equal(it->second, expr);
        }
    }

    if (pattern->index() != expr->index()) {
        return false;
    }

    return std::visit(
        [&](const auto& lhs) -> bool {
            using T = std::decay_t<decltype(lhs)>;
            const auto& rhs = std::get<T>(*expr);

            if constexpr (std::is_same_v<T, Symbol>) {
                return lhs.name == rhs.name;
            } else if constexpr (std::is_same_v<T, Number>) {
                return lhs.value == rhs.value;
            } else if constexpr (std::is_same_v<T, Complex>) {
                return lhs.real == rhs.real && lhs.imag == rhs.imag;
            } else if constexpr (std::is_same_v<T, Rational>) {
                return lhs.numerator == rhs.numerator &&
                       lhs.denominator == rhs.denominator;
            } else if constexpr (std::is_same_v<T, Boolean>) {
                return lhs.value == rhs.value;
            } else if constexpr (std::is_same_v<T, String>) {
                return lhs.value == rhs.value;
            } else if constexpr (std::is_same_v<T, FunctionCall>) {
                return lhs.head == rhs.head &&
                       match_list(lhs.args, rhs.args, bindings);
            } else if constexpr (std::is_same_v<T, FunctionDefinition>) {
                if (lhs.name != rhs.name ||
                    lhs.delayed != rhs.delayed ||
                    lhs.params.size() != rhs.params.size()) {
                    return false;
                }
                for (std::size_t index = 0; index < lhs.params.size(); ++index) {
                    if (lhs.params[index].name != rhs.params[index].name) {
                        return false;
                    }
                    if (!match_pattern(lhs.params[index].default_value,
                                       rhs.params[index].default_value,
                                       bindings)) {
                        return false;
                    }
                }
                return match_pattern(lhs.body, rhs.body, bindings);
            } else if constexpr (std::is_same_v<T, Assignment>) {
                return lhs.name == rhs.name &&
                       match_pattern(lhs.value, rhs.value, bindings);
            } else if constexpr (std::is_same_v<T, Rule>) {
                return match_pattern(lhs.lhs, rhs.lhs, bindings) &&
                       match_pattern(lhs.rhs, rhs.rhs, bindings);
            } else if constexpr (std::is_same_v<T, List>) {
                return match_list(lhs.elements, rhs.elements, bindings);
            } else if constexpr (std::is_same_v<T, Infinity> ||
                                 std::is_same_v<T, ComplexInfinity> ||
                                 std::is_same_v<T, Indeterminate>) {
                return true;
            } else {
                return false;
            }
        },
        *pattern);
}

bool match_list(
    const std::vector<ExprPtr>& pattern,
    const std::vector<ExprPtr>& expr,
    PatternBindings& bindings) {
    if (pattern.size() != expr.size()) {
        return false;
    }
    for (std::size_t index = 0; index < pattern.size(); ++index) {
        if (!match_pattern(pattern[index], expr[index], bindings)) {
            return false;
        }
    }
    return true;
}

ExprPtr substitute_pattern_bindings(const ExprPtr& expr, const PatternBindings& bindings) {
    if (expr == nullptr) {
        return nullptr;
    }

    return std::visit(
        [&](const auto& node) -> ExprPtr {
            using T = std::decay_t<decltype(node)>;

            if constexpr (std::is_same_v<T, Symbol>) {
                auto it = bindings.find(node.name);
                return it == bindings.end() ? clone_expr(expr) : clone_expr(it->second);
            } else if constexpr (std::is_same_v<T, FunctionCall>) {
                std::vector<ExprPtr> args;
                args.reserve(node.args.size());
                for (const auto& arg : node.args) {
                    args.push_back(substitute_pattern_bindings(arg, bindings));
                }
                return make_expr<FunctionCall>(node.head, args);
            } else if constexpr (std::is_same_v<T, List>) {
                std::vector<ExprPtr> elements;
                elements.reserve(node.elements.size());
                for (const auto& element : node.elements) {
                    elements.push_back(substitute_pattern_bindings(element, bindings));
                }
                return std::make_shared<Expr>(List{elements});
            } else if constexpr (std::is_same_v<T, Rule>) {
                return make_expr<Rule>(
                    substitute_pattern_bindings(node.lhs, bindings),
                    substitute_pattern_bindings(node.rhs, bindings));
            } else if constexpr (std::is_same_v<T, Assignment>) {
                return make_expr<Assignment>(
                    node.name,
                    substitute_pattern_bindings(node.value, bindings));
            } else if constexpr (std::is_same_v<T, FunctionDefinition>) {
                auto params = node.params;
                for (auto& param : params) {
                    param.default_value = substitute_pattern_bindings(param.default_value, bindings);
                }
                return make_expr<FunctionDefinition>(
                    node.name,
                    params,
                    substitute_pattern_bindings(node.body, bindings),
                    node.delayed);
            } else {
                return clone_expr(expr);
            }
        },
        *expr);
}

RewriteResult rewrite_once_impl(const ExprPtr& expr, const Rule& rule) {
    if (expr == nullptr) {
        return {};
    }

    PatternBindings bindings;
    if (match_pattern(rule.lhs, expr, bindings)) {
        RewriteResult result;
        result.expr = substitute_pattern_bindings(rule.rhs, bindings);
        result.changed = true;
        result.rewrites_applied = 1;
        return result;
    }

    return std::visit(
        [&](const auto& node) -> RewriteResult {
            using T = std::decay_t<decltype(node)>;

            if constexpr (std::is_same_v<T, FunctionCall>) {
                std::vector<ExprPtr> rewritten_args;
                rewritten_args.reserve(node.args.size());
                bool changed = false;
                std::size_t rewrites_applied = 0;
                for (const auto& arg : node.args) {
                    auto rewritten = rewrite_once_impl(arg, rule);
                    changed = changed || rewritten.changed;
                    rewrites_applied += rewritten.rewrites_applied;
                    rewritten_args.push_back(rewritten.changed ? rewritten.expr : arg);
                }
                if (!changed) {
                    return RewriteResult{expr, false, 0};
                }
                RewriteResult result;
                result.expr = make_expr<FunctionCall>(node.head, rewritten_args);
                result.changed = true;
                result.rewrites_applied = rewrites_applied;
                return result;
            } else if constexpr (std::is_same_v<T, List>) {
                std::vector<ExprPtr> rewritten_elements;
                rewritten_elements.reserve(node.elements.size());
                bool changed = false;
                std::size_t rewrites_applied = 0;
                for (const auto& element : node.elements) {
                    auto rewritten = rewrite_once_impl(element, rule);
                    changed = changed || rewritten.changed;
                    rewrites_applied += rewritten.rewrites_applied;
                    rewritten_elements.push_back(rewritten.changed ? rewritten.expr : element);
                }
                if (!changed) {
                    return RewriteResult{expr, false, 0};
                }
                RewriteResult result;
                result.expr = std::make_shared<Expr>(List{rewritten_elements});
                result.changed = true;
                result.rewrites_applied = rewrites_applied;
                return result;
            } else if constexpr (std::is_same_v<T, Rule>) {
                auto lhs = rewrite_once_impl(node.lhs, rule);
                if (lhs.changed) {
                    RewriteResult result;
                    result.expr = make_expr<Rule>(lhs.expr, node.rhs);
                    result.changed = true;
                    result.rewrites_applied = lhs.rewrites_applied;
                    return result;
                }
                auto rhs = rewrite_once_impl(node.rhs, rule);
                if (rhs.changed) {
                    RewriteResult result;
                    result.expr = make_expr<Rule>(node.lhs, rhs.expr);
                    result.changed = true;
                    result.rewrites_applied = rhs.rewrites_applied;
                    return result;
                }
                return RewriteResult{expr, false, 0};
            } else if constexpr (std::is_same_v<T, Assignment>) {
                auto value = rewrite_once_impl(node.value, rule);
                if (!value.changed) {
                    return RewriteResult{expr, false, 0};
                }
                RewriteResult result;
                result.expr = make_expr<Assignment>(node.name, value.expr);
                result.changed = true;
                result.rewrites_applied = value.rewrites_applied;
                return result;
            } else if constexpr (std::is_same_v<T, FunctionDefinition>) {
                for (std::size_t index = 0; index < node.params.size(); ++index) {
                    if (node.params[index].default_value == nullptr) {
                        continue;
                    }
                    auto rewritten = rewrite_once_impl(node.params[index].default_value, rule);
                    if (rewritten.changed) {
                        auto params = node.params;
                        params[index].default_value = rewritten.expr;
                        RewriteResult result;
                        result.expr = make_expr<FunctionDefinition>(
                            node.name,
                            params,
                            node.body,
                            node.delayed);
                        result.changed = true;
                        result.rewrites_applied = rewritten.rewrites_applied;
                        return result;
                    }
                }
                auto body = rewrite_once_impl(node.body, rule);
                if (!body.changed) {
                    return RewriteResult{expr, false, 0};
                }
                RewriteResult result;
                result.expr = make_expr<FunctionDefinition>(
                    node.name,
                    node.params,
                    body.expr,
                    node.delayed);
                result.changed = true;
                result.rewrites_applied = body.rewrites_applied;
                return result;
            } else {
                return RewriteResult{expr, false, 0};
            }
        },
        *expr);
}

}  // namespace

bool structurally_equal(const ExprPtr& left, const ExprPtr& right) {
    if (left == right) {
        return true;
    }
    if (left == nullptr || right == nullptr) {
        return left == nullptr && right == nullptr;
    }
    return structurally_equal_impl(*left, *right);
}

bool matches_pattern(const ExprPtr& pattern, const ExprPtr& expr) {
    PatternBindings bindings;
    return match_pattern(pattern, expr, bindings);
}

RewriteResult rewrite_once(const ExprPtr& expr, const Rule& rule) {
    auto result = rewrite_once_impl(expr, rule);
    if (result.expr == nullptr) {
        result.expr = expr;
    }
    return result;
}

RewriteResult rewrite_repeated(
    const ExprPtr& expr,
    const Rule& rule,
    std::size_t max_rewrites) {
    RewriteResult accumulated;
    accumulated.expr = expr;

    for (std::size_t iteration = 0; iteration < max_rewrites; ++iteration) {
        auto step = rewrite_once(accumulated.expr, rule);
        if (!step.changed) {
            break;
        }
        accumulated.expr = std::move(step.expr);
        accumulated.changed = true;
        accumulated.rewrites_applied += step.rewrites_applied;
    }

    return accumulated;
}

RewriteResult rewrite_repeated(
    const ExprPtr& expr,
    const Rule& rule,
    EvaluationContext& ctx,
    std::size_t max_rewrites) {
    RewriteResult accumulated;
    accumulated.expr = expr;

    for (std::size_t iteration = 0; iteration < max_rewrites; ++iteration) {
        auto step = rewrite_once(accumulated.expr, rule);
        if (!step.changed) {
            break;
        }
        ctx.consume_evaluation_step();
        accumulated.expr = std::move(step.expr);
        accumulated.changed = true;
        accumulated.rewrites_applied += step.rewrites_applied;
    }

    return accumulated;
}

std::optional<ExprPtr> rewrite_normalized_head(
    const FunctionCall& func,
    EvaluationContext& ctx) {
    const auto* specs = ctx.function_registry().find_head_rewrites(func.head);
    if (specs == nullptr) {
        return std::nullopt;
    }

    for (const auto& spec : *specs) {
        if (spec.stage != RewriteStage::normalized_head) {
            continue;
        }
        if (auto rewritten = spec.handler(func, ctx)) {
            return rewritten;
        }
    }

    return std::nullopt;
}

std::optional<ExprPtr> rewrite_normalized_arithmetic_head(
    const FunctionCall& func,
    EvaluationContext& ctx) {
    if ((func.head != "Plus" && func.head != "Times") || contains_list_argument(func.args)) {
        return std::nullopt;
    }

    bool changed = false;
    ExprPtr rewritten;
    if (func.head == "Plus") {
        rewritten = build_plus_bucket_expr(func.args, changed);
    } else {
        rewritten = build_times_bucket_expr(func.args, changed);
    }

    if (!changed) {
        const auto original = normalize_expr(make_fcall(func.head, func.args));
        if (!structurally_equal(original, rewritten)) {
            changed = true;
        }
    }
    if (!changed) {
        return std::nullopt;
    }

    ctx.consume_evaluation_step();
    return rewritten;
}

std::optional<ExprPtr> rewrite_normalized_power_identity_head(
    const FunctionCall& func,
    EvaluationContext& ctx) {
    if (func.head != "Power" || func.args.size() != 2 || contains_list_argument(func.args)) {
        return std::nullopt;
    }

    const auto& base = func.args[0];
    const auto& exponent = func.args[1];

    if (std::holds_alternative<Number>(*exponent)) {
        const double exponent_value = get_number_value(exponent);
        if (exponent_value == 0.0) {
            ctx.consume_evaluation_step();
            return make_expr<Number>(1.0);
        }
        if (exponent_value == 1.0) {
            ctx.consume_evaluation_step();
            return base;
        }
    }

    if (std::holds_alternative<Number>(*base) && get_number_value(base) == 1.0) {
        ctx.consume_evaluation_step();
        return make_expr<Number>(1.0);
    }

    return std::nullopt;
}

std::optional<ExprPtr> rewrite_normalized_symbolic_coefficient_head(
    const FunctionCall& func,
    EvaluationContext& ctx) {
    if (func.head != "Plus" || contains_list_argument(func.args)) {
        return std::nullopt;
    }

    struct MonomialBucket {
        ExprPtr basis;
        ScalarCoefficient coefficient;
    };

    std::unordered_map<std::string, std::size_t> bucket_index;
    std::vector<MonomialBucket> buckets;
    std::vector<ExprPtr> opaque_terms;
    bool changed = false;

    for (const auto& arg : func.args) {
        SupportedMonomialTerm term;
        if (!extract_supported_monomial_term(arg, term)) {
            opaque_terms.push_back(arg);
            continue;
        }

        const auto key = to_string_raw(normalize_expr(term.basis));
        const auto [it, inserted] = bucket_index.emplace(key, buckets.size());
        if (inserted) {
            buckets.push_back(MonomialBucket{normalize_expr(term.basis), term.coefficient});
            continue;
        }

        buckets[it->second].coefficient.add(term.coefficient);
        changed = true;
    }

    std::vector<ExprPtr> rebuilt_terms;
    rebuilt_terms.reserve(opaque_terms.size() + buckets.size());
    rebuilt_terms.insert(rebuilt_terms.end(), opaque_terms.begin(), opaque_terms.end());

    for (const auto& bucket : buckets) {
        auto rebuilt = rebuild_supported_monomial_term(
            SupportedMonomialTerm{bucket.basis, bucket.coefficient});
        if (rebuilt == nullptr) {
            changed = true;
            continue;
        }
        rebuilt_terms.push_back(std::move(rebuilt));
    }

    ExprPtr rewritten;
    if (rebuilt_terms.empty()) {
        rewritten = make_expr<Number>(0.0);
    } else if (rebuilt_terms.size() == 1) {
        rewritten = rebuilt_terms.front();
    } else {
        rewritten = normalize_expr(make_fcall("Plus", rebuilt_terms));
    }

    const auto original = normalize_expr(make_fcall("Plus", func.args));
    if (!changed && !structurally_equal(original, rewritten)) {
        changed = true;
    }
    if (!changed) {
        return std::nullopt;
    }

    ctx.consume_evaluation_step();
    return rewritten;
}

std::optional<ExprPtr> rewrite_normalized_algebraic_head(
    const FunctionCall& func,
    EvaluationContext& ctx) {
    if (contains_list_argument(func.args)) {
        return std::nullopt;
    }

    if (func.head == "Times") {
        std::map<std::string, double> symbol_powers;
        std::map<std::string, std::size_t> symbol_term_counts;
        std::vector<ExprPtr> opaque_terms;
        bool changed = false;

        for (const auto& arg : func.args) {
            std::string symbol_name;
            double exponent = 0.0;
            if (extract_symbol_power_term(arg, symbol_name, exponent)) {
                symbol_powers[symbol_name] += exponent;
                symbol_term_counts[symbol_name] += 1;
                if (!(std::holds_alternative<Symbol>(*arg) && exponent == 1.0)) {
                    changed = true;
                }
                continue;
            }
            opaque_terms.push_back(arg);
        }

        std::size_t repeated_symbol_families = 0;
        for (const auto& [symbol_name, count] : symbol_term_counts) {
            (void)symbol_name;
            if (count > 1) {
                repeated_symbol_families += 1;
            }
        }
        if (repeated_symbol_families > 1) {
            return std::nullopt;
        }

        std::vector<ExprPtr> rebuilt_terms;
        rebuilt_terms.reserve(symbol_powers.size() + opaque_terms.size());
        for (const auto& [symbol_name, exponent] : symbol_powers) {
            if (exponent == 0.0) {
                changed = true;
                continue;
            }
            if (exponent == 1.0) {
                rebuilt_terms.push_back(make_expr<Symbol>(symbol_name));
            } else {
                rebuilt_terms.push_back(
                    make_fcall("Power", {make_expr<Symbol>(symbol_name), make_expr<Number>(exponent)}));
            }
        }
        rebuilt_terms.insert(rebuilt_terms.end(), opaque_terms.begin(), opaque_terms.end());

        ExprPtr rewritten;
        if (rebuilt_terms.empty()) {
            rewritten = make_expr<Number>(1.0);
        } else if (rebuilt_terms.size() == 1) {
            rewritten = rebuilt_terms.front();
        } else {
            rewritten = normalize_expr(make_fcall("Times", rebuilt_terms));
        }

        const auto original = normalize_expr(make_fcall("Times", func.args));
        if (!changed && !structurally_equal(original, rewritten)) {
            changed = true;
        }
        if (!changed) {
            return std::nullopt;
        }

        ctx.consume_evaluation_step();
        return rewritten;
    }

    if (func.head == "Power" && func.args.size() == 2) {
        const auto* nested_power = std::get_if<FunctionCall>(func.args[0].get());
        if (nested_power == nullptr ||
            nested_power->head != "Power" ||
            nested_power->args.size() != 2 ||
            !std::holds_alternative<Number>(*nested_power->args[1]) ||
            !std::holds_alternative<Number>(*func.args[1])) {
            return std::nullopt;
        }

        ctx.consume_evaluation_step();
        return make_fcall(
            "Power",
            {nested_power->args[0],
             make_expr<Number>(
                 get_number_value(nested_power->args[1]) * get_number_value(func.args[1]))});
    }

    return std::nullopt;
}

}  // namespace aleph3::kernel
