# Contract Test Matrix

## Purpose

This document defines the contracts the embedded engine must satisfy.

It is the authoritative answer to:

`What behavior is the engine required to preserve, and what behavior is it
free to ignore?`

These contracts are product contracts. Current engine behavior matters only if
it supports one of these contracts.

## Scope

This matrix applies to the trusted embedded v1 subset. It does not require
compatibility with the full current language surface.

The matrix is organized around the host product lifecycle:

1. configure engine
2. compile formula
3. validate formula
4. evaluate formula
5. bound behavior under policy
6. integrate and ship with confidence

## Contract Levels

Each contract is assigned a level:

- `P0`: required for the first trustworthy engine
- `P1`: required before pilot customer use
- `P2`: desirable after the trusted core is stable

## Contract Format

Each contract includes:

- ID
- level
- guarantee
- test type
- examples
- explicit non-goals

## Contract Matrix

## C1. Engine Construction

### C1.1 Engine Can Be Constructed With Explicit Options

Level:

- `P0`

Guarantee:

- A host application can construct an engine instance with explicit schema and
  policy configuration inputs.

Test type:

- SDK unit tests

Examples:

- construct default engine
- construct engine with strict function allowlist
- construct engine with evaluation budget

Non-goals:

- backward compatibility with superseded global registration behavior

## C2. Syntax Compilation

### C2.1 Valid Trusted-Subset Formula Compiles

Level:

- `P0`

Guarantee:

- A syntactically valid formula in the trusted subset compiles successfully.

Test type:

- frontend unit tests
- SDK compile tests

Examples:

- `x + 1`
- `If[x > 0, x, 0]`
- `Clamp[x, 0, 1]` with host function schema support

Non-goals:

- compatibility with unsupported syntax such as assignments, rules, or
  user-defined functions

### C2.2 Invalid Syntax Produces Structured Diagnostics

Level:

- `P0`

Guarantee:

- Invalid syntax does not crash or leak raw parser exceptions through the SDK.
- It returns one or more structured diagnostics with code and source location.

Test type:

- frontend negative tests
- SDK compile tests

Examples:

- unmatched `)`
- unterminated string
- malformed function call
- incomplete `If`

Non-goals:

- preserving old exception message strings

### C2.3 Diagnostics Include Source Spans

Level:

- `P0`

Guarantee:

- Compile-time syntax diagnostics include at least line and column.

Test type:

- frontend diagnostics tests

Examples:

- syntax error points to the unexpected token

Non-goals:

- perfect recovery or multi-error parsing in v1

## C3. Validation

### C3.1 Unknown Variables Are Rejected By Validation

Level:

- `P0`

Guarantee:

- Validation fails when a formula references variables not allowed by the
  schema.

Test type:

- semantics tests
- SDK validation tests

Examples:

- schema allows `x` and `y`
- formula uses `z`
- validation returns diagnostic `validation.unknown_variable`

Non-goals:

- implicit global symbol creation

### C3.2 Unknown Functions Are Rejected By Validation

Level:

- `P0`

Guarantee:

- Validation fails when a function call is not allowed by built-in or host
  function schema.

Test type:

- semantics tests

Examples:

- `Foo[x]` when `Foo` is not registered or allowed

Non-goals:

- best-effort fallback to symbolic unevaluated calls

### C3.3 Disallowed Features Are Rejected

Level:

- `P0`

Guarantee:

- Validation rejects syntax or AST features outside the trusted subset or
  outside the configured policy.

Test type:

- semantics tests

Examples:

- assignments
- user-defined functions
- rules

Non-goals:

- carrying forward unsupported legacy constructs for convenience

### C3.4 Arity Validation Is Enforced

Level:

- `P1`

Guarantee:

- Validation rejects function calls with wrong arity when arity is known.

Test type:

- semantics tests

Examples:

- `If[x]`
- `Clamp[x, 0]`

Non-goals:

- advanced type inference across all function forms in v1

## C4. Evaluation

### C4.1 Deterministic Numeric Evaluation

Level:

- `P0`

Guarantee:

- The same compiled formula, bindings, engine version, and policy produce the
  same result deterministically.

Test type:

- runtime unit tests
- SDK evaluation tests

Examples:

- `x + 1` with `x = 2` evaluates to `3`
- `If[x > 0, x, 0]` behaves predictably

Non-goals:

- exact replication of all legacy simplification decisions

### C4.2 Runtime Inputs Are Explicit

Level:

- `P0`

Guarantee:

- Evaluation depends only on explicit host-provided bindings and engine
  configuration.

Test type:

- runtime tests

Examples:

- formula cannot implicitly read ambient variables not provided in bindings

Non-goals:

- mutable session-style global variables

### C4.3 Host Functions Execute Through Engine-Scoped Registration

Level:

- `P0`

Guarantee:

- Host-defined functions are registered per engine instance and invoked through
  explicit metadata.

Test type:

- runtime tests
- SDK integration tests

Examples:

- engine A registers `Clamp`
- engine B does not
- formula compiles or validates differently per engine/schema

