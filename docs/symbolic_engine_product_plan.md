# Symbolic Engine Product Plan

## Purpose

This document defines the symbolic engine as the core product asset in Aleph3.
It is not just shared infrastructure beneath the SDK. It is the mathematical
core that other product layers depend on and the main long-term source of
product leverage.

The target is a production-grade symbolic engine inspired by Mathematica,
implemented incrementally through a clearly defined supported subset rather
than through vague feature parity claims.

## Product Position

Aleph3 should be understood as a layered system:

1. `aleph3_symbolic`
   The core symbolic math engine and the main strategic asset.

2. `aleph3_sdk`
   A controlled embedding layer built on top of that core.

3. Future products
   Additional tools and services that reuse the same symbolic semantics.

This architecture matters because the engine is not only a developer utility.
It is the base for monetizable surfaces such as:

- embeddable commercial SDKs
- hosted symbolic APIs
- niche vertical tools
- internal and enterprise symbolic automation products

Related implementation plan:

- [Symbolic Evaluator Cleanup Plan](symbolic_evaluator_cleanup_plan.md)

## Product Architecture

### Core: `aleph3_symbolic`

The symbolic engine owns:

- expression model
- parser
- symbolic and mixed evaluation
- simplification and transforms
- polynomial and algebra utilities

Current code is concentrated in:

- [include/expr/Expr.hpp](/home/sergio/dev/aleph3/include/expr/Expr.hpp)
- [include/parser/Parser.hpp](/home/sergio/dev/aleph3/include/parser/Parser.hpp)
- [include/evaluator/Evaluator.hpp](/home/sergio/dev/aleph3/include/evaluator/Evaluator.hpp)
- [src/evaluator](/home/sergio/dev/aleph3/src/evaluator)
- [include/transforms/Transforms.hpp](/home/sergio/dev/aleph3/include/transforms/Transforms.hpp)
- [src/transforms/Transforms.cpp](/home/sergio/dev/aleph3/src/transforms/Transforms.cpp)
- [include/algebra/Polynomial.hpp](/home/sergio/dev/aleph3/include/algebra/Polynomial.hpp)
- [src/algebra](/home/sergio/dev/aleph3/src/algebra)

### SDK: `aleph3_sdk`

The SDK sits on top of the symbolic core and adds:

- structured diagnostics
- schema and policy validation
- bounded runtime behavior
- stable host-facing types and APIs

### Future Product Layers

The same symbolic core should support more than one commercial surface:

- embeddable SDKs
- interactive tooling
- symbolic preprocessing and normalization services
- hosted APIs
- future higher-level math products

## Current Symbolic Surface

The current symbolic engine already provides:

- symbolic expression trees with numbers, rationals, complex values, booleans,
  strings, lists, rules, assignments, and function definitions
- broad parser coverage including implicit multiplication, logic, lists, rules,
  assignments, and function definitions
- symbolic and mixed numeric evaluation
- simplification and expansion helpers
- exact rational arithmetic
- basic complex arithmetic
- polynomial arithmetic and symbolic polynomial helpers such as `Expand`,
  `Factor`, `Collect`, `GCD`, and `PolynomialQuotient`

The current CLI surface for the symbolic layer is:

- `aleph3_cli symbolic-evaluate <expr>`
- `aleph3_cli symbolic-simplify <expr>`
- `aleph3_cli symbolic-fullform <expr>`

## Product Standard

The target standard is not “useful symbolic behavior.” It is a production-grade
symbolic engine for a defined supported subset.

That means the engine must eventually offer:

- stable semantic contracts
- canonical forms where the subset intends them
- explicit evaluation-control behavior
- predictable simplification boundaries
- algebra behavior that is documented rather than best-effort
- clear error and unsupported-feature behavior
- regression coverage based on semantic invariants, not only examples

It does not require broad Mathematica parity in the short term.
It does require that the supported subset behave coherently and predictably.

## Current Contract And Limitations

Until the cleanup work expands or changes behavior intentionally,
`evaluate(expr, ctx)` should be treated as having the following contract:

- numeric and exact arithmetic reduce when enough concrete information exists
- algebra entrypoints evaluate through the algebra layer when their supported
  preconditions are met
- `If[cond, then, else]` evaluates only the selected branch when `cond`
  resolves to a boolean
