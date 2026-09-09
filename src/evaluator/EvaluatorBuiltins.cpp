#include "evaluator/EvaluatorBuiltins.hpp"

#include "Constants.hpp"
#include "ExtraMath.hpp"
#include "evaluator/Evaluator.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "evaluator/GammaUtils.hpp"
#include "evaluator/EvaluatorSemantics.hpp"
#include "evaluator/SimplificationRules.hpp"
#include "expr/ExprUtils.hpp"
#include "expr/ExprStructural.hpp"
#include "kernel/Diagnostics.hpp"
#include "kernel/FunctionRegistry.hpp"
#include "normalizer/Normalizer.hpp"
#include "parser/Parser.hpp"
#include "util/Logging.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aleph3 {

namespace {

bool is_finite_number(double value) noexcept {
    return std::isfinite(value);
}

[[noreturn]] void throw_runtime_type_mismatch(std::string message) {
    kernel::throw_runtime_error(kernel::ErrorCode::type_mismatch, std::move(message));
}

[[noreturn]] void throw_runtime_invalid_call(std::string message) {
    kernel::throw_runtime_error(kernel::ErrorCode::invalid_call, std::move(message));
}

void ensure_finite_number(double value) {
    if (!is_finite_number(value)) {
        kernel::throw_runtime_error(
            kernel::ErrorCode::non_finite_number,
            "Numeric operations require finite input values.");
    }
}

std::optional<kernel::ExactRational> exact_rational_atom(const ExprPtr& expr) {
    if (const auto* integer = std::get_if<Integer>(expr.get())) {
        return kernel::ExactRational(integer->value, kernel::ExactInteger(1));
    }
    if (const auto* rational = std::get_if<Rational>(expr.get())) {
        return rational->exact();
    }
    return std::nullopt;
}

ExprPtr evaluate_exact_power(
    const kernel::ExactRational& base,
    const kernel::ExactInteger& exponent,
    EvaluationContext& ctx) {
    if (!exponent.to_int64().has_value()) {
        return nullptr;
    }

    const int64_t bounded_exponent = *exponent.to_int64();
    if (bounded_exponent == 0) {
        if (base.is_zero()) {
            return nullptr;
        }
        return make_expr<Integer>(1);
    }
    if (base.is_zero()) {
        if (bounded_exponent < 0) {
            return nullptr;
        }
        return make_expr<Integer>(0);
    }

    kernel::ExactRational factor = base;
    uint64_t remaining = unsigned_abs_int64(bounded_exponent);
    if (bounded_exponent < 0) {
        factor = kernel::ExactRational(base.denominator(), base.numerator());
    }

    kernel::ExactRational result(1, 1);
    while (remaining > 0) {
        if ((remaining & 1U) != 0U) {
            ctx.consume_evaluation_step();
            result = result * factor;
        }
        remaining >>= 1U;
        if (remaining > 0) {
            ctx.consume_evaluation_step();
            factor = factor * factor;
        }
    }
    return make_exact_scalar_expr(result);
}

std::optional<double> finite_scalar_atom(const ExprPtr& expr) {
    if (const auto* number = std::get_if<Number>(expr.get())) {
        return number->value;
    }
    if (const auto* integer = std::get_if<Integer>(expr.get())) {
        return finite_double_from_exact_integer(integer->value);
    }
    if (const auto* rational = std::get_if<Rational>(expr.get())) {
        return finite_double_from_exact_rational(*rational);
    }
    return std::nullopt;
}

const std::unordered_map<std::string, std::function<double(double)>>& unary_functions() {
    static const std::unordered_map<std::string, std::function<double(double)>> value = {
        {"Sin",   [](double x) { return std::sin(x); }},
        {"Cos",   [](double x) { return std::cos(x); }},
        {"Tan",   [](double x) { return std::tan(x); }},
        {"Sinc",  [](double x) { return x == 0.0 ? 1.0 : std::sin(x) / x; }},
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
        {"Ln",    [](double x) { return std::log(x); }},
        {"Log",   [](double x) { return std::log(x); }},
        {"Floor", [](double x) { return std::floor(x); }},
        {"Ceil",  [](double x) { return std::ceil(x); }},
        {"Ceiling",[](double x) { return std::ceil(x); }},
        {"Round", [](double x) { return std::round(x); }},
        {"ArcSin",[](double x) { return std::asin(x); }},
        {"ArcCos",[](double x) { return std::acos(x); }},
        {"ArcTan",[](double x) { return std::atan(x); }},
        {"ArcSec",[](double x) { return std::acos(1.0 / x); }},
        {"ArcCsc",[](double x) { return std::asin(1.0 / x); }},
        {"ArcCot",[](double x) { return std::atan2(1.0, x); }},
        {"Gamma", [](double x) { return std::tgamma(x); }}
    };
    return value;
}

const std::unordered_map<std::string, std::function<double(double, double)>>& binary_functions() {
    static const std::unordered_map<std::string, std::function<double(double, double)>> value = {
        {"Plus",   [](double a, double b) { return a + b; }},
        {"Minus",  [](double a, double b) { return a - b; }},
        {"Times",  [](double a, double b) { return a * b; }},
        {"Divide", [](double a, double b) { return a / b; }},
        {"Power",  [](double a, double b) { return std::pow(a, b); }},
        {"Log",    [](double b, double x) { return std::log(x) / std::log(b); }},
        {"ArcTan", [](double x, double y) { return std::atan2(y, x); }}
    };
    return value;
}

const std::unordered_map<std::string, std::function<bool(double, double)>>& comparison_functions() {
    static const std::unordered_map<std::string, std::function<bool(double, double)>> value = {
        {"Equal",         [](double a, double b) { return a == b; }},
        {"NotEqual",      [](double a, double b) { return a != b; }},
        {"Less",          [](double a, double b) { return a < b; }},
        {"Greater",       [](double a, double b) { return a > b; }},
        {"LessEqual",     [](double a, double b) { return a <= b; }},
        {"GreaterEqual",  [](double a, double b) { return a >= b; }}
    };
    return value;
}

using KnownUnaryValues = std::unordered_map<std::string, std::vector<std::pair<ExprPtr, ExprPtr>>>;

ExprPtr known_unary_key(std::string_view source) {
    return normalize_expr(parse_expression(std::string(source)));
}

std::optional<ExprPtr> without_leading_minus(const ExprPtr& expr) {
    if (const auto* integer = std::get_if<Integer>(expr.get());
        integer != nullptr && integer->value < 0) {
        return make_expr<Integer>(-integer->value);
    }
    if (const auto* number = std::get_if<Number>(expr.get());
        number != nullptr && number->value < 0.0) {
        return make_expr<Number>(-number->value);
    }
    if (const auto* rational = std::get_if<Rational>(expr.get());
        rational != nullptr && rational->numerator < 0) {
        return make_exact_scalar_expr(-rational->exact());
    }
    const auto* call = std::get_if<FunctionCall>(expr.get());
    if (call == nullptr) {
        return std::nullopt;
    }
    if (call->head == "Times" && !call->args.empty()) {
        if (auto coefficient = exact_int64_from_expr(call->args[0]); coefficient.has_value() && *coefficient == -1) {
            if (call->args.size() == 2) {
                return call->args[1];
            }
            std::vector<ExprPtr> args(call->args.begin() + 1, call->args.end());
            return make_fcall("Times", args);
        }
    }
    if (call->head == "Divide" && call->args.size() == 2) {
        if (auto positive_numerator = without_leading_minus(call->args[0])) {
            return make_fcall("Divide", {*positive_numerator, call->args[1]});
        }
    }
    return std::nullopt;
}

bool has_odd_unary_symmetry(const std::string& head) {
    return head == "Sin" || head == "Tan" || head == "Cot" || head == "Csc";
}

bool has_even_unary_symmetry(const std::string& head) {
    return head == "Cos" || head == "Sinc";
}

ExprPtr apply_unary_symmetry(const std::string& head, const ExprPtr& positive_value) {
    if (!has_odd_unary_symmetry(head)) {
        return positive_value;
    }
    if (const auto* number = std::get_if<Number>(positive_value.get())) {
        return make_expr<Number>(-number->value);
    }
    if (const auto* integer = std::get_if<Integer>(positive_value.get())) {
        return make_expr<Integer>(-integer->value);
    }
    if (const auto* rational = std::get_if<Rational>(positive_value.get())) {
        return make_exact_scalar_expr(-rational->exact());
    }
    return positive_value;
}

const KnownUnaryValues& known_symbolic_unary() {
    static const KnownUnaryValues value = {
        {"Sin", {
            {known_unary_key("0"), make_expr<Number>(0.0)},
            {known_unary_key("Pi"), make_expr<Number>(0.0)},
            {known_unary_key("2*Pi"), make_expr<Number>(0.0)},
            {known_unary_key("-1*Pi"), make_expr<Number>(0.0)},
            {known_unary_key("Pi/2"), make_expr<Number>(1.0)},
            {known_unary_key("-1*Pi/2"), make_expr<Number>(-1.0)},
            {known_unary_key("Pi/4"), make_expr<Number>(std::sqrt(2.0) / 2.0)},
            {known_unary_key("-1*Pi/4"), make_expr<Number>(-std::sqrt(2.0) / 2.0)},
            {known_unary_key("3*Pi/2"), make_expr<Number>(-1.0)},
            {known_unary_key("-1*3*Pi/2"), make_expr<Number>(1.0)}
        }},
        {"Cos", {
            {known_unary_key("0"), make_expr<Number>(1.0)},
            {known_unary_key("Pi"), make_expr<Number>(-1.0)},
            {known_unary_key("2*Pi"), make_expr<Number>(1.0)},
            {known_unary_key("-1*Pi"), make_expr<Number>(-1.0)},
            {known_unary_key("Pi/2"), make_expr<Number>(0.0)},
            {known_unary_key("-1*Pi/2"), make_expr<Number>(0.0)},
            {known_unary_key("Pi/4"), make_expr<Number>(std::sqrt(2.0) / 2.0)},
            {known_unary_key("-1*Pi/4"), make_expr<Number>(std::sqrt(2.0) / 2.0)},
            {known_unary_key("3*Pi/2"), make_expr<Number>(0.0)},
            {known_unary_key("-1*3*Pi/2"), make_expr<Number>(0.0)}
        }},
        {"Tan", {
            {known_unary_key("0"), make_expr<Number>(0.0)},
            {known_unary_key("Pi"), make_expr<Number>(0.0)},
            {known_unary_key("2*Pi"), make_expr<Number>(0.0)},
            {known_unary_key("-1*Pi"), make_expr<Number>(0.0)},
            {known_unary_key("Pi/4"), make_expr<Number>(1.0)},
            {known_unary_key("-1*Pi/4"), make_expr<Number>(-1.0)},
            {known_unary_key("Pi/6"), make_expr<Number>(std::tan(PI / 6))},
            {known_unary_key("-1*Pi/6"), make_expr<Number>(std::tan(-PI / 6))},
            {known_unary_key("Pi/3"), make_expr<Number>(std::tan(PI / 3))},
            {known_unary_key("-1*Pi/3"), make_expr<Number>(std::tan(-PI / 3))}
        }},
        {"Sinc", {
            {known_unary_key("0"), make_expr<Number>(1.0)},
            {known_unary_key("Pi"), make_expr<Number>(std::sin(PI) / PI)},
            {known_unary_key("-1*Pi"), make_expr<Number>(std::sin(-PI) / -PI)},
            {known_unary_key("2*Pi"), make_expr<Number>(std::sin(2 * PI) / (2 * PI))},
            {known_unary_key("-1*2*Pi"), make_expr<Number>(std::sin(-2 * PI) / (-2 * PI))}
        }},
        {"Cot", {
            {known_unary_key("0"), make_expr<Infinity>()},
            {known_unary_key("Pi/4"), make_expr<Number>(1.0)},
            {known_unary_key("-1*Pi/4"), make_expr<Number>(-1.0)},
            {known_unary_key("Pi/2"), make_expr<Number>(0.0)},
            {known_unary_key("-1*Pi/2"), make_expr<Number>(0.0)},
            {known_unary_key("Pi"), make_expr<Infinity>()},
            {known_unary_key("-1*Pi"), make_expr<Infinity>()}
        }},
        {"Csc", {
            {known_unary_key("0"), make_expr<Infinity>()},
            {known_unary_key("Pi/2"), make_expr<Number>(1.0)},
            {known_unary_key("-1*Pi/2"), make_expr<Number>(-1.0)},
            {known_unary_key("Pi"), make_expr<Infinity>()},
            {known_unary_key("-1*Pi"), make_expr<Infinity>()},
            {known_unary_key("Pi/6"), make_expr<Number>(2.0)},
            {known_unary_key("-1*Pi/6"), make_expr<Number>(-2.0)}
        }}
    };
    return value;
}

ExprPtr find_known_symbolic_unary_value(const std::string& head, const ExprPtr& arg) {
    const auto& known_values = known_symbolic_unary();
    const auto known_func = known_values.find(head);
    if (known_func == known_values.end()) {
        return nullptr;
    }

    const auto normalized_arg = normalize_expr(arg);
    for (const auto& [known_arg, value] : known_func->second) {
        if (structural_equal(known_arg, normalized_arg)) {
            return value;
        }
    }
    if (has_odd_unary_symmetry(head) || has_even_unary_symmetry(head)) {
        if (auto positive_arg = without_leading_minus(normalized_arg)) {
            const auto normalized_positive = normalize_expr(*positive_arg);
            for (const auto& [known_arg, value] : known_func->second) {
                if (structural_equal(known_arg, normalized_positive)) {
                    return apply_unary_symmetry(head, value);
                }
            }
        }
    }
    return nullptr;
}

const std::unordered_map<std::string, std::function<bool(double)>>& unary_real_domains() {
    static const std::unordered_map<std::string, std::function<bool(double)>> value = {
        {"ArcSin", [](double x) { return x >= -1.0 && x <= 1.0; }},
        {"ArcCos", [](double x) { return x >= -1.0 && x <= 1.0; }},
        {"ArcTan", [](double) { return true; }},
        {"Log",    [](double x) { return x > 0.0; }},
        {"Ln",     [](double x) { return x > 0.0; }},
        {"Sqrt",   [](double x) { return x >= 0.0; }},
        {"Root",   [](double x) { return x >= 0.0; }},
        {"ArcSec", [](double x) { return std::abs(x) >= 1.0; }},
        {"ArcCsc", [](double x) { return std::abs(x) >= 1.0; }},
        {"ArcCot", [](double) { return true; }},
        {"Gamma",  [](double x) { return x > 0.0; }}
    };
    return value;
}

const std::unordered_map<std::string, std::function<bool(double, double)>>& binary_real_domains() {
    static const std::unordered_map<std::string, std::function<bool(double, double)>> value = {
        {"Log", [](double base, double x) {
            return base > 0.0 && base != 1.0 && x > 0.0;
        }},
        {"ArcTan", [](double, double) {
            return true;
        }},
        {"Power", [](double base, double exponent) {
            if (base == 0.0 && exponent < 0.0) {
                return false;
            }
            if (base >= 0.0) {
                return true;
            }
            return std::floor(exponent) == exponent;
        }}
    };
    return value;
}

const std::unordered_map<std::string, std::string>& inverse_unary_pairs() {
    static const std::unordered_map<std::string, std::string> value = {
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
    return value;
}

ExprPtr evaluate_elementwise_binary(
    const std::string& op,
    const ExprPtr& a,
    const ExprPtr& b,
    EvaluationContext& ctx) {
    if (std::holds_alternative<List>(*a) && std::holds_alternative<List>(*b)) {
        const auto& l1 = std::get<List>(*a).elements;
        const auto& l2 = std::get<List>(*b).elements;
        if (l1.size() != l2.size()) {
            throw_invalid_form("List sizes must match for elementwise operation");
        }
        std::vector<ExprPtr> result;
        for (size_t i = 0; i < l1.size(); ++i) {
            result.push_back(evaluate(make_fcall(op, {l1[i], l2[i]}), ctx));
        }
        return std::make_shared<Expr>(List{result});
    }
    if (std::holds_alternative<List>(*a)) {
        const auto& l1 = std::get<List>(*a).elements;
        std::vector<ExprPtr> result;
        for (const auto& elem : l1) {
            result.push_back(evaluate(make_fcall(op, {elem, b}), ctx));
        }
        return std::make_shared<Expr>(List{result});
    }
    if (std::holds_alternative<List>(*b)) {
        const auto& l2 = std::get<List>(*b).elements;
        std::vector<ExprPtr> result;
        for (const auto& elem : l2) {
            result.push_back(evaluate(make_fcall(op, {a, elem}), ctx));
        }
        return std::make_shared<Expr>(List{result});
    }
    return nullptr;
}

ExprPtr evaluate_builtin_unary(const FunctionCall& func, EvaluationContext& ctx) {
    const auto& unary = unary_functions();
    auto it = unary.find(func.head);
    if (it == unary.end() || func.args.size() != 1) {
        return nullptr;
    }

    auto arg_eval = evaluate(func.args[0], ctx);

    const auto& inverse_pairs = inverse_unary_pairs();
    auto inv_it = inverse_pairs.find(func.head);
    if (inv_it != inverse_pairs.end()) {
        auto* inner_call = std::get_if<FunctionCall>(arg_eval.get());
        if (inner_call && inner_call->head == inv_it->second && inner_call->args.size() == 1) {
            return evaluate(inner_call->args[0], ctx);
        }
    }

    if (auto known_value = find_known_symbolic_unary_value(func.head, arg_eval)) {
        return known_value;
    }

    if (std::holds_alternative<Symbol>(*arg_eval)) {
        const auto& sym = std::get<Symbol>(*arg_eval);
        if (sym.name == "E") arg_eval = make_expr<Number>(E);
        else if (sym.name == "Pi") arg_eval = make_expr<Number>(PI);
        else if (sym.name == "Degree") arg_eval = make_expr<Number>(PI / 180.0);
    }

    if (func.head == "Gamma") {
        if (auto gamma_result = simplify_gamma_argument(arg_eval)) {
            return *gamma_result;
        }
    }

    if (auto finite_arg = finite_scalar_atom(arg_eval)) {
        double arg = *finite_arg;
        if (ctx.strict_runtime_semantics()) {
            ensure_finite_number(arg);
        }
        const auto& domains = unary_real_domains();
        auto domain_it = domains.find(func.head);
        if (domain_it != domains.end() && !domain_it->second(arg)) {
            if (ctx.strict_runtime_semantics()) {
                kernel::throw_runtime_error(
                    kernel::ErrorCode::invalid_numeric_domain,
                    func.head + " is undefined for the given numeric input.");
            }
            return make_fcall(func.head, {arg_eval});
        }
        double result = it->second(arg);
        if (ctx.strict_runtime_semantics() && !is_finite_number(result)) {
            kernel::throw_runtime_error(
                kernel::ErrorCode::invalid_numeric_result,
                "Numeric evaluation produced a non-finite result.");
        }
        return make_expr<Number>(result);
    }

    if (is_listable_function(func.head) && std::holds_alternative<List>(*arg_eval)) {
        const auto& elements = std::get<List>(*arg_eval).elements;
        std::vector<ExprPtr> result;
        result.reserve(elements.size());
        for (const auto& element : elements) {
            result.push_back(evaluate(make_fcall(func.head, {element}), ctx));
        }
        return std::make_shared<Expr>(List{result});
    }

    return make_fcall(func.head, {arg_eval});
}

ExprPtr evaluate_builtin_binary(const FunctionCall& func, EvaluationContext& ctx) {
    const auto& binary = binary_functions();
    auto it = binary.find(func.head);
    if (it == binary.end() || func.args.size() != 2) {
        return nullptr;
    }

    auto left = evaluate(func.args[0], ctx);
    auto right = evaluate(func.args[1], ctx);
    if (is_listable_function(func.head)) {
        if (auto ew = evaluate_elementwise_binary(func.head, left, right, ctx)) {
            return ew;
        }
    }

    if (func.head == "Plus" || func.head == "Minus") {
        const auto real_value = finite_scalar_atom(left);
        if (real_value.has_value()) {
        double real = *real_value;
        int sign = (func.head == "Plus") ? 1 : -1;
        if (auto* times = std::get_if<FunctionCall>(right.get())) {
            if (times->head == "Times" && times->args.size() == 2) {
                if (auto imag_value = finite_scalar_atom(times->args[0]);
                    imag_value.has_value() && std::holds_alternative<Complex>(*times->args[1])) {
                    double imag = *imag_value;
                    const auto& c = std::get<Complex>(*times->args[1]);
                    if (c.real == 0.0 && c.imag == 1.0) {
                        return make_expr<Complex>(real, sign * imag);
                    }
                }
            }
        }
        }
    }
    if (func.head == "Plus" &&
        std::holds_alternative<Complex>(*left) &&
        std::holds_alternative<Complex>(*right)) {
        const auto& a = std::get<Complex>(*left);
        const auto& b = std::get<Complex>(*right);
        return make_expr<Complex>(a.real + b.real, a.imag + b.imag);
    }
    if (func.head == "Times" &&
        std::holds_alternative<Complex>(*left) &&
        std::holds_alternative<Complex>(*right)) {
        const auto& a = std::get<Complex>(*left);
        const auto& b = std::get<Complex>(*right);
        double real = a.real * b.real - a.imag * b.imag;
        double imag = a.real * b.imag + a.imag * b.real;
        return make_expr<Complex>(real, imag);
    }
    if (func.head == "Plus") {
        if (std::holds_alternative<Complex>(*left)) {
            const auto& c = std::get<Complex>(*left);
            if (auto n = finite_scalar_atom(right)) {
                return make_expr<Complex>(c.real + *n, c.imag);
            }
        }
        if (std::holds_alternative<Complex>(*right)) {
            const auto& c = std::get<Complex>(*right);
            if (auto n = finite_scalar_atom(left)) {
                return make_expr<Complex>(*n + c.real, c.imag);
            }
        }
    }
    if (func.head == "Times") {
        if (std::holds_alternative<Complex>(*left)) {
            const auto& c = std::get<Complex>(*left);
            if (auto n = finite_scalar_atom(right)) {
                return make_expr<Complex>(c.real * *n, c.imag * *n);
            }
        }
        if (std::holds_alternative<Complex>(*right)) {
            const auto& c = std::get<Complex>(*right);
            if (auto n = finite_scalar_atom(left)) {
                return make_expr<Complex>(*n * c.real, *n * c.imag);
            }
        }
    }
    if (auto left_exact = exact_rational_atom(left), right_exact = exact_rational_atom(right);
        left_exact.has_value() && right_exact.has_value()) {
        const auto& a = *left_exact;
        const auto& b = *right_exact;
        if (func.head == "Plus") {
            return make_exact_scalar_expr(a + b);
        }
        if (func.head == "Minus") {
            return make_exact_scalar_expr(a - b);
        }
        if (func.head == "Times") {
            return make_exact_scalar_expr(a * b);
        }
        if (func.head == "Divide") {
            if (b.numerator().is_zero()) throw_domain_violation("Division by zero");
            return make_exact_scalar_expr(a / b);
        }
        if (func.head == "Power" && b.denominator().is_one()) {
            if (auto exact_power = evaluate_exact_power(a, b.numerator(), ctx)) {
                return exact_power;
            }
            return make_fcall(func.head, {left, right});
        }
    }
    if (auto left_finite = finite_scalar_atom(left), right_finite = finite_scalar_atom(right);
        left_finite.has_value() && right_finite.has_value()) {
        double a = *left_finite;
        double b = *right_finite;
        if (func.head == "Power" && a == 0.0 && b == 0.0 && !ctx.strict_runtime_semantics()) {
            return make_fcall(func.head, {left, right});
        }
        if (ctx.strict_runtime_semantics()) {
            ensure_finite_number(a);
            ensure_finite_number(b);
            if (func.head == "Divide" && b == 0.0) {
                kernel::throw_runtime_error(
                    kernel::ErrorCode::division_by_zero,
                    "Division by zero is not allowed.");
            }
            if (func.head == "Power") {
                if (a == 0.0 && b == 0.0) {
                    kernel::throw_runtime_error(
                        kernel::ErrorCode::invalid_power_domain,
                        "Power is undefined for the given numeric inputs.");
                }
                if (a < 0.0 && std::floor(b) != b) {
                    kernel::throw_runtime_error(
                        kernel::ErrorCode::invalid_power_domain,
                        "Power is undefined for the given numeric inputs.");
                }
            }
        }
        const auto& domains = binary_real_domains();
        auto domain_it = domains.find(func.head);
        if (domain_it != domains.end() && !domain_it->second(a, b)) {
            if (ctx.strict_runtime_semantics()) {
                if (func.head == "Power") {
                    kernel::throw_runtime_error(
                        kernel::ErrorCode::invalid_power_domain,
                        "Power is undefined for the given numeric inputs.");
                }
                kernel::throw_runtime_error(
                    kernel::ErrorCode::invalid_numeric_domain,
                    func.head + " is undefined for the given numeric inputs.");
            }
            return make_fcall(func.head, {left, right});
        }
        double result = it->second(a, b);
        if (ctx.strict_runtime_semantics() && !is_finite_number(result)) {
            kernel::throw_runtime_error(
                kernel::ErrorCode::invalid_numeric_result,
                "Numeric evaluation produced a non-finite result.");
        }
        return make_expr<Number>(result);
    }
    if (std::holds_alternative<Rational>(*left) && std::holds_alternative<Number>(*right)) {
        const auto& a = std::get<Rational>(*left);
        double b = std::get<Number>(*right).value;
        if (auto integer = exact_int64_from_number(b)) {
            return evaluate(make_fcall(func.head, {left, make_expr<Rational>(*integer, 1)}), ctx);
        }
        double a_val = finite_double_from_exact_rational(a).value_or(std::numeric_limits<double>::infinity());
        return make_expr<Number>(it->second(a_val, b));
    }
    if (std::holds_alternative<Number>(*left) && std::holds_alternative<Rational>(*right)) {
        double a = std::get<Number>(*left).value;
        const auto& b = std::get<Rational>(*right);
        if (auto integer = exact_int64_from_number(a)) {
            return evaluate(make_fcall(func.head, {make_expr<Rational>(*integer, 1), right}), ctx);
        }
        double b_val = finite_double_from_exact_rational(b).value_or(std::numeric_limits<double>::infinity());
        return make_expr<Number>(it->second(a, b_val));
    }
    if (std::holds_alternative<Number>(*left) && std::holds_alternative<Number>(*right)) {
        double a = get_number_value(left);
        double b = get_number_value(right);
        if (func.head == "Power" && a == 0.0 && b == 0.0 && !ctx.strict_runtime_semantics()) {
            return make_fcall(func.head, {left, right});
        }
        if (ctx.strict_runtime_semantics()) {
            ensure_finite_number(a);
            ensure_finite_number(b);
            if (func.head == "Divide" && b == 0.0) {
                kernel::throw_runtime_error(
                    kernel::ErrorCode::division_by_zero,
                    "Division by zero is not allowed.");
            }
            if (func.head == "Power") {
                if (a == 0.0 && b == 0.0) {
                    kernel::throw_runtime_error(
                        kernel::ErrorCode::invalid_power_domain,
                        "Power is undefined for the given numeric inputs.");
                }
                if (a < 0.0 && std::floor(b) != b) {
                    kernel::throw_runtime_error(
                        kernel::ErrorCode::invalid_power_domain,
                        "Power is undefined for the given numeric inputs.");
                }
            }
        }
        const auto& domains = binary_real_domains();
        auto domain_it = domains.find(func.head);
        if (domain_it != domains.end() && !domain_it->second(a, b)) {
            if (ctx.strict_runtime_semantics()) {
                if (func.head == "Power") {
                    kernel::throw_runtime_error(
                        kernel::ErrorCode::invalid_power_domain,
                        "Power is undefined for the given numeric inputs.");
                }
                kernel::throw_runtime_error(
                    kernel::ErrorCode::invalid_numeric_domain,
                    func.head + " is undefined for the given numeric inputs.");
            }
            return make_fcall(func.head, {left, right});
        }
        double result = it->second(a, b);
        if (ctx.strict_runtime_semantics() && !is_finite_number(result)) {
            kernel::throw_runtime_error(
                kernel::ErrorCode::invalid_numeric_result,
                "Numeric evaluation produced a non-finite result.");
        }
        return make_expr<Number>(result);
    }

    if (ctx.strict_runtime_semantics()) {
        throw_runtime_type_mismatch("Arithmetic operators require numeric values.");
    }

    const auto& rules = simplification_rules();
    auto simp_it = rules.find(func.head);
    if (simp_it != rules.end()) {
        const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)> evaluator =
            [](const ExprPtr& expr, EvaluationContext& local_ctx) {
                return evaluate(expr, local_ctx);
            };
        return simp_it->second({left, right}, ctx, evaluator);
    }

    ALEPH3_LOG("No numeric/special case for " << func.head << " with args: "
        << to_string_raw(left) << ", " << to_string_raw(right)
        << " (types: "
        << left->index() << ", " << right->index() << ")");
    return make_fcall(func.head, {left, right});
}

ExprPtr evaluate_builtin_comparison(const FunctionCall& func, EvaluationContext& ctx) {
    const auto& cmp = comparison_functions();
    auto it = cmp.find(func.head);
    if (it == cmp.end() || func.args.size() != 2) {
        return nullptr;
    }

    auto left = evaluate(func.args[0], ctx);
    auto right = evaluate(func.args[1], ctx);
    if (std::holds_alternative<Number>(*left) && std::holds_alternative<Number>(*right)) {
        double arg1 = get_number_value(left);
        double arg2 = get_number_value(right);
        if (ctx.strict_runtime_semantics()) {
            ensure_finite_number(arg1);
            ensure_finite_number(arg2);
        }
        return make_expr<Boolean>(it->second(arg1, arg2));
    }
    if (auto left_exact = exact_rational_atom(left), right_exact = exact_rational_atom(right);
        left_exact.has_value() && right_exact.has_value()) {
        const auto& a = *left_exact;
        const auto& b = *right_exact;
        if (func.head == "Equal")
            return make_expr<Boolean>(a == b);
        if (func.head == "NotEqual")
            return make_expr<Boolean>(!(a == b));
        const int comparison = kernel::compare(a, b);
        if (func.head == "Less")
            return make_expr<Boolean>(comparison < 0);
        if (func.head == "Greater")
            return make_expr<Boolean>(comparison > 0);
        if (func.head == "LessEqual")
            return make_expr<Boolean>(comparison <= 0);
        if (func.head == "GreaterEqual")
            return make_expr<Boolean>(comparison >= 0);
    }
    if (auto left_finite = finite_scalar_atom(left), right_finite = finite_scalar_atom(right);
        left_finite.has_value() && right_finite.has_value()) {
        if (ctx.strict_runtime_semantics()) {
            ensure_finite_number(*left_finite);
            ensure_finite_number(*right_finite);
        }
        return make_expr<Boolean>(it->second(*left_finite, *right_finite));
    }
    if (std::holds_alternative<Rational>(*left) && std::holds_alternative<Number>(*right)) {
        const auto& a = std::get<Rational>(*left);
        double b = std::get<Number>(*right).value;
        double a_val = finite_double_from_exact_rational(a).value_or(std::numeric_limits<double>::infinity());
        return make_expr<Boolean>(it->second(a_val, b));
    }
    if (std::holds_alternative<Number>(*left) && std::holds_alternative<Rational>(*right)) {
        double a = std::get<Number>(*left).value;
        const auto& b = std::get<Rational>(*right);
        double b_val = finite_double_from_exact_rational(b).value_or(std::numeric_limits<double>::infinity());
        if (ctx.strict_runtime_semantics()) {
            ensure_finite_number(a);
        }
        return make_expr<Boolean>(it->second(a, b_val));
    }

    if ((func.head == "Equal" || func.head == "NotEqual")) {
        if (std::holds_alternative<Boolean>(*left) && std::holds_alternative<Boolean>(*right)) {
            const bool result = std::get<Boolean>(*left).value == std::get<Boolean>(*right).value;
            return make_expr<Boolean>(func.head == "Equal" ? result : !result);
        }
        if (std::holds_alternative<String>(*left) && std::holds_alternative<String>(*right)) {
            const bool result = std::get<String>(*left).value == std::get<String>(*right).value;
            return make_expr<Boolean>(func.head == "Equal" ? result : !result);
        }
        if (ctx.strict_runtime_semantics()) {
            throw_runtime_type_mismatch("Equality comparisons require comparable value types.");
        }
    } else if (ctx.strict_runtime_semantics()) {
        throw_runtime_type_mismatch("Comparison operators require numeric values.");
    }

    if (auto assumed = ctx.assumptions.evaluate_comparison(func.head, left, right); assumed.has_value()) {
        return make_expr<Boolean>(*assumed);
    }

    return make_fcall(func.head, {left, right});
}

ExprPtr evaluate_builtin_negate(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 1) {
        throw_invalid_arity_exact("Negate", 1);
    }

