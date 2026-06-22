# Aleph3 Embedded Formula Engine Controlled Rewrite Plan

## Goal

This plan assumes the current engine may be conceptually useful but is not yet
trustworthy enough to serve as the foundation of a commercial embedded product.

The goal is not to incrementally harden the entire current codebase. The goal
is to build a small, reliable embedded formula engine for native applications,
while salvaging only those pieces of the current repository that earn their
place through clarity and verification.

Product target:

`A trustworthy, embeddable formula engine for native applications with a stable
SDK, structured diagnostics, host-controlled validation, and bounded local
evaluation.`

## Rewrite Strategy

This is a controlled rewrite, not a blind rewrite.

That means:

- The current repo remains a reference implementation and test quarry.
- New product-critical code is built behind a new SDK-oriented architecture.
- Old code is reused only when it is simple, isolated, and easy to verify.
- Compatibility with current internal behavior is not a goal by default.

The rewrite is controlled because it preserves:

- product direction
- useful docs
- any validated tests
- any small reusable utility code

It rejects:

- coupling to questionable internals
- global mutable engine state
- CLI-first architecture
- unclear semantics preserved only for backward compatibility

## Core Decision

The new engine should be rebuilt around the embedded use case only.

That means the first trustworthy engine should support only:

- literals: numbers, booleans, strings
- variables
- arithmetic: `+`, `-`, `*`, `/`, `^`
- comparisons: `==`, `!=`, `<`, `<=`, `>`, `>=`
- function calls
- `If`
- host-defined functions
- schema validation
- structured diagnostics
- bounded evaluation

Everything else should be excluded from the first trusted product core:

- user-defined functions
- assignments
- rules
- symbolic transforms beyond a narrow, explicit subset
- polynomial toolchain
- broad string/list semantics unless directly needed
- full Mathematica-like language behavior

## What To Keep, Archive, Rewrite

## Keep Immediately

These assets are useful now:

- [docs/embedded_formula_engine_design.md](/home/sergio/dev/aleph3/docs/embedded_formula_engine_design.md)
- this implementation plan
- the existing CMake-based repo layout
- some expression ideas from [include/expr/Expr.hpp](/home/sergio/dev/aleph3/include/expr/Expr.hpp)
- the current tests as reference material, not as binding truth

## Archive As Pre-SDK Symbolic Engine

These should be treated as outside the trusted SDK path unless later proven worth salvaging:

- [include/parser/Parser.hpp](/home/sergio/dev/aleph3/include/parser/Parser.hpp)
- [include/evaluator/Evaluator.hpp](/home/sergio/dev/aleph3/include/evaluator/Evaluator.hpp)
- [src/evaluator/Evaluator.cpp](/home/sergio/dev/aleph3/src/evaluator/Evaluator.cpp)
- [include/evaluator/FunctionRegistry.hpp](/home/sergio/dev/aleph3/include/evaluator/FunctionRegistry.hpp)
- current transform and polynomial paths for the v1 embedded core

Archive here does not mean delete immediately. It means:

- not the default implementation path
- not part of the trusted public surface
- only mined for ideas, tests, or isolated utilities

## Rewrite First

These areas should be rebuilt as new product code:

- SDK facade
- public value model
- diagnostics model
- schema validation
- parser for the trusted subset
- evaluator for the trusted subset
- host function registration model
- policy and resource limits

## Proposed New Architecture

The new implementation should use a clean split between trusted product code
and the broader symbolic engine code.

### New Top-Level Layers

1. `frontend`
   Parsing, tokens, source spans, diagnostics.

2. `ir`
   Minimal trusted AST for the embedded subset.

3. `semantics`
   Validation, schema checks, feature gating, type rules.

4. `runtime`
   Evaluation, value model, host function dispatch, budgets.

5. `sdk`
   Public API consumed by host applications.

6. `tooling`
   CLI, examples, test harnesses, benchmarks.

The key point is that the trusted v1 product should not depend on the old AST
or evaluator model.

## Suggested Repo Structure