- `If[cond, then, else]` remains symbolic when `cond` does not resolve to a
  boolean
- unresolved symbolic calls remain symbolic
- unknown functions preserve their original arguments instead of being
  partially reduced through the generic evaluator path
- algebra entrypoints that require symbolic selectors fail explicitly when the
  selector is invalid

Current limitations:

- eager-vs-lazy behavior is not yet documented broadly
- canonicalization is not yet a stable product contract
- simplification boundaries are still underdefined
- structured symbolic diagnostics are not complete
- algebra support is still narrow relative to the long-term goal

Current normalization scope:

- rationals are normalized into a stable sign convention
- subtraction is lowered into canonical additive form
- nested commutative `Plus` and `Times` forms are flattened
- commutative `Plus` and `Times` arguments are ordered deterministically
- normalization is intended to be structural and idempotent, not a substitute
  for simplification

## Main Product Gaps

Current status by area:

| Area | Current State | Main Gap |
| --- | --- | --- |
| Expression/value model | indirectly covered | lacks invariant-focused tests |
| Parser breadth | broad spot coverage | weak negative/error and ambiguity coverage |
| Symbolic evaluation | meaningful but uneven | weak contract definition for fallback and normalization |
| Canonical forms | partial and incidental | not yet a stable semantic contract |
| Simplification | narrow but defined rewrite subsystem | scope remains intentionally conservative |
| Built-in math | many examples | weak domain/error/canonical symbolic behavior coverage |
| Evaluation control | minimal | lacks attribute-like semantics and explicit supported subset |
| User-defined functions | functional | weak scoping/recursion/error/default-argument contract coverage |
| Rational arithmetic | relatively strong | weak integration with broader symbolic simplification |
| Complex arithmetic | basic | shallow coverage |
| Polynomial and algebra core | real but narrow | weak multivariate and canonical-form confidence |
| Differentiation | absent | capability missing |
| Integration | absent | capability missing |

## Product Roadmap

### Near-Term Priorities

1. Establish canonical-form and normalization contracts for the supported
   subset.
2. Define evaluation-control behavior explicitly instead of relying on ad hoc
   branching.
3. Harden algebra behavior and expand toward differentiation.
4. Expand UDF and cross-subsystem regression coverage.
5. Keep docs aligned with the actual supported subset and architecture.

### Medium-Term Priorities

1. Expand algebra capabilities beyond the current polynomial helpers.
2. Introduce differentiation with a narrow but defensible contract.
3. Add stronger semantic regression validation against a bounded
   Mathematica-inspired corpus.
4. Define which subset is stable enough for external commercial use.

### Long-Term Priorities

1. Grow the supported subset without diluting semantic rigor.
2. Add richer execution and program capabilities once symbolic semantics are
   stable enough to lower reliably.
3. Package the core into multiple monetizable surfaces without fragmenting the
   underlying semantics.

## Validation Strategy

The test strategy needs to evolve from feature examples into product-contract
validation.

Required testing modes:

- direct contract tests for evaluator behavior
- invariant tests for canonical forms and normalization
- simplification idempotence tests
- negative/error behavior tests
- algebra edge-case and canonical-output tests
- cross-feature interaction tests
- bounded differential validation against Mathematica for selected supported
  cases

The goal is not to mimic every Mathematica behavior. The goal is to ensure the
Aleph3-supported subset is coherent, predictable, and defensible.

## Future Architecture Track: Aleph3 VM

Aleph3 may eventually need a VM to execute Aleph3 programs efficiently, but
that work should follow semantic stabilization, not precede it.

This is a separate architecture track. The VM should amplify a stable symbolic
core, not compensate for unresolved symbolic semantics.

Purpose:

- efficient execution of Aleph3 programs
- reusable compiled artifacts
- better performance for repeated workloads
- richer embedding/runtime models

Likely ingredients:

- a lowered executable IR
- a bytecode or instruction representation
- runtime support for variables, calls, and control flow
- host-function bridging at the runtime boundary

Prerequisites:

- stable symbolic semantics for the supported subset
- clearer function and program model
- stable normalization and evaluation rules
- a trustworthy IR boundary between symbolic forms and executable forms

Product rule:

- the VM is a force multiplier for the engine, not a substitute for unresolved
  semantic design
