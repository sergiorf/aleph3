# Kernel Exact Algebra Spec

## Status

Draft implementation spec referenced by the
[Aleph3 Unified Plan](aleph3_unified_plan.md).

## Purpose

This document will define the exact arithmetic and algebra-facing foundations
required for stronger symbolic math.

It should specify:

- exact scalar abstractions
- polynomial coefficient strategy
- ownership boundary between kernel exact math and algebra packs
- migration away from floating-point-centered algebra internals

## Scope

This spec should cover:

- integer and rational foundation choices
- exact complex support expectations
- coefficient-ring abstractions
- algebra helper interfaces needed by the kernel and packs
- overflow and large-number strategy

## Required Sections

1. exact scalar model
2. coefficient abstractions
3. polynomial/algebra ownership boundary
4. interaction with evaluator simplification
5. migration strategy from current algebra internals
6. testing invariants

## Initial Design Questions

- whether big-integer support should be introduced immediately or staged
- which exact abstractions belong in the kernel versus algebra pack layer
- how exact coefficients interact with current polynomial APIs

## Current Decision Relevant To Rewrite Migration

Exact algebra should not be used as the gating dependency for the first
symbolic coefficient contract.

Near-term implication:

- like-term collection can grow through a smaller symbolic coefficient contract
  before the full exact-polynomial/algebra foundation is complete

But the following should still wait for stronger exact algebra:

- broad multivariate polynomial reasoning
- coefficient-ring-general algorithms
- exponent and monomial laws that depend on richer algebra metadata

That means exact algebra remains the owner of:

- long-term coefficient-ring abstractions
- exact polynomial semantics
- algebra-heavy transformations whose safety depends on those abstractions

While the near-term symbolic coefficient contract only needs enough exactness
to preserve today’s supported numeric and rational coefficient behavior.

## Current Exact Polynomial Boundary

The first explicit exact-polynomial layer now lives in the algebra module and
is public to pack-owned helpers through:

- `ExactCoefficient`
- `ExactPolynomial`
- exact conversion helpers between `Expr` and exact polynomial form
- exact low-level `expand`, `collect`, `gcd`, and `divide` overloads
- exact univariate coefficient extraction for `Coefficient` and
  `CoefficientList`
- exact rational-expression part extraction for `Numerator` and `Denominator`
  over supported exact polynomial numerators and denominators
- exact rational-expression transformation for `Together` and `Cancel` over
  the same bounded rational-expression subset, with `Cancel` limited to the
  existing exact polynomial GCD and division contracts
- exact single-divisor multivariate division using explicit variable
  precedence and fixed graded-lexicographic leading terms
- exact monomial-bounded multivariate GCD using explicit selectors

Current ownership is intentionally narrow:

- the algebra layer owns exact integer/rational coefficient preservation for
  `Expand`, `Collect`, `Coefficient`, `CoefficientList`, supported univariate
  `GCD`, supported univariate `PolynomialQuotient`, `Numerator`, and
  `Denominator`, including explicitly selected multivariate inputs where that
  function supports them
- the current `Polynomial` type with `double` coefficients remains in place
  for inexact inputs and transitional internals, while supported univariate
  integer and rational `Factor` inputs now use the exact polynomial path
- general multivariate exact `GCD` and broader factorization remain unsupported
- multivariate division is limited to one divisor, explicit variable
  precedence, and the fixed graded-lexicographic order

Practical implication:

- exact multivariate coefficient preservation is now an explicit pack-facing
  contract for safe helper paths
- rational-expression transformation is pack-owned and does not introduce
  first-class domain-restriction carriers yet
- broad exact factorization remains out of scope until the broader
  coefficient-ring and algorithm story is stronger; the current exact
  factorization support is limited to the documented univariate rational-root
  subset
- exact coefficient operations detect `int64_t` overflow and fail explicitly;
  arbitrary precision remains outside this contract

## Current Algebra Implementation Ownership

The algebra pack implementation keeps the public evaluator-facing entrypoints
separate from polynomial representation and algorithm helpers:

- `PolyUtils` is the adapter layer used by the registered algebra pack
  functions. It infers selectors, dispatches between exact and transitional
  polynomial paths, and maps implementation exceptions into kernel diagnostics.
- `PolynomialOps` owns the transitional `Polynomial` path with `double`
  coefficients: expression conversion, variable inference, low-level
  `expand`/`collect`/`gcd`/`divide`, and the approximate univariate
  factorization fallback.
- `ExactPolynomialConversion` owns conversion between `Expr` and
  `ExactPolynomial`.
- `ExactPolynomialOps` owns low-level operations over `ExactPolynomial`,
  including exact normalization helpers, coefficient content helpers, and exact
  `expand`/`collect`/`gcd`/`divide` overloads.
- `ExactFactorization` owns exact univariate factorization over the documented
  rational-root subset.
- `ExactRationalExpression` owns exact rational-expression normalization,
  `Together` composition, numerator/denominator extraction, and `Cancel`
  reduction over the bounded exact polynomial subset.

`PolyUtils.hpp` remains a compatibility umbrella for algebra helper tests and
pack consumers, but new implementation code should include the narrower owner
header directly.

## Acceptance Criteria

This spec is sufficient when:

- exact numeric ownership is explicit
- algebra growth no longer depends on unclear floating-point foundations

## Monomial-Bounded Multivariate GCD Contract

- both operands must be exact polynomials and callers must provide a non-empty
  ordered selector list
- when both operands are nonzero, at least one must contain exactly one
  monomial term
- the result is monic and uses the minimum exponent of each selected variable
  shared by both operands
- `GCD[x*y + x, x, {x, y}]` returns `x`
- `GCD[x^2*y, x*y^2, {x, y}]` returns `x*y`
- zero with a nonzero supported operand returns its monic form; two zero
  operands remain invalid
- unit input returns `1`; exact coefficient overflow remains explicit

For a selected variable, the polynomial valuation is the minimum exponent of
that variable among all nonzero terms. The result uses the minimum valuation
from the two operands for each selected variable and has coefficient one.

Two multi-term nonzero operands, inferred multivariate selectors, inexact
coefficients, non-polynomial inputs, and non-monomial common-factor discovery
remain unsupported. Two zero operands are a domain violation.
