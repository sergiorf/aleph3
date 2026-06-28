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
- `PolynomialQuotient[a, b]`
- `PolynomialQuotient[a, b, var]`

These functions operate on polynomial expressions only. Unsupported forms fail
explicitly rather than silently approximating or partially rewriting.

## Canonical Output Contract

Supported algebra outputs follow these rules:

- expanded and collected outputs use the engine's canonical commutative order
- monomials are ordered by descending total degree, then lexicographically
- collected and expanded sums present algebraic terms ahead of opaque calls
- factoring preserves a deterministic factor order for extracted linear factors
- renormalizing or simplifying a supported algebra result should keep the same
  mathematical structure

Examples:

- `Expand[(y + x) * (x + z)]` -> `x^2 + x * y + x * z + y * z`
- `Collect[y*x + x^2 + z*x, x]` -> `x^2 + x * y + x * z`
- `Factor[x^5 - x^3]` -> `x^3 * (x - 1) * (x + 1)`

## Variable Policy

Variable selectors follow this contract:

- a selector must be a symbol or a non-empty list of symbols
- duplicate symbols in a selector list are ignored after the first occurrence
- `Collect` accepts a selector that names variables not present in the input
- `GCD` and `PolynomialQuotient` infer variables from both operands when no
  explicit selector is provided
- multivariate `GCD` and `PolynomialQuotient` remain unsupported for now

Examples of explicit failures:

- `Collect[x^2 + 1, 3]` -> invalid selector
- `Collect[x^2 + y, {}]` -> empty selector list
- `GCD[x^2 - 1, y - 1]` -> unsupported multivariate inference

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
- `Expand`, `Collect`, supported univariate `GCD`, and supported univariate
  `PolynomialQuotient` preserve exact rational coefficients
- inexact `Number` inputs stay on the existing floating-point path
- `Factor` still rejects exact rational coefficients explicitly instead of
  demoting them to floating-point or pretending to preserve exactness

Examples:

- `1/2 + 1/3` -> `5/6`
- `1/2 + 2` -> `5/2`
- `1/2 + 0.5` -> inexact `Number`
- `Expand[(1/2) * (x + 1)]` -> `1/2 * x + 1/2`
- `PolynomialQuotient[x^2 - 1/4, x - 1/2, x]` -> `{x + 1/2, 0}`
- `Factor[(1/2) * x^2 + x]` -> unsupported exact-rational factor input

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
- rational-root cases that reduce to integer-coefficient linear factors

`Factor` does not yet support:

- exact rational coefficients
- general multivariate factorization beyond content extraction
- higher-degree irreducible decomposition beyond the supported rational-root
  path
- arbitrary-precision exact arithmetic

The current coefficient storage uses `double` in the polynomial layer and
`int64_t` for exact rationals. Large intermediates and overflow risk remain a
known limitation.

## Future Work

Not part of the current supported subset:

- multivariate polynomial GCD and division
- symbolic differentiation
- broader factorization algorithms
- arbitrary-precision exact algebra
