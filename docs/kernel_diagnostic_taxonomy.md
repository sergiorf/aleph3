# Kernel Diagnostic Taxonomy

## Purpose

This document defines the first shared kernel-level taxonomy for evaluation and
resolution failures.

Related documents:

- [Kernel Refactor Urgent Plan](kernel_refactor_urgent_plan.md)
- [Kernel Refactor Backlog](kernel_refactor_backlog.md)
- [Kernel Evaluation Context](kernel_evaluation_context.md)

Primary implementation:

- [include/kernel/Diagnostics.hpp](/home/sergio/dev/aleph3/include/kernel/Diagnostics.hpp)

## Problem

Aleph3 currently has at least two error surfaces:

- symbolic evaluator exceptions via `EvaluatorErrorKind`
- SDK/runtime `RuntimeError.code` strings such as `runtime.type_mismatch`

Those surfaces were created independently.
They are useful, but they are not yet a shared kernel contract.

## Taxonomy Rule

The kernel should own the canonical error taxonomy.

Current canonical layer:

- `aleph3::kernel::ErrorCode`

Current projection layers:

- symbolic evaluator exception kinds map into `kernel::ErrorCode`
- SDK/runtime `RuntimeError.code` strings are projections of
  `kernel::ErrorCode`

## Current Canonical Categories

The initial kernel taxonomy currently covers:

- structural form failures
- arity failures
- domain violations
- unsupported constructs
- internal inconsistencies
- unknown binding and unknown function failures
- type mismatches
- numeric domain and numeric result failures
- division by zero
- invalid host call and invalid host result failures
- step budget exhaustion
- empty or unsupported runtime node/input states
- unsupported operator states

## Current Projection Strategy

The kernel taxonomy does not yet replace public SDK/runtime codes.

Instead:

- `kernel::kernel_error_code_name(...)` gives the canonical kernel code string
- `kernel::sdk_runtime_projection_code(...)` gives the current SDK/runtime code
  string
- `kernel::make_runtime_error(...)` builds SDK/runtime-facing errors from the
  kernel taxonomy

This preserves existing SDK/runtime test expectations while establishing one
canonical error vocabulary underneath.

## Symbolic Evaluator Mapping

`EvaluatorErrorKind` now maps into the kernel taxonomy.

Current mappings:

- `invalid_form` -> `kernel.invalid_form`
- `invalid_arity` -> `kernel.invalid_arity`
- `domain_violation` -> `kernel.domain_violation`
- `unsupported_construct` -> `kernel.unsupported_construct`
- `internal_inconsistency` -> `kernel.internal_inconsistency`

## SDK Runtime Mapping

Examples of current runtime projections:

- `kernel.type_mismatch` -> `runtime.type_mismatch`
- `kernel.unknown_binding` -> `runtime.unknown_binding`
- `kernel.invalid_numeric_domain` -> `runtime.invalid_numeric_domain`
- `kernel.invalid_host_result` -> `runtime.invalid_host_result`

## Current Non-Goals

This step does not yet:

- unify parser and validator diagnostics into the same type
- replace all raw runtime string literals immediately
- turn symbolic evaluator exceptions into structured non-exception results
- rename the SDK/runtime public error codes away from `runtime.*`

## Next Follow-On Steps

1. replace more runtime string construction with kernel projection helpers
2. define how validator/frontend diagnostics relate to the kernel taxonomy
3. use the kernel taxonomy when routing SDK and symbolic evaluation through
   more shared execution paths
