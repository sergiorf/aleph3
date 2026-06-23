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

## Status

Completed:

- Freeze evaluator contract
- Add evaluator contract tests
- Extract algebra dispatch
- Extract special forms
- Extract built-in dispatch
- Extract user-defined function dispatch

Active:

- Phase 6. Standardize evaluator error helpers
- Phase 7. Canonical forms and normalization contracts
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

## Phase 6. Standardize Evaluator Error Helpers

Goal:

- remove ad hoc evaluator error construction
- make failure behavior stable enough to test intentionally
- separate symbolic fallback from true failure states

Work:

- centralize evaluator error creation
- define consistent wording for arity, invalid symbolic usage, and domain
  failures
- introduce explicit error categories for:
  - invalid form
  - invalid arity
  - domain violation
  - unsupported construct
  - internal inconsistency
- keep algebra-specific errors and generic evaluator errors clearly separated

Success condition:

- evaluator modules stop constructing scattered message strings inline
- error-path tests can target shared wording and behavior
- unevaluated symbolic results are clearly distinct from actual evaluator
  failures

## Phase 7. Canonical Forms And Normalization Contracts

Goal:

- make canonical representation a product contract rather than incidental output

Initial supported contract:

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

Work:

- define canonical ordering for commutative forms
- define normalization rules for algebraically equivalent expressions where the
  engine intends canonical output
- document when `evaluate`, `simplify`, and algebra routines may change form
- add idempotence expectations for normalization-sensitive operations

Required tests:

- canonical ordering for `Plus` and `Times`
- stable handling of negation and rational normalization
- idempotence for repeated normalization
- stable output for semantically equivalent input forms

Success condition:

- canonical output behavior is documented and tested for the supported subset
- equivalent expressions stop drifting across repeated operations

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
- remove text that still frames the symbolic engine as legacy or background-only
- document the evaluator, simplification, and algebra subsystem boundaries
- document the supported subset and explicitly unsupported areas

Success condition:

- docs describe the current architecture and semantic boundaries clearly
- readers can understand what the engine guarantees today and what remains on
  the roadmap

## Future Track: Aleph3 VM

This is a future architecture track, not a current blocking phase.

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
