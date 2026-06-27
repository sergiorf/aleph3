# Aleph3 Unified Plan

## Status

This is the canonical planning document for Aleph3.

It replaces the previous split across:

- `kernel_refactor_urgent_plan.md`
- `kernel_refactor_backlog.md`
- `symbolic_engine_product_plan.md`
- `symbolic_evaluator_cleanup_plan.md`

Supporting architecture and contract documents remain valid, but they should
refer back to this file for roadmap, sequencing, and implementation priority.

For the stable, high-level system picture, see
[Architecture](architecture.md).

Implementation specs referenced by this plan:

- [Kernel Design Spec](kernel_design_spec.md)
- [Kernel Symbol Model Spec](kernel_symbol_model_spec.md)
- [Kernel Rewrite Spec](kernel_rewrite_spec.md)
- [Kernel Execution Bridge Spec](kernel_execution_bridge_spec.md)
- [Kernel Exact Algebra Spec](kernel_exact_algebra_spec.md)

## Purpose

Aleph3 needs one plan because its highest-level product direction, core
architecture migration, symbolic-kernel evolution, and cleanup backlog all
describe the same program.

That program is:

1. converge on one symbolic kernel
2. make the SDK a constrained consumer of that kernel
3. move higher math growth into packs
4. harden the supported symbolic subset into a production-grade core

## Strategic Position

Aleph3 is a layered symbolic math system.

The intended product stack is:

1. `aleph3_kernel`
   The core symbolic engine and main strategic asset.
2. `aleph3_sdk`
   The host-facing embedding layer built on top of the kernel and the first
   practical adoption surface for external developers.
3. `aleph3_packs_*`
   Domain math growth such as algebra, special functions, calculus, signal,
   and electrical engineering.
4. tools and future products
   CLI, notebook-style interactive applications, examples, hosted services,
   commercial SDK packaging, and other product surfaces.

The kernel is not just implementation detail beneath the SDK.
It is the semantic center that all product surfaces depend on.

## Current Repository Reality

The repository is in a transitional state.

What is already true:

- `aleph3_kernel` exists as an explicit build target
- `aleph3_sdk` already depends on `aleph3_kernel`
- pack placeholder targets already exist
- the symbolic engine already has a meaningful supported subset with parser,
  evaluation, normalization, simplification, exact rationals, and a narrow
  algebra layer
- the SDK now evaluates through the kernel and retains lowered kernel
  execution state rather than trusted-subset IR
- the public SDK surface is documented and split into stable versus
  transitional API areas
- the kernel now has an initial symbolic registration contract, symbol
  metadata/definition records, and exact-rule rewrite infrastructure
- evaluator flow now populates those kernel contracts during registered
  symbolic resolution, assignment, and user-definition registration

What is still unresolved:

- higher math boundaries are still blended into evaluator-oriented code
- symbol definition and extension contracts are not yet strong enough for the
  intended pack model
- several advanced roadmap areas still depend on stronger kernel contracts

The main remaining architectural risk is not the old runtime split anymore.
It is whether the kernel grows a clean extension model before more symbolic
surface area gets added.

## Accepted Core Decisions

### One Kernel

Aleph3 must converge on one kernel.
There must not be two permanent peer evaluator cores.

### Representation Decision

`Expr` is the long-term kernel semantic representation.

`ir::Node` remains useful, but only as an SDK/trusted-subset representation for
parsing, validation, compile artifacts, or lowering inputs.
It is not a second kernel AST.

### SDK Position

The SDK owns:

- `Engine`
- `Schema`
- `Policy`
- host-facing types and diagnostics
- trusted-subset validation
- deployment controls and budgets

The SDK does not own a separate permanent semantic runtime.

The SDK should be treated as the first practical product surface:

- a small, stable, well-documented embedding API
- the easiest open-source adoption path for external developers
- a bridge from host applications into kernel-backed symbolic evaluation
- a validation layer for whether kernel contracts are usable from the outside

### Pack Position

Higher math growth should move through pack registration on top of the kernel,
not through unchecked evaluator branching.

### Product Surface Position

Interactive products such as a notebook-like application belong above the
kernel and SDK.

They may eventually supersede parts of the CLI surface, but they must remain
consumers of unified kernel semantics rather than becoming a third execution
center.

## Kernel Functional Scope

