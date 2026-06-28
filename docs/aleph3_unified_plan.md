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

Near-term external positioning should stay honest:

- Aleph3 is already a strong embeddable formula and symbolic engine
- the most product-ready surface today is the trusted SDK subset plus the
  kernel-backed symbolic surface around it
- Aleph3 is not yet close enough to Mathematica-class CAS breadth to market
  itself that way without inviting the wrong comparison

Recommended near-term message:

- a safe embeddable symbolic/formula engine in C++20, with a path toward
  richer CAS features

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
  symbolic resolution, builtin execution, host execution, assignment, and
  user-definition registration

What is still unresolved:

- the best-supported product surface is still the trusted SDK subset rather
  than a broad CAS workflow
- exact rational internals are ahead of exact value exposure in the public SDK
  runtime surface
- polynomial and algebra internals still mix narrow exact support with
  floating-point foundations
- higher math boundaries are still blended into evaluator-oriented code
- symbol definition and extension contracts are not yet strong enough for the
  intended pack model
- the current global mutable registration model is still too prototype-like
  for long-term embedding, plugin isolation, and thread-safe multi-tenant use
- the architecture writing is ahead of the finished product surface in several
  areas
- CI and repository hygiene still need product-grade hardening
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
- the first monetizable surface Aleph3 should make excellent before broader
  CAS or notebook claims

### Near-Term Product Honesty

Until exact algebra, assumptions, richer matching, and pack extensibility are
materially stronger, Aleph3 should be positioned as:

- an embeddable symbolic/formula engine
- a kernel-first system with a safe trusted SDK subset
- a project on the road to richer CAS features

It should not yet be positioned as:

- a modern Mathematica-like CAS with near-parity expectations

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

Aleph3 still should not invest heavily in the following until the kernel
contracts are stronger:

- notebook-style applications
- large domain packs
- hosted products
- broad CAS expansion that hardens the wrong foundation

The intended sequence is:

1. harden `aleph3_kernel` as the single semantic core
2. make `aleph3_sdk` a clean consumer and adoption layer over that kernel
3. make the SDK V1 excellent as the first real product and use it to validate
   kernel contracts from an external developer
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
- the supported symbolic subset is documented as a product contract, including
  the small coefficient-layer basis class and algebra-aware exponent class
- tests are organized by layer and validate semantic invariants

## Program Rules

These rules apply to all implementation work until the migration is complete:

- no new feature may deepen both evaluator stacks independently
- SDK constraints should be implemented as validation or policy over kernel
  semantics, not as a second semantic system
- external positioning should describe Aleph3 as an embeddable symbolic/formula
  engine until the broader CAS foundation is materially stronger
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
7. decide the durable registration and extension lifetime model for embedding
8. extract higher math behind explicit pack boundaries
9. expand product-facing symbolic capabilities on top of the stronger kernel
10. harden the SDK and product surfaces around the stronger kernel
11. build richer interactive product surfaces such as a notebook-like
   application on top of the unified kernel

## Milestones

Completed foundation:

- canonical plan and one-kernel direction are in place
- kernel boundary and representation choice are explicit
- shared execution contracts exist for evaluation context, diagnostics, and
  registration
- SDK evaluation is kernel-backed and no longer acts as a peer runtime
- the SDK is substantially complete as the first external adoption layer

Active milestone:

### M5. Kernel Extensibility Becomes Real

Outcome:

- Aleph3 remains honest about being SDK-first and kernel-first during this phase
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

Remaining tasks:

- keep target ownership clear as code moves
- continue separating kernel-owned, SDK-owned, and pack-owned code
- align test grouping with those boundaries

Success criteria:

- the build graph no longer suggests two long-term semantic cores

### C. Shared Kernel Contracts

Goals:

- define the kernel contracts that both symbolic and SDK-facing execution use

Success criteria:

- embedded and symbolic evaluation can be expressed through one kernel context
  model

### D. SDK V1 As First Product

Goals:

- make the trusted SDK subset the first polished commercial and adoption-ready
  surface

Tasks:

- keep the stable SDK subset small, explicit, and well-documented
- improve host-integration examples and fast-start embedding documentation
- keep public runtime exactness boundaries honest and explicit
- defer broader CAS positioning until the kernel can support it credibly

Success criteria:

- Aleph3 can be presented confidently as an embeddable formula/symbolic engine
- SDK adoption does not depend on unfinished broader CAS claims

### E. Symbol Definition And Extension Model

Goals:

- move from narrow evaluator dispatch toward a symbol-centric kernel

Current status:

- initial registration metadata is implemented
- initial symbol metadata and definition records are implemented
- evaluator dispatch now syncs and consults symbol-definition and registration
  contracts for registered symbolic handlers, builtin evaluator functions, user
  functions, and host functions
- builtin and host ownership are now explicit in kernel definition records
- evaluator dispatch now derives a primary owner from shared symbol and
  registration facts, and builtin execution is now registry-backed

Remaining tasks:

- move more execution semantics behind registry- or definition-backed contracts
- define how rewrite rules register against the same symbol contract
- decide how much attribute metadata should control dispatch versus remain
  descriptive
- decide whether symbolic handlers, builtins, and rewrites are registered
  globally, per engine, per context, or per environment
- make test-isolation and thread-safety expectations explicit for the
  registration model

Success criteria:

- new symbolic behavior can increasingly be added through definitions and
  registration rather than evaluator branching
- the extension model is credible for long-lived embedding and future packs

### F. Pattern Matching And Rewrite Engine

Goals:

- add the general symbolic transformation machinery higher-level features need

Current status:

- exact structural rule rewriting is implemented
- the first named-binder pattern language is implemented
- bounded repeated rewrite entrypoints exist
- the first evaluator-facing rewrite-owned simplification slice now covers
  fixed-arity arithmetic identity rewrites
- that dedicated n-ary arithmetic rewrite contract is now implemented for
  scalar `Plus` and `Times` neutral elimination, scalar `Times` annihilator
  handling, and scalar numeric/rational bucket folding
- a first symbolic coefficient layer now exists for normalized `Plus` over
  single-symbol and single-power bases with numeric and exact-rational
  coefficients
- a first algebra-aware layer now exists for same-symbol exponent accumulation
  in normalized `Times` and nested numeric `Power` forms
- conditional rules and richer pattern classes are still open

Near-term tasks:

- keep the symbolic coefficient contract limited to single-symbol and
  single-power bases until stronger exact algebra exists
- keep the algebra-aware layer limited to same-symbol exponent accumulation and
  numeric nested-power collapse until stronger exact algebra exists
- keep division cancellation,
  list-aware arithmetic, and special-function shortcuts evaluator-owned until
  stronger kernel contracts exist

Success criteria:

- symbolic transforms no longer depend only on hardcoded evaluator cases

Detailed guidance for the next rewrite migration slice:

- do not wait for general sequence patterns before moving broader arithmetic
  cleanup out of evaluator-local code
- use a dedicated n-ary contract for normalized `Plus` and `Times`
- treat that contract as head-aware variadic reduction, not as a generic
  matcher feature
- migrate only reductions that preserve current scalar arithmetic contracts
  without depending on list semantics or stronger algebra metadata
- move like-term collection into a separate symbolic coefficient contract
  instead of broadening arithmetic rewrite
- keep exponent merging in the separate algebra-aware layer rather than
  broadening arithmetic rewrite

### G. Exact Algebra Backbone

Goals:

- replace floating-point-centered symbolic algebra internals with exact
  foundations

Tasks:

- introduce exact coefficient-ring abstractions
- support exact rational polynomial coefficients
- harden multivariate polynomial algorithms
- make overflow and large-integer strategy explicit
- remove growth-facing dependence on `double` polynomial internals

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
- expand CI beyond the current basic matrix with Debug, Clang, sanitizers,
  formatting, and lightweight performance checks over time

Success criteria:

- the supported symbolic subset behaves like a documented product contract
- engineering quality is credible for a C++ math runtime, not just for a
  prototype

### L. Transitional Removal And Cleanup

Goals:

- finish the migration instead of letting the transitional architecture linger

Tasks:

- remove stale plan files and duplicated roadmap text
- remove duplicate runtime paths once kernel execution is live
- clean up docs, tests, and class-collaboration descriptions that imply two
  lasting semantic cores
- remove tracked build artifacts and keep `.gitignore` aligned with actual
  generated directories such as `build-sdk/` and `build-test/`

Success criteria:

- the repository no longer normalizes the transitional dual-core state
- repository hygiene matches a maintained product codebase

## Immediate Action Queue

If work starts now, the next active tranche should be:

1. keep the current rewrite-owned arithmetic layers within their documented
   boundaries until exact algebra is stronger
2. define the exact algebra backbone so symbolic coefficient and algebra-aware
   layers have a stable long-term foundation
3. define how rewrite rules register against the shared symbol and extension
   model
4. tighten tests and docs around the supported symbolic subset as a product
   contract

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
- SDK tutorials, host-integration examples, and other adoption-facing polish

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
