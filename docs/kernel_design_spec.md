# Kernel Design Spec

## Status

Draft implementation spec referenced by the
[Aleph3 Unified Plan](aleph3_unified_plan.md).

## Purpose

This document defines the implementation-level structure of `aleph3_kernel`.

It should answer:

- which kernel subsystems exist
- which interfaces they own
- how they depend on each other
- how kernel execution flows at a high level
- how migration should be sliced into implementable steps

## Scope

This spec covers:

- kernel module map
- subsystem ownership
- key interfaces and dependency rules
- evaluation pipeline
- migration slices

This spec does not cover in depth:

- symbol-definition semantics
- rewrite semantics
- SDK lowering mechanics
- exact algebra architecture

Those are covered by dedicated specs.

## Proposed Kernel Module Map

The kernel should be decomposed into these primary subsystems:

1. `expr`
   Expression and value representation centered on `Expr`.
2. `eval`
   Evaluation loop, fallback behavior, and evaluation-control semantics.
3. `symbols`
   Symbol metadata, definition storage, attributes, and lookup support.
4. `rewrite`
   Pattern matching, rewrite rules, and traversal primitives.
5. `exact`
   Exact arithmetic types and arithmetic helpers.
6. `normalize`
   Canonical structural normalization and bounded simplification helpers.
7. `assumptions`
   Assumption storage, domain facts, and assumption-aware query hooks.
8. `diagnostics`
   Kernel-owned diagnostics, error taxonomy, and source projection support.
9. `registration`
   Registration contracts for built-ins, packs, and host-backed functions.
10. `bridge`
   Transitional lowering or adaptation boundaries used to converge SDK
   execution onto the kernel.

## Ownership Rules

Each subsystem should have a clear owner and stable responsibility.

### `expr`

Owns:

- expression node representation
- literal/value representation visible to kernel semantics
- structural helpers

Must not own:

- evaluator policy
- host deployment concerns

### `eval`

Owns:

- evaluation entrypoints
- recursive or iterative evaluation flow
- fallback and reduction decisions
- evaluation budgets at kernel semantic level

Depends on:

- `expr`
- `symbols`
- `rewrite`
- `normalize`
- `diagnostics`
- `registration`
- `assumptions`

### `symbols`

Owns:

- symbol identity and metadata
- attribute queries
- definition lookup and mutation contracts

### `rewrite`

Owns:

- pattern representation
- matching
- rewrite application
- traversal semantics

### `exact`

Owns:

- integer and rational arithmetic support
- exact complex support where part of the supported subset
- algebra-facing exact scalar helpers

### `normalize`

Owns:

- canonical structural ordering where defined
- bounded simplification utilities
- structural flattening and ordering support where part of kernel semantics

### `assumptions`

Owns:

- assumption storage structures
- domain facts and query API
- assumption-aware hooks used by `eval`, `normalize`, and `rewrite`

Current practical surface:

- temporary assumptions through `Assuming[assumptions, expr]`
- targeted refinement through `Refine[expr, assumptions]`
- narrow boolean and sign facts used by comparisons and a small symbolic
  refinement layer
- a small derived-sign query layer for simple arithmetic forms such as `-x`,
  `2*x`, and `x^2`

### `diagnostics`

Owns:

- canonical kernel error taxonomy
- diagnostic payload structure
- projection helpers used by higher layers

### `registration`

Owns:

- the function catalog used by one engine or symbolic session
- builtin registration contracts
- pack registration contracts
- host-function registration bridge contracts where needed by kernel execution

Plain-language model:

- a function catalog is the set of kernel-registered behaviors available during
  one evaluation environment
- the SDK engine owns its own catalog
- a symbolic CLI session can use the default catalog
- tests can create local catalogs without affecting other tests

### `bridge`

Owns:

- temporary or long-lived adapters that map constrained SDK forms into kernel
  execution inputs

Must not become:

- a second evaluator core

## Dependency Rules

The intended dependency direction is:

- `expr` and `diagnostics` are foundational
- `exact`, `symbols`, `assumptions`, and `registration` build on those basics
- `normalize` and `rewrite` build on expression and symbol contracts
- `eval` orchestrates the others
- `bridge` depends on kernel interfaces but should not be depended on by core
  kernel semantics

Rules:

- `expr` must not depend on `eval`
- `symbols` must not depend on SDK types
- `rewrite` must not reach into SDK runtime code
- `bridge` may depend on SDK-facing forms, but the core kernel must not depend
  on SDK representations

## Evaluation Pipeline

The kernel evaluation pipeline should become explicit.

At a high level:

1. receive an `Expr` plus kernel evaluation context
2. perform bounded structural normalization where required
3. resolve symbol definitions and attributes relevant to the current form
4. decide whether evaluation control alters argument evaluation
5. evaluate arguments according to symbol semantics
6. attempt builtin or registered-function execution
7. attempt rewrite-driven transformations where applicable
8. apply algebra or pack-backed semantic hooks where appropriate
9. return a reduced expression or preserve symbolic fallback
10. emit kernel diagnostics when reduction is invalid, unsupported, or blocked
    by policy

