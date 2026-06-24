# Symbolic Evaluator Cleanup Plan

## Purpose

This plan tracks the evaluator and semantic-stability work needed to move
Aleph3 toward a production-grade symbolic engine inspired by Mathematica.

This is not only a refactor plan. It is a semantic hardening plan for the core
engine that future SDK surfaces, hosted services, and niche products will
depend on.

Aleph3 should have a clear layered architecture:

1. `aleph3_symbolic`
   The core symbolic math engine:
   - expression model
   - parser
   - symbolic evaluation
   - simplification and transforms
   - polynomial and algebra utilities

2. `aleph3_sdk`
   The host-facing embedding layer built on top of the symbolic core:
   - validation
   - runtime policy
   - stable host API
   - host-function integration

3. Future products
   Additional commercial surfaces built on the same symbolic semantics:
   - vertical applications
   - hosted APIs
   - interactive tooling
   - future higher-level math products

The main rule for this plan is simple: runtime machinery should follow stable
symbolic semantics, not compensate for unstable semantics.

## Current Focus

This document tracks only the remaining evaluator-semantic work. Completed
refactor history is intentionally omitted so the plan stays operational.

Active:

- Phase 8. Evaluation-control semantics and attributes
- Phase 9. Simplification and rewrite-system contract
- Phase 10. Algebra capability hardening
- Phase 11. Documentation pass

## Core Risks Still Open

The main remaining fragilities are not just code-organization issues.

- symbolic fallback is not yet defined sharply enough
- canonical forms are not yet stable product behavior
- simplification is useful but not governed by a clear contract
- error behavior is still too ad hoc
- algebra capability is too narrow for a production symbolic-core story
- evaluator control semantics are still too limited for a Mathematica-inspired
  engine

## Architecture Target

The evaluator should be an orchestrator, not the place where all semantics
accumulate. Each behavior class should have a clear home.

### Evaluator Core

Owns:

- top-level `evaluate`
- expression-kind dispatch
- symbolic fallback and reconstruction

Files:

- [include/evaluator/Evaluator.hpp](/home/sergio/dev/aleph3/include/evaluator/Evaluator.hpp)
- [src/evaluator/Evaluator.cpp](/home/sergio/dev/aleph3/src/evaluator/Evaluator.cpp)

### Special Forms And Evaluation Control

Owns:

- `If`
- future lazy or non-standard-evaluation constructs
- evaluation-control behavior for special cases

Files:

- [include/evaluator/EvaluatorSpecialForms.hpp](/home/sergio/dev/aleph3/include/evaluator/EvaluatorSpecialForms.hpp)
- [src/evaluator/EvaluatorSpecialForms.cpp](/home/sergio/dev/aleph3/src/evaluator/EvaluatorSpecialForms.cpp)

### Built-In Functions

Owns:

- unary and binary numeric built-ins
- comparisons
- symbolic constants and identity cases
- built-in domain checks

Files:

- [include/evaluator/EvaluatorBuiltins.hpp](/home/sergio/dev/aleph3/include/evaluator/EvaluatorBuiltins.hpp)
- [src/evaluator/EvaluatorBuiltins.cpp](/home/sergio/dev/aleph3/src/evaluator/EvaluatorBuiltins.cpp)

### Algebra Dispatch

Owns:

- polynomial function routing
- symbolic argument policy for algebra functions
- variable selector handling

Files:

- [include/evaluator/EvaluatorAlgebra.hpp](/home/sergio/dev/aleph3/include/evaluator/EvaluatorAlgebra.hpp)
- [src/evaluator/EvaluatorAlgebra.cpp](/home/sergio/dev/aleph3/src/evaluator/EvaluatorAlgebra.cpp)

### User-Defined Function Dispatch

Owns:

- argument binding
- default argument handling
- delayed vs immediate definition semantics
- local call context creation
- recursion boundaries

Files:

- [include/evaluator/EvaluatorFunctions.hpp](/home/sergio/dev/aleph3/include/evaluator/EvaluatorFunctions.hpp)
- [src/evaluator/EvaluatorFunctions.cpp](/home/sergio/dev/aleph3/src/evaluator/EvaluatorFunctions.cpp)

### Evaluator Error Helpers

Owns:

- centralized evaluator error construction
- consistent message wording
- future error categorization

Recommended files:

- `include/evaluator/EvaluatorErrors.hpp`
- `src/evaluator/EvaluatorErrors.cpp`

## Remaining Phases

## Phase 7. Canonical Forms And Normalization Contracts

Status:

- complete

Delivered contract:

- normalize rationals to a sign-on-numerator form with positive denominators
- lower `Minus[a, b]` and `Negate[x]` into the canonical additive/multiplicative
  forms used by the engine
- flatten nested `Plus` and `Times`
- sort commutative `Plus` and `Times` forms deterministically
- place algebraic symbols, powers, and monomial-like products ahead of opaque
  function calls in canonical commutative order
- ignore leading numeric coefficients when ordering symbolic monomials
- render negative additive terms consistently as subtraction at the string
  presentation boundary while preserving canonical internal structure
- keep normalization structural rather than using it as a general arithmetic
  simplifier
- require normalization to be idempotent for the supported subset

Covered by tests:

- canonical ordering for `Plus` and `Times`
- stable handling of negation and rational normalization
- idempotence for repeated normalization
- stable output for semantically equivalent input forms
- recursive normalization through lists, rules, assignments, function
  definitions, and opaque call arguments
- cross-subsystem normalization checks for evaluator, algebra, and UDF flows

## Phase 8. Evaluation-Control Semantics And Attributes

Goal:

- define the evaluation model beyond the current hardcoded special cases

Work:

- decide which Mathematica-inspired evaluation controls are part of the target
  subset
- define attribute-like behavior for supported functions where needed
- document eager, lazy, and symbolic-preserving behavior by category
- keep unsupported evaluation-control features explicit rather than accidental

Initial scope candidates:

- hold-style behavior where required
- orderless/flat semantics where intended
- listability if adopted
- numeric-function classification where intended

Concrete checklist:

- [x] introduce a central function-semantics registry for explicit evaluation
  behavior
- [x] keep `If` as an explicit hold-rest special form with enforced exact arity
- [x] make listability opt-in instead of accidental
- [x] support unary listable mapping while preserving symbolic fallback and
  domain behavior elementwise
- [x] add evaluation-control contract tests for explicit listability semantics
- [x] define `And` and `Or` as supported hold-style evaluator constructs with
  explicit short-circuit semantics
- [x] decide that `Orderless` and `Flat` remain normalization-and-canonicalization
  contracts for the current subset, while their presence is still recorded in
  function semantics
- [x] decide that comparisons remain non-listable in the current supported
  subset and cover that contract with tests
- [x] define numeric-function classification and make builtin numeric dispatch
  depend on that semantics layer
- [ ] move additional evaluator dispatch decisions behind the semantics registry

Success condition:

- evaluation behavior is driven by explicit semantic rules rather than ad hoc
  branching alone
- product docs state which evaluation controls are supported and which are not

## Phase 9. Simplification And Rewrite-System Contract

Goal:

- turn simplification from a cluster of useful transforms into a defined product
  subsystem

Work:

- define what `simplify` is allowed to do
- define boundaries between `evaluate`, `simplify`, and algebra transforms
- document termination and idempotence expectations for supported rewrite paths
- introduce targeted rewrite tests for cross-feature interaction

Required tests:

- idempotence for supported simplifications
- no unintended evaluation through simplification-only paths
- consistent handling of nested sums, products, powers, and rationals
- interaction tests between simplification and symbolic fallback

Success condition:

- simplification behavior is intentional, documented, and regression-tested

## Phase 10. Algebra Capability Hardening

Goal:

- strengthen the algebra layer so it is credible as part of a production-grade
  symbolic core

Work:

- harden multivariate polynomial behavior
- define canonical-form expectations for supported algebra outputs
- improve selector and variable-policy rules
- add differentiation as a planned algebra capability
- identify which algebra features are in the supported product subset now
  versus later

Required tests:

- multivariate polynomial edge cases
- canonical algebra output checks
- negative/error behavior for invalid selectors and unsupported cases
- differentiation contract tests once introduced

Success condition:

- algebra behavior is documented as a supported product surface rather than a
  best-effort helper layer

## Phase 11. Documentation Pass

Goal:

- simplify engine-facing docs
- remove outdated transitional wording
- make the symbolic-core-first architecture explicit

