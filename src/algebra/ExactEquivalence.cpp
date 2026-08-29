#include "algebra/ExactEquivalence.hpp"

#include "algebra/ExactPolynomialConversion.hpp"
#include "algebra/ExactRationalExpression.hpp"
#include "algebra/PolynomialOps.hpp"
#include "expr/Expr.hpp"
#include "expr/ExprStructural.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace aleph3 {

namespace {

std::vector<std::string> merged_variables(const ExprPtr& left, const ExprPtr& right) {
    auto variables = infer_polynomial_variables(left);
    for (const auto& variable : infer_polynomial_variables(right)) {
        if (std::find(variables.begin(), variables.end(), variable) == variables.end()) {
            variables.push_back(variable);
        }
    }
    std::sort(variables.begin(), variables.end());
    return variables;
}

bool same_restrictions(
    const kernel::DomainRestrictions& left,
    const kernel::DomainRestrictions& right) {
    if (left.excluded_zero_expressions.size() != right.excluded_zero_expressions.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.excluded_zero_expressions.size(); ++index) {
        if (!structural_equal(
                left.excluded_zero_expressions[index],
                right.excluded_zero_expressions[index])) {
            return false;
        }
    }
    return true;
}

bool same_rational_expression(
    const ExactRationalExpression& left,
    const ExactRationalExpression& right) {
    return left.numerator.terms == right.numerator.terms
        && left.denominator.terms == right.denominator.terms
        && same_restrictions(left.restrictions, right.restrictions);
}

}  // namespace

ExactEquivalenceKind prove_exact_equivalence(
    const ExprPtr& left,
    const ExprPtr& right) {
    if (structural_equal(left, right)) {
        return ExactEquivalenceKind::equivalent;
    }

    const auto variables = merged_variables(left, right);
    try {
        const ExactPolynomial difference =
            expr_to_exact_polynomial(left, variables)
            - expr_to_exact_polynomial(right, variables);
        return difference.is_zero()
            ? ExactEquivalenceKind::equivalent
            : ExactEquivalenceKind::not_equivalent;
    } catch (const std::overflow_error&) {
        throw;
    } catch (const std::exception&) {
    }

    try {
        const auto left_rational = cancel_exact_rational_expression(
            exact_rational_expression_from_expr(left, variables));
        const auto right_rational = cancel_exact_rational_expression(
            exact_rational_expression_from_expr(right, variables));
        if (same_rational_expression(left_rational, right_rational)) {
            return ExactEquivalenceKind::equivalent;
        }
        if (left_rational.numerator.terms == right_rational.numerator.terms
            && left_rational.denominator.terms == right_rational.denominator.terms) {
            return ExactEquivalenceKind::unknown;
        }
        return ExactEquivalenceKind::not_equivalent;
    } catch (const std::overflow_error&) {
        throw;
    } catch (const std::domain_error&) {
        throw;
    } catch (const std::exception&) {
    }

    return ExactEquivalenceKind::unknown;
}

}  // namespace aleph3
