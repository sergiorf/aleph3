/*
 * Aleph3 Evaluator
 * ----------------
 * This header defines the core evaluation logic for Aleph3, a modern C++20-based computer algebra system.
 * The evaluator traverses and computes the value of Expr trees, handling symbolic and numeric computation,
 * function application, user-defined functions, and built-in mathematical operations.
 *
 * Features:
 * - Recursive evaluation of expressions
 * - Support for numbers, rationals, booleans, lists, and user-defined functions
 * - Built-in function and operator handling (arithmetic, comparison, etc.)
 * - Extensible via function registry and simplification rules
 *
 * See README.md for project overview.
 */
#pragma once

#include "expr/Expr.hpp"
#include "evaluator/EvaluationContext.hpp"
#include "evaluator/FunctionRegistry.hpp"
#include "evaluator/SimplificationRules.hpp"
#include "normalizer/Normalizer.hpp"
#include "util/Overloaded.hpp"
#include "expr/ExprUtils.hpp"
#include "transforms/Transforms.hpp"
#include "ExtraMath.hpp"
#include "Constants.hpp"
#include "util/Logging.hpp"

#include <stdexcept>
#include <cmath>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <unordered_set>

namespace aleph3 {

ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx);

ExprPtr evaluate_polynomial_function(const FunctionCall& func, EvaluationContext& ctx);

inline bool is_polynomial_function(const std::string& name) {
    static const std::unordered_set<std::string> poly_functions = {
        "Expand", "Factor", "Collect", "GCD", "PolynomialQuotient"
    };
    return poly_functions.count(name) > 0;
}

inline std::string expr_to_key(const ExprPtr& expr) {
    auto norm = normalize_expr(expr);
    std::string s = to_string_raw(norm);
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    return s;
}

