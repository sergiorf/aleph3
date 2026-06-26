# Kernel Refactor Backlog

## Purpose

This document turns the kernel refactor program into execution backlogs sized
for implementation in the current Aleph3 repository.

Primary plan:

- [Kernel Refactor Urgent Plan](kernel_refactor_urgent_plan.md)

This backlog is intentionally concrete:

- repo areas are named directly
- tasks have IDs
- dependencies are explicit
- exit criteria are testable

## Current Code Mapping

Current symbolic-oriented areas:

- `include/expr`
- `include/evaluator`
- `include/parser`
- `include/normalizer`
- `include/transforms`
- `include/algebra`
- `include/packs`
- `include/symbols`
- `src/expr`
- `src/evaluator`
- `src/algebra`
- `src/transforms`

Current SDK/runtime-oriented areas:

- `include/sdk`
- `include/ir`
- `include/frontend`
- `include/semantics`
- `include/runtime`
- `src/sdk`
- `src/frontend`
- `src/semantics`
- `src/runtime`

Current top-level build boundary:

- `aleph3_symbolic`
- `aleph3_sdk`
- `aleph3_cli`

## Execution Rules

Apply these rules to every backlog item:

- no new feature may deepen both evaluator stacks independently
- code movement without ownership clarification does not count as progress
- any new semantic behavior must declare whether it belongs to kernel, SDK, or
  pack
- SDK constraints must be implemented as policy/validation over kernel
  semantics, not as a second evaluator

## Milestones

### M0. Direction Freeze

Outcome:

- the repo clearly states that the dual evaluator state is transitional

### M1. Kernel Boundary Chosen

Outcome:

- one semantic representation is chosen for the long-term kernel
- a first `aleph3_kernel` build boundary exists

### M2. Shared Execution Contracts

Outcome:

- one evaluation context, one diagnostic taxonomy, one registration path

### M3. SDK Uses The Kernel

Outcome:

- the SDK no longer owns a peer runtime evaluator implementation

### M4. Packs Extracted

Outcome:

- domain math growth moves through pack registration instead of evaluator
  branching

### M5. Transitional Code Removed

Outcome:

- duplicate evaluator paths are retired

## Epic A. Architecture Decision And Freeze

### A1. Publish The Transitional-State Warning

Scope:

- `README.md`
- `docs/system_architecture.md`
- `docs/sdk/build_and_targets.md`

Tasks:

- mark the current SDK runtime path as transitional rather than permanent
- point readers at the kernel refactor plan and backlog
- remove wording that implies two long-term cores are acceptable

Dependencies:

- none

Acceptance criteria:

- repo docs consistently describe one-kernel end state

### A2. Add A Kernel Decision Record

Scope:

- new doc under `docs/`, for example `docs/kernel_representation_decision.md`

Tasks:

- compare `Expr` and `ir::Node` against kernel requirements
- record chosen long-term kernel representation
- record the role of the non-chosen form
- record migration implications for parser, validator, runtime, and evaluator

Dependencies:

- none

Acceptance criteria:

- contributors can answer whether `Expr` or `ir::Node` is the kernel semantic
  form without inference

### A3. Define Layer Ownership Matrix

Scope:

- new doc or section in `docs/kernel_refactor_urgent_plan.md`

Tasks:

- map each current directory to `kernel`, `sdk`, `pack`, or `tooling`
- mark directories that are transitional or mislayered
- list forbidden ownership patterns

Dependencies:

- A2

Acceptance criteria:

- every major current directory has a target owner

## Epic B. Build Graph Refactor

### B1. Introduce `aleph3_kernel`

Scope:

- [CMakeLists.txt](/home/sergio/dev/aleph3/CMakeLists.txt:1)

Tasks:

- add a first explicit `aleph3_kernel` target
- move kernel-owned source globs or explicit file lists behind it
- keep `aleph3_symbolic` as an alias only if needed during migration

Dependencies:

- A2
- A3

Acceptance criteria:

- the build graph contains `aleph3_kernel`
- kernel-owned code no longer builds only through `aleph3_symbolic`

### B2. Make `aleph3_sdk` Depend On `aleph3_kernel`

Scope:

- [CMakeLists.txt](/home/sergio/dev/aleph3/CMakeLists.txt:36)

Tasks:

