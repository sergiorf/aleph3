# Exact Dense-Matrix Specification

## Ownership And Representation

Exact dense matrices are owned by the `core-algebra` pack. Public expressions
use non-empty rectangular nested lists; the pack converts them to the
algebra-owned `DenseMatrix<ExactCoefficient>` row-major value type. Matrices do
not add a kernel `Expr` alternative and do not change scalar `Plus` or `Times`.

## Supported Surface

- `MatrixAdd[a, b]` requires equal shapes.
- `MatrixMultiply[a, b]` requires compatible inner dimensions.
- `IdentityMatrix[n]` accepts an integer from 1 through 64.
- `Transpose[a]` accepts rectangular matrices.
- `Det[a]` requires a square matrix.
- `RowReduce[a]` returns exact reduced row-echelon form.
- `LinearSolve[a, b]` requires square `a`, a matching vector `b`, and one
  unique solution.

Entries must be exact integers or normalized rationals. Results use canonical
nested lists, except determinants use an exact scalar and linear solutions use
an exact vector. Checked `int64_t` rational arithmetic never falls back to
floating point.

## Diagnostics And Budgets

- Empty, non-list, or ragged matrices report `kernel.invalid_form`.
- Shape mismatches, invalid identity sizes, oversized matrices, and singular
  or non-unique systems report `kernel.domain_violation`.
- Symbolic, decimal, and complex entries report
  `kernel.unsupported_construct`.
- Checked arithmetic overflow reports `kernel.exact_overflow`.
- Matrix multiplication and elimination charge one shared evaluation step per
  scalar arithmetic update and report `kernel.step_budget_exhausted` when the
  configured budget is exhausted.

Inputs are limited to 4,096 elements. Output dimensions are bounded by valid
input dimensions; `IdentityMatrix[64]` is the largest identity.

## Unsupported Boundaries

The first surface excludes empty and 0-by-0 matrices, symbolic entries or
dimensions, approximate and complex matrices, inverses, sparse matrices,
eigenvalue workflows, advanced decompositions, multiple right-hand sides,
parametric solutions, and arbitrary-rank tensors.
