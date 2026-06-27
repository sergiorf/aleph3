# Kernel Evaluation Context

## Purpose

This document defines the first shared kernel evaluation context contract used
by the Aleph3 refactor.

Related documents:

- [Aleph3 Unified Plan](aleph3_unified_plan.md)
- [Kernel Representation Decision](kernel_representation_decision.md)
- [Layer Ownership Matrix](layer_ownership_matrix.md)

Primary implementation:

- [include/kernel/EvaluationContext.hpp](/home/sergio/dev/aleph3/include/kernel/EvaluationContext.hpp)

## Contract

The kernel evaluation context must be able to carry both:

- symbolic-kernel state
- embedded SDK/runtime state

The shared context now owns or references:

- symbolic symbol values
- symbolic user-defined function definitions
- runtime bindings
- runtime constants
- runtime host function registrations
- runtime policy

## Current Design

The current design uses one kernel-owned context type:

- `aleph3::kernel::EvaluationContext`

Compatibility aliases currently remain in place for migration:

- `aleph3::EvaluationContext` through `include/evaluator/EvaluationContext.hpp`

That means existing call sites can keep compiling while the repo converges on a
single context contract.

## Why This Matters

Before this change, the repo had two unrelated context models:

- symbolic evaluation context for symbol/function state
- SDK runtime context for bindings/constants/host functions/policy

That separation encouraged two execution systems.

The new contract makes it possible to express both execution modes through one
kernel-owned context shape before deeper semantic unification.

## Current Guarantees

- symbolic evaluation still has direct access to symbol tables and UDF storage
- runtime evaluation can read bindings, constants, host functions, and policy
  through the same kernel context object
- context copies preserve symbolic state and preserve the referenced or owned
  runtime sources
- SDK-facing code can use the kernel-owned context directly through the bridge
  and evaluator layers during migration

## Current Non-Goals

This contract does not yet mean:

- diagnostics are unified everywhere
- registration is unified everywhere
- the lowering boundary is fully simplified

Those are later phases.

## Next Follow-On Steps

1. define the kernel diagnostic taxonomy
2. define the kernel registration interfaces
3. begin routing runtime and symbolic evaluation through shared kernel
   contracts instead of only shared storage shape
