# Algebra Supported Subset

## Purpose

This document defines the current product contract for Aleph3's symbolic
algebra layer.

The goal is not broad CAS parity. The goal is a narrow subset whose behavior
is explicit, regression-tested, and stable enough to build on.

## Supported Functions

The current symbolic algebra surface is:

- `Expand[expr]`
- `Factor[expr]`
- `Collect[expr, var]`
- `Collect[expr, {var1, ...}]`
- `GCD[a, b]`
- `GCD[a, b, var]`
- `GCD[a, b, {var1, ...}]` for exact monomial-bounded multivariate inputs
- `PolynomialQuotient[a, b]`
- `PolynomialQuotient[a, b, var]`
- `PolynomialQuotient[a, b, {var1, ...}]` for exact multivariate inputs
- `MatrixAdd[a, b]`, `MatrixMultiply[a, b]`, `IdentityMatrix[n]`, and `Transpose[a]`
- `Det[a]`, `RowReduce[a]`, and `LinearSolve[a, b]` for bounded exact dense matrices

The polynomial functions operate on polynomial expressions. Matrix functions
operate on exact rectangular nested lists. Unsupported forms fail
explicitly rather than silently approximating or partially rewriting.

## Exact Dense Matrices

Matrices use nested lists at the expression boundary and an algebra-owned
row-major value type internally. The supported 4,096-element exact surface
includes shape-checked addition and multiplication, identity construction,
transpose, determinant, reduced row-echelon form, and unique square-system
solving. See the [dense-matrix specification](algebra_dense_matrix_spec.md).

Symbolic, decimal, complex, empty, sparse, and arbitrary-rank inputs remain
unsupported. Matrix operations never reinterpret scalar `Plus` or `Times`.

## Ownership Contract

This algebra surface is now pack-owned rather than evaluator-local.

Current ownership boundary:

- polynomial helper functions are registered through the `core-algebra` pack
- kernel contracts provide the shared expression, diagnostics, registration,
  and evaluation context layers beneath that pack
- the current product contract depends on registry-backed pack ownership, not
  on special evaluator-only dispatch branches

## Canonical Output Contract