    auto arg = evaluate(func.args[0], ctx);
    if (std::holds_alternative<Number>(*arg)) {
        return make_expr<Number>(-get_number_value(arg));
    }
    if (std::holds_alternative<Integer>(*arg)) {
        return make_expr<Integer>(-std::get<Integer>(*arg).value);
    }
    if (std::holds_alternative<Rational>(*arg)) {
        return make_exact_scalar_expr(-std::get<Rational>(*arg).exact());
    }
    if (std::holds_alternative<Complex>(*arg)) {
        const auto& c = std::get<Complex>(*arg);
        return make_expr<Complex>(-c.real, -c.imag);
    }
    if (auto inner = std::get_if<FunctionCall>(arg.get())) {
        if (inner->head == "Times" && !inner->args.empty()) {
            if (auto n = std::get_if<Number>(inner->args[0].get()); n && n->value == -1) {
                return arg;
            }
        }
    }
    return make_fcall("Times", {make_expr<Integer>(-1), arg});
}

ExprPtr evaluate_builtin_clamp(const FunctionCall& func, EvaluationContext& ctx) {
    if (func.args.size() != 3) {
        if (ctx.strict_runtime_semantics()) {
            throw_runtime_invalid_call("Clamp expects three numeric arguments.");
        }
        throw_invalid_arity_exact("Clamp", 3);
    }

    auto value = evaluate(func.args[0], ctx);
    auto low = evaluate(func.args[1], ctx);
    auto high = evaluate(func.args[2], ctx);
    auto numeric_value_opt = finite_scalar_atom(value);
    auto numeric_low_opt = finite_scalar_atom(low);
    auto numeric_high_opt = finite_scalar_atom(high);
    if (!numeric_value_opt.has_value() ||
        !numeric_low_opt.has_value() ||
        !numeric_high_opt.has_value()) {
        if (ctx.strict_runtime_semantics()) {
            throw_runtime_invalid_call("Clamp expects three numeric arguments.");
        }
        return make_fcall("Clamp", {value, low, high});
    }

    const double numeric_value = *numeric_value_opt;
    const double numeric_low = *numeric_low_opt;
    const double numeric_high = *numeric_high_opt;
    if (ctx.strict_runtime_semantics()) {
        ensure_finite_number(numeric_value);
        ensure_finite_number(numeric_low);
        ensure_finite_number(numeric_high);
    }
    if (numeric_low > numeric_high) {
        if (ctx.strict_runtime_semantics()) {
            kernel::throw_runtime_error(
                kernel::ErrorCode::invalid_numeric_domain,
                "Clamp requires the lower bound to be less than or equal to the upper bound.");
        }
        return make_fcall("Clamp", {value, low, high});
    }
    return make_expr<Number>(std::clamp(numeric_value, numeric_low, numeric_high));
}