Work:

- align the README and symbolic planning docs around the architecture:
  symbolic engine core, SDK on top, future products above that
- remove completed-work history and text that still frames the symbolic engine
  as transitional or secondary
- document the evaluator, simplification, and algebra subsystem boundaries
- document the supported subset and explicitly unsupported areas
- make UDF test coverage part of the production-readiness checklist
- keep the VM track documented as a later architecture track rather than part
  of current semantic stabilization

Success condition:

- docs describe the current architecture and semantic boundaries clearly
- readers can understand what the engine guarantees today and what remains on
  the roadmap

## Future Track: Aleph3 VM

This is a future architecture track, not a current blocking phase.

## Additional Implementation Steps

1. Gamma function support toward Mathematica-level behavior
   - numeric support for positive and negative non-integer reals
   - numeric support for complex inputs
   - explicit pole behavior at non-positive integers
   - regression coverage for half-integers, recurrence, and conjugation
   - remaining:
     - symbolic simplification identities and recurrence-based reductions
     - broader exact special values beyond the current half-integer coverage
     - tighter integration with simplification and rewrite rules
     - clearer analytic-domain and branch-behavior contract for symbolic cases

2. Complex transcendental and special-function expansion
   - extend unary transcendental support beyond basic arithmetic over complex
     values
   - define complex-domain contracts for powers, logarithms, roots, and special
     functions
   - add symbolic fallback and branch-behavior tests for the supported subset
   - include follow-on Gamma-family and complex special-function interactions

Purpose:

- run Aleph3 programs efficiently
- support reusable compiled artifacts
- reduce repeated evaluation overhead
- enable richer embedding and execution models

Possible scope:

- a lowered IR for executable Aleph3 programs
- a bytecode or instruction format
- a runtime for variables, calls, and control flow
- host-function bridging at the VM boundary

Prerequisites:

- stable symbolic semantics for the supported subset
- stable normalization and evaluation rules
- clearer function and program model
- a defined executable IR boundary

Risk rule:

- do not build VM machinery to paper over unresolved symbolic semantics

Exit condition for starting the VM track:

- the supported symbolic subset is stable enough that execution can be lowered
  without semantic churn invalidating the runtime model

## Future Track: Aleph3 Extensions

This is a future product and architecture track, not a current blocking phase.

Rule:

- keep `aleph3_symbolic` domain-neutral
- extensions must consume core symbolic semantics rather than fork them
- domain modules should compile their concepts into symbolic expressions,
  equations, and matrices that the core can normalize, simplify, and solve

Purpose:

- let Aleph3 support niche commercial domains without polluting the core
- reuse one symbolic engine across multiple vertical products
- preserve a clean separation between semantic infrastructure and domain logic

Extension layering:

1. Core symbolic engine
   - expressions
   - normalization
   - simplification
   - algebra
   - equations
   - matrices
   - assumptions
   - export and code-generation surfaces

2. Shared engineering modules
   - units and dimensions
   - transform libraries
   - domain-neutral numeric and structural helpers

3. Domain extensions
   - electrical engineering
   - DSP
   - control systems
   - other future verticals

Initial product rule:

- the core should know how to simplify expressions such as impedance,
  transfer-function, and rational forms
- the core should not know what a resistor, capacitor, circuit, or controller
  is as a first-class semantic primitive

Responsibilities by layer:

- symbolic core:
  canonicalization, simplification, algebra, solving, symbolic transforms
- extensions:
  domain objects, domain validation, lowering to symbolic equations, product
  workflows

Suggested extension build order:

1. Strengthen the symbolic core enough that rational and transform-heavy output
   simplifies well.
2. Add shared units and dimensions support only once a concrete product need is
   clear.
3. Add a first domain extension with a narrow scope, likely linear electrical
   circuits.
4. Add sibling extensions such as DSP or control systems on top of the same
   core semantics.

Prerequisites:

- stable canonical-form contract
- stronger simplification and algebra contracts
- adequate equation and matrix capabilities for extension lowering targets
- clear extension APIs so domain packages do not depend on evaluator internals

Exit condition for starting the extensions track:

- the symbolic core is strong enough that extension output looks product-grade
  without domain modules compensating for weak algebra or simplification
