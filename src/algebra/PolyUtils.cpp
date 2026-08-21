#include "algebra/PolyUtils.hpp"

#include "algebra/ExactFactorization.hpp"
#include "algebra/ExactPolynomialConversion.hpp"
#include "algebra/ExactPolynomialOps.hpp"
#include "algebra/ExactRationalExpression.hpp"
#include "algebra/PolynomialOps.hpp"
#include "evaluator/EvaluatorErrors.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace aleph3 {

ExprPtr expand_polynomial(const ExprPtr& expr, EvaluationContext& ctx) {
    static_cast<void>(ctx);
    const std::vector<std::string> variables = infer_polynomial_variables(expr);
    if (is_exact_polynomial_candidate(expr)) {
        return exact_polynomial_to_expr(expand(expr_to_exact_polynomial(expr, variables)));
    }

    const Polynomial poly = expr_to_polynomial(expr, variables);
    return polynomial_to_expr(expand(poly));
}

ExprPtr factor_polynomial(const ExprPtr& expr, EvaluationContext& ctx) {
    static_cast<void>(ctx);
    const std::vector<std::string> variables = infer_polynomial_variables(expr);
    if (is_exact_polynomial_candidate(expr)) {
        try {
            const auto exact = expr_to_exact_polynomial(expr, variables);
            const bool has_rational_coefficient = std::any_of(
                exact.terms.begin(),
                exact.terms.end(),
                [](const auto& term) {
                    return term.second.denominator != 1;
                });
            if (has_rational_coefficient && variables.size() > 1) {
                throw_unsupported_construct(
                    "Factor does not support multivariate rational coefficients");
            }
            return factor_exact_polynomial(exact, variables);
        } catch (const std::overflow_error& error) {
            kernel::throw_runtime_error(kernel::ErrorCode::exact_overflow, error.what());
        }
    }

    return factor_approximate_polynomial(expr_to_polynomial(expr, variables));
}

ExprPtr collect_polynomial(
    const ExprPtr& expr,
    const std::vector<std::string>& variables,
    EvaluationContext& ctx) {
    static_cast<void>(ctx);
    auto polynomial_variables = infer_polynomial_variables(expr);
    for (const auto& variable : variables) {
        if (std::find(
                polynomial_variables.begin(),
                polynomial_variables.end(),
                variable) == polynomial_variables.end()) {
            polynomial_variables.push_back(variable);
        }
    }

    if (is_exact_polynomial_candidate(expr)) {
        return exact_polynomial_to_expr(
            collect(expr_to_exact_polynomial(expr, polynomial_variables), variables));
    }

    const Polynomial poly = expr_to_polynomial(expr, polynomial_variables);
    return polynomial_to_expr(collect(poly, variables));
}

ExprPtr gcd_polynomial(
    const ExprPtr& a,
    const ExprPtr& b,
    const std::vector<std::string>& variables,
    EvaluationContext& ctx) {
    static_cast<void>(ctx);
    if (is_exact_polynomial_candidate(a) && is_exact_polynomial_candidate(b)) {
        const ExactPolynomial left = expr_to_exact_polynomial(a, variables);
        const ExactPolynomial right = expr_to_exact_polynomial(b, variables);
        return exact_polynomial_to_expr(gcd(left, right, variables));
    }

    if (variables.size() > 1) {
        throw_unsupported_construct(
            "gcd: multivariate GCD requires exact polynomial coefficients");
    }

    const Polynomial left = expr_to_polynomial(a, variables);
    const Polynomial right = expr_to_polynomial(b, variables);
    return polynomial_to_expr(gcd(left, right, variables));
}

std::pair<ExprPtr, ExprPtr> divide_polynomial(
    const ExprPtr& dividend,
    const ExprPtr& divisor,
    const std::vector<std::string>& variables,
    EvaluationContext& ctx) {
    static_cast<void>(ctx);
    if (is_exact_polynomial_candidate(dividend) &&
        is_exact_polynomial_candidate(divisor)) {
        const ExactPolynomial left = expr_to_exact_polynomial(dividend, variables);
        const ExactPolynomial right = expr_to_exact_polynomial(divisor, variables);
        auto [quotient, remainder] = divide(left, right, variables);
        return {
            exact_polynomial_to_expr(quotient),
            exact_polynomial_to_expr(remainder)
        };
    }

    const Polynomial left = expr_to_polynomial(dividend, variables);
    const Polynomial right = expr_to_polynomial(divisor, variables);
    auto [quotient, remainder] = divide(left, right, variables);
    return {polynomial_to_expr(quotient), polynomial_to_expr(remainder)};
}

