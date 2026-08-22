# Kernel Exact Algebra Spec

## Status

Current implementation contract referenced by the
[Aleph3 Unified Plan](aleph3_unified_plan.md).

## Purpose

This document defines the current exact arithmetic and algebra-facing
foundations required for stronger symbolic math. It is intentionally narrower
than arbitrary-precision or general coefficient-ring algebra.

The current exact algebra layer provides:

- checked integer and rational coefficient storage;
- exact polynomial conversion and helper operations for the algebra pack;
- explicit overflow and unsupported-case behavior;
- a migration boundary away from floating-point-centered polynomial internals.

## Scope

This spec covers:

- the current integer and rational foundation choices;
- coefficient abstractions used by polynomial helpers;
- the polynomial/algebra ownership boundary;
- evaluator and simplification interaction;
- migration strategy from current transitional internals;
- testing invariants.

Exact complex coefficients, symbolic coefficients, broad coefficient-ring
abstractions, arbitrary precision, algebraic-number coefficients, and
approximate polynomial algorithms are outside this contract.

## Exact Scalar Model

`ExactCoefficient` is the current algebra coefficient value. It stores a
normalized rational number as checked `int64_t` numerator and denominator.

Invariants:

- denominator is positive after normalization;
- zero normalizes to `0/1`;
- equal rational values compare structurally equal after normalization;
- addition, subtraction, multiplication, and division preserve exactness;
- denominator zero is invalid;
- arithmetic overflow throws an exact-overflow condition before wraparound.

The current model deliberately does not allocate arbitrary-precision integers.
When an intermediate numerator, denominator, scale factor, content value, or
least common multiple cannot fit in `int64_t`, the operation fails explicitly.
No exact algebra operation may silently demote to `Number` or a `double`
polynomial path to avoid overflow.

## Coefficient Abstractions

`ExactCoefficient` is the only current exact polynomial coefficient
abstraction. It is sufficient for the supported integer/rational polynomial
subset but is not a general coefficient-ring interface.

Near-term algorithms may rely on:

- exact zero and one checks;
- rational normalization;
- checked arithmetic;
- exact division by a nonzero coefficient;
- integer-content extraction after denominators are cleared.

Algorithms must not assume:

- arbitrary-precision growth;
- symbolic coefficients;
- algebraic-number coefficients;
- approximate fallback;
- field operations outside checked rationals.

## Polynomial Representation

`ExactPolynomial` maps monomials to `ExactCoefficient` values. A monomial is
the shared `Monomial` map from variable name to non-negative integer exponent.

Invariants:

- zero terms are removed during normalization;
- the zero polynomial has one constant zero term;
- monomial exponents are non-negative;
- exact polynomial operations preserve exact coefficients or fail explicitly;
- leading terms for current multivariate division use fixed graded
  lexicographic order with caller-provided variable precedence.

The exact polynomial layer is the supported foundation for exact
integer/rational algebra helper behavior. The older `Polynomial` type with
`double` coefficients remains a transitional inexact representation and must
not receive new exact-only algorithms.

## Ownership Boundary

Ownership is layered as follows:

- the kernel owns expressions, exact scalar value types, evaluation context,
  diagnostics, budgets, and function registration contracts;
- the `core-algebra` pack owns public algebra functions and maps helper
  failures to public diagnostics;
- `ExactPolynomialConversion` owns conversion between `Expr` and
  `ExactPolynomial`;
- `ExactPolynomialOps` owns low-level exact polynomial operations such as
  exact normalization helpers, `expand`, `collect`, `gcd`, and `divide`;
- `ExactFactorization` owns supported exact univariate rational-root
  factorization;
- `ExactRationalExpression` owns supported exact rational-expression
  extraction, `Together`, and `Cancel`;
- `PolynomialOps` owns the transitional `double` polynomial path for inexact
  inputs and legacy internals.

Public consumers must enter algebra behavior through the registered pack
functions. They must not create CLI-, session-, SDK-, notebook-, or web-only
polynomial semantics.

## Evaluator And Simplification Interaction

Exact algebra helpers consume evaluated `Expr` inputs through pack-registered
functions. They may reuse normal expression rendering and simplification after
constructing results, but they do not replace the kernel's general evaluator or
rewrite system.

The narrow kernel-owned symbolic coefficient rewrite contract remains separate
from full exact polynomial algebra. Like-term collection for the documented
single-symbol basis shapes may proceed without requiring this full exact
polynomial layer. Algebra-heavy transformations such as polynomial division,
GCD, rational-expression cancellation, and future solving/equivalence helpers
must use explicit exact-algebra contracts instead of broad rewrite heuristics.