ExprPtr evaluate_builtin_numeric_or_comparison(const FunctionCall& func, EvaluationContext& ctx) {
    if (is_comparison_function(func.head)) {
        if (auto comparison = evaluate_builtin_comparison(func, ctx)) {
            return comparison;
        }
    }

    if (is_numeric_function(func.head)) {
        if (func.args.size() == 1) {
            if (auto unary = evaluate_builtin_unary(func, ctx)) {
                return unary;
            }
        }
        if (func.args.size() == 2) {
            if (auto binary = evaluate_builtin_binary(func, ctx)) {
                return binary;
            }
        }
    }

    if (auto comparison = evaluate_builtin_comparison(func, ctx)) {
        return comparison;
    }

    const auto& rules = simplification_rules();
    auto simp_it = rules.find(func.head);
    if (simp_it != rules.end()) {
        const std::function<ExprPtr(const ExprPtr&, EvaluationContext&)> evaluator =
            [](const ExprPtr& expr, EvaluationContext& local_ctx) {
                return evaluate(expr, local_ctx);
            };
        return simp_it->second(func.args, ctx, evaluator);
    }

    return nullptr;
}

void register_builtin_evaluator_execution_specs_impl(kernel::FunctionRegistry& registry) {
    registry.register_builtin_function("Negate", evaluate_builtin_negate);
    registry.register_builtin_function("Clamp", evaluate_builtin_clamp);

    const auto register_family_handler = [&registry](std::string_view name) {
        registry.register_builtin_function(
            std::string(name),
            [](const FunctionCall& func, EvaluationContext& ctx) -> ExprPtr {
                return evaluate_builtin_numeric_or_comparison(func, ctx);
            });
    };

    for (const auto& [name, _] : unary_functions()) {
        register_family_handler(name);
    }
    for (const auto& [name, _] : binary_functions()) {
        register_family_handler(name);
    }
    for (const auto& [name, _] : comparison_functions()) {
        register_family_handler(name);
    }
    for (const auto& [name, _] : simplification_rules()) {
        register_family_handler(name);
    }
}

