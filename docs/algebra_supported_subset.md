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
- `PolynomialRemainder[a, b]`
- `PolynomialRemainder[a, b, var]`
- `PolynomialRemainder[a, b, {var1, ...}]` for exact multivariate inputs
- `PolynomialDegree[poly, var]`
- `LeadingCoefficient[poly, var]`
- `Coefficient[poly, var]`
- `Coefficient[poly, var, n]`
- `CoefficientList[poly, var]`
- `Numerator[expr]`
- `Denominator[expr]`
- `Together[expr]`
- `Cancel[expr]`
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
- `GCD`, `PolynomialQuotient`, and `PolynomialRemainder` infer a single
  variable from both operands when no explicit selector is provided
- exact multivariate `PolynomialQuotient` and `PolynomialRemainder` require an
  explicit selector list; its order defines variable precedence under fixed
  graded lexicographic order
- exact multivariate `GCD` requires an explicit selector list and at least one
  single-term monomial operand when both operands are nonzero
- inexact multivariate `GCD` and division remain unsupported

Examples of explicit failures:

- `Collect[x^2 + 1, 3]` -> invalid selector
- `Collect[x^2 + y, {}]` -> empty selector list
- `GCD[x^2 - 1, y - 1]` -> unsupported multivariate inference
- `GCD[x + y, x - y, {x, y}]` -> unsupported two-multi-term case
- `PolynomialQuotient[x*y, x]` -> explicit selector required
- `PolynomialRemainder[x*y, x]` -> explicit selector required
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
  `PolynomialQuotient` and `PolynomialRemainder` preserve exact rational
  coefficients
- `PolynomialDegree` and `LeadingCoefficient` inspect exact univariate
  integer/rational polynomials without approximate fallback
- `Coefficient` and `CoefficientList` preserve exact integer and rational
  coefficients in the selected univariate polynomial subset
- supported explicitly selected multivariate `GCD` returns the monic common
  monomial determined by the operands' minimum variable exponents
- inexact `Number` inputs stay on the existing floating-point path
- `Factor` supports exact rational coefficients for univariate rational-root
  factorization through the exact polynomial path by clearing denominators and
  restoring exact scalar content
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
- `PolynomialRemainder[x^2 + 1, x + 1, x]` -> `2`
- `PolynomialRemainder[x^2*y + x*y^2 + y, x*y, {x, y}]` -> `y`
- `PolynomialDegree[3*x^2 + 2*x + 1, x]` -> `2`
- `PolynomialDegree[7, x]` -> `0`
- `LeadingCoefficient[3*x^2 + 2*x + 1, x]` -> `3`
- `LeadingCoefficient[(1/2)*x^2 + x, x]` -> `1/2`
- `GCD[x*y + x, x, {x, y}]` -> `x`
- `GCD[x^2*y, x*y^2, {x, y}]` -> `x*y`
- `PolynomialQuotient[x^2*y + x*y^2 + y, x*y, {x, y}]` -> `{x + y, y}`
- `Coefficient[3*x^2 + 2*x + 1, x]` -> `2`
- `Coefficient[3*x^2 + 2*x + 1, x, 2]` -> `3`
- `CoefficientList[(1/2)*x^2 + x, x]` -> `{0, 1, 1/2}`
- `Factor[(1/2) * x^2 + x]` -> `1/2 * x * (x + 2)`
- `Factor[(1/2) * x^2 + x + 1/2]` -> `1/2 * (x + 1) * (x + 1)`
- `Numerator[(1/2)*x]` -> `x`
- `Denominator[(1/2)*x]` -> `2`
- `Denominator[x/(x + 1)]` -> `x + 1`
- `Together[1/x + 1/y]` -> `(x + y) / (x * y)`
- `Cancel[(x^2 - 1)/(x - 1)]` -> `x + 1`

## Exact Coefficient Extraction

`Coefficient` and `CoefficientList` are exact extraction helpers over the
current polynomial subset. They do not collect symbolic coefficients and they
do not simplify rational expressions.

Supported forms:

- `Coefficient[poly, x]` returns the coefficient of `x^1`;
- `Coefficient[poly, x, n]` returns the coefficient of `x^n`, where `n` is a
  non-negative exact integer;
- `CoefficientList[poly, x]` returns coefficients from degree zero through the
  largest exponent of `x`.

Boundaries:

- the selector must be a single symbol;
- input must be univariate in the selected symbol for the current contract;
- exact integer and rational coefficients are preserved;
- decimal coefficients, symbolic coefficients, non-polynomial inputs,
  unsupported variables outside the selected univariate polynomial, negative
  or symbolic exponents, and exact coefficient overflow fail explicitly.

## Polynomial Inspection

`PolynomialRemainder`, `PolynomialDegree`, and `LeadingCoefficient` expose
small exact-polynomial facts without broadening the factorization or solving
contract.

`PolynomialRemainder` returns only the remainder part of the same supported
division contract used by `PolynomialQuotient`.

Supported forms:

- `PolynomialRemainder[a, b]`
- `PolynomialRemainder[a, b, x]`
- `PolynomialRemainder[a, b, {x, y}]`

Examples:

- `PolynomialRemainder[x^2 + 1, x + 1, x]` -> `2`
- `PolynomialRemainder[x^2 - 1, x - 1, x]` -> `0`
- `PolynomialRemainder[x^2*y + x*y^2 + y, x*y, {x, y}]` -> `y`

