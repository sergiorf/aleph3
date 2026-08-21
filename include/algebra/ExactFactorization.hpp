/*
 * ExactFactorization.hpp
 * ----------------------
 * Exact factorization helpers for the supported algebra-pack subset.
 */

#pragma once

#include "algebra/ExactPolynomial.hpp"
#include "expr/Expr.hpp"

#include <string>
#include <vector>

namespace aleph3 {

ExprPtr factor_exact_polynomial(
    const ExactPolynomial& poly,
    const std::vector<std::string>& variables);

}  // namespace aleph3