```text
include/
  sdk/
    Engine.hpp
    Types.hpp
    Schema.hpp
    Policy.hpp
  frontend/
    Diagnostic.hpp
    Token.hpp
    Lexer.hpp
    Parser.hpp
  ir/
    Node.hpp
  semantics/
    Validator.hpp
  runtime/
    Value.hpp
    Evaluator.hpp
    HostFunction.hpp

src/
  sdk/
    Engine.cpp
  frontend/
    Lexer.cpp
    Parser.cpp
  ir/
  semantics/
    Validator.cpp
  runtime/
    Evaluator.cpp
    HostFunction.cpp

tests/
  sdk/
  frontend/
  semantics/
  runtime/

legacy/
  parser/
  evaluator/
  transforms/
  algebra/
```

Note:

You do not need to physically move all current code into `legacy/` on day one.
But the plan should behave as if the old engine is legacy unless explicitly
promoted back into the trusted path.

## Rewrite Principles

### Principle 1: Design Public API First

Do not let current internals shape the trusted product boundary.

### Principle 2: Build Only the Narrow Product Core

The first commercial wedge does not need a broad CAS.

### Principle 3: Tests Define Truth

Not old behavior. Not intuition. Not current output strings.

### Principle 4: Every Feature Must Justify Itself

If a feature does not help the embedded formula-engine wedge, defer it.

### Principle 5: Prefer Opaque Product Types

Host applications should not see internal AST nodes.

## Functionality Unlocked By Each Milestone

This section is the reason for the sequencing.

### After R0

Unblocks:

- knowing what the current repo is good for as reference
- deciding what to salvage versus ignore
- setting correct scope for the rewrite

### After R1

Unblocks:

- a minimal compilable SDK surface
- host-side proof-of-concept integration
- freedom to keep changing internals behind the new boundary

### After R2

Unblocks:

- first trustworthy formula execution for a constrained feature subset
- deterministic evaluation demos
- realistic early design-partner prototypes

### After R3

Unblocks:

- save-time validation in a real host product
- user-authored formulas with diagnostics
- schema-controlled function and variable access

### After R4

Unblocks:

- safer production embedding
- bounded evaluation of untrusted formulas
- multi-instance embedding without global state conflicts

### After R5

Unblocks:

- external adoption through a documented SDK
- customer pilot integrations
- commercial packaging discussions

## Milestone Overview

## R0: Triage and Salvage Decision

Objective:

Stop treating the current engine as a foundation and classify it as:

- reusable
- reference-only
- discard

Expected duration:

1 week

Deliverables:

- current engine audit
- contract-test matrix for the new engine
- feature support classification
- rewrite scope confirmation

Functionality unblocked:

- evidence-based rewrite decisions

## R1: New SDK Boundary and Minimal IR

Objective:

Define the product boundary and minimal internal representation for the new
engine.

Expected duration:

1 to 2 weeks

Deliverables:

- public SDK headers
- `Engine` facade
- `Value`, `Diagnostic`, `Schema`, and `Policy` public types
- minimal IR for literals, variables, calls, arithmetic, comparisons, `If`

Functionality unblocked:

- basic embedding with a stable conceptual API

## R2: Trusted Subset Parser and Evaluator

Objective:

Implement a fresh parser and evaluator for the narrow embedded subset.

Expected duration:

2 to 3 weeks

Deliverables:

- lexer
- parser with source spans
- runtime evaluator
- host function dispatch
- deterministic evaluation tests

Functionality unblocked:

- first trustworthy compile and evaluate flow

## R3: Validation and Diagnostics

Objective:

Add schema-driven validation and structured diagnostics around the trusted core.

Expected duration:

2 weeks

Deliverables:

- validator
- symbol and function allowlists
- arity checks
- machine-readable diagnostics

Functionality unblocked:

- formula editors and pre-save validation

## R4: Safety and Isolation

Objective:

Make the new engine safe for production embedding.

Expected duration:

2 weeks

Deliverables:

- engine-scoped host function registry
- AST complexity analysis
- evaluation budgets
- policy profiles
- threading model docs

Functionality unblocked:

- bounded execution for semi-trusted user formulas

## R5: Packaging and Adoption

Objective:

Make the rewrite adoptable by external engineering teams.

Expected duration:

2 weeks

Deliverables:

- example host application
- quickstart docs
- install/export CMake support
- benchmark harness
- release policy draft

Functionality unblocked:

- customer pilots and easier third-party evaluation

## Concrete Tasks By Milestone

## R0: Triage and Salvage Decision

### Task R0.1: Create Current Engine Audit

Files to add:

- `docs/current_engine_audit.md`

Contents:

- what appears to work
- what appears brittle
- what is irrelevant to the embedded v1
- what may be salvageable later