void validate_builtin_arity(const FunctionCall& func) {
    const auto* semantics = lookup_function_semantics(func.head);
    if (semantics == nullptr || semantics->special_form || validate_function_arity(func.head, func.args.size())) {
        return;
    }

    if (semantics->min_arity && semantics->max_arity && *semantics->min_arity == *semantics->max_arity) {
        throw_invalid_arity_exact(func.head, *semantics->min_arity);
    }
    if (semantics->min_arity && semantics->max_arity) {
        throw_invalid_arity_between(func.head, *semantics->min_arity, *semantics->max_arity);
    }
    if (semantics->min_arity) {
        throw_invalid_arity_at_least(func.head, *semantics->min_arity, func.args.size());
    }
    if (semantics->max_arity) {
        throw_invalid_arity_at_most(func.head, *semantics->max_arity, func.args.size());
    }
}

}  // namespace

void register_builtin_evaluator_execution_specs(kernel::FunctionRegistry& registry) {
    register_builtin_evaluator_execution_specs_impl(registry);
}

bool is_builtin_evaluator_function(std::string_view name, const kernel::FunctionRegistry& registry) {
    return registry.has_builtin_function(std::string(name));
}

ExprPtr evaluate_builtin_function(const FunctionCall& func, EvaluationContext& ctx) {
    const auto* spec = ctx.function_registry().find_builtin_function_spec(func.head);
    if (spec == nullptr) {
        return nullptr;
    }

    validate_builtin_arity(func);
    return spec->handler(func, ctx);
}

}  // namespace aleph3