ExprPtr coefficient_polynomial(
    const ExprPtr& expr,
    const std::string& variable,
    int exponent,
    EvaluationContext& ctx) {
    static_cast<void>(ctx);
    if (exponent < 0) {
        throw_invalid_form("Coefficient exponent must be a non-negative integer");
    }

    const std::vector<std::string> variables{variable};
    const ExactPolynomial poly = expr_to_exact_polynomial(expr, variables);
    ExactCoefficient coefficient = ExactCoefficient::zero();
    for (const auto& [monomial, term_coefficient] : poly.terms) {
        if (monomial_exponent(monomial, variable) == exponent) {
            coefficient = coefficient + term_coefficient;
        }
    }
    return exact_coefficient_to_expr(coefficient);
}

ExprPtr coefficient_list_polynomial(
    const ExprPtr& expr,
    const std::string& variable,
    EvaluationContext& ctx) {
    static_cast<void>(ctx);
    const std::vector<std::string> variables{variable};
    const ExactPolynomial poly = expr_to_exact_polynomial(expr, variables);
    const int degree = exact_degree_in_variable(poly, variable);
    std::vector<ExprPtr> coefficients;
    coefficients.reserve(static_cast<std::size_t>(degree) + 1);
    for (int exponent = 0; exponent <= degree; ++exponent) {
        ExactCoefficient coefficient = ExactCoefficient::zero();
        for (const auto& [monomial, term_coefficient] : poly.terms) {
            if (monomial_exponent(monomial, variable) == exponent) {
                coefficient = coefficient + term_coefficient;
            }
        }
        coefficients.push_back(exact_coefficient_to_expr(coefficient));
    }
    return make_expr<List>(List{std::move(coefficients)});
}

ExprPtr numerator_rational_expression(const ExprPtr& expr, EvaluationContext& ctx) {
    static_cast<void>(ctx);
    try {
        const auto variables = infer_polynomial_variables(expr);
        return exact_polynomial_to_expr(
            exact_rational_expression_from_expr(expr, variables).numerator);
    } catch (const std::overflow_error& error) {
        kernel::throw_runtime_error(kernel::ErrorCode::exact_overflow, error.what());
    } catch (const std::domain_error& error) {
        kernel::throw_runtime_error(kernel::ErrorCode::division_by_zero, error.what());
    }
}

ExprPtr denominator_rational_expression(const ExprPtr& expr, EvaluationContext& ctx) {
    static_cast<void>(ctx);
    try {
        const auto variables = infer_polynomial_variables(expr);
        return exact_polynomial_to_expr(
            exact_rational_expression_from_expr(expr, variables).denominator);
    } catch (const std::overflow_error& error) {
        kernel::throw_runtime_error(kernel::ErrorCode::exact_overflow, error.what());
    } catch (const std::domain_error& error) {
        kernel::throw_runtime_error(kernel::ErrorCode::division_by_zero, error.what());
    }
}

ExprPtr together_rational_expression(const ExprPtr& expr, EvaluationContext& ctx) {
    static_cast<void>(ctx);
    try {
        const auto variables = infer_polynomial_variables(expr);
        return exact_rational_expression_to_expr(
            exact_rational_expression_from_expr(expr, variables));
    } catch (const std::overflow_error& error) {
        kernel::throw_runtime_error(kernel::ErrorCode::exact_overflow, error.what());
    } catch (const std::domain_error& error) {
        kernel::throw_runtime_error(kernel::ErrorCode::division_by_zero, error.what());
    }
}

ExprPtr cancel_rational_expression(const ExprPtr& expr, EvaluationContext& ctx) {
    static_cast<void>(ctx);
    try {
        const auto variables = infer_polynomial_variables(expr);
        return exact_rational_expression_to_expr(
            cancel_exact_rational_expression(
                exact_rational_expression_from_expr(expr, variables)));
    } catch (const std::overflow_error& error) {
        kernel::throw_runtime_error(kernel::ErrorCode::exact_overflow, error.what());
    } catch (const std::domain_error& error) {
        kernel::throw_runtime_error(kernel::ErrorCode::division_by_zero, error.what());
    }
}

}  // namespace aleph3
