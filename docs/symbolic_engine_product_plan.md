# Symbolic Engine Product Plan

## Purpose

This document defines the Aleph3 symbolic engine as a product asset in its own
right, not merely as background material for the SDK.

The SDK remains a primary embedding surface. The symbolic engine is the broader
mathematical core that can support multiple product families:

- embeddable SDKs
- interactive tooling
- symbolic preprocessing and normalization services
- future higher-level math products built on shared core semantics

This plan focuses on two things:

- what the symbolic engine should become as product-grade core technology
- where current test coverage and capability coverage are still weak

## Product Position

Aleph3 now has two related but distinct product layers:

1. `aleph3_symbolic`
   The broader symbolic engine stack:
   - expression model
   - parser
   - symbolic and mixed evaluation
   - transforms
   - polynomial utilities

2. `aleph3_sdk`
   The controlled host-facing embedding layer:
   - structured diagnostics
   - schema/policy validation
   - bounded runtime behavior
   - stable host API

The symbolic engine should not be treated as deprecated. It should be treated
as a first-class core that needs its own contracts, tests, and roadmap.

## Current Symbolic Surface

Current symbolic-core code is concentrated in:

- [include/expr/Expr.hpp](/home/sergio/dev/aleph3/include/expr/Expr.hpp)
- [include/parser/Parser.hpp](/home/sergio/dev/aleph3/include/parser/Parser.hpp)
- [include/evaluator/Evaluator.hpp](/home/sergio/dev/aleph3/include/evaluator/Evaluator.hpp)
- [src/evaluator/Evaluator.cpp](/home/sergio/dev/aleph3/src/evaluator/Evaluator.cpp)
- [src/evaluator/SimplificationRules.cpp](/home/sergio/dev/aleph3/src/evaluator/SimplificationRules.cpp)
- [include/transforms/Transforms.hpp](/home/sergio/dev/aleph3/include/transforms/Transforms.hpp)
- [src/transforms/Transforms.cpp](/home/sergio/dev/aleph3/src/transforms/Transforms.cpp)
- [include/algebra/Polynomial.hpp](/home/sergio/dev/aleph3/include/algebra/Polynomial.hpp)
- [src/algebra/Polynomial.cpp](/home/sergio/dev/aleph3/src/algebra/Polynomial.cpp)
- [src/algebra/PolyUtils.cpp](/home/sergio/dev/aleph3/src/algebra/PolyUtils.cpp)

Current implemented areas include:

- symbolic expression trees with numbers, rationals, complex values, booleans,
  strings, lists, rules, assignments, and function definitions
- broad parser support including implicit multiplication, definitions,
  assignments, logic, lists, and rules
- symbolic and mixed numeric evaluation
- user-defined functions and delayed definitions
- simplification and expansion helpers
- exact rational arithmetic
- basic complex arithmetic
- polynomial arithmetic and some polynomial functions such as `Expand`,
  `Factor`, `Collect`, `GCD`, and `PolynomialQuotient`

## What Exists Today

Current symbolic-oriented tests are spread across:

- parser: [tests/parser](/home/sergio/dev/aleph3/tests/parser)
- evaluator: [tests/evaluator](/home/sergio/dev/aleph3/tests/evaluator)
- transforms: [tests/SimplifyTests.cpp](/home/sergio/dev/aleph3/tests/SimplifyTests.cpp),
  [tests/ExpandTests.cpp](/home/sergio/dev/aleph3/tests/ExpandTests.cpp)
- algebra: [tests/algebra](/home/sergio/dev/aleph3/tests/algebra)

That is real coverage, but it is not yet a product test strategy.

The suite currently leans heavily toward:

- feature spot checks
- happy-path examples
- parser shape tests
- direct output-string expectations

The suite is much weaker on:

- semantic contracts
- canonical forms
- negative/error behavior
- stability of symbolic normalization
- cross-feature interactions
- boundary cases for algebraic operations

## Test Gap Analysis

## Summary

The main problem is not absence of all tests. The problem is mismatch between
the current tests and what a product-grade symbolic core needs.

