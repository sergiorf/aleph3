# Kernel Refactor Urgent Plan

## Purpose

This document defines the urgent refactor program required to stop Aleph3 from
solidifying around two separate evaluator cores.

Today the repository contains:

- an SDK/runtime path built around `ir::Node` and `runtime::Evaluator`
- a symbolic/CAS path built around `Expr` and `evaluator::Evaluator`

That split is now the main architectural risk in the project.

If it continues, Aleph3 will accumulate:

- duplicated semantics
- incompatible extension models
- duplicated diagnostics and testing effort
- pressure to add new math features twice
- increasing difficulty building domain packs such as signal processing and
  electrical engineering

This plan treats the kernel unification as an urgent foundational refactor, not
as optional cleanup.

Implementation backlog:

- [Kernel Refactor Backlog](kernel_refactor_backlog.md)
- [Kernel Representation Decision](kernel_representation_decision.md)
- [Layer Ownership Matrix](layer_ownership_matrix.md)
- [Kernel Evaluation Context](kernel_evaluation_context.md)
- [Kernel Diagnostic Taxonomy](kernel_diagnostic_taxonomy.md)
- [Kernel Registration Interfaces](kernel_registration_interfaces.md)
- [Kernel Symbol Definition Precedence](kernel_symbol_definition_precedence.md)
- [Header Documentation Guideline](header_documentation_guideline.md)
- [Representation Gap Inventory](representation_gap_inventory.md)

## Decision

Aleph3 must converge on one kernel.

Target layering:

1. `aleph3_kernel`
   Owns symbolic semantics, expression/value model, evaluation, symbol
   definitions, normalization, rewrite hooks, exact scalar arithmetic,
   diagnostics, and extension registration.
2. `aleph3_sdk`
   Owns embedding API, policy, schema validation, trusted deployment controls,
   host integration, and operational budgets.
3. `aleph3_packs_*`
   Own domain math such as elementary functions, algebra, special functions,
   signal processing, and electrical engineering.
4. tools and products
   CLI, tests, examples, and future commercial surfaces.

Non-negotiable rule:

- there must not be two peer evaluator kernels after this program.

## Current Problem Statement

The current build and code layout show two execution systems:

- SDK path:
  - `frontend -> ir -> semantics -> runtime`
  - `src/sdk/Engine.cpp`
  - `src/runtime/Evaluator.cpp`
- symbolic path:
  - parser -> `Expr` -> symbolic evaluator
  - `src/evaluator/Evaluator.cpp`

This is acceptable only as a temporary migration state.
It is not an acceptable medium-term architecture.

## Target Architecture

## Kernel

The kernel should own:

- core expression/value representation
- symbol table and definition storage
- built-in metadata and attributes
- evaluation loop
- symbolic fallback behavior
- exact scalar arithmetic contracts
- structural normalization
- rewrite and traversal extension hooks
- assumptions/domain context hooks
- kernel diagnostics
- pack registration interfaces

The kernel should not own:

- host deployment policy
- schema allowlists
- host function product ergonomics
- CLI behavior
- domain-specific math growth that can live in packs

## SDK

The SDK should become a consumer of the kernel, not a parallel evaluator stack.

It should own:

- `Engine`
- `Schema`
- `Policy`
- host function registration facade
- compile/validate/evaluate lifecycle
- trusted-subset profiles for embedded use
- runtime budgets and operational controls

## Packs

Packs should own higher math growth through kernel registration contracts.

Initial pack direction:

- `packs/core_math`
- `packs/algebra`
- `packs/special_functions`
- later `packs/signal`
- later `packs/electrical`

## Program Rules

During this refactor, the following work should mostly stop unless it is needed
for migration:

- broad new CAS features
- differentiation/integration growth
- major solver work
- significant symbolic special-function expansion
- new domain verticals implemented directly inside the current evaluators

Allowed in parallel:

- bug fixes
- regression tests
- documentation cleanup
- narrow contract hardening required by migration

## Migration Strategy

The safest strategy is not "merge everything at once."

The strategy should be:

1. define the kernel contract
2. move shared semantics into the kernel
3. make the SDK call the kernel
4. move CAS growth into packs on top of the kernel
5. delete or retire the duplicate evaluator path

## Phases

### Phase 0. Freeze The Direction

Goal:

- stop accidental expansion of the dual-core architecture

Work:

- declare `kernel-first` as the repo-wide architecture rule
- link this plan from the existing architecture and cleanup docs
- explicitly mark the current dual evaluator state as transitional
- reject new features that deepen both stacks independently

Success condition:

- contributors can tell which layer new code belongs to
- no new feature is accepted without kernel ownership clarity

### Phase 1. Kernel Contract And Naming

Goal:

- define the kernel as a first-class code and build boundary

Work:

- introduce a target name such as `aleph3_kernel`
- document kernel-owned headers versus SDK-owned headers versus pack-owned
  headers
- choose the single internal semantic representation the kernel will own
- define what survives from:
  - `Expr`
  - `ir::Node`
  - evaluator contexts
- define the public internal contracts for:
  - expression/value nodes
  - symbol definitions
  - diagnostics
  - evaluation context
  - pack registration

Hard decision required:

- do not preserve both `Expr` and `ir::Node` as long-term semantic peers

Recommended bias:

- keep one kernel expression model
- treat any second form as a frontend or lowering artifact only

Success condition:

- there is a written representation decision
- there is a target/module map for the next phases

### Phase 2. Build Graph Refactor

Goal:

- make the target graph reflect the architecture before deeper code movement

