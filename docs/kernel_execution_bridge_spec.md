# Kernel Execution Bridge Spec

## Status

Implemented execution contract referenced by the
[Aleph3 Unified Plan](aleph3_unified_plan.md).

## Purpose

The trusted-subset SDK path now executes through one explicit kernel bridge:

- `kernel::stage_trusted_subset_formula`
- `kernel::evaluate_trusted_subset_formula`

This keeps the SDK facade thin while making kernel lowering, execution
configuration, and runtime-error projection explicit.

## Current Contract

### Staging

`stage_trusted_subset_formula(const ir::NodePtr&)` accepts trusted-subset IR
that has already passed SDK parsing and validation.

It returns:

- the lowered kernel `Expr`
- any lowering diagnostics

The IR remains an SDK-owned frontend artifact. The lowered kernel expression is
the execution input.

### Evaluation

`evaluate_trusted_subset_formula(...)` is the canonical execution adapter for
trusted-subset SDK formulas.

It owns:

- builtin registration bootstrap
- construction of `kernel::EvaluationContext`
- propagation of SDK bindings, constants, host functions, and policy
- strict runtime semantics enablement
- symbol seeding for SDK values
- projection of kernel failures back to `EvaluationResult`

`Engine::evaluate` should only gather engine-scoped host functions and delegate
to this entrypoint.

## Ownership Boundary

SDK owns:

- parsing
- validation
- schema and policy authoring
- the public `Engine` facade

Kernel owns:

- lowered execution form
- evaluation context
- runtime semantics
- host-function invocation during kernel evaluation
- runtime diagnostic taxonomy and SDK projection

## Transitional Rules

Accepted transitional state:

- SDK-facing results continue using `EvaluationResult`, `RuntimeError`, and
  `Value`

Rejected transitional state:

- `Engine` carrying its own evaluator semantics
- a second SDK-only execution path beside kernel evaluation
- duplicate runtime-error projection logic outside the kernel bridge

## Tests Required

The bridge contract is covered when tests prove:

- staging lowers trusted-subset IR into kernel expressions
- bridge evaluation honors SDK bindings and schema constants
- runtime failures project to SDK runtime error codes
- registered host functions execute through the kernel path
- SDK evaluation agrees with direct kernel evaluation on trusted-subset
  results and failure codes
- `Engine::evaluate` remains a thin delegation layer

## Next Cleanup

The bridge is explicit now, but the remaining migration cleanup is still:

- keep bridge tests focused on kernel semantics rather than facade behavior
- remove stale docs that still imply two long-term execution cores