`PolynomialDegree[poly, x]` returns the largest non-negative exponent of `x`
in a supported exact univariate polynomial. Constants have degree zero.

`LeadingCoefficient[poly, x]` returns the exact coefficient of that largest
power.

Examples:

- `PolynomialDegree[3*x^2 + 2*x + 1, x]` -> `2`
- `PolynomialDegree[7, x]` -> `0`
- `LeadingCoefficient[3*x^2 + 2*x + 1, x]` -> `3`
- `LeadingCoefficient[(1/2)*x^2 + x, x]` -> `1/2`

Boundaries:

- `PolynomialRemainder` has the same division-by-zero, selector, inexact
  multivariate, multiple-divisor, and configurable-order boundaries as
  `PolynomialQuotient`;
- `PolynomialDegree` and `LeadingCoefficient` accept one symbol selector and
  the current exact univariate polynomial subset only;
- the zero polynomial is a domain violation for `PolynomialDegree` and
  `LeadingCoefficient` in this slice because Aleph3 does not expose a
  first-class negative-infinity degree value;
- decimal coefficients, symbolic coefficients, non-polynomial inputs,
  unsupported variables outside the selected univariate polynomial, negative
  or symbolic exponents, and exact coefficient overflow fail explicitly.

## Rational Expression Parts

`Numerator` and `Denominator` expose the exact polynomial numerator and
denominator of a bounded rational-expression subset. They clear exact rational
coefficients before returning their result.

Supported forms:

- exact integer and rational values;
- supported exact polynomial expressions;
- products of supported rational-expression factors;
- explicit division with a nonzero supported denominator.

Examples:

- `Numerator[1/2]` -> `1`
- `Denominator[1/2]` -> `2`
- `Numerator[x]` -> `x`
- `Denominator[x]` -> `1`
- `Numerator[(1/2)*x]` -> `x`
- `Denominator[(1/2)*x]` -> `2`
- `Numerator[x/(x + 1)]` -> `x`
- `Denominator[x/(x + 1)]` -> `x + 1`

Boundaries:

- decimal coefficients and unsupported powers are rejected explicitly;
- symbolic coefficients outside the selected exact polynomial subset are
  rejected explicitly;
- denominator zero is a domain failure;
- these part-extraction helpers do not cancel common polynomial factors;
  `Together` and `Cancel` own those transformations.

## Rational Expression Transformations

`Together` and `Cancel` transform the same bounded exact rational-expression
subset used by `Numerator` and `Denominator`.

`Together[expr]` combines supported sums and products of exact rational
expressions into one fraction. It clears exact rational coefficients and uses
deterministic denominator ordering through the exact polynomial renderer, but
it does not cancel common polynomial factors.

Examples:

- `Together[1/x + 1/y]` -> `(x + y) / (x * y)`
- `Together[1/2 + 1/x]` -> `(x + 2) / (2 * x)`
- `Together[x/(x + 1) + 1/(x + 1)]` -> `(x + 1) / (x + 1)`

`Cancel[expr]` cancels common numerator and denominator factors only when the
existing exact polynomial GCD and division contracts support that factor
discovery. Supported cases include univariate exact polynomial cancellation
and monomial-bounded multivariate cancellation where at least one side's
common-factor query is monomial-bounded.

Examples:

- `Cancel[(x^2 - 1)/(x - 1)]` -> `x + 1`
- `Cancel[(1/2*x)/(1/4)]` -> `2 * x`
- `Cancel[(x*y)/x]` -> `y`

Domain boundary:

- cancellation is valid on the original expression's nonzero-denominator
  domain;
- rational-expression transformations now carry internal excluded-denominator
  metadata for supported nonconstant denominator factors, so future
  condition-aware consumers can distinguish the simplified expression from the
  original domain;
- this metadata is not yet rendered as a public `ConditionalExpression` or
  exposed through a user-facing equivalence function;
- cancellation across unsupported symbolic or general multivariate
  denominators is rejected rather than silently erasing possible
  singularities.

Unsupported cases:

- decimal coefficients;
- symbolic coefficients outside the selected exact polynomial variables;
- unsupported powers or expression heads;
- denominator zero;
- general multivariate cancellation such as `Cancel[(x*y + x)/(x + 1)]`;
- `Apart` and partial-fraction decomposition.

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

Supported univariate integer and rational `Factor` inputs use the exact
polynomial path. The legacy `double` polynomial layer remains present for
inexact inputs and transitional internals. Large exact intermediates are still
bounded by checked `int64_t` coefficient storage; overflow is reported rather
than wrapped.

## Future Work

Not part of the current supported subset:

- bounded `Apart` after factorization and partial-fraction preconditions are
  specified
- general multivariate polynomial GCD and configurable or multi-divisor division
- exact multivariate factorization beyond current content extraction
- broader factorization algorithms
- arbitrary-precision exact algebra
- symbolic, approximate, sparse, or arbitrary-rank matrix algebra

## Planned Rational-Expression Follow-Up

The next rational-expression tranche should consume the internal
domain-restriction metadata from condition-aware operations such as a future
equivalence or validation contract before broadening cancellation. `Apart`
remains second in this area because it requires separate partial-fraction
preconditions and variable-selector behavior.