Acceptance criteria:

- each major current subsystem is classified:
  - salvage now
  - salvage later
  - reference only
  - discard

### Task R0.2: Create New Engine Contract Matrix

Files to add:

- `docs/contract_test_matrix.md`

Define contracts for the rewrite:

- compile valid formula
- reject invalid syntax with source diagnostics
- validate against a schema
- evaluate deterministic results
- respect function allowlists
- respect resource budgets

Acceptance criteria:

- all rewrite work can be mapped to explicit contracts

### Task R0.3: Define Trusted Feature Subset

Files to add:

- `docs/trusted_subset_v1.md`

Contents:

- exact syntax supported
- exact runtime features supported
- explicitly unsupported language features

Acceptance criteria:

- there is no ambiguity about the rewrite scope

## R1: New SDK Boundary and Minimal IR

### Task R1.1: Create New Public SDK Headers

Files to add:

- `include/sdk/Engine.hpp`
- `include/sdk/Types.hpp`
- `include/sdk/Schema.hpp`
- `include/sdk/Policy.hpp`

Acceptance criteria:

- host-facing API compiles independently of symbolic-engine headers

### Task R1.2: Create Minimal IR

Files to add:

- `include/ir/Node.hpp`
- `src/ir/Node.cpp` only if needed

IR should support:

- literals
- variables
- unary and binary operators
- function calls
- `If`

Acceptance criteria:

- IR is smaller and narrower than the legacy `Expr`

### Task R1.3: Create SDK Engine Stub

Files to add:

- `src/sdk/Engine.cpp`

Behavior:

- expose compile, validate, and evaluate entry points
- return "not yet implemented" structured diagnostics where appropriate

Acceptance criteria:

- external code can link against the SDK surface before full implementation

Current status:

- completed and extended beyond a pure stub
- `validate` now runs parser + validator
- `compile` now returns reusable opaque `CompiledFormula` handles on success

### Task R1.4: Split Build Targets Around SDK And Symbolic Engine

Files:

- `CMakeLists.txt`

Changes:

- add new targets for the SDK path
- keep symbolic-engine target separate
- allow both to coexist during transition

Suggested targets:

- `aleph3_runtime`
- `aleph3_sdk`
- `aleph3_symbolic`

Acceptance criteria:

- SDK path can be built without relying on symbolic-engine internals

## R2: Trusted Subset Parser and Evaluator

### Task R2.1: Implement Lexer

Files to add:

- `include/frontend/Token.hpp`
- `include/frontend/Lexer.hpp`
- `src/frontend/Lexer.cpp`

Acceptance criteria:

- token stream includes source spans

### Task R2.2: Implement Parser

Files to add:

- `include/frontend/Parser.hpp`
- `src/frontend/Parser.cpp`

Acceptance criteria:

- parser builds minimal IR
- parser emits structured syntax diagnostics

### Task R2.3: Implement Runtime Value Model

Files to add:

- `include/runtime/Value.hpp`
- `src/runtime/Value.cpp` if needed

Acceptance criteria:

- runtime values are separate from IR and SDK diagnostics

### Task R2.4: Implement Evaluator

Files to add:

- `include/runtime/Evaluator.hpp`
- `src/runtime/Evaluator.cpp`

Acceptance criteria:

- supports trusted subset only
- no global function registry
- deterministic result tests pass

Current status:

- implemented as a trusted-subset tree interpreter
- covers literals, bindings, unary/binary operators, comparisons, `If`, and
  basic function dispatch
- runtime still needs richer binding/CLI support, but registered host functions
  now enforce argument and return contracts

### Task R2.5: Implement Host Function Dispatch

Files to add:

- `include/runtime/HostFunction.hpp`
- `src/runtime/HostFunction.cpp`

Acceptance criteria:

- host can register per-engine functions with arity metadata

Current status:

- engine-scoped host functions are callable during runtime evaluation
- registration now validates arity/parameter metadata consistency, and runtime
  checks host argument and return contracts
- broader registration ergonomics remain future work

### Task R2.6: Add Trusted-Core Tests

Files to add:

- `tests/frontend/`
- `tests/runtime/`
- `tests/sdk/TrustedCoreTests.cpp`

Acceptance criteria:

- tests cover parser, evaluator, and host functions for the minimal subset

### Task R2.7: Add Thin SDK Tooling CLI