- link `aleph3_sdk` against `aleph3_kernel`
- stop implying that `aleph3_sdk` is self-sufficient for semantics
- preserve current public SDK headers

Dependencies:

- B1

Acceptance criteria:

- the target graph reflects SDK-on-kernel layering

### B3. Define First Pack Targets

Scope:

- `CMakeLists.txt`
- new pack directory layout if introduced during this phase

Tasks:

- add placeholder or initial targets:
  - `aleph3_pack_core_math`
  - `aleph3_pack_algebra`
- link them against `aleph3_kernel`
- avoid exposing them as public SDK API

Dependencies:

- B1

Acceptance criteria:

- the build graph has explicit expansion points for pack extraction

### B4. Align Tests With Layer Boundaries

Scope:

- `tests/`
- `CMakeLists.txt`

Tasks:

- separate kernel tests from SDK tests by ownership
- identify transitional compatibility tests
- add a place for pack-specific tests

Dependencies:

- B1

Acceptance criteria:

- tests are grouped by layer rather than old historical subsystem naming only

## Epic C. Kernel Contract Definition

### C1. Define Kernel Evaluation Context

Scope:

- likely new headers under `include/`
- current:
  - `include/evaluator/EvaluationContext.hpp`
  - `include/runtime/Evaluator.hpp`

Tasks:

- define one kernel execution context model
- specify symbol lookup, function lookup, assumptions hooks, and operational
  limits
- map SDK bindings and constants into that model

Dependencies:

- A2

Acceptance criteria:

- both symbolic and embedded execution can be expressed using one context
  contract

### C2. Define Kernel Diagnostic Taxonomy

Scope:

- `include/sdk/Types.hpp`
- symbolic error headers under `include/evaluator`
- runtime error use sites

Tasks:

- inventory current symbolic and runtime errors
- define one kernel-level taxonomy for evaluation and resolution failures
- define how SDK-facing diagnostics wrap or project kernel errors

Dependencies:

- C1

Acceptance criteria:

- there is one kernel error vocabulary for execution semantics

### C3. Define Kernel Registration Interfaces

Scope:

- `include/packs/PackRegistry.hpp`
- evaluator builtin registration
- SDK host function registration path

Tasks:

- define one registration path for built-ins, user definitions, and packs
- define where SDK host functions adapt into that registration model
- prohibit new evaluator-only dispatch islands

Dependencies:

- C1
- C2

Acceptance criteria:

- extensions do not need separate runtime and symbolic registration code paths

### C4. Define Symbol Definition Precedence

Scope:

- evaluator and future kernel definition model docs or headers

Tasks:

- specify precedence between:
  - literal values
  - symbol values
  - user function definitions
  - built-ins
  - pack registrations
  - future rewrite rules
- define what remains unsupported for now

Dependencies:

- C3

Acceptance criteria:

- precedence questions have explicit answers before more code is moved

## Epic D. Representation Convergence

### D1. Inventory Representation Loss And Gaps

Scope:

- `include/expr/Expr.hpp`
- `include/ir/Node.hpp`
- parsers and evaluators

Tasks:

- list features modeled by `Expr` but not `ir::Node`
- list features modeled by `ir::Node` but not `Expr`
- identify what the SDK trusted subset needs immediately
- identify what the future symbolic kernel needs eventually

Dependencies:

- A2

Acceptance criteria:

- migration work is based on a gap inventory rather than intuition

### D2. Define Lowering Or Promotion Strategy

Scope:

- parser/frontend design docs

Tasks:

- if `Expr` is chosen as kernel form:
  - define how SDK parse/validate lowers into kernel execution
- if `ir::Node` is chosen as kernel form:
  - define how symbolic features are represented without semantic loss
- document whether the non-kernel form is:
  - frontend-only
  - validator-only
  - cache-only
  - temporary migration scaffolding

Dependencies:

- D1

Acceptance criteria:

- the secondary representation has a limited, explicit role

### D3. Create Migration Staging Modules

Scope:

- new headers and source files under chosen target directories

Tasks:

- add staging adapters between current representations if needed
- keep adapters narrow and temporary
- add comments marking them as migration-only

Dependencies:

- D2

Acceptance criteria:

- code movement can proceed without silently making both representations
  permanent peers

