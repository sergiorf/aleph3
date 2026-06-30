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

Ownership in this slice is intentionally narrow:

- the algebra layer owns exact integer/rational coefficient preservation for
  `Expand`, `Collect`, supported univariate `GCD`, and supported univariate
  `PolynomialQuotient`
- the current `Polynomial` type with `double` coefficients remains in place
  for the existing factorization path
- multivariate exact `GCD`, division, and broader factorization do not become
  supported merely because exact coefficients are now public

Practical implication:

- exact multivariate coefficient preservation is now an explicit pack-facing
  contract for safe helper paths
- exact factorization remains out of scope until the broader coefficient-ring
  and algorithm story is stronger

## Acceptance Criteria

This spec is sufficient when:

- exact numeric ownership is explicit
- algebra growth no longer depends on unclear floating-point foundations