Files to add:

- `src/tooling/aleph3_cli.cpp`

Behavior:

- consume the SDK and frontend as a developer test harness
- expose `tokens`, `parse`, `validate`, `compile`, `evaluate`, and host-function demo commands
- print structured diagnostics for manual debugging

Acceptance criteria:

- the SDK path can be exercised from the terminal without relying on a separate symbolic-engine CLI
- the CLI does not depend on symbolic parser or evaluator internals

Current status:

- implemented with REPL, history/editing support, command completion, help/examples,
  trusted-subset `evaluate`, and demo host-function evaluation
- `evaluate` now accepts `--var name=value` bindings so variable-driven formulas
  can be exercised without writing host code first
- `evaluate-host` registers a fixed demo host-function bundle, and
  `aleph3_sdk_example` provides a compiled embedding reference

## R3: Validation and Diagnostics

### Task R3.1: Implement Diagnostic Types

Files to add:

- `include/frontend/Diagnostic.hpp`

Acceptance criteria:

- diagnostics are machine-readable and include codes and source spans

### Task R3.2: Implement Schema Model

Files:

- `include/sdk/Schema.hpp`
- `src/sdk/Schema.cpp`

Schema should support:

- allowed variables
- allowed built-ins
- allowed host functions
- enabled features

Acceptance criteria:

- host can express a constrained execution environment

### Task R3.3: Implement Validator

Files to add:

- `include/semantics/Validator.hpp`
- `src/semantics/Validator.cpp`

Acceptance criteria:

- validator traverses minimal IR
- detects unknown variables, unknown functions, disallowed features

Current status:

- first pass implemented for schema allowlists, known arity checks, basic
  feature gates, AST node/depth limits, obvious type mismatches, incompatible
  `If` branch shapes, constant-condition branch pruning, schema-valued
  constants, constant zero-denominator checks, and host-function return-type
  composition checks
- deeper type reasoning, flow-sensitive inference beyond current constant
  pruning/schema-constant/runtime-trap checks, and richer runtime-facing
  validation still remain future work

### Task R3.4: Add Validation Tests

Files to add:

- `tests/semantics/ValidationTests.cpp`

Acceptance criteria:

- schema validation works independently of runtime evaluation

## R4: Safety and Isolation

### Task R4.1: Implement Policy Object

Files:

- `include/sdk/Policy.hpp`
- `src/sdk/Policy.cpp`

Policy should include:

- max AST depth
- max node count
- max evaluation steps
- feature flags

Acceptance criteria:

- engine can be configured for bounded evaluation

### Task R4.2: Add AST Complexity Analyzer

Files to add:

- `include/semantics/AstAnalysis.hpp`
- `src/semantics/AstAnalysis.cpp`

Acceptance criteria:

- formulas can be rejected before runtime if too complex

### Task R4.3: Add Evaluation Budgeting

Files:

- runtime evaluator files

Acceptance criteria:

- evaluation aborts deterministically when budget is exceeded

### Task R4.4: Add Engine-Scoped Function Registry

Files:

- runtime and sdk files

Acceptance criteria:

- separate engine instances have separate function sets

### Task R4.5: Document Threading Model

Files to add:

- `docs/sdk_threading_model.md`

Acceptance criteria:

- thread-safety guarantees are explicit

## R5: Packaging and Adoption

### Task R5.1: Add Example Host App

Files to add:

- `examples/embedded_console_app/CMakeLists.txt`
- `examples/embedded_console_app/main.cpp`

Acceptance criteria:

- example uses only public SDK

### Task R5.2: Add SDK Quickstart

Files to add:

- `docs/sdk_quickstart.md`

Acceptance criteria:

- user can integrate and validate a formula quickly

### Task R5.3: Add Install and Export Support

Files:

- `CMakeLists.txt`
- `cmake/` support files as needed

Acceptance criteria:

- external project can consume Aleph3 as a package

### Task R5.4: Add Benchmarks

Files to add:

- `benchmarks/`

Acceptance criteria:

- parse/evaluate performance is measurable

### Task R5.5: Add Release Policy

Files to add:

- `docs/release_policy.md`

Acceptance criteria:

- versioning and compatibility expectations are written down

## Immediate Backlog

### Priority 0