For the underlying vocabulary—coefficient, monomial, total degree, variable
precedence, and graded lexicographic order—see
[Polynomial Vocabulary](manual/concepts-and-terminology.md#polynomial-vocabulary).

Supported algebra outputs follow these rules:

- expanded and collected outputs use the engine's canonical commutative order
- monomials are ordered by descending total degree, then lexicographically
- collected and expanded sums present algebraic terms ahead of opaque calls
- factoring preserves a deterministic factor order for extracted linear factors
- zero results normalize to `0`
- constant-one factors are not retained after normalization
- extracted negative content stays on the leading scalar factor
- renormalizing or simplifying a supported algebra result should keep the same
  mathematical structure

Examples:

- `Expand[(y + x) * (x + z)]` -> `x^2 + x * y + x * z + y * z`
- `Collect[y*x + x^2 + z*x, x]` -> `x^2 + x * y + x * z`
- `Factor[x^5 - x^3]` -> `x^3 * (x - 1) * (x + 1)`
- `Factor[(-2)*x^2 + (-4)*x]` -> `-2 * x * (x + 2)`

## Variable Policy

Variable selectors follow this contract:

- a selector must be a symbol or a non-empty list of symbols
- duplicate symbols in a selector list are ignored after the first occurrence
- `Collect` accepts a selector that names variables not present in the input
- `GCD` and `PolynomialQuotient` infer a single variable from both operands
  when no explicit selector is provided
- exact multivariate `PolynomialQuotient` requires an explicit selector list;
  its order defines variable precedence under fixed graded lexicographic order
- exact multivariate `GCD` requires an explicit selector list and at least one
  single-term monomial operand when both operands are nonzero
- inexact multivariate `GCD` and division remain unsupported

Examples of explicit failures:

- `Collect[x^2 + 1, 3]` -> invalid selector
- `Collect[x^2 + y, {}]` -> empty selector list
- `GCD[x^2 - 1, y - 1]` -> unsupported multivariate inference
- `GCD[x + y, x - y, {x, y}]` -> unsupported two-multi-term case
- `PolynomialQuotient[x*y, x]` -> explicit selector required
- `PolynomialQuotient[0.5*x*y, x, {x, y}]` -> unsupported inexact division

## Exact Rational Contract

Exact rationals are a supported core value type in the parser, evaluator, and
general simplification paths.

That support now extends to a narrow exact-polynomial path for the safe helpers
above, but not to the whole algebra stack.

Current boundary:

- exact rational arithmetic such as `1/2 + 1/3` stays exact
- mixed rational and integer arithmetic stays exact when no inexact numeric
  value is introduced
- mixed rational and floating-point arithmetic demotes to inexact `Number`
- `Expand` and `Collect` preserve exact rational coefficients for both
  univariate and multivariate supported polynomial inputs
- supported univariate `GCD` and univariate or explicitly selected multivariate
  `PolynomialQuotient` preserve exact rational coefficients
- supported explicitly selected multivariate `GCD` returns the monic common
  monomial determined by the operands' minimum variable exponents
- inexact `Number` inputs stay on the existing floating-point path
- `Factor` supports exact rational coefficients for univariate rational-root
  factorization by clearing denominators and restoring exact scalar content
- multivariate rational factorization remains explicitly unsupported

Examples:

- `1/2 + 1/3` -> `5/6`
- `1/2 + 2` -> `5/2`
- `1/2 + 0.5` -> inexact `Number`
- `Expand[(1/2) * (x + 1)]` -> `1/2 * x + 1/2`
- `Expand[(1/2) * (x + y)]` -> `1/2 * x + 1/2 * y`
- `Collect[(1/2) * x + 1, x]` -> `1/2 * x + 1`
- `Collect[(1/2) * x * y + (3/2) * y, y]` -> `1/2 * x * y + 3/2 * y`
- `PolynomialQuotient[x^2 - 1/4, x - 1/2, x]` -> `{x + 1/2, 0}`
- `GCD[x*y + x, x, {x, y}]` -> `x`
- `GCD[x^2*y, x*y^2, {x, y}]` -> `x*y`
- `PolynomialQuotient[x^2*y + x*y^2 + y, x*y, {x, y}]` -> `{x + y, y}`
- `Factor[(1/2) * x^2 + x]` -> `1/2 * x * (x + 2)`
- `Factor[(1/2) * x^2 + x + 1/2]` -> `1/2 * (x + 1) * (x + 1)`

## Symbolic Rewrite Product Contracts

The current symbolic simplification surface also includes two narrow
kernel-owned contracts below the broader polynomial helpers above.

### Coefficient Layer Basis Contract

Supported basis shapes for like-term collection are:

- `x`
- `x^n`
- `c * x`
- `c * x^n`

Where:

- `x` is a single symbol
- `c` is `Number` or `Rational`
- `n` is a supported numeric exponent

This layer is intentionally not a general monomial collector. The following are
outside the supported subset:

- multivariate bases such as `x*y`
- grouped symbolic bases such as `(x + y)`
- call-shaped bases such as `f[x]`
- symbolic coefficients

Outside those shapes, the product contract is preservation, not best-effort
collection.

### Algebra-aware Exponent Contract

Supported exponent behavior is limited to:

- same-symbol exponent accumulation in normalized multiplicative forms
- nested numeric power collapse such as `(x^2)^3 -> x^6`

The following remain outside the supported subset:

- base-sensitive transforms across different symbols
- division cancellation
- branch- or domain-sensitive power laws
- list-aware arithmetic

Outside those shapes, the product contract is preservation, not heuristic power
simplification.

## Factorization Contract

`Factor` currently supports:

- constant inputs
- monomial-content extraction
- deterministic linear-factor extraction for supported univariate integer
  polynomials
- exact rational content and coefficients for supported univariate
  rational-root factorization
- rational-root cases that reduce to integer-coefficient linear factors

`Factor` does not yet support:

- multivariate exact rational factorization
- general multivariate factorization beyond content extraction
- higher-degree irreducible decomposition beyond the supported rational-root
  path
- arbitrary-precision exact arithmetic

The current coefficient storage uses `double` in the polynomial layer and
`int64_t` for exact rationals. Large intermediates and overflow risk remain a
known limitation.

## Future Work

Not part of the current supported subset:

- general multivariate polynomial GCD and configurable or multi-divisor division
- exact multivariate factorization beyond current content extraction
- symbolic differentiation
- broader factorization algorithms
- arbitrary-precision exact algebra
- symbolic, approximate, sparse, or arbitrary-rank matrix algebra