## Epic E. SDK Runtime Collapse

### E1. Inventory Overlapping Semantics

Scope:

- `src/runtime/Evaluator.cpp`
- `src/evaluator/Evaluator.cpp`
- related tests

Tasks:

- list overlapping behavior:
  - arithmetic
  - comparisons
  - `If`
  - symbol lookup
  - function dispatch
- identify mismatches in error behavior, fallback behavior, and numeric domain
  handling

Dependencies:

- C1
- C2

Acceptance criteria:

- overlap is explicitly tracked before unification work starts

### E2. Move Shared Primitive Semantics Into Kernel Modules

Scope:

- new kernel-owned implementation files

Tasks:

- extract primitive evaluation helpers for arithmetic and comparison
- extract common conditional evaluation behavior
- extract shared value conversion or normalization helpers where valid

Dependencies:

- E1
- D3

Acceptance criteria:

- shared semantics live in kernel-owned code rather than runtime-only code

### E3. Route `Engine::evaluate` Through Kernel Execution

Scope:

- [src/sdk/Engine.cpp](/home/sergio/dev/aleph3/src/sdk/Engine.cpp:196)

Tasks:

- replace direct dependence on `runtime::Evaluator` with kernel execution
- keep SDK policy and schema constraints intact
- preserve current successful trusted-subset behavior where intended

Dependencies:

- E2
- C3

Acceptance criteria:

- `Engine::evaluate` no longer depends on an independent SDK evaluator core

### E4. Retire Or Hollow Out `runtime::Evaluator`

Scope:

- `include/runtime/Evaluator.hpp`
- `src/runtime/Evaluator.cpp`

Tasks:

- either remove `runtime::Evaluator` or reduce it to a thin adapter over the
  kernel during transition
- mark any surviving adapter as transitional

Dependencies:

- E3

Acceptance criteria:

- runtime no longer owns distinct semantic logic

## Epic F. Symbol And Extension Model

### F1. Introduce Kernel Symbol Metadata Storage

Scope:

- likely new kernel-owned symbol subsystem
- current:
  - `include/symbols`
  - evaluator context structures

Tasks:

- define symbol records for values, attributes, and callable definitions
- avoid scattering metadata across evaluator helper files

Dependencies:

- C4

Acceptance criteria:

- symbol behavior is stored in a coherent subsystem

### F2. Convert Built-In Dispatch To Registration

Scope:

- `src/evaluator/EvaluatorBuiltins.cpp`
- `src/evaluator/BuiltInFunctions.cpp`
- `include/packs/PackRegistry.hpp`

Tasks:

- move built-ins toward registration-driven availability
- reduce direct evaluator branching for built-in selection
- define minimal metadata required for registration

Dependencies:

- F1
- C3

Acceptance criteria:

- adding built-ins does not require widening a central evaluator switch/if tree

### F3. Define SDK Host Function Adapter Layer

Scope:

- `src/sdk/Engine.cpp`
- host function types in `include/sdk`

Tasks:

- map `HostFunctionSpec` into kernel registration contracts
- preserve SDK-specific metadata validation
- keep host-facing API stable while unifying runtime semantics

Dependencies:

- C3
- E3

Acceptance criteria:

- host functions are no longer a separate evaluator concept

## Epic G. Pack Extraction

### G1. Extract Core Math Pack

Scope:

- likely current evaluator built-ins and simple math helpers

Tasks:

- define a small `core_math` pack boundary
- move elementary built-ins out of evaluator core where feasible
- keep kernel ownership only for semantics that are truly global

Dependencies:

- F2

Acceptance criteria:

- elementary math lives behind a pack registration interface

### G2. Extract Algebra Pack

Scope:

- `include/algebra`
- `src/algebra`
- algebra entrypoints in evaluator logic

Tasks:

- make algebra capabilities register through a pack boundary
- remove algebra-specific branching from the kernel evaluator where feasible
- document algebra as pack-owned rather than permanent kernel logic

Dependencies:

- G1

Acceptance criteria:

- algebra is no longer architecturally fused into the evaluator core

### G3. Extract Special Functions Pack

Scope:

- Gamma and related special-function code paths

Tasks:

- group special-function behavior behind pack registration
- keep analytic contracts documented separately from kernel semantics

Dependencies:

- G1

Acceptance criteria:

