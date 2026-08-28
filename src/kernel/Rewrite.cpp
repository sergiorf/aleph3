#include "kernel/Rewrite.hpp"

#include "kernel/EvaluationContext.hpp"
#include "kernel/FunctionRegistry.hpp"
#include "evaluator/Evaluator.hpp"
#include "expr/ExprStructural.hpp"
#include "expr/ExprUtils.hpp"
#include "normalizer/Normalizer.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <unordered_map>
#include <type_traits>
#include <utility>

namespace aleph3::kernel {

namespace {

using PatternBindings = std::unordered_map<std::string, ExprPtr>;
constexpr std::string_view BUILTIN_REWRITE_PROVIDER = "kernel-rewrite";

bool is_integral(double value) {
    return std::floor(value) == value;
}

std::optional<int64_t> exact_integer_value(const ExprPtr& expr) {
    if (const auto* number = std::get_if<Number>(expr.get())) {
        if (!std::isfinite(number->value) || !is_integral(number->value)) {
            return std::nullopt;
        }
        if (number->value < static_cast<double>(std::numeric_limits<int64_t>::min()) ||
            number->value > static_cast<double>(std::numeric_limits<int64_t>::max())) {
            return std::nullopt;
        }
        return static_cast<int64_t>(number->value);
    }

    if (const auto* rational = std::get_if<Rational>(expr.get());
        rational != nullptr && rational->denominator == 1) {
        return rational->numerator;
    }

    return std::nullopt;
}

bool contains_list_expr(const ExprPtr& expr) {
    if (expr == nullptr) {
        return false;
    }
    if (std::holds_alternative<List>(*expr)) {
        return true;
    }
    if (const auto* call = std::get_if<FunctionCall>(expr.get())) {
        if (call->head == "List") {
            return true;
        }
        for (const auto& arg : call->args) {
            if (contains_list_expr(arg)) {
                return true;
            }
        }
    }
    return false;
}

ExprPtr clone_expr(const ExprPtr& expr) {
    return expr == nullptr ? nullptr : std::make_shared<Expr>(*expr);
}

void ensure_symbol_metadata(
    EvaluationContext& ctx,
    const std::string& name,
    symbols::DefinitionOrigin origin,
    std::string provider,
    std::string documentation = {}) {
    if (auto* existing = ctx.symbol_metadata.lookup(name)) {
        if (existing->documentation.empty() && !documentation.empty()) {
            existing->documentation = std::move(documentation);
        }
        if (existing->provider.empty() && !provider.empty()) {
            existing->provider = std::move(provider);
        }
        if (existing->origin == symbols::DefinitionOrigin::user &&
            origin != symbols::DefinitionOrigin::user) {
            existing->origin = origin;
        }
        return;
    }

    ctx.symbol_metadata.set(name, symbols::SymbolMetadata{
        name,
        {},
        std::move(documentation),
        origin,
        std::move(provider)});
}

void ensure_definition_record(
    EvaluationContext& ctx,
    const std::string& name,
    symbols::SymbolDefinitionKind kind,
    symbols::DefinitionOrigin origin,
    std::string provider) {
    ctx.definition_records.add_unique(name, symbols::SymbolDefinitionRecord{
        kind,
        origin,
        std::move(provider)});
}

std::string rewrite_provider_for(const HeadRewriteSpec& spec) {
    if (spec.metadata.source == RegistrationSource::pack) {
        return spec.metadata.owning_package;
    }
    return std::string(BUILTIN_REWRITE_PROVIDER);
}

symbols::DefinitionOrigin rewrite_origin_for(const HeadRewriteSpec& spec) {
    return spec.metadata.source == RegistrationSource::pack
        ? symbols::DefinitionOrigin::pack
        : symbols::DefinitionOrigin::builtin;
}

void sync_registered_head_rewrite_metadata(
    const HeadRewriteSpec& spec,
    EvaluationContext& ctx) {
    const auto origin = rewrite_origin_for(spec);
    auto provider = rewrite_provider_for(spec);
    ensure_symbol_metadata(
        ctx,
        spec.metadata.name,
        origin,
        provider,
        spec.metadata.documentation);
    ensure_definition_record(
        ctx,
        spec.metadata.name,
        symbols::SymbolDefinitionKind::rewrite_rule,
        origin,
        std::move(provider));
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

    void multiply_number(double value) {
        if (exact && is_integral(value)) {
            auto [nn, dd] = normalize_rational(
                numerator * static_cast<int64_t>(value),
                denominator);
            numerator = nn;
            denominator = dd;
            return;
        }

        if (exact) {
            approximate = static_cast<double>(numerator) / denominator;
            exact = false;
        }
        approximate *= value;
    }

    void multiply_rational(int64_t num, int64_t den) {
        if (exact) {
            auto [nn, dd] = normalize_rational(numerator * num, denominator * den);
            numerator = nn;
            denominator = dd;
            return;
        }
        approximate *= static_cast<double>(num) / den;
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

struct SupportedCoefficientTerm {
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

bool is_collectable_symbolic_body(const ExprPtr& expr) {
    if (contains_list_expr(expr)) {
        return false;
    }

    if (std::holds_alternative<Symbol>(*expr)) {
        return true;
    }

    const auto* call = std::get_if<FunctionCall>(expr.get());
    return call != nullptr && call->head != "List";
}

bool multiply_numeric_factor_into_coefficient(const ExprPtr& expr, ScalarCoefficient& coefficient) {
    if (std::holds_alternative<Number>(*expr)) {
        coefficient.multiply_number(std::get<Number>(*expr).value);
        return true;
    }
    if (std::holds_alternative<Rational>(*expr)) {
        const auto& rational = std::get<Rational>(*expr);
        coefficient.multiply_rational(rational.numerator, rational.denominator);
        return true;
    }
    return false;
}

bool extract_supported_coefficient_term(
    const ExprPtr& expr,
    SupportedCoefficientTerm& term) {
    if (std::holds_alternative<Number>(*expr) || std::holds_alternative<Rational>(*expr)) {
        return false;
    }

    const auto* times = std::get_if<FunctionCall>(expr.get());
    if (times == nullptr || times->head != "Times") {
        if (is_collectable_symbolic_body(expr)) {
            term.basis = normalize_expr(expr);
            term.coefficient.numerator = 1;
            term.coefficient.denominator = 1;
            return true;
        }
        return false;
    }

    if (times->args.empty()) {
        return false;
    }

    ScalarCoefficient coefficient;
    coefficient.numerator = 1;
    coefficient.denominator = 1;
    std::vector<ExprPtr> basis_factors;
    basis_factors.reserve(times->args.size());
    for (const auto& arg : times->args) {
        if (multiply_numeric_factor_into_coefficient(arg, coefficient)) {
            continue;
        }

        if (!is_collectable_symbolic_body(arg)) {
            return false;
        }
        basis_factors.push_back(arg);
    }

    if (basis_factors.empty()) {
        return false;
    }

    term.basis = basis_factors.size() == 1
        ? basis_factors.front()
        : normalize_expr(make_fcall("Times", basis_factors));
    term.coefficient = coefficient;
    return true;
}

ExprPtr rebuild_supported_coefficient_term(const SupportedCoefficientTerm& term) {
    if (term.coefficient.is_zero()) {
        return nullptr;
    }
    if (term.coefficient.is_one()) {
        return clone_expr(term.basis);
    }
    return normalize_expr(make_fcall("Times", {term.coefficient.to_expr(), term.basis}));
}

bool is_pattern_symbol_name(const std::string& name) {
    return name == "_" || name.find('_') != std::string::npos;
}

struct TimesPowerFactor {
    ExprPtr base;
    int64_t exponent = 1;
};

bool extract_exact_integer_power_factor(const ExprPtr& expr, TimesPowerFactor& factor) {
    if (std::holds_alternative<Number>(*expr) ||
        std::holds_alternative<Rational>(*expr) ||
        contains_list_expr(expr)) {
        return false;
    }

    const auto* power = std::get_if<FunctionCall>(expr.get());
    if (power != nullptr && power->head == "Power" && power->args.size() == 2) {
        auto exponent = exact_integer_value(power->args[1]);
        if (!exponent.has_value() || contains_list_expr(power->args[0])) {
            return false;
        }
        factor.base = normalize_expr(power->args[0]);
        factor.exponent = *exponent;
        return true;
    }

    factor.base = expr;
    factor.exponent = 1;
    return true;
}

bool is_explicit_numeric_zero(const ExprPtr& expr) {
    if (const auto* number = std::get_if<Number>(expr.get())) {
        return number->value == 0.0;
    }
    if (const auto* rational = std::get_if<Rational>(expr.get())) {
        return rational->numerator == 0;
    }
    if (const auto* complex = std::get_if<Complex>(expr.get())) {
        return complex->real == 0.0 && complex->imag == 0.0;
    }
    return false;
}

std::string pattern_binding_name(const std::string& name) {
    const auto separator = name.find('_');
    if (separator == 0 || separator == std::string::npos) {
        return {};
    }
    return name.substr(0, separator);
}

std::string pattern_type_name(const std::string& name) {
    const auto separator = name.find('_');
    if (separator == std::string::npos) {
        return {};
    }
    if (separator + 1 < name.size() && name[separator + 1] == '_') {
        throw_unsupported_construct("Sequence patterns are not supported");
    }
    return name.substr(separator + 1);
}

bool matches_pattern_type(const std::string& type, const ExprPtr& expr) {
    if (type.empty()) return true;
    if (type == "Integer") {
        const auto* number = std::get_if<Number>(&*expr);
        return number != nullptr && is_integral(number->value);
    }
    if (type == "Rational") return std::holds_alternative<Rational>(*expr);
    if (type == "Real") return std::holds_alternative<Number>(*expr);
    if (type == "Symbol") return std::holds_alternative<Symbol>(*expr);
    if (type == "String") return std::holds_alternative<String>(*expr);
    if (type == "Boolean") return std::holds_alternative<Boolean>(*expr);
    if (type == "Function") return std::holds_alternative<FunctionCall>(*expr);
    throw_unsupported_construct("Unknown pattern type constraint: " + type);
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
            if (!matches_pattern_type(pattern_type_name(symbol->name), expr)) {
                return false;
            }
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

void validate_pattern_contract(const ExprPtr& pattern) {
    if (pattern == nullptr) return;
    if (const auto* symbol = std::get_if<Symbol>(&*pattern)) {
        if (is_pattern_symbol_name(symbol->name)) {
            static_cast<void>(matches_pattern_type(pattern_type_name(symbol->name), pattern));
        }
        return;
    }
    if (const auto* call = std::get_if<FunctionCall>(&*pattern)) {
        if (call->head == "Condition") {
            if (call->args.size() != 2) {
                throw_invalid_form("Condition patterns require a pattern and a predicate");
            }
            if (const auto* nested = std::get_if<FunctionCall>(call->args[0].get());
                nested != nullptr && nested->head == "Condition") {
                throw_unsupported_construct("Nested conditional patterns are not supported");
            }
            validate_pattern_contract(call->args[0]);
            return;
        }
        for (const auto& argument : call->args) validate_pattern_contract(argument);
        return;
    }
    if (const auto* list = std::get_if<List>(&*pattern)) {
        for (const auto& element : list->elements) validate_pattern_contract(element);
    }
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

bool match_conditional_pattern(
    const ExprPtr& pattern,
    const ExprPtr& expr,
    PatternBindings& bindings,
    EvaluationContext* ctx) {
    const auto* condition = std::get_if<FunctionCall>(pattern.get());
    if (condition == nullptr || condition->head != "Condition") {
        return match_pattern(pattern, expr, bindings);
    }
    if (condition->args.size() != 2) {
        throw_invalid_form("Condition patterns require a pattern and a predicate");
    }
    if (ctx == nullptr) {
        throw_unsupported_construct("Conditional patterns require an evaluation context");
    }
    PatternBindings candidate = bindings;
    if (!match_pattern(condition->args[0], expr, candidate)) {
        return false;
    }
    const auto predicate = substitute_pattern_bindings(condition->args[1], candidate);
    const auto evaluated = evaluate(predicate, *ctx);
    const auto* boolean = std::get_if<Boolean>(evaluated.get());
    if (boolean == nullptr || !boolean->value) {
        return false;
    }
    bindings = std::move(candidate);
    return true;
}

RewriteResult rewrite_once_impl(
    const ExprPtr& expr,
    const Rule& rule,
    EvaluationContext* ctx,
    const RewriteTraversal& traversal,
    std::size_t depth) {
    if (expr == nullptr) {
        return {};
    }

    PatternBindings bindings;
    if (depth >= traversal.min_depth && depth <= traversal.max_depth &&
        match_conditional_pattern(rule.lhs, expr, bindings, ctx)) {
        RewriteResult result;
        result.expr = substitute_pattern_bindings(rule.rhs, bindings);
        result.changed = true;
        result.rewrites_applied = 1;
        return result;
    }

    if (depth >= traversal.max_depth) {
        return RewriteResult{expr, false, 0};
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
                    auto rewritten = rewrite_once_impl(arg, rule, ctx, traversal, depth + 1);
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
                    auto rewritten = rewrite_once_impl(element, rule, ctx, traversal, depth + 1);
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
                auto lhs = rewrite_once_impl(node.lhs, rule, ctx, traversal, depth + 1);
                if (lhs.changed) {
                    RewriteResult result;
                    result.expr = make_expr<Rule>(lhs.expr, node.rhs);
                    result.changed = true;
                    result.rewrites_applied = lhs.rewrites_applied;
                    return result;
                }
                auto rhs = rewrite_once_impl(node.rhs, rule, ctx, traversal, depth + 1);
                if (rhs.changed) {
                    RewriteResult result;
                    result.expr = make_expr<Rule>(node.lhs, rhs.expr);
                    result.changed = true;
                    result.rewrites_applied = rhs.rewrites_applied;
                    return result;
                }
                return RewriteResult{expr, false, 0};
            } else if constexpr (std::is_same_v<T, Assignment>) {
                auto value = rewrite_once_impl(node.value, rule, ctx, traversal, depth + 1);
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
                    auto rewritten = rewrite_once_impl(
                        node.params[index].default_value, rule, ctx, traversal, depth + 1);
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
                auto body = rewrite_once_impl(node.body, rule, ctx, traversal, depth + 1);
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
    return ::aleph3::structural_equal(left, right);
}

bool matches_pattern(const ExprPtr& pattern, const ExprPtr& expr) {
    validate_pattern_contract(pattern);
    PatternBindings bindings;
    return match_pattern(pattern, expr, bindings);
}

bool matches_pattern(
    const ExprPtr& pattern,
    const ExprPtr& expr,
    EvaluationContext& ctx) {
    validate_pattern_contract(pattern);
    PatternBindings bindings;
    return match_conditional_pattern(pattern, expr, bindings, &ctx);
}

RewriteResult rewrite_once(const ExprPtr& expr, const Rule& rule) {
    return rewrite_once(expr, rule, RewriteTraversal{});
}

RewriteResult rewrite_once(
    const ExprPtr& expr,
    const Rule& rule,
    RewriteTraversal traversal) {
    validate_pattern_contract(rule.lhs);
    auto result = rewrite_once_impl(expr, rule, nullptr, traversal, 0);
    if (result.expr == nullptr) {
        result.expr = expr;
    }
    return result;
}

RewriteResult rewrite_once(
    const ExprPtr& expr,
    const Rule& rule,
    EvaluationContext& ctx) {
    return rewrite_once(expr, rule, ctx, RewriteTraversal{});
}

RewriteResult rewrite_once(
    const ExprPtr& expr,
    const Rule& rule,
    EvaluationContext& ctx,
    RewriteTraversal traversal) {
    validate_pattern_contract(rule.lhs);
    auto result = rewrite_once_impl(expr, rule, &ctx, traversal, 0);
    if (result.expr == nullptr) {
        result.expr = expr;
    }
    return result;
}

RewriteResult rewrite_repeated(
    const ExprPtr& expr,
    const Rule& rule,
    std::size_t max_rewrites) {
    return rewrite_repeated(expr, rule, RewriteTraversal{}, max_rewrites);
}

RewriteResult rewrite_repeated(
    const ExprPtr& expr,
    const Rule& rule,
    RewriteTraversal traversal,
    std::size_t max_rewrites) {
    RewriteResult accumulated;
    accumulated.expr = expr;

    for (std::size_t iteration = 0; iteration < max_rewrites; ++iteration) {
        auto step = rewrite_once(accumulated.expr, rule, traversal);
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
    return rewrite_repeated(expr, rule, ctx, RewriteTraversal{}, max_rewrites);
}

RewriteResult rewrite_repeated(
    const ExprPtr& expr,
    const Rule& rule,
    EvaluationContext& ctx,
    RewriteTraversal traversal,
    std::size_t max_rewrites) {
    RewriteResult accumulated;
    accumulated.expr = expr;

    for (std::size_t iteration = 0; iteration < max_rewrites; ++iteration) {
        auto step = rewrite_once(accumulated.expr, rule, ctx, traversal);
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
        sync_registered_head_rewrite_metadata(spec, ctx);
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
            const auto nonzero = ctx.assumptions.evaluate_predicate("NonZeroQ", base);
            if (nonzero.has_value() && *nonzero) {
                ctx.consume_evaluation_step();
                return make_expr<Number>(1.0);
            }
            return std::nullopt;
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
        SupportedCoefficientTerm term;
        if (!extract_supported_coefficient_term(arg, term)) {
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
        auto rebuilt = rebuild_supported_coefficient_term(
            SupportedCoefficientTerm{bucket.basis, bucket.coefficient});
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
        struct PowerBucket {
            ExprPtr base;
            int64_t exponent = 0;
            std::size_t term_count = 0;
            std::vector<ExprPtr> original_terms;
        };

        std::map<std::string, PowerBucket> power_buckets;
        std::vector<ExprPtr> opaque_terms;
        bool changed = false;

        for (const auto& arg : func.args) {
            TimesPowerFactor factor;
            if (!extract_exact_integer_power_factor(arg, factor)) {
                opaque_terms.push_back(arg);
                continue;
            }

            const auto key = to_string_raw(normalize_expr(factor.base));
            auto [it, inserted] = power_buckets.emplace(
                key,
                PowerBucket{normalize_expr(factor.base), 0, 0, {}});
            it->second.exponent += factor.exponent;
            it->second.term_count += 1;
            it->second.original_terms.push_back(arg);
        }

        std::vector<ExprPtr> rebuilt_terms;
        rebuilt_terms.reserve(power_buckets.size() + opaque_terms.size());
        for (const auto& [key, bucket] : power_buckets) {
            (void)key;
            if (bucket.exponent == 0) {
                if (!is_explicit_numeric_zero(bucket.base)) {
                    changed = true;
                    continue;
                }
                rebuilt_terms.insert(
                    rebuilt_terms.end(),
                    bucket.original_terms.begin(),
                    bucket.original_terms.end());
                continue;
            }

            if (bucket.term_count == 1 &&
                bucket.exponent == 1 &&
                structurally_equal(bucket.original_terms.front(), bucket.base)) {
                rebuilt_terms.push_back(bucket.original_terms.front());
                continue;
            }

            if (bucket.exponent == 1) {
                rebuilt_terms.push_back(bucket.base);
                if (bucket.term_count > 1 ||
                    !structurally_equal(bucket.original_terms.front(), bucket.base)) {
                    changed = true;
                }
            } else {
                auto rebuilt_power = make_fcall(
                    "Power",
                    {bucket.base, make_expr<Number>(static_cast<double>(bucket.exponent))});
                if (bucket.term_count > 1 ||
                    !structurally_equal(bucket.original_terms.front(), rebuilt_power)) {
                    changed = true;
                }
                rebuilt_terms.push_back(std::move(rebuilt_power));
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