inline ExprPtr evaluate_function(const FunctionCall& func, EvaluationContext& ctx) {
    const std::string& name = func.head;
    size_t nargs = func.args.size();

    // 1. Try FunctionRegistry (for extensible built-ins)
    auto& registry = FunctionRegistry::instance();
    if (registry.has_function(name)) {
        auto handler = registry.get_function(name);
        return handler(func, ctx);
    }

    // 2. Special forms
    if (name == "If") {
        if (nargs != 3) throw std::runtime_error("If expects exactly 3 arguments");
        auto condition = evaluate(func.args[0], ctx);
        if (std::holds_alternative<Boolean>(*condition)) {
            bool cond = std::get<Boolean>(*condition).value;
            return cond ? evaluate(func.args[1], ctx) : evaluate(func.args[2], ctx);
        }
        return make_expr<FunctionCall>(name, func.args);
    }
    if (name == "Expand") {
        if (nargs != 1) throw std::runtime_error("Expand expects exactly one argument");
        auto arg = evaluate(func.args[0], ctx);
        return expand(arg);
    }
    if (name == "Negate") {
        if (nargs != 1) throw std::runtime_error("Negate expects exactly 1 argument");
        auto arg = evaluate(func.args[0], ctx);
        if (std::holds_alternative<Number>(*arg)) {
            return make_expr<Number>(-get_number_value(arg));
        }
        // If arg is Complex, negate both real and imaginary parts
        if (std::holds_alternative<Complex>(*arg)) {
            const auto& c = std::get<Complex>(*arg);
            return make_expr<Complex>(-c.real, -c.imag);
        }
        // If arg is already Times(-1, ...), flatten
        if (auto inner = std::get_if<FunctionCall>(arg.get())) {
            if (inner->head == "Times" && !inner->args.empty()) {
                if (auto n = std::get_if<Number>(inner->args[0].get()); n && n->value == -1) {
                    // Already normalized
                    return arg;
                }
            }
        }
        // Otherwise, return Times(-1, arg)
        return make_fcall("Times", { make_expr<Number>(-1), arg });
    }

    // 3. Built-in function maps
    static const std::unordered_map<std::string, std::function<double(double)>> unary_functions = {
        {"Sin",   [](double x) { return std::sin(x); }},
        {"Cos",   [](double x) { return std::cos(x); }},
        {"Tan",   [](double x) { return std::tan(x); }},
        {"Csc",   [](double x) { return csc(x); }},
        {"Sec",   [](double x) { return sec(x); }},
        {"Sinh",  [](double x) { return std::sinh(x); }},
        {"Cosh",  [](double x) { return std::cosh(x); }},
        {"Tanh",  [](double x) { return std::tanh(x); }},
        {"Coth",  [](double x) { return coth(x); }},
        {"Sech",  [](double x) { return sech(x); }},
        {"Csch",  [](double x) { return csch(x); }},
        {"Cot",   [](double x) { return cot(x); }},
        {"Abs",   [](double x) { return std::fabs(x); }},
        {"Sqrt",  [](double x) { return std::sqrt(x); }},
        {"Exp",   [](double x) { return std::exp(x); }},
        {"Log",   [](double x) { return std::log(x); }},
        {"Floor", [](double x) { return std::floor(x); }},
        {"Ceiling",[](double x) { return std::ceil(x); }},
        {"Round", [](double x) { return std::round(x); }},
        {"ArcSin",[](double x) { return std::asin(x); }},
        {"ArcCos",[](double x) { return std::acos(x); }},
        {"ArcTan",[](double x) { return std::atan(x); }},
        {"Gamma", [](double x) { return std::tgamma(x); }}
    };
    static const std::unordered_map<std::string, std::function<double(double, double)>> binary_functions = {
        {"Plus",   [](double a, double b) { return a + b; }},
        {"Minus",  [](double a, double b) { return a - b; }},
        {"Times",  [](double a, double b) { return a * b; }},
        {"Divide", [](double a, double b) { return a / b; }},
        {"Power",  [](double a, double b) { return std::pow(a, b); }},
        {"Log",    [](double b, double x) { return std::log(x) / std::log(b); }},
        {"ArcTan", [](double x, double y) { return std::atan2(y, x); }}
    };
    static const std::unordered_map<std::string, std::function<bool(double, double)>> comparison_functions = {
        {"Equal",         [](double a, double b) { return a == b; }},
        {"NotEqual",      [](double a, double b) { return a != b; }},
        {"Less",          [](double a, double b) { return a < b; }},
        {"Greater",       [](double a, double b) { return a > b; }},
        {"LessEqual",     [](double a, double b) { return a <= b; }},
        {"GreaterEqual",  [](double a, double b) { return a >= b; }}
    };

    static const std::unordered_map<std::string, std::unordered_map<std::string, ExprPtr>> known_symbolic_unary = {
        {"Sin", {
            {"0", make_expr<Number>(0.0)},
            {"Pi", make_expr<Number>(0.0)},
            {"2*Pi", make_expr<Number>(0.0)},
            {"-1*Pi", make_expr<Number>(0.0)},
            {"Pi/2", make_expr<Number>(1.0)},
            {"-1*Pi/2", make_expr<Number>(-1.0)},
            {"Pi/4", make_expr<Number>(std::sqrt(2.0) / 2.0)},
            {"-1*Pi/4", make_expr<Number>(-std::sqrt(2.0) / 2.0)},
            {"3*Pi/2", make_expr<Number>(-1.0)},
            {"-1*3*Pi/2", make_expr<Number>(1.0)}
        }},
        {"Cos", {
            {"0", make_expr<Number>(1.0)},
            {"Pi", make_expr<Number>(-1.0)},
            {"2*Pi", make_expr<Number>(1.0)},
            {"-1*Pi", make_expr<Number>(-1.0)},
            {"Pi/2", make_expr<Number>(0.0)},
            {"-1*Pi/2", make_expr<Number>(0.0)},
            {"Pi/4", make_expr<Number>(std::sqrt(2.0) / 2.0)},
            {"-1*Pi/4", make_expr<Number>(std::sqrt(2.0) / 2.0)},
            {"3*Pi/2", make_expr<Number>(0.0)},
            {"-1*3*Pi/2", make_expr<Number>(0.0)}
        }},
        {"Tan", {
            {"0", make_expr<Number>(0.0)},
            {"Pi", make_expr<Number>(0.0)},
            {"2*Pi", make_expr<Number>(0.0)},
            {"-1*Pi", make_expr<Number>(0.0)},
            {"Pi/4", make_expr<Number>(1.0)},
            {"-1*Pi/4", make_expr<Number>(-1.0)},
            {"Pi/6", make_expr<Number>(std::tan(PI / 6))},
            {"-1*Pi/6", make_expr<Number>(std::tan(-PI / 6))},
            {"Pi/3", make_expr<Number>(std::tan(PI / 3))},
            {"-1*Pi/3", make_expr<Number>(std::tan(-PI / 3))}
        }},
        {"Sinc", {
            {"0", make_expr<Number>(1.0)},
            {"Pi", make_expr<Number>(std::sin(PI) / PI)},
            {"-1*Pi", make_expr<Number>(std::sin(-PI) / -PI)},
            {"2*Pi", make_expr<Number>(std::sin(2 * PI) / (2 * PI))},
            {"-1*2*Pi", make_expr<Number>(std::sin(-2 * PI) / (-2 * PI))}
        }},
        {"Cot", {
            {"0", make_expr<Infinity>()},
            {"Pi/4", make_expr<Number>(1.0)},
            {"-1*Pi/4", make_expr<Number>(-1.0)},
            {"Pi/2", make_expr<Number>(0.0)},
            {"-1*Pi/2", make_expr<Number>(0.0)},
            {"Pi", make_expr<Infinity>()},
            {"-1*Pi", make_expr<Infinity>()}
        }},
        {"Csc", {
            {"0", make_expr<Infinity>()},
            {"Pi/2", make_expr<Number>(1.0)},
            {"-1*Pi/2", make_expr<Number>(-1.0)},
            {"Pi", make_expr<Infinity>()},
            {"-1*Pi", make_expr<Infinity>()},
            {"Pi/6", make_expr<Number>(2.0)},
            {"-1*Pi/6", make_expr<Number>(-2.0)}
        }}
    };

    static const std::unordered_map<std::string, std::function<bool(double)>> unary_real_domains = {
        // Inverse trig
        {"ArcSin", [](double x) { return x >= -1.0 && x <= 1.0; }},
        {"ArcCos", [](double x) { return x >= -1.0 && x <= 1.0; }},
        {"ArcTan", [](double) { return true; }},

        // Logarithm
        {"Log",    [](double x) { return x > 0.0; }},
        {"Ln",     [](double x) { return x > 0.0; }},

        // Square root and roots
        {"Sqrt",   [](double x) { return x >= 0.0; }},
        {"Root",   [](double x) { return x >= 0.0; }}, // for even roots

        // Reciprocal trig
        {"ArcSec", [](double x) { return std::abs(x) >= 1.0; }},
        {"ArcCsc", [](double x) { return std::abs(x) >= 1.0; }},
        {"ArcCot", [](double) { return true; }},

        // Factorial/Gamma
        {"Gamma",  [](double x) { return x > 0.0; }}, // real-valued for x > 0
    };

    static const std::unordered_map<std::string, std::string> inverse_unary_pairs = {
        {"Sin", "ArcSin"},
        {"Cos", "ArcCos"},
        {"Tan", "ArcTan"},
        {"Exp", "Log"},
        {"Log", "Exp"},
        {"Abs", "Abs"},
        {"ArcSin", "Sin"},
        {"ArcCos", "Cos"},
        {"ArcTan", "Tan"}
    };

    // 4. Elementwise/broadcasted binary operations
    auto elementwise = [&ctx](const std::string& op, const ExprPtr& a, const ExprPtr& b) -> ExprPtr {
        if (std::holds_alternative<List>(*a) && std::holds_alternative<List>(*b)) {
            const auto& l1 = std::get<List>(*a).elements;
            const auto& l2 = std::get<List>(*b).elements;
            if (l1.size() != l2.size())
                throw std::runtime_error("List sizes must match for elementwise operation");
            std::vector<ExprPtr> result;
            for (size_t i = 0; i < l1.size(); ++i) {
                result.push_back(evaluate(make_fcall(op, { l1[i], l2[i] }), ctx));
            }
            return std::make_shared<Expr>(List{ result });
        }
        if (std::holds_alternative<List>(*a)) {
            const auto& l1 = std::get<List>(*a).elements;
            std::vector<ExprPtr> result;
            for (const auto& elem : l1) {
                result.push_back(evaluate(make_fcall(op, { elem, b }), ctx));
            }
            return std::make_shared<Expr>(List{ result });
        }
        if (std::holds_alternative<List>(*b)) {
            const auto& l2 = std::get<List>(*b).elements;
            std::vector<ExprPtr> result;
            for (const auto& elem : l2) {
                result.push_back(evaluate(make_fcall(op, { a, elem }), ctx));
            }
            return std::make_shared<Expr>(List{ result });
        }
        return nullptr;
        };

    // 5. Built-in unary
    if (nargs == 1) {
        auto it = unary_functions.find(name);
        if (it != unary_functions.end()) {
            auto arg_eval = evaluate(func.args[0], ctx);

            // 5.1 Inverse function simplification (table-driven)
            auto inv_it = inverse_unary_pairs.find(name);
            if (inv_it != inverse_unary_pairs.end()) {
                auto* inner_call = std::get_if<FunctionCall>(arg_eval.get());
                if (inner_call && inner_call->head == inv_it->second && inner_call->args.size() == 1) {
                    return evaluate(inner_call->args[0], ctx);
                }
            }

            // 5.2 Check for known symbolic values
            auto known_func = known_symbolic_unary.find(name);
            if (known_func != known_symbolic_unary.end()) {
                std::string key = expr_to_key(arg_eval);
                auto val_it = known_func->second.find(key);
                if (val_it != known_func->second.end()) {
                    return val_it->second;
                }
            }

            // 5.3 If argument is a known constant symbol, convert to number for numeric evaluation
            if (std::holds_alternative<Symbol>(*arg_eval)) {
                const auto& sym = std::get<Symbol>(*arg_eval);
                if (sym.name == "E") arg_eval = make_expr<Number>(E);
                else if (sym.name == "Pi") arg_eval = make_expr<Number>(PI);
                else if (sym.name == "Degree") arg_eval = make_expr<Number>(PI / 180.0);
            }

            // 5.4 Numeric evaluation if argument is now a number
            if (std::holds_alternative<Number>(*arg_eval)) {
                double arg = get_number_value(arg_eval);
                auto domain_it = unary_real_domains.find(name);
                if (domain_it != unary_real_domains.end() && !domain_it->second(arg)) {
                    // Out of domain, return symbolic
                    return make_fcall(name, { arg_eval });
                }
                return make_expr<Number>(it->second(arg));
            }

            // 5.5 Fallback: symbolic
            return make_fcall(name, { arg_eval });
        }
    }

    // 6. Built-in binary (with elementwise support)
    if (nargs == 2) {
        auto it = binary_functions.find(name);
        if (it != binary_functions.end()) {
            auto left = evaluate(func.args[0], ctx);
            auto right = evaluate(func.args[1], ctx);
            if (auto ew = elementwise(name, left, right)) return ew;
            // Complex ops
            if ((name == "Plus" || name == "Minus") &&
                std::holds_alternative<Number>(*left)) {
                double real = std::get<Number>(*left).value;
                int sign = (name == "Plus") ? 1 : -1;
                if (auto* times = std::get_if<FunctionCall>(right.get())) {
                    if (times->head == "Times" && times->args.size() == 2) {
                        if (std::holds_alternative<Number>(*times->args[0]) &&
                            std::holds_alternative<Complex>(*times->args[1])) {
                            double imag = std::get<Number>(*times->args[0]).value;
                            const auto& c = std::get<Complex>(*times->args[1]);
                            if (c.real == 0.0 && c.imag == 1.0) {
                                return make_expr<Complex>(real, sign * imag);
                            }
                        }
                    }
                }
            }
            // Complex + Complex
            if (name == "Plus" &&
                std::holds_alternative<Complex>(*left) &&
                std::holds_alternative<Complex>(*right)) {
                const auto& a = std::get<Complex>(*left);
                const auto& b = std::get<Complex>(*right);
                return make_expr<Complex>(a.real + b.real, a.imag + b.imag);
            }
            // Complex * Complex
            if (name == "Times" &&
                std::holds_alternative<Complex>(*left) &&
                std::holds_alternative<Complex>(*right)) {
                const auto& a = std::get<Complex>(*left);
                const auto& b = std::get<Complex>(*right);
                double real = a.real * b.real - a.imag * b.imag;
                double imag = a.real * b.imag + a.imag * b.real;
                return make_expr<Complex>(real, imag);
            }
            // Complex + Number or Number + Complex
            if (name == "Plus") {
                if (std::holds_alternative<Complex>(*left) && std::holds_alternative<Number>(*right)) {
                    const auto& c = std::get<Complex>(*left);
                    double n = std::get<Number>(*right).value;
                    return make_expr<Complex>(c.real + n, c.imag);
                }
                if (std::holds_alternative<Number>(*left) && std::holds_alternative<Complex>(*right)) {
                    double n = std::get<Number>(*left).value;
                    const auto& c = std::get<Complex>(*right);
                    return make_expr<Complex>(n + c.real, c.imag);
                }
            }

            // Complex * Number or Number * Complex
            if (name == "Times") {
                if (std::holds_alternative<Complex>(*left) && std::holds_alternative<Number>(*right)) {
                    const auto& c = std::get<Complex>(*left);
                    double n = std::get<Number>(*right).value;
                    return make_expr<Complex>(c.real * n, c.imag * n);
                }
                if (std::holds_alternative<Number>(*left) && std::holds_alternative<Complex>(*right)) {
                    double n = std::get<Number>(*left).value;
                    const auto& c = std::get<Complex>(*right);
                    return make_expr<Complex>(n * c.real, n * c.imag);
                }
            }
            // Rational op Rational
            if (std::holds_alternative<Rational>(*left) && std::holds_alternative<Rational>(*right)) {
                const auto& a = std::get<Rational>(*left);
                const auto& b = std::get<Rational>(*right);
                if (name == "Plus") {
                    auto [n, d] = normalize_rational(a.numerator * b.denominator + b.numerator * a.denominator,
                        a.denominator * b.denominator);
                    return make_expr<Rational>(n, d);
                }
                if (name == "Minus") {
                    auto [n, d] = normalize_rational(a.numerator * b.denominator - b.numerator * a.denominator,
                        a.denominator * b.denominator);
                    return make_expr<Rational>(n, d);
                }
                if (name == "Times") {
                    auto [n, d] = normalize_rational(a.numerator * b.numerator, a.denominator * b.denominator);
                    return make_expr<Rational>(n, d);
                }
                if (name == "Divide") {
                    if (b.numerator == 0) throw std::runtime_error("Division by zero");
                    auto [n, d] = normalize_rational(a.numerator * b.denominator, a.denominator * b.numerator);
                    return make_expr<Rational>(n, d);
                }
            }
            // Rational op Number
            if (std::holds_alternative<Rational>(*left) && std::holds_alternative<Number>(*right)) {
                const auto& a = std::get<Rational>(*left);
                double b = std::get<Number>(*right).value;
                if (std::floor(b) == b) {
                    auto b_rat = Rational(static_cast<int64_t>(b), 1);
                    return evaluate(make_fcall(name, { left, make_expr<Rational>(b_rat.numerator, b_rat.denominator) }), ctx);
                }
                else {
                    double a_val = static_cast<double>(a.numerator) / a.denominator;
                    return make_expr<Number>(it->second(a_val, b));
                }
            }
            // Number op Rational
            if (std::holds_alternative<Number>(*left) && std::holds_alternative<Rational>(*right)) {
                double a = std::get<Number>(*left).value;
                const auto& b = std::get<Rational>(*right);
                if (std::floor(a) == a) {
                    auto a_rat = Rational(static_cast<int64_t>(a), 1);
                    return evaluate(make_fcall(name, { make_expr<Rational>(a_rat.numerator, a_rat.denominator), right }), ctx);
                }
                else {
                    double b_val = static_cast<double>(b.numerator) / b.denominator;
                    return make_expr<Number>(it->second(a, b_val));
                }
            }
            if (std::holds_alternative<Number>(*left) && std::holds_alternative<Number>(*right)) {
                double a = get_number_value(left);
                double b = get_number_value(right);
                return make_expr<Number>(it->second(a, b));
            }
            // If we reach here, try the simplification rule for this operation
            auto simp_it = simplification_rules.find(name);
            if (simp_it != simplification_rules.end()) {
                return simp_it->second({left, right}, ctx, evaluate);
            }
            ALEPH3_LOG("No numeric/special case for " << name << " with args: "
                << to_string_raw(left) << ", " << to_string_raw(right)
                << " (types: "
                << left->index() << ", " << right->index() << ")");
            return make_fcall(name, { left, right });
        }
        // Comparison functions
        auto cmp = comparison_functions.find(name);
        if (cmp != comparison_functions.end()) {
            auto left = evaluate(func.args[0], ctx);
            auto right = evaluate(func.args[1], ctx);
            if (std::holds_alternative<Number>(*left) && std::holds_alternative<Number>(*right)) {
                double arg1 = get_number_value(left);
                double arg2 = get_number_value(right);
                return make_expr<Boolean>(cmp->second(arg1, arg2));
            }
            // Rational op Rational
            if (std::holds_alternative<Rational>(*left) && std::holds_alternative<Rational>(*right)) {
                const auto& a = std::get<Rational>(*left);
                const auto& b = std::get<Rational>(*right);
                if (name == "Equal")
                    return make_expr<Boolean>(a.numerator == b.numerator && a.denominator == b.denominator);
                if (name == "NotEqual")
                    return make_expr<Boolean>(a.numerator != b.numerator || a.denominator != b.denominator);
                if (name == "Less")
                    return make_expr<Boolean>(a.numerator * b.denominator < b.numerator * a.denominator);
                if (name == "Greater")
                    return make_expr<Boolean>(a.numerator * b.denominator > b.numerator * a.denominator);
                if (name == "LessEqual")
                    return make_expr<Boolean>(a.numerator * b.denominator <= b.numerator * a.denominator);
                if (name == "GreaterEqual")
                    return make_expr<Boolean>(a.numerator * b.denominator >= b.numerator * a.denominator);
            }
            // Rational op Number
            if (std::holds_alternative<Rational>(*left) && std::holds_alternative<Number>(*right)) {
                const auto& a = std::get<Rational>(*left);
                double b = std::get<Number>(*right).value;
                double a_val = static_cast<double>(a.numerator) / a.denominator;
                return make_expr<Boolean>(cmp->second(a_val, b));
            }
            // Number op Rational
            if (std::holds_alternative<Number>(*left) && std::holds_alternative<Rational>(*right)) {
                double a = std::get<Number>(*left).value;
                const auto& b = std::get<Rational>(*right);
                double b_val = static_cast<double>(b.numerator) / b.denominator;
                return make_expr<Boolean>(cmp->second(a, b_val));
            }
            // Return unevaluated symbolic comparison if not both numbers
            return make_fcall(name, { left, right });
        }
    }

    // 7. Centralized simplification for known functions
    auto simp_it = simplification_rules.find(name);
    if (simp_it != simplification_rules.end()) {
        return simp_it->second(func.args, ctx, evaluate);
    }

    // 8. User-defined functions
    auto user_it = ctx.user_functions.find(name);
    if (user_it != ctx.user_functions.end()) {
        const FunctionDefinition& def = user_it->second;
        size_t param_count = def.params.size();
        size_t arg_count = func.args.size();
        std::vector<ExprPtr> final_args;
        for (size_t i = 0; i < param_count; ++i) {
            if (i < arg_count) {
                final_args.push_back(func.args[i]);
            }
            else if (def.params[i].default_value != nullptr) {
                final_args.push_back(def.params[i].default_value);
            }
            else {
                throw std::runtime_error("Function " + name + " expects at least " +
                    std::to_string(i + 1) + " arguments, got " +
                    std::to_string(arg_count));
            }
        }
        if (arg_count > param_count) {
            throw std::runtime_error("Function " + name + " expects at most " +
                std::to_string(param_count) + " arguments, got " +
                std::to_string(arg_count));
        }
        EvaluationContext local_ctx = ctx;
        for (size_t i = 0; i < param_count; ++i) {
            local_ctx.variables[def.params[i].name] = final_args[i];
        }
        return evaluate(def.body, local_ctx);
    }

    // 9. Fallback: return unevaluated
    std::vector<ExprPtr> unevaluated_args;
    for (const auto& arg : func.args) {
        unevaluated_args.push_back(arg);
    }
    return make_expr<FunctionCall>(name, unevaluated_args);
}

