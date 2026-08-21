#include "algebra/ExactPolynomialConversion.hpp"

#include "evaluator/EvaluatorErrors.hpp"
#include "expr/ExprUtils.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>

namespace aleph3 {

namespace {

constexpr double EPSILON = 1e-10;

bool is_near_integer(double value) {
    return std::abs(value - std::round(value)) < EPSILON;
}

int64_t rounded_integer(double value) {
    return static_cast<int64_t>(std::llround(value));
}

bool contains_variable(
    const std::vector<std::string>& variables,
    const std::string& name) {
    return std::find(variables.begin(), variables.end(), name) != variables.end();
}

bool monomial_precedes(const Monomial& left, const Monomial& right) {
    return exact_monomial_precedes(
        left,
        right,
        MonomialOrder::graded_lexicographic);
}

std::vector<std::pair<Monomial, ExactCoefficient>> ordered_exact_terms(
    const std::map<Monomial, ExactCoefficient>& terms) {
    std::vector<std::pair<Monomial, ExactCoefficient>> ordered;
    ordered.reserve(terms.size());
    for (const auto& [monomial, coefficient] : terms) {
        if (!coefficient.is_zero()) ordered.emplace_back(monomial, coefficient);
    }

    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const auto& left, const auto& right) {
            return monomial_precedes(left.first, right.first);
        });

    return ordered;
}

ExactPolynomial expr_to_exact_polynomial_impl(
    const ExprPtr& expr,
    const std::vector<std::string>& variables) {
    if (!expr) throw_internal_inconsistency("Null expression");

    auto make_monomial = [&](const std::map<std::string, int>& exponents) -> Monomial {
        Monomial mono;
        for (const auto& variable : variables) {
            auto it = exponents.find(variable);
            if (it != exponents.end() && it->second != 0) mono[variable] = it->second;
        }
        return mono;
    };

    std::function<ExactPolynomial(const ExprPtr&)> recur =
        [&](const ExprPtr& current) -> ExactPolynomial {
        if (const auto* number = std::get_if<Number>(&(*current))) {
            if (!is_near_integer(number->value)) {
                throw_unsupported_construct(
                    "Exact polynomial conversion does not accept inexact numeric coefficients");
            }
            return ExactPolynomial(ExactCoefficient(rounded_integer(number->value), 1));
        }
        if (const auto* rational = std::get_if<Rational>(&(*current))) {
            return ExactPolynomial(
                ExactCoefficient(rational->numerator, rational->denominator));
        }
        if (const auto* symbol = std::get_if<Symbol>(&(*current))) {
            if (!contains_variable(variables, symbol->name)) {
                throw_invalid_form(
                    "expr_to_polynomial: Symbol `" + symbol->name +
                    "` is not in the selected polynomial variable set");
            }
            std::map<std::string, int> exponents;
            exponents[symbol->name] = 1;
            return ExactPolynomial({{make_monomial(exponents), ExactCoefficient::one()}});
        }
        if (const auto* plus = std::get_if<FunctionCall>(&(*current));
            plus && plus->head == "Plus") {
            ExactPolynomial result;
            for (const auto& arg : plus->args) result = result + recur(arg);
            return result;
        }
        if (const auto* minus = std::get_if<FunctionCall>(&(*current));
            minus && minus->head == "Minus" && minus->args.size() == 2) {
            return recur(minus->args[0]) - recur(minus->args[1]);
        }
        if (const auto* negate = std::get_if<FunctionCall>(&(*current));
            negate && negate->head == "Negate" && negate->args.size() == 1) {
            return ExactPolynomial(ExactCoefficient::zero()) - recur(negate->args[0]);
        }
        if (const auto* times = std::get_if<FunctionCall>(&(*current));
            times && times->head == "Times") {
            ExactPolynomial result(ExactCoefficient::one());
            for (const auto& arg : times->args) result = result * recur(arg);
            return result;
        }
        if (const auto* power = std::get_if<FunctionCall>(&(*current));
            power && power->head == "Power" && power->args.size() == 2) {
            const auto& base = power->args[0];
            const auto& exponent = power->args[1];
            if (const auto* symbol = std::get_if<Symbol>(&(*base))) {
                if (!contains_variable(variables, symbol->name)) {
                    throw_invalid_form(
                        "expr_to_polynomial: Symbol `" + symbol->name +
                        "` is not in the selected polynomial variable set");
                }
                if (const auto* number_exponent = std::get_if<Number>(&(*exponent))) {
                    if (!is_near_integer(number_exponent->value) ||
                        number_exponent->value < 0.0) {
                        throw_invalid_form(
                            "expr_to_polynomial: Polynomial powers require non-negative "
                            "integer exponents");
                    }
                    std::map<std::string, int> exponents;
                    exponents[symbol->name] = static_cast<int>(number_exponent->value);
                    return ExactPolynomial({
                        {make_monomial(exponents), ExactCoefficient::one()}
                    });
                }
            }
        }
        throw_unsupported_construct("expr_to_polynomial: Not implemented for this expression");
    };

    return recur(expr);
}

}  // namespace

bool is_exact_polynomial_candidate(const ExprPtr& expr) {
    if (!expr) return false;
    if (const auto* number = std::get_if<Number>(&(*expr))) return is_near_integer(number->value);
    if (std::holds_alternative<Rational>(*expr)) return true;
    if (const auto* call = std::get_if<FunctionCall>(&(*expr))) {
        for (const auto& arg : call->args) {
            if (!is_exact_polynomial_candidate(arg)) return false;
        }
    }
    return true;
}

ExprPtr exact_coefficient_to_expr(const ExactCoefficient& coefficient) {
    if (coefficient.denominator == 1) {
        return make_expr<Number>(static_cast<double>(coefficient.numerator));
    }
    return make_expr<Rational>(coefficient.numerator, coefficient.denominator);
}

ExactPolynomial expr_to_exact_polynomial(
    const ExprPtr& expr,
    const std::vector<std::string>& variables) {
    return expr_to_exact_polynomial_impl(expr, variables);
}

ExprPtr exact_polynomial_to_expr(const ExactPolynomial& poly) {
    const auto terms_in_order = ordered_exact_terms(poly.terms);
    std::vector<ExprPtr> terms;
    for (const auto& [monomial, coefficient] : terms_in_order) {
        if (monomial.empty()) {
            terms.push_back(exact_coefficient_to_expr(coefficient));
            continue;
        }

        std::vector<ExprPtr> factors;
        if (!coefficient.is_one()) factors.push_back(exact_coefficient_to_expr(coefficient));
        for (const auto& [var, exponent] : monomial) {
            ExprPtr variable = make_expr<Symbol>(var);
            if (exponent == 1) {
                factors.push_back(variable);
            } else {
                factors.push_back(make_fcall(
                    "Power",
                    {variable, make_expr<Number>(static_cast<double>(exponent))}));
            }
        }
        terms.push_back(factors.size() == 1 ? factors.front() : make_times(factors));
    }

    if (terms.empty()) return make_expr<Number>(0.0);
    if (terms.size() == 1) return terms[0];
    return make_expr<FunctionCall>("Plus", terms);
}

}  // namespace aleph3