## Dispatch Contract

Pack-facing dispatch follows this rule:

- exact integer/rational polynomial candidates enter the exact polynomial path;
- decimal or other inexact polynomial inputs may use the transitional
  `Polynomial` path only where the supported subset documents that behavior;
- exact-only helpers such as coefficient extraction and rational-expression
  transformations reject inexact inputs explicitly;
- exact multivariate `GCD` and `PolynomialQuotient` require exact polynomial
  coefficients and explicit selector lists;
- exact overflow maps to `runtime.exact_overflow`;
- division by a zero exact polynomial denominator maps to the stable
  division-by-zero diagnostic where it reaches a public runtime boundary.

Unsupported cases must be rejected deterministically. They must not be
partially rewritten through `double` arithmetic.

## Migration Strategy

New exact algebra implementation work should include the narrow owning header
directly and should not add exact-only behavior to `PolynomialOps`.

Acceptable near-term migration steps:

- add tests proving exact integer/rational inputs stay on exact paths;
- move exact helper logic from transitional files to exact owner files when a
  concrete boundary issue is found;
- preserve `Polynomial` for documented inexact behavior until that path is
  either replaced or explicitly retired;
- keep documentation synchronized whenever an exact/inexact boundary changes.

Non-goals for the current migration:

- removing all `double` polynomial code in one large refactor;
- introducing arbitrary precision as a cleanup side effect;
- broadening factorization, GCD, division, or rational-expression cancellation
  without a focused specification.

## Testing Invariants

Tests for exact algebra growth should cover:

- exact coefficient sign and denominator normalization;
- rational arithmetic preservation;
- explicit overflow;
- zero polynomial normalization;
- exact polynomial addition and multiplication;
- division reconstruction,
  `dividend = divisor * quotient + remainder`, for supported cases;
- fixed monomial ordering under explicit variable precedence;
- pack-level exact dispatch for supported integer/rational public helpers;
- explicit rejection of inexact inputs in exact-only paths;
- stable public diagnostics for overflow, invalid forms, unsupported
  constructs, and division by zero.

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
- exact polynomial remainder exposure through the division result
- exact degree and leading-coefficient inspection over the selected
  univariate polynomial subset
- exact univariate coefficient extraction for `Coefficient` and
  `CoefficientList`
- exact rational-expression part extraction for `Numerator` and `Denominator`
  over supported exact polynomial numerators and denominators
- exact rational-expression transformation for `Together` and `Cancel` over
  the same bounded rational-expression subset, with `Cancel` limited to the
  existing exact polynomial GCD and division contracts
- internal domain-restriction metadata for supported rational-expression
  denominators and canceled factors, represented as excluded-zero expression
  conditions and consumed by the first bounded public `Equivalent` contract
- exact single-divisor multivariate division using explicit variable
  precedence and fixed graded-lexicographic leading terms
- exact monomial-bounded multivariate GCD using explicit selectors

Current ownership is intentionally narrow:

- the algebra layer owns exact integer/rational coefficient preservation for
  `Expand`, `Collect`, `Coefficient`, `CoefficientList`, supported univariate
  `GCD`, supported univariate `PolynomialQuotient`,
  `PolynomialRemainder`, `PolynomialDegree`, `LeadingCoefficient`,
  `Numerator`, and `Denominator`, including explicitly selected multivariate
  inputs where that function supports them
- the current `Polynomial` type with `double` coefficients remains in place
  for inexact inputs and transitional internals, while supported univariate
  integer and rational `Factor` inputs now use the exact polynomial path
- general multivariate exact `GCD` and broader factorization remain unsupported
- multivariate division is limited to one divisor, explicit variable
  precedence, and the fixed graded-lexicographic order
- `PolynomialDegree` and `LeadingCoefficient` are exact univariate inspection
  helpers; the zero polynomial is diagnosed rather than represented with a
  negative-infinity degree value

Practical implication:

- exact multivariate coefficient preservation is now an explicit pack-facing
  contract for safe helper paths
- rational-expression transformation is pack-owned and now carries an internal
  domain-restriction set for supported denominator exclusions; this metadata is
  consumed by `Equivalent` to avoid unconditional proofs that would drop
  denominator restrictions, but it is not yet a public condition wrapper
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
  reduction over the bounded exact polynomial subset. It also preserves
  internal excluded-zero denominator metadata through those transformations.

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