The kernel must eventually own the semantic functionality required for Aleph3
to behave like one coherent symbolic engine.

### Kernel-Owned Now Or Immediately Next

- expression and value representation centered on `Expr`
- symbolic evaluation loop and fallback behavior
- structural normalization and bounded simplification
- exact scalar arithmetic contracts, especially integers, rationals, and
  complex numbers
- symbol lookup, symbol values, and current user-defined function handling
- built-in metadata needed for evaluation behavior
- shared diagnostics and evaluation context
- registration interfaces used by symbolic functions, packs, and SDK-facing
  host integration

### Kernel Capabilities That Need To Be Expanded

- richer symbol definition storage beyond narrow call-definition handling
- explicit attribute model for evaluation control and symbol behavior
- general pattern matching
- rewrite and traversal APIs
- assumptions and domain-aware transformation hooks
- exact algebra-facing abstractions that higher symbolic math can depend on
- canonical unsupported-feature and partial-support behavior
- convergence path for SDK trusted-subset lowering into kernel execution

### Kernel Boundaries

The kernel should own:

- semantics
- expression transformation
- exact math foundations
- symbolic diagnostics
- extension registration contracts

The kernel should not own:

- host deployment policy
- schema allowlists
- UI concerns
- notebook/session presentation behavior
- product packaging ergonomics
- domain packs whose logic can live above stable kernel contracts

## Plan And Spec Split

This document is the canonical plan.

It answers:

- what Aleph3 is trying to become
- what work is most important
- what order the work should happen in
- which major milestones define progress

The implementation specs answer:

- how the kernel is decomposed
- what interfaces each subsystem owns
- how execution should flow
- how migration should proceed in detail

Rule:

- roadmap and priority changes belong here
- subsystem design detail belongs in the referenced specs

## Product Sequencing Rule

Aleph3 should remain kernel-first for the current phase.

The project should avoid investing heavily in product surfaces built on top of
transitional semantics.

Until SDK execution routes through the kernel, Aleph3 should not invest heavily
in:

- notebook-style applications
- large domain packs
- hosted products
- broad CAS expansion that hardens the wrong foundation

The intended sequence is:

1. harden `aleph3_kernel` as the single semantic core
2. make `aleph3_sdk` a clean consumer and adoption layer over that kernel
3. use the SDK to validate kernel contracts from an external developer
   perspective
4. implement one serious pack to prove the extension model
5. only then build richer tools such as notebooks, engineering workbenches,
   hosted services, and vertical products

## End State

The program is complete when all of the following are true:

- `aleph3_kernel` is the only semantic core
- SDK evaluation routes through the kernel
- the legacy runtime evaluator path is removed
- kernel-owned contracts exist for evaluation context, diagnostics, symbol
  definitions, and registration
- higher math growth follows pack boundaries instead of accumulating in the
  evaluator core
- the supported symbolic subset is documented as a product contract
- tests are organized by layer and validate semantic invariants

## Program Rules

These rules apply to all implementation work until the migration is complete:

- no new feature may deepen both evaluator stacks independently
- SDK constraints should be implemented as validation or policy over kernel
  semantics, not as a second semantic system
- broad differentiation, integration, solver work, or large special-function
  expansion should wait until the kernel boundary is stronger
- bug fixes, regression tests, documentation cleanup, and narrow contract
  hardening are allowed in parallel
- new semantic behavior must declare whether it belongs to kernel, SDK, or pack

## Priority Order

The order of work is:

1. keep docs and repo messaging aligned with the one-kernel direction
2. finish collapsing SDK execution into the kernel
3. define the symbol and extension model the kernel will grow through
4. add general rewrite infrastructure
5. replace weak algebra foundations with exact infrastructure
6. add assumptions and domain-aware semantics
7. extract higher math behind explicit pack boundaries
8. expand product-facing symbolic capabilities on top of the stronger kernel
9. build richer interactive product surfaces such as a notebook-like
   application on top of the unified kernel

## Milestones

### M0. Canonical Plan And Direction Freeze

Outcome:

- one plan document exists
- repo docs consistently describe the dual-evaluator state as transitional
- no roadmap ambiguity remains about the long-term architecture

### M1. Kernel Boundary Chosen

Outcome:

- kernel representation decision is explicit
- ownership boundaries are documented
- `aleph3_kernel` is the acknowledged semantic core