Current status by area:

| Area | Current State | Main Gap |
| --- | --- | --- |
| Expression/value model | indirectly covered | lacks invariant-focused tests |
| Parser breadth | broad spot coverage | weak negative/error and ambiguity coverage |
| Symbolic evaluation | meaningful but uneven | weak contract definition for fallback and normalization |
| Simplification | minimal direct coverage | not close to a stable algebraic contract |
| Built-in math | many examples | weak domain/error/canonical symbolic behavior coverage |
| User-defined functions | some good coverage | weak scoping/recursion/error/override edge coverage |
| Rational arithmetic | relatively strong | weak integration with broader symbolic simplification |
| Complex arithmetic | basic | very shallow |
| Polynomial core | real but narrow | multivariate and canonical-form confidence is weak |
| String/list/rule logic | some coverage exists | weak product framing and edge-case coverage |
| Differentiation | absent | capability missing |
| Integration | absent | capability missing |

## Detailed Gaps

### 1. Expression And Value Invariants

What exists:

- parser and evaluator tests exercise many `Expr` variants indirectly

What is missing:

- direct tests for canonical construction invariants
- direct tests for stable pretty-printing vs full-form structure
- direct tests for null/shared ownership assumptions
- direct tests for special values such as `Infinity` and `Indeterminate` across
  operators, not just isolated examples

Risk:

- the symbolic engine can drift internally while still passing surface examples

### 2. Parser Contract Coverage

What exists:

- strong breadth across [tests/parser/ParserTests.cpp](/home/sergio/dev/aleph3/tests/parser/ParserTests.cpp)
- function syntax coverage in [tests/parser/ParserFunctionsTests.cpp](/home/sergio/dev/aleph3/tests/parser/ParserFunctionsTests.cpp)
- rational, symbol, identifier, match, and complex parsing coverage

What is missing:

- malformed input coverage as a first-class contract
- precedence ambiguity coverage across logic, comparison, rules, and
  implicit multiplication
- round-trip stability expectations
- parser behavior around unsupported or partial constructs with explicit product
  decisions

Risk:

- the parser is broad, but precedence and unsupported-form behavior are still
  not locked down as product contracts

### 3. Symbolic Evaluation Contract

What exists:

- broad direct examples in [tests/evaluator/EvaluatorTests.cpp](/home/sergio/dev/aleph3/tests/evaluator/EvaluatorTests.cpp)
- function and built-in coverage in
  [tests/evaluator/EvaluatorFunctionsTests.cpp](/home/sergio/dev/aleph3/tests/evaluator/EvaluatorFunctionsTests.cpp)

What is missing:

- a crisp contract for when evaluation returns a value vs an unevaluated
  symbolic form
- strong tests for mixed symbolic/numeric comparisons
- error-path expectations for invalid symbolic calls
- determinism requirements for symbolic fallback
- consistent tests for boolean handling and symbolic truthiness

Risk:

- current behavior can be surprising and still appear to “work”

### 4. Simplification Contract

What exists:

- a few direct simplification tests in
  [tests/SimplifyTests.cpp](/home/sergio/dev/aleph3/tests/SimplifyTests.cpp)
- many simplification side effects are tested indirectly through evaluator tests

What is missing:

- canonical simplification rules documented as product behavior
- conflict tests between arithmetic simplification and symbolic preservation
- idempotence tests: `simplify(simplify(x)) == simplify(x)`
- ordering/canonicalization tests for commutative expressions
- tests for simplification of rationals, powers, nested sums/products,
  booleans, and mixed symbolic expressions

Risk:

- simplification is currently a cluster of useful behaviors, not a stable
  algebraic contract

### 5. Built-In Math Semantics

What exists:

- many built-in examples in
  [tests/evaluator/EvaluatorFunctionsTests.cpp](/home/sergio/dev/aleph3/tests/evaluator/EvaluatorFunctionsTests.cpp)
- unary numeric and symbolic cases for trig/log/abs/sqrt/floor/round classes

What is missing:

- systematic domain/error coverage
- canonical symbolic fallback coverage
- exact-value coverage for known constants beyond a few examples
- consistency checks between aliases and case variants
- tests for interaction with rationals and complex values

Risk:

- built-ins appear broad, but semantics are uneven and not frozen

### 6. User-Defined Functions

What exists:

- immediate vs delayed definitions
- recursion
- defaults
- overwrite behavior

What is missing:

- failure cases for bad arity and missing parameters
- lexical vs dynamic scoping expectations
- variable capture tests
- recursive budget/termination strategy
- interaction with symbolic arguments that should remain unevaluated

Risk:

- user-defined functions are product-relevant, but not yet controlled enough

### 7. Rational Arithmetic

What exists:

- one of the stronger areas in
  [tests/evaluator/EvaluatorRationalTests.cpp](/home/sergio/dev/aleph3/tests/evaluator/EvaluatorRationalTests.cpp)

What is missing:

- rational interaction with simplification and transforms
- rational interaction with user-defined functions
- rational behavior inside polynomial conversion/manipulation
- exact-vs-inexact promotion rules as an explicit contract

Risk:

- exact arithmetic is a differentiator, but its boundaries are not well defined

### 8. Complex Arithmetic

What exists:

- basic construction and arithmetic in
  [tests/evaluator/EvaluatorComplexTests.cpp](/home/sergio/dev/aleph3/tests/evaluator/EvaluatorComplexTests.cpp)

What is missing:

- division, powers, and transcendental functions on complex values
- comparison and non-orderability expectations
- simplification and display behavior
- interaction with exact arithmetic and symbolic arguments

Risk:

- current complex support is too shallow to present as reliable broad support

### 9. Polynomial And Algebra Core

What exists:

- univariate division/GCD and general arithmetic coverage in
  [tests/algebra/PolynomialUnivariateTests.cpp](/home/sergio/dev/aleph3/tests/algebra/PolynomialUnivariateTests.cpp),
  [tests/algebra/PolynomialAdditionTests.cpp](/home/sergio/dev/aleph3/tests/algebra/PolynomialAdditionTests.cpp),
  and [tests/algebra/PolynomialMultiplicationTests.cpp](/home/sergio/dev/aleph3/tests/algebra/PolynomialMultiplicationTests.cpp)
- polynomial entrypoints are wired through the evaluator in
  [src/evaluator/Evaluator.cpp](/home/sergio/dev/aleph3/src/evaluator/Evaluator.cpp)

What is missing:

- canonical-form contract for multivariate polynomials
- product tests for `Factor`, `Collect`, `GCD`, and `PolynomialQuotient`
- explicit conversion tests between `Expr` and polynomial form
- hard edge cases for sparse/high-degree/multivariate operations
- reliable non-happy-path behavior and error diagnostics

Risk:

- the polynomial layer is useful, but not yet strong enough to build multiple
  product families on top of without more contract work

### 10. Missing Foundational Capabilities

Currently absent or not meaningfully present:

- symbolic differentiation
- symbolic integration
- equation solving strategy
- substitution/rewrite contract beyond ad hoc rules
- normalization/canonical-form policy across symbolic expressions

These are not just test gaps. They are capability gaps.

## Capability Priorities

If the symbolic engine is part of the product, the next capabilities should be
prioritized by leverage, not by mathematical glamour.

## P1. Simplification Contract

Why first:

- every product family built on symbolic forms depends on predictable
  simplification and normalization

Needed outcomes:

- define what `simplify` guarantees
- define what it explicitly does not guarantee
- add canonical/idempotence/regression tests

Current near-term `simplify` contract for the symbolic engine should include:

- additive neutral elimination: `x + 0 -> x`
- multiplicative neutral elimination: `x * 1 -> x`
- multiplicative annihilation: `x * 0 -> 0`
- basic power identities: `x^0 -> 1`, `x^1 -> x`, `1^x -> 1`
- numeric power evaluation when both operands are numeric
- numeric relational simplification for `Equal`, `NotEqual`, `Less`,
  `Greater`, `LessEqual`, and `GreaterEqual`