Work:

- split CMake targets so the kernel is explicit
- make `aleph3_sdk` depend on `aleph3_kernel`
- move symbolic-core code behind the kernel target
- keep tooling targets consuming higher-level libraries rather than owning core
  semantics

Recommended target direction:

- `aleph3_kernel`
- `aleph3_sdk`
- `aleph3_pack_core_math`
- `aleph3_pack_algebra`
- `aleph3_cli`
- test targets aligned to those boundaries

Success condition:

- the build graph no longer implies two product cores

### Phase 3. Shared Evaluation Context And Diagnostics

Goal:

- remove duplicated execution contracts before touching advanced semantics

Work:

- define one kernel evaluation context model
- define one diagnostics/error taxonomy for kernel evaluation
- define one symbol/function registration path
- map SDK host functions into kernel call registration rather than letting the
  SDK runtime invent separate semantics

Success condition:

- both embedded evaluation and symbolic evaluation can be expressed through one
  kernel context model

### Phase 4. SDK Runtime Collapse Into The Kernel

Goal:

- remove `runtime::Evaluator` as a peer execution engine

Work:

- make `Engine::evaluate(...)` invoke the kernel
- keep `Schema` and `Policy` as SDK-side preconditions and runtime guards
- preserve the trusted embedded subset through validation and policy, not
  through a separate evaluator implementation
- keep SDK-specific structured errors if needed, but derive them from kernel
  results rather than parallel evaluation logic

Important rule:

- the SDK should constrain the kernel, not reimplement it

Success condition:

- the SDK no longer has an independent evaluator semantic core

### Phase 5. Symbol Definition And Extension Model

Goal:

- replace evaluator branching as the primary extensibility path

Work:

- introduce symbol metadata/definition storage in the kernel
- define precedence between:
  - kernel built-ins
  - user definitions
  - pack registrations
  - future rewrite rules
- move ad hoc builtin dispatch toward symbol-centric registration

Success condition:

- new symbolic capability can be registered without widening evaluator branch
  sprawl

### Phase 6. Pack Extraction

Goal:

- move domain math growth out of the kernel evaluator

Work:

- extract elementary numeric and symbolic functions into an early core math pack
- treat current algebra as the first explicit migration target
- keep only truly kernel-wide semantics in the kernel
- define pack lifecycle and registration tests

Migration order:

1. core math pack
2. algebra pack
3. special functions pack
4. later signal pack
5. later electrical pack

Success condition:

- new domain features land as packs by default

### Phase 7. Delete Transitional Duplicate Paths

Goal:

- finish the refactor instead of preserving migration scaffolding forever

Work:

- retire obsolete evaluator entry points
- retire obsolete duplicated contexts and helper registries
- collapse tests that were only proving old migration compatibility
- update docs to remove transitional wording

Success condition:

- there is one kernel evaluator architecture in the repo

## Representation Decision Criteria

The single most important technical choice in this program is the long-term
kernel representation.

Use these criteria:

- Can it represent symbolic structure without loss?
- Can it support exact arithmetic and symbolic fallback?
- Can it support definitions, rewrite rules, and assumptions later?
- Can the SDK trusted subset be represented as a constrained case of it?
- Can host-facing compile/validate/evaluate flows stay efficient and bounded?

If a representation cannot support the full symbolic kernel, it should not be
the kernel representation.

That means the trusted SDK subset should likely be modeled as a restricted view
of the kernel, not as a separate semantic universe.

## Concrete Near-Term Deliverables

These are the first deliverables that should happen in the repo:

1. Add `aleph3_kernel` target and document target ownership.
2. Write a representation decision record for `Expr` versus `ir::Node`.
3. Define one kernel evaluation context and one diagnostic taxonomy.
4. Change `aleph3_sdk` planning docs so the SDK is explicitly a constrained
   consumer of the kernel.
5. Create a small migration backlog for moving:
   - arithmetic
   - comparisons
   - `If`
   - function dispatch
   - symbol lookup
   into one evaluator path.

## Testing Strategy

This refactor needs architecture tests, not only feature tests.

Add or strengthen tests for:

- same input, same semantics across SDK and symbolic entry points where they
  overlap
- pack registration behavior
- symbol definition precedence
- policy-constrained execution over kernel semantics
- exact and symbolic fallback invariants
- diagnostics stability for the supported embedded subset

Success measure:

- overlapping features are proven to share one semantic engine, not just
  produce similar outputs by coincidence

## Risks

Main risks:

- trying to preserve both representations indefinitely
- hiding the duplicate evaluator problem behind CMake target renames
- letting SDK convenience needs distort kernel semantics
- moving algebra and special functions into the kernel instead of pack layers
- starting signal processing or electrical engineering before pack contracts
  exist

## Recommended Order Of Execution

This should now become the urgent architecture program:

1. Phase 0. Freeze the direction
2. Phase 1. Kernel contract and naming
3. Phase 2. Build graph refactor
4. Phase 3. Shared evaluation context and diagnostics
5. Phase 4. SDK runtime collapse into the kernel
6. Phase 5. Symbol definition and extension model
7. Phase 6. Pack extraction
8. Phase 7. Delete transitional duplicate paths

## Final Standard

This refactor is complete only when all of the following are true:

- Aleph3 has one kernel evaluator architecture
- the SDK is a constrained consumer of that kernel
- CAS growth lands through kernel contracts and packs
- future signal processing and electrical engineering work can be added as
  packs rather than as evaluator forks
- the repository no longer incentivizes parallel semantic implementations