### M2. Shared Execution Contracts

Outcome:

- one evaluation context model
- one diagnostic taxonomy
- one registration path

### M3. SDK Uses The Kernel

Outcome:

- SDK evaluation no longer depends on a peer semantic runtime

### M4. SDK Becomes The First Adoption Layer

Outcome:

- the public SDK surface is small, stable, and clearly documented
- SDK diagnostics and evaluation flow are recognizably kernel-backed
- external developers can adopt Aleph3 through the SDK without needing kernel
  internals

Current status:

- substantially complete
### M5. Kernel Extensibility Becomes Real

Outcome:

- symbol definition model exists
- registration and metadata replace more evaluator branching
- rewrite infrastructure begins to support symbolic growth

### M6. Math Growth Moves To Stronger Foundations

Outcome:

- exact algebra backbone is in place
- assumptions and domain semantics start to exist
- pack boundaries carry more of the higher math surface

### M7. Transitional Paths Retired

Outcome:

- duplicate runtime semantics are removed
- tests and docs align to the one-kernel architecture

### M8. Interactive Product Surfaces

Outcome:

- a notebook-style application can sit above the kernel without introducing a
  separate semantic runtime
- the CLI becomes either a thin developer tool or is partly superseded by the
  richer interactive surface

## Workstreams

### A. Documentation And Direction Freeze

Goals:

- keep the repo explicit about the architecture direction
- keep focused contract docs separate from the roadmap
- avoid stale transitional wording

Tasks:

- keep `README.md` and architecture docs aligned with the one-kernel message
- point all roadmap references to this document
- keep supported-subset and unsupported-area docs explicit

Success criteria:

- contributors can identify the canonical plan without inference
- readers can tell which docs are contracts and which doc is the roadmap

### B. Kernel Boundary And Build Graph

Goals:

- make the build graph and ownership model reflect the intended architecture

Current status:

- partially complete
- `aleph3_kernel` exists
- `aleph3_sdk` depends on it
- pack placeholder targets exist

Remaining tasks:

- keep target ownership clear as code moves
- continue separating kernel-owned, SDK-owned, and pack-owned code
- align test grouping with those boundaries

Success criteria:

- the build graph no longer suggests two long-term semantic cores

### C. Shared Kernel Contracts

Goals:

- define the kernel contracts that both symbolic and SDK-facing execution use

Tasks:

- define one kernel evaluation context
- define one kernel diagnostic taxonomy
- define one symbol/function registration path
- define how host functions map into kernel execution

Success criteria:

- embedded and symbolic evaluation can be expressed through one kernel context
  model

### D. SDK Runtime Collapse

Goals:

- remove the SDK’s status as a peer execution engine
- make the SDK a clean kernel-backed adoption layer

Current status:

- complete for the current migration slice
- SDK evaluation is kernel-backed
- trusted-subset IR is limited to parser and validator concerns
- the legacy runtime evaluator path is retired
- SDK/runtime diagnostics are projected from kernel-owned errors
- regression coverage compares SDK evaluation against direct kernel evaluation
- the stable versus transitional SDK API split is now documented

Success criteria:

- the SDK no longer owns a separate semantic runtime
- the SDK can be understood and used without understanding kernel internals

### E. Symbol Definition And Extension Model

Goals:

- move from narrow evaluator dispatch toward a symbol-centric kernel

Current status:

- initial registration metadata is implemented
- initial symbol metadata and definition records are implemented
- evaluator dispatch now syncs and consults symbol-definition and registration
  contracts for registered symbolic handlers, user functions, and host
  functions
- builtin evaluator functions still retain local precedence code

Success criteria:

- new symbolic behavior can increasingly be added through definitions and
  registration rather than evaluator branching

### F. Pattern Matching And Rewrite Engine

Goals:

- add the general symbolic transformation machinery higher-level features need

Current status:

- exact structural rule rewriting is implemented
- the first named-binder pattern language is implemented
- bounded repeated rewrite entrypoints exist
- conditional rules and richer pattern classes are still open

Success criteria:

- symbolic transforms no longer depend only on hardcoded evaluator cases

### G. Exact Algebra Backbone

Goals:

- replace floating-point-centered symbolic algebra internals with exact
  foundations

Tasks:

- introduce exact coefficient-ring abstractions
- support exact rational polynomial coefficients
- harden multivariate polynomial algorithms
- make overflow and large-integer strategy explicit

Success criteria:

- algebra growth no longer rests on the current narrow floating polynomial core

### H. Assumptions And Domain Semantics

Goals:

- make simplification and transformation context-aware where justified

Tasks:

- define assumptions storage in evaluation context
- define core domain categories and propagation rules
- make selected transforms assumption-aware
- keep unsupported cases explicit

Success criteria:

- targeted domain-sensitive rewrites can happen without ad hoc unsafe behavior

### I. Math Packs And Product Surface Growth

Goals:

- move higher math out of the kernel core while keeping semantics unified

Tasks:

- split elementary functions, algebra, calculus, and special functions into
  clearer pack-style modules over time
- define pack registration interfaces
- keep kernel contracts small and stable while allowing math-surface growth
- choose one serious early pack that proves the extension model with meaningful
  workflows rather than only placeholder packaging
- keep vertical domains such as electrical engineering out of the kernel and
  implement them only once kernel extension points are stable

Success criteria:

- higher math growth follows pack boundaries instead of evaluator sprawl
- at least one serious pack proves that meaningful domain behavior can be added
  without modifying evaluator internals

### J. Interactive Applications And Product Surfaces

Goals:

- provide richer end-user interaction without fragmenting semantics

Tasks:

- define a notebook-style application as a future consumer of the kernel and
  SDK
- keep session state, cell execution, rendering, and inspection features out
  of the kernel
- decide whether the CLI remains a developer tool, a compatibility surface, or
  a thin shell over notebook-facing services
- ensure any interactive surface reuses kernel evaluation, diagnostics, and
  pack registration rather than adding separate execution logic

Priority:

- after kernel/SDK convergence is stable enough that the interactive surface
  will not be built on transitional semantics

Success criteria:

- Aleph3 has a path to a notebook-like surface without creating a third
  semantic center

### K. Tests, Validation, And Product Hardening

Goals:

- move from feature-example testing toward product-contract testing

Tasks:

- group tests by kernel, SDK, transitional compatibility, and packs
- add invariant-oriented regression coverage
- strengthen UDF, cross-subsystem, diagnostics, and unsupported-case coverage
- keep supported-subset behavior documented and testable

Success criteria:

- the supported symbolic subset behaves like a documented product contract

### L. Transitional Removal And Cleanup

Goals:

- finish the migration instead of letting the transitional architecture linger

Tasks:

- remove stale plan files and duplicated roadmap text
- remove duplicate runtime paths once kernel execution is live
- clean up docs, tests, and class-collaboration descriptions that imply two
  lasting semantic cores

Success criteria:

- the repository no longer normalizes the transitional dual-core state

## Immediate Action Queue

If work starts now, the next implementation tranche should be:

1. finish documentation consolidation and stale-plan removal
2. define rewrite budgeting relative to evaluation budgets
3. explain how rewrite should be scheduled relative to evaluator-owned
   simplification and normalization
4. only then start designing richer interactive surfaces such as a notebook
    around the unified execution path

## Deferred Work

The following should mostly wait until the kernel program is further along:

- broad differentiation work
- large solver efforts
- major symbolic special-function expansion beyond targeted contract hardening
- significant algebra growth on top of the current floating algebra core
- new domain verticals implemented directly inside evaluator branches
- notebook-style product work built on top of transitional runtime semantics

## Allowed Parallel Work

These can proceed without waiting for the whole program:

- bug fixes
- regression tests
- documentation cleanup
- narrow builtin contract hardening
- limited symbolic-domain corrections that reduce future migration risk

## Product Standard

The target is not vague symbolic usefulness.
The target is a production-grade symbolic engine for an explicit supported
subset.

That requires:

- stable semantic contracts
- predictable fallback behavior
- canonical forms where the subset intends them
- explicit unsupported behavior
- coherent diagnostics
- regression coverage based on semantic invariants

## Success Test

This unified plan is succeeding if:

- contributors stop asking which plan is authoritative
- architecture decisions stop being duplicated across roadmap docs
- new code follows the kernel/SDK/pack split consistently
- SDK and symbolic execution visibly converge
- future product growth depends on stronger kernel contracts, not on more
  duplicate evaluator logic
