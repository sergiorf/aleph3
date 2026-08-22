/*
 * ExactEquivalence.hpp
 * --------------------
 * Bounded proof helper for algebra-pack mathematical equivalence.
 */

#pragma once

#include "expr/Expr.hpp"

namespace aleph3 {

enum class ExactEquivalenceKind {
    equivalent,
    not_equivalent,
    unknown
};

ExactEquivalenceKind prove_exact_equivalence(
    const ExprPtr& left,
    const ExprPtr& right);

}  // namespace aleph3
