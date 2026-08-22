#include "algebra/ExactRationalExpression.hpp"

#include "algebra/ExactPolynomialConversion.hpp"
#include "algebra/ExactPolynomialOps.hpp"
#include "evaluator/EvaluatorErrors.hpp"
#include "expr/ExprUtils.hpp"

#include <numeric>
#include <stdexcept>
#include <utility>

namespace aleph3 {

namespace {

bool is_exact_nonzero_constant(const ExactPolynomial& polynomial) {
    return polynomial.terms.size() == 1
        && polynomial.terms.begin()->first.empty()
        && !polynomial.terms.begin()->second.is_zero();
}

void add_nonconstant_nonzero_restriction(
    kernel::DomainRestrictions& restrictions,
    const ExactPolynomial& polynomial) {
    if (is_exact_nonzero_constant(polynomial)) return;
    restrictions.add_excluded_zero(exact_polynomial_to_expr(polynomial));
}

}  // namespace

ExactRationalExpression normalize_exact_rational_expression(
    ExactPolynomial numerator,
    ExactPolynomial denominator,
    std::vector<std::string> variables) {
    return normalize_exact_rational_expression(
        std::move(numerator),
        std::move(denominator),
        std::move(variables),
        {});
}

ExactRationalExpression normalize_exact_rational_expression(
    ExactPolynomial numerator,
    ExactPolynomial denominator,
    std::vector<std::string> variables,
    kernel::DomainRestrictions restrictions) {
    if (is_exact_constant_zero(denominator)) {
        throw std::domain_error("Rational expression denominator is zero");
    }

    const int64_t numerator_lcm = coefficient_denominator_lcm(numerator);
    const int64_t denominator_lcm = coefficient_denominator_lcm(denominator);
    numerator = multiply_by_scalar(numerator, ExactCoefficient(numerator_lcm, 1));
    denominator = multiply_by_scalar(denominator, ExactCoefficient(denominator_lcm, 1));
    numerator = multiply_by_scalar(numerator, ExactCoefficient(denominator_lcm, 1));
    denominator = multiply_by_scalar(denominator, ExactCoefficient(numerator_lcm, 1));

    const int64_t common_integer_content = std::gcd(
        integer_content(numerator),
        integer_content(denominator));
    numerator = divide_by_integer_content(std::move(numerator), common_integer_content);
    denominator = divide_by_integer_content(std::move(denominator), common_integer_content);

    const ExactCoefficient denominator_lead =
        leading_coefficient_for_order(denominator, variables);
    if (denominator_lead.numerator < 0) {
        numerator = multiply_by_scalar(numerator, ExactCoefficient(-1, 1));
        denominator = multiply_by_scalar(denominator, ExactCoefficient(-1, 1));
    }

    return {
        std::move(numerator),
        std::move(denominator),
        std::move(variables),
        std::move(restrictions)
    };
}

ExactRationalExpression exact_rational_expression_from_expr(
    const ExprPtr& expr,
    const std::vector<std::string>& variables) {
    if (const auto* divide = std::get_if<FunctionCall>(&(*expr));
        divide && divide->head == "Divide" && divide->args.size() == 2) {
        auto numerator = exact_rational_expression_from_expr(divide->args[0], variables);
        auto denominator = exact_rational_expression_from_expr(divide->args[1], variables);
        kernel::DomainRestrictions restrictions = numerator.restrictions;
        restrictions.merge(denominator.restrictions);
        add_nonconstant_nonzero_restriction(restrictions, denominator.numerator);
        return normalize_exact_rational_expression(
            numerator.numerator * denominator.denominator,
            numerator.denominator * denominator.numerator,
            variables,
            std::move(restrictions));
    }

    if (const auto* times = std::get_if<FunctionCall>(&(*expr));
        times && times->head == "Times") {
        ExactPolynomial numerator(ExactCoefficient::one());
        ExactPolynomial denominator(ExactCoefficient::one());
        kernel::DomainRestrictions restrictions;
        for (const auto& arg : times->args) {
            auto factor = exact_rational_expression_from_expr(arg, variables);
            restrictions.merge(factor.restrictions);
            numerator = numerator * factor.numerator;
            denominator = denominator * factor.denominator;
        }
        return normalize_exact_rational_expression(
            std::move(numerator),
            std::move(denominator),
            variables,
            std::move(restrictions));
    }

    if (const auto* plus = std::get_if<FunctionCall>(&(*expr));
        plus && plus->head == "Plus") {
        ExactPolynomial numerator(ExactCoefficient::zero());
        ExactPolynomial denominator(ExactCoefficient::one());
        kernel::DomainRestrictions restrictions;
        for (const auto& arg : plus->args) {
            auto term = exact_rational_expression_from_expr(arg, variables);
            restrictions.merge(term.restrictions);
            if (variables.empty()) {
                numerator = numerator * term.denominator + term.numerator * denominator;
                denominator = denominator * term.denominator;
                continue;
            }

            const ExactPolynomial common_denominator =
                gcd(denominator, term.denominator, variables);
            auto [left_scale, left_remainder] =
                divide(term.denominator, common_denominator, variables);
            auto [right_scale, right_remainder] =
                divide(denominator, common_denominator, variables);
            if (!left_remainder.is_zero() || !right_remainder.is_zero()) {
                throw_internal_inconsistency(
                    "Exact rational-expression denominator GCD did not divide both denominators");
            }
            numerator = numerator * left_scale + term.numerator * right_scale;
            denominator = denominator * left_scale;
        }
        return normalize_exact_rational_expression(
            std::move(numerator),
            std::move(denominator),
            variables,
            std::move(restrictions));
    }

    const ExactPolynomial polynomial = expr_to_exact_polynomial(expr, variables);
    return normalize_exact_rational_expression(
        polynomial,
        exact_constant(ExactCoefficient::one()),
        variables,
        {});
}

ExprPtr exact_rational_expression_to_expr(
    const ExactRationalExpression& expression) {
    ExprPtr numerator = exact_polynomial_to_expr(expression.numerator);
    if (is_exact_constant_one(expression.denominator)) return numerator;
    return make_expr<FunctionCall>(
        "Divide",
        std::vector<ExprPtr>{
            std::move(numerator),
            exact_polynomial_to_expr(expression.denominator)
        });
}

ExactRationalExpression cancel_exact_rational_expression(
    ExactRationalExpression expression) {
    if (expression.variables.empty()) return expression;

    const ExactPolynomial common =
        gcd(expression.numerator, expression.denominator, expression.variables);
    if (is_exact_constant_one(common)) return expression;
    add_nonconstant_nonzero_restriction(expression.restrictions, common);

    auto [numerator, numerator_remainder] =
        divide(expression.numerator, common, expression.variables);
    auto [denominator, denominator_remainder] =
        divide(expression.denominator, common, expression.variables);
    if (!numerator_remainder.is_zero() || !denominator_remainder.is_zero()) {
        throw_internal_inconsistency("Exact rational-expression GCD did not divide both parts");
    }

    return normalize_exact_rational_expression(
        std::move(numerator),
        std::move(denominator),
        expression.variables,
        std::move(expression.restrictions));
}

}  // namespace aleph3