Open design questions:

- where pre-normalization versus post-normalization boundaries should sit for
  future rewrite-heavy flows
- which existing reductions should move from evaluator-local code into
  rewrite-owned APIs

Current implementation contract:

- evaluation entry normalizes once before evaluator dispatch
- builtin simplification remains evaluator-owned
- rewrite is currently an explicit kernel API, not an ambient evaluator phase
- `rewrite_repeated(..., max_rewrites)` is locally bounded by caller-provided
  rewrite count
- `rewrite_repeated(..., EvaluationContext&, max_rewrites)` additionally
  consumes the shared evaluation-step budget, one step per successful rewrite
- builtin lookup, symbolic-handler lookup, and normalized-head rewrite lookup
  now read from the active evaluation context's function catalog

Current rewrite migration decision:

- do not block broader arithmetic simplification on full sequence-pattern
  support
- add a dedicated n-ary rewrite contract for normalized `Plus` and `Times`
  before broadening the general matcher
- keep like-term collection, container-aware arithmetic, and domain-sensitive
  power/division logic out of that first variadic rewrite slice
- move like-term collection through a separate symbolic coefficient contract
  rather than through the arithmetic rewrite layer
- keep exponent merging in the separate algebra-aware layer rather than
  folding it into arithmetic rewrite

Planned near-term rewrite layering:

1. general structural and named-binder rule engine
2. fixed-arity arithmetic identity rewrites
3. dedicated n-ary arithmetic reductions for normalized `Plus` and `Times`
4. only later, if still justified, broader sequence-pattern machinery

Current implementation status of that layering:

- steps 1 through 3 now exist in initial form
- the n-ary arithmetic layer currently covers neutral elimination, scalar
  annihilator handling for `Times`, and scalar numeric/rational bucket folding
- it still does not cover like-term collection, division
  cancellation, or container-aware arithmetic
- a first coefficient-aware symbolic layer now exists for small monomial-shaped
  terms in normalized `Plus` forms
- a first algebra-aware layer now exists for same-symbol exponent accumulation
  in normalized `Times` and nested numeric `Power` forms

Current product-contract boundaries for those two upper layers are:

- symbolic coefficient layer:
  supported basis class is `x`, `x^n`, `c*x`, and `c*x^n`, with single-symbol
  bases only and `c` restricted to `Number` or `Rational`
- algebra-aware layer:
  supported exponent class is same-symbol numeric exponent accumulation in
  normalized `Times` plus nested `Power[Power[x, a], b]` collapse for numeric
  `a` and `b`
- both layers preserve unsupported structures rather than partially
  reinterpreting them

Current evaluator-owned boundary around those layers is also explicit:

- division cancellation remains evaluator-owned
- power-domain-sensitive behavior remains evaluator-owned
- list-aware arithmetic and broadcasting remain evaluator-owned
- special-function shortcuts and recurrence-style symbolic transforms remain
  evaluator-owned

Those behaviors are not merely "not yet in rewrite". They are intentionally
kept outside the current kernel rewrite slice until the surrounding algebra,
domain, and container contracts are stronger.

## Interface Inventory To Define

The first interfaces that should be made explicit are:

- kernel evaluation entrypoint
- kernel evaluation context
- symbol definition lookup API
- attribute query API
- registration API for built-ins and packs
- rewrite invocation entrypoints
- exact numeric helper surface used by evaluator and algebra
- diagnostic creation and projection helpers

## Migration Slices

Suggested implementation slices:

### Slice 1. Explicit Kernel Interface Layer

Deliverables:

- stable kernel entrypoint headers
- explicit module ownership notes
- no semantic change required

### Slice 2. Shared Evaluation Context And Diagnostics

Deliverables:

- one context model
- one diagnostic taxonomy

### Slice 3. SDK Execution Bridge

Deliverables:

- SDK evaluation reaches kernel entrypoint through a defined bridge
- the legacy runtime evaluator path stops being a peer semantic center

### Slice 4. Symbol And Registration Expansion

Deliverables:

- symbol metadata and definition interfaces become first-class
- registration becomes the main extension path

### Slice 5. Rewrite Infrastructure

Deliverables:

- pattern and rewrite interfaces exist
- selected transforms stop being evaluator-only special cases

### Slice 6. Exact Algebra Foundation

Deliverables:

- exact scalar abstractions support stronger algebra growth

### Slice 7. Pack Extraction

Deliverables:

- more domain math moves behind pack boundaries

## Acceptance Criteria

This spec becomes actionable when:

- each kernel subsystem has a named interface owner
- dependencies between subsystems are explicit
- the evaluation pipeline is precise enough to drive refactors
- migration slices can be implemented without guessing subsystem boundaries
