# Math Core Audit

This document evaluates the current trusted-subset math core from the
perspective of shipping a monetizable embedded SDK quickly.

The question is not whether the engine can eventually become a broader CAS.
The question is whether the current numeric core is trustworthy enough to sell
for real host-application formula work.

## Executive Summary

The SDK math core is already usable, but it is still narrow and somewhat
fragile.

What is good enough now:

- deterministic numeric arithmetic on `double`
- basic comparisons
- `If`
- SDK numeric built-ins: `Abs`, `Min`, `Max`, `Clamp`, `Floor`, `Ceil`/`Ceiling`, `Round`, `Sqrt`
- schema-driven validation
- constant-condition pruning and constant runtime-trap detection
- schema-valued constants participating in validation and runtime
- non-finite numeric results rejected with structured runtime errors
- explicit power-domain failures for `0 ^ 0` and negative-base non-integer powers
- signed-zero outputs normalized to positive zero, with equality and ordering
  treating `-0` and `0` identically
- mixed-type equality rejected instead of silently returning `False`
- all numeric comparisons reject `NaN` and infinities instead of exposing raw
  floating-point comparison behavior

What is not strong enough yet for a commercial "solid math core" claim:

- the numeric contract is still incomplete around broader floating-point
  behavior beyond the current signed-zero normalization and non-finite-input
  rejection
- comparison semantics are intentionally narrow and stricter than the syntax
  surface alone may suggest
- built-in math coverage is still selective, but now credible for a constrained SDK v1
- there is no canonical constant-folding pass, only targeted validation-time
  reasoning
- the algebra/polynomial layer is not part of the trusted SDK path

Bottom line:

- the current SDK core is good enough for a constrained v1
- it is not yet good enough to market as a strong general-purpose math engine
- the fastest path to monetization is to harden the numeric core first, then
  expand built-ins, and only then revisit symbolic algebra layers

## Current Trusted Math Core

## Runtime

Current runtime behavior in [src/runtime/Evaluator.cpp](/home/sergio/dev/aleph3/src/runtime/Evaluator.cpp):

- arithmetic operators: `+`, `-`, `*`, `/`, `^`
- comparisons: `==`, `!=`, `<`, `<=`, `>`, `>=`
- conditionals: `If`
- optional numeric built-ins: `Abs`, `Min`, `Max`, `Clamp`, `Floor`, `Ceil`/`Ceiling`, `Round`, `Sqrt`
- engine host functions

Current numeric model:

- all numeric values are `double`
- arithmetic requires both operands to already be numeric
- ordered comparisons require both operands to be numeric
- equality is structural across numbers, booleans, strings, and null
- numeric equality and ordering both reject non-finite numeric inputs

## Validation

Current validator behavior in [src/semantics/Validator.cpp](/home/sergio/dev/aleph3/src/semantics/Validator.cpp):

- rejects obvious type mismatches early
- rejects constant zero denominators early
- prunes unreachable `If` branches when the condition is a trusted constant
- treats schema-valued constants as compile-time facts
- propagates host-function return types into surrounding expressions

This is a strong start because it shifts some failures from runtime to save
time or compile time.

## Tests

Current coverage is meaningful:

- [tests/runtime/EvaluatorTests.cpp](/home/sergio/dev/aleph3/tests/runtime/EvaluatorTests.cpp)
- [tests/semantics/ValidatorTests.cpp](/home/sergio/dev/aleph3/tests/semantics/ValidatorTests.cpp)
- [tests/sdk/EngineTests.cpp](/home/sergio/dev/aleph3/tests/sdk/EngineTests.cpp)

That said, the tests currently prove a narrow happy path and a few negative
cases. They do not yet define a full numeric contract.

## Findings

### 1. The numeric core is real, but still underspecified

The runtime uses `double`, rejects non-finite numeric inputs/results for
arithmetic operations, canonicalizes zero-valued numeric results to positive
zero, and delegates exponentiation to `std::pow` behind explicit domain checks.
That is a reasonable first commercial posture, but the contract is still
unclear for broader floating-point cases.

Without documenting and testing these, users cannot rely on cross-version
stability for nontrivial formulas.

### 2. Comparison semantics are stricter than many users will expect

Ordered comparisons are numeric-only.
Equality is only meaningful when both sides reduce to the same concrete type.
If a numeric comparison sees `NaN` or an infinity, it fails with a structured
runtime error instead of returning a platform-level floating-point result.
Mixed-type equality is treated as an error rather than a silent `False`.

That is a defendable v1 choice, but it must be explicit in the product
positioning because many formula users expect broader coercion or richer
comparable domains.

### 3. Built-in math coverage is too small to feel like a strong SDK

The built-in tier is now substantially better for common host-app use cases,
but it is still intentionally selective rather than broad CAS-style math.

Still-likely follow-up asks are:

- `Log`, `Exp`
- trigonometric functions

The right move is not to add everything. The right move is to add a deliberate
numeric built-in tier with contracts and tests.

### 4. Constant reasoning exists, but there is no formal constant-folding layer

The validator already performs targeted constant reasoning for:

- `If` conditions
- equality/comparison checks
- zero-denominator detection

That is useful, but it is scattered helper logic rather than a dedicated
constant-folding or normalization layer. As features grow, this will become
harder to reason about and maintain.

### 5. The polynomial/algebra core is not trustworthy enough for the SDK path

There is legacy algebra/polynomial code in `include/algebra/` and `src/algebra/`,
but it is explicitly outside the trusted SDK path today.

That is the correct current decision.

For monetization, the worst move would be to advertise polynomial support
before the numeric SDK core is fully nailed down and before the algebra
semantics are re-verified behind SDK contracts.

## Monetization-Oriented Priorities

If the goal is to monetize as soon as possible, the order should be:

1. define and freeze the numeric contract
2. expand the small numeric built-in set
3. improve deterministic diagnostics and packaging
4. only then explore a trusted algebra/polynomial layer

That order maximizes the chance of landing practical embedding use cases
quickly without promising a broad CAS too early.

## Recommended Near-Term Work

### M1. Numeric Contract Hardening

Add explicit tests and docs for:

- remaining power edge cases beyond the current explicit domain failures
- division sign/zero behavior
- constant evaluation behavior
- interaction between schema constants and runtime bindings

Success condition:

- the SDK can publish a crisp "numeric semantics v1" statement

### M2. Numeric Built-In Tier 1

Implemented:

- `Clamp`
- `Floor`
- `Ceil`
- `Round`
- `Sqrt`

This tier also keeps:

- `Abs`
- `Min`
- `Max`
- `Ceiling` as a compatibility alias for `Ceil`

Remaining success condition:

- keep the built-in set credible for line-of-business formula embedding without
  letting it turn into an unbounded math-surface expansion

### M3. Constant Folding Boundary

Introduce a small internal constant-folding pass or equivalent normalized
evaluation helper for trusted numeric expressions.

Success condition:

- constant reasoning stops being scattered ad hoc logic

### M4. Algebra/Polynomial Audit

Only after M1-M3, audit whether polynomial support should be:

- promoted into the SDK later
- isolated as a separate layer
- or left outside the commercial v1 entirely

## Recommendation

For the next implementation step, do not jump into derivatives or integrals.

The best next move is:

- harden the numeric contract around the existing arithmetic/comparison core
- keep the numeric built-in tier deliberate and well-tested
- keep algebra/polynomials as a separate evaluation track until the core is
  strong enough to support them responsibly