- special-function growth can proceed without widening the kernel scope

### G4. Define Future Vertical Pack Contracts

Scope:

- docs only in first pass

Tasks:

- define the expected contract surface for:
  - `packs/signal`
  - `packs/electrical`
- specify what those packs can assume from kernel and SDK
- prohibit vertical-domain semantics from bypassing pack registration

Dependencies:

- G1

Acceptance criteria:

- future vertical work has a contract before code begins

## Epic H. Tests And Validation

### H1. Add Shared-Semantics Contract Tests

Scope:

- `tests/`

Tasks:

- add tests asserting overlapping SDK and symbolic entry points share behavior
- cover:
  - arithmetic
  - comparisons
  - `If`
  - unknown symbol handling
  - basic function dispatch

Dependencies:

- E1

Acceptance criteria:

- overlapping semantics are tested as a single contract

### H2. Add Registration And Precedence Tests

Scope:

- `tests/evaluator`
- future kernel and pack tests

Tasks:

- test built-in versus user-defined resolution
- test pack registration visibility
- test host-function adaptation into kernel registration

Dependencies:

- C4
- F3

Acceptance criteria:

- extension precedence is regression-tested

### H3. Add Pack Lifecycle Tests

Scope:

- future pack tests

Tasks:

- test pack registration
- test pack enablement/disablement if supported
- test that pack behavior does not leak when unregistered

Dependencies:

- G1

Acceptance criteria:

- pack loading behavior is explicit and testable

### H4. Add Transitional Deletion Checklist Tests

Scope:

- docs and tests

Tasks:

- identify which tests prove migration scaffolding only
- replace them with end-state contract tests before deleting transitional code

Dependencies:

- E4
- G2

Acceptance criteria:

- code deletion is gated by end-state contract coverage

## Epic I. Removal And Cleanup

### I1. Retire Duplicate Entry Points

Scope:

- obsolete evaluator/runtime surfaces after migration

Tasks:

- remove or deprecate duplicate evaluation APIs
- remove compatibility aliases that only existed for migration

Dependencies:

- E4
- H4

Acceptance criteria:

- there is one primary kernel evaluation entry architecture

### I2. Clean Docs And Naming

Scope:

- repo-wide docs

Tasks:

- remove transitional wording once the kernel path is real
- update diagrams and target descriptions
- document pack architecture as the default growth model

Dependencies:

- I1

Acceptance criteria:

- docs describe the implemented architecture rather than the old split

## Recommended Sprint Breakdown

### Sprint 1

Target:

- freeze direction and remove ambiguity

Items:

- A1
- A2
- A3
- B1

### Sprint 2

Target:

- establish real kernel boundary in build and docs

Items:

- B2
- B3
- B4
- C1
- D1

### Sprint 3

Target:

- unify contracts before evaluator collapse

Items:

- C2
- C3
- C4
- D2
- E1
- H1

### Sprint 4

Target:

- collapse SDK runtime into kernel path

Items:

- D3
- E2
- E3
- E4
- F3

### Sprint 5

Target:

- shift extensibility to symbols and registration

Items:

- F1
- F2
- H2

### Sprint 6

Target:

- extract first packs

Items:

- G1
- G2
- G3
- G4
- H3

### Sprint 7

Target:

- remove transitional code and finalize naming

Items:

- H4
- I1
- I2

## Critical Path

The critical path is:

1. A2
2. B1
3. C1
4. C2
5. C3
6. D2
7. E1
8. E2
9. E3
10. E4
11. F1
12. F2
13. G1
14. G2
15. I1

If this path stalls, the architecture stalls.

## Stop-Doing List

Until at least M3 is complete, avoid:

- adding SDK-only evaluator semantics
- adding symbolic-only equivalents of SDK runtime features without a kernel
  plan
- expanding algebra through direct evaluator branches
- starting signal-processing or electrical-engineering code directly in the
  evaluator
- introducing third representations that compete with `Expr` and `ir::Node`

## Definition Of Done

This backlog is done only when:

- `aleph3_kernel` is the single semantic core
- `aleph3_sdk` consumes the kernel instead of owning a peer evaluator
- built-ins and domain math register through kernel contracts
- algebra and future verticals have pack-style growth paths
- duplicate transitional evaluator logic has been removed