- like-term combination for simple linear symbolic forms such as
  `2*x + 3*x -> 5*x`
- idempotence for supported simplifications:
  `simplify(simplify(expr)) == simplify(expr)`

It should explicitly not yet promise:

- broad canonical ordering for all symbolic expressions
- deep algebraic factorization through `simplify`
- full polynomial normalization through `simplify` alone
- calculus-aware rewrites

## P2. Polynomial Contract

Why next:

- polynomial manipulation is already in the codebase and commercially useful
- it is also foundational for later symbolic features

Needed outcomes:

- define supported polynomial domains and limitations
- harden conversion, operations, and canonical forms
- expand tests around `Factor`, `Collect`, `GCD`, `PolynomialQuotient`

## P3. Exact Arithmetic Contract

Why next:

- exact arithmetic is one of the most valuable differentiators in this codebase

Needed outcomes:

- document and test rational/integer/float/complex promotion rules
- make mixed exact/inexact behavior predictable

## P4. User-Defined Function Semantics

Why next:

- they are powerful for product workflows and formula authoring
- they are also a source of hidden semantic instability

Needed outcomes:

- define delayed vs immediate semantics more formally
- define bad-arity and recursion behavior
- add scoping and symbolic-argument tests

## P5. Symbolic Differentiation

Why after P1-P4:

- differentiation is commercially important
- but adding it before simplification/polynomial/exact semantics are solid will
  create fragile downstream behavior

Initial supported subset should be narrow:

- constants and symbols
- `+`, `-`, `*`, `/`, `^` with documented limits
- elementary unary functions already present in the engine
- explicit unsupported cases returning symbolic forms or structured failures

## P6. Symbolic Integration

Why later:

- it is harder to make reliable than differentiation
- it should not precede a trustworthy derivative and simplification layer

## Proposed Test Strategy

The symbolic engine needs its own test matrix distinct from the SDK one.

## Layered Test Buckets

1. Structure tests
   - `Expr` construction
   - normalization invariants
   - pretty-print/full-form stability

2. Parser contract tests
   - valid syntax
   - malformed syntax
   - precedence/ambiguity

3. Evaluation contract tests
   - numeric
   - symbolic
   - mixed numeric/symbolic
   - user-defined functions

4. Transform tests
   - `simplify`
   - `expand`
   - future symbolic differentiation

5. Algebra tests
   - polynomial operations
   - conversion boundaries
   - factor/collect/gcd/quotient behavior

6. Regression tests
   - every bug fix gets a narrow permanent test

## Suggested New Test Files

- `tests/evaluator/EvaluatorSymbolicContractTests.cpp`
- `tests/evaluator/EvaluatorBuiltinDomainTests.cpp`
- `tests/transforms/SimplifyContractTests.cpp`
- `tests/transforms/NormalizationTests.cpp`
- `tests/algebra/PolynomialFunctionTests.cpp`
- `tests/algebra/PolynomialExprConversionTests.cpp`
- `tests/symbolic/DifferentiationTests.cpp`

## Near-Term Roadmap

## M1. Symbolic Test Audit Completion

Deliverables:

- this plan
- classification of existing symbolic tests
- first-pass symbolic contract categories

## M2. Simplification Hardening

Deliverables:

- documented `simplify` contract
- idempotence and canonicalization tests
- bug fixes needed to satisfy the contract

## M3. Polynomial Hardening

Deliverables:

- polynomial function contract doc
- expanded algebra tests
- multivariate and conversion-gap fixes

## M4. Exact Arithmetic And Mixed Semantics

Deliverables:

- exact/inexact promotion rules
- rational + symbolic + complex interaction tests

## M5. Differentiation Subset V1

Deliverables:

- initial differentiation implementation
- documented supported subset
- regression suite

## Recommendation

The symbolic engine should now be treated as a product core with two parallel
workstreams:

- SDK hardening for embedding
- symbolic-core hardening for reusable math capabilities

The next implementation work should start with `simplify` and polynomial
contracts, not integration.

That ordering gives the highest chance of turning the current symbolic engine
into reusable core technology for multiple future product families.