inline ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx, std::unordered_set<std::string>& visited) {
    ALEPH3_LOG("evaluate: input = " << to_string_raw(expr));
    auto result = std::visit(overloaded{
        [](const Number& num) -> ExprPtr {
            return make_expr<Number>(num.value);
        },
        [](const Complex& c) -> ExprPtr {
            return make_expr<Complex>(c.real, c.imag);
        },
        [](const Rational& r) -> ExprPtr {
            auto [n, d] = normalize_rational(r.numerator, r.denominator);
            return make_expr<Rational>(n, d);
        },
        [](const Boolean& boolean) -> ExprPtr {
            return make_expr<Boolean>(boolean.value);
        },
        [](const String& str) -> ExprPtr {
            return make_expr<String>(str.value);
        },
        [&](const Symbol& sym) -> ExprPtr {
            if (visited.count(sym.name)) {
                // Detected recursion, return symbol unevaluated
                return make_expr<Symbol>(sym.name);
            }
            auto it = ctx.variables.find(sym.name);
            if (it == ctx.variables.end()) {
                return make_expr<Symbol>(sym.name);
            }
            visited.insert(sym.name);
            auto result = evaluate(it->second, ctx, visited);
            visited.erase(sym.name);
            return result;
        },
        [&ctx](const FunctionCall& func) -> ExprPtr {
            if (is_polynomial_function(func.head)) {
                return evaluate_polynomial_function(func, ctx);
            }
            // Special case: List
            if (func.head == "List") {
                std::vector<ExprPtr> evaluated_elements;
                for (const auto& arg : func.args) {
                    evaluated_elements.push_back(evaluate(arg, ctx));
                }
                return std::make_shared<Expr>(List{evaluated_elements});
            }

            // Check for user-defined functions
            auto user_it = ctx.user_functions.find(func.head);
            if (user_it != ctx.user_functions.end()) {
                const FunctionDefinition& def = user_it->second;
                size_t param_count = def.params.size();
                size_t arg_count = func.args.size();

                // Prepare argument list, filling in defaults if needed
                std::vector<ExprPtr> final_args;
                for (size_t i = 0; i < param_count; ++i) {
                    if (i < arg_count) {
                        final_args.push_back(func.args[i]);
                    }
                    else if (def.params[i].default_value != nullptr) {
                        final_args.push_back(def.params[i].default_value);
                    }
                    else {
                        throw std::runtime_error("Function " + func.head + " expects at least " +
                            std::to_string(i + 1) + " arguments, got " +
                            std::to_string(arg_count));
                    }
                }

                // Too many arguments
                if (arg_count > param_count) {
                    throw std::runtime_error("Function " + func.head + " expects at most " +
                        std::to_string(param_count) + " arguments, got " +
                        std::to_string(arg_count));
                }

                // Create a new context for the function evaluation
                EvaluationContext local_ctx = ctx;

                // Bind formal parameters to actual arguments
                for (size_t i = 0; i < param_count; ++i) {
                    auto arg_value = evaluate(final_args[i], ctx);
                    local_ctx.variables[def.params[i].name] = arg_value;
                }

                // Evaluate the function body in the new context
                return evaluate(def.body, local_ctx);
            }
            // If not a user-defined function, evaluate as a built-in function
            return evaluate_function(func, ctx);
        },
        [&ctx](const FunctionDefinition& def) -> ExprPtr {

            // Store the function definition in the context
            if (def.delayed) {
                // Delayed assignment: store the unevaluated body
                ctx.user_functions[def.name] = def;
            }
            else {
                // Immediate assignment: evaluate the body immediately
                EvaluationContext local_ctx = ctx;
                for (const auto& param : def.params) {
                    local_ctx.variables[param.name] = make_expr<Symbol>(param.name); // Bind parameters symbolically
                }
                auto evaluated_body = evaluate(def.body, local_ctx);
                ctx.user_functions[def.name] = FunctionDefinition(def.name, def.params, evaluated_body, false);
                return evaluated_body;
            }
            // Return the full function definition as feedback
            return make_expr<FunctionDefinition>(def.name, def.params, def.body, def.delayed);
        },
        [&ctx](const Assignment& assign) -> ExprPtr {
            // Evaluate the assigned value and store it in the context
            ctx.variables[assign.name] = evaluate(assign.value, ctx);

            // Return the variable name as feedback
            return make_expr<Symbol>(assign.name);
        },
        [&](const Rule& rule) -> ExprPtr {
            auto lhs = evaluate(rule.lhs, ctx, visited);
            auto rhs = evaluate(rule.rhs, ctx, visited);
            return make_expr<Rule>(lhs, rhs);
        },
        [](const List& list) -> ExprPtr {
            // Lists are already evaluated, just return as-is
            return std::make_shared<Expr>(list);
        },
        [](const Infinity&) -> ExprPtr {
            return make_expr<Infinity>();
        },
        [](const Indeterminate&) -> ExprPtr {
            return make_expr<Indeterminate>();
        },
        }, 
        *expr);
        ALEPH3_LOG("evaluate: result = " << to_string_raw(result));
        return result;
}

inline ExprPtr evaluate(const ExprPtr& expr, EvaluationContext& ctx) {
    ExprPtr norm = normalize_expr(expr);
    std::unordered_set<std::string> visited;
    return evaluate(norm, ctx, visited);
}

}