Non-goals:

- process-global registration

### C4.4 Unsupported Runtime Operations Return Structured Errors

Level:

- `P1`

Guarantee:

- Runtime failures are surfaced as structured runtime errors, not raw C++
  exceptions through the SDK.

Test type:

- runtime negative tests

Examples:

- division by zero policy failure
- budget exhaustion
- invalid host function result

Non-goals:

- preserving current exception text

## C5. Policy and Safety

### C5.1 AST Complexity Limits Are Enforced

Level:

- `P1`

Guarantee:

- Validation or compile rejects formulas exceeding configured complexity
  thresholds.

Test type:

- semantics tests

Examples:

- too-deep nesting
- too many nodes

Non-goals:

- accepting arbitrarily large formulas and relying only on runtime limits

### C5.2 Evaluation Budget Is Enforced

Level:

- `P1`

Guarantee:

- Evaluation stops deterministically when the configured budget is exceeded.

Test type:

- runtime negative tests

Examples:

- repeated nested calls
- expensive recursive-like evaluation forms if later supported

Non-goals:

- unbounded runtime on host-supplied formulas

### C5.3 Engine Instances Are Isolated

Level:

- `P1`

Guarantee:

- Engine state such as registered functions and policies is instance-scoped.

Test type:

- SDK integration tests

Examples:

- two engines with different policies behave independently

Non-goals:

- singleton-backed shared mutable engine behavior

## C6. SDK Boundary

### C6.1 Public SDK Does Not Expose Internal AST

Level:

- `P0`

Guarantee:

- A host application can compile, validate, and evaluate formulas without
  including or manipulating internal AST types.

Test type:

- SDK-only compile tests

Examples:

- example project includes only public SDK headers

Non-goals:

- reusing internal symbolic types directly in the host-facing API

### C6.2 Compiled Formula Is Reusable

Level:

- `P1`

Guarantee:

- A compiled formula can be reused for repeated evaluations with different
  bindings.

Test type:

- SDK evaluation tests

Examples:

- compile once, evaluate many with different `x`

Non-goals:

- reparsing on every evaluation through the public API

## C7. Product Integration

### C7.1 Minimal Example Integration Works

Level:

- `P1`

Guarantee:

- A sample host application can compile, validate, and evaluate a formula using
  only the public SDK.

Test type:

- example integration build test

Examples:

- console app with schema and host function

Non-goals:

- relying on internal headers in examples

### C7.2 Package Consumption Works

Level:

- `P2`

Guarantee:

- An external CMake project can consume Aleph3 as a package.

Test type:

- packaging integration test

Examples:

- `find_package(aleph3 CONFIG REQUIRED)`

Non-goals:

- source-tree-only integration as the only supported path

## Explicit Non-Contracts

The engine is explicitly not required to preserve the following:

- current CLI prompt behavior
- current pretty-print output strings
- current user-defined function semantics
- current assignment semantics
- current polynomial pipeline behavior
- broad Mathematica-like syntax outside the trusted subset
- current exception message text
- symbolic unevaluated fallback for unsupported forms

These are deliberately excluded because preserving them would make the engine
broader, riskier, and less aligned with the embedded product wedge.

## Current Test Reuse Guidance

The existing test suite should be mined selectively.

### Likely Reusable, With Adaptation

- basic parser arithmetic tests
- comparison tests
- simple conditional tests
- deterministic numeric evaluation tests

### Reusable Only As Reference

- user-defined function tests
- assignment tests
- expand and simplify tests
- polynomial tests
- broad built-in function coverage

### Likely Not Reused For v1 Contracts

- tests depending on symbolic fallback behavior
- tests depending on older formatting quirks
- tests centered on unsupported language constructs

## Minimum Test Suites Required

## Suite A: Frontend Compile Tests

Must cover:

- successful parse of trusted subset
- syntax errors with source locations
- malformed calls
- malformed `If`

## Suite B: Validation Tests

Must cover:

- allowed variables
- unknown variables
- allowed functions
- unknown functions
- disallowed features
- arity violations

## Suite C: Runtime Evaluation Tests

Must cover:

- arithmetic
- comparisons
- `If`
- host function dispatch
- deterministic repeated evaluation

## Suite D: Safety Tests

Must cover:

- AST depth limit
- AST node count limit
- evaluation budget exhaustion
- per-engine function isolation

## Suite E: SDK Boundary Tests

Must cover:

- SDK-only includes compile
- compile once evaluate many
- no internal AST dependency in example app

## Release Gate For Trustworthy v1

The new engine is not trustworthy enough for the embedded product until all
`P0` contracts pass and all of the following are true:

- parser diagnostics are structured
- validation is schema-driven
- evaluation is deterministic for supported forms
- host functions are engine-scoped
- SDK does not expose internal AST

The new engine is not ready for pilot customers until all `P1` contracts pass.

## Recommendation

Use this matrix as the acceptance boundary for the embedded engine.

If a current behavior is not represented here, it is not a required contract.
That is the discipline that prevents the product from collapsing back into
backward-compatibility work.