1. Create `docs/current_engine_audit.md`.
2. Create `docs/contract_test_matrix.md`.
3. Create `docs/trusted_subset_v1.md`.
4. Add new SDK headers under `include/sdk`.
5. Add new IR under `include/ir`.
6. Split CMake so rewrite and legacy can coexist.

### Priority 1

1. Implement new lexer.
2. Implement new parser.
3. Add thin rewrite tooling CLI.
4. Implement new runtime value model.
5. Implement new evaluator.
6. Add trusted-core tests.

### Priority 2

1. Implement schema model.
2. Implement validator.
3. Implement structured diagnostics end-to-end.
4. Add validation tests.

### Priority 3

1. Implement policy object.
2. Add complexity analysis.
3. Add evaluation budgets.
4. Finalize engine-scoped host function registration.

### Priority 4

1. Add example host app.
2. Add quickstart docs.
3. Add install/export support.
4. Add benchmarks.
5. Add release policy.

## Definition Of Success

The controlled rewrite is successful when:

- the trusted engine path no longer depends on symbolic parser/evaluator code
- a host app can compile, validate, and evaluate formulas through the SDK only
- diagnostics are structured and source-aware
- execution is bounded and engine-scoped
- the supported feature subset is explicit and tested

## Definition Of Failure

The rewrite is failing if:

- more and more symbolic-engine internals leak into the new SDK path
- unsupported language features keep being added to "help compatibility"
- tests merely mirror legacy behavior instead of asserting product contracts
- the trusted subset keeps expanding before becoming stable

## Status Snapshot

The initial rewrite kickoff work described below has largely been completed in
this repository state:

- the trusted subset and contract matrix documents exist
- the SDK, IR, runtime, and tooling layers are present
- the SDK path compiles, validates, and evaluates the trusted subset
- the primary remaining work is hardening, packaging, and contract cleanup

The first four-week plan remains useful as historical sequencing, but it should
no longer be read as the live task list for this branch.

## Original First 4 Weeks

### Week 1

- write `current_engine_audit.md`
- write `contract_test_matrix.md`
- write `trusted_subset_v1.md`
- split build targets between legacy and SDK paths

Functionality unblocked:

- clear rewrite scope and clean technical decision-making

### Week 2

- add public SDK headers
- add minimal IR
- add engine stub
- add first SDK compile tests

Functionality unblocked:

- stable product boundary for the rewrite

### Week 3

- implement lexer and parser
- implement source spans and syntax diagnostics
- add parser tests
- add thin rewrite tooling CLI

Functionality unblocked:

- trustworthy syntax handling for the supported subset
- fast manual SDK/frontend checks from the terminal

### Week 4

- implement runtime value model
- implement evaluator
- implement host function registration
- add trusted-core runtime tests

Functionality unblocked:

- first working embedded engine demo

## Recommendation

The controlled rewrite is now underway and the SDK path is the primary product
path in this branch.

Do not spend the next phase trying to prove the old engine is good enough for a
commercial embedded product. Treat it as reference material and keep moving the
product-critical path onto the smaller, cleaner, explicitly trusted engine.

The next phase should focus on:

- finishing numeric-contract hardening and keeping its tests/docs explicit
- keeping the new SDK-grade numeric built-in tier explicit, bounded, and well-tested
- reducing ad hoc constant reasoning into a clearer folding boundary
- tightening product-facing docs, examples, and target naming around the SDK
- leaving symbolic-engine code available for reference without letting it shape the
  trusted public surface

For math-core direction, see [docs/sdk/math_core_audit.md](/home/sergio/dev/aleph3/docs/sdk/math_core_audit.md).
The near-term recommendation there is to harden the numeric contract and add a
small commercially useful numeric built-in tier before revisiting broader
algebra ambitions.

After that core work, the roadmap should explicitly reopen engine-expansion
work as a separate track, not as scope creep inside trusted-subset v1.

Post-core engine expansion candidates:

- a broader built-in function set with explicit contracts, tests, and policy
  controls
- a better polynomial layer, including clearer normal forms and more reliable
  symbolic polynomial operations
- symbolic differentiation support with narrow, documented rules before any
  broad CAS ambitions
- symbolic or partially symbolic integration only after derivative and
  polynomial semantics are trustworthy enough to support it

These should be treated as deliberate future milestones, not as assumptions of
the current trusted subset.

The win condition is not "Aleph3 still supports everything it used to." The win
condition is "Aleph3 has a small embedded core that is trustworthy enough to
sell."
