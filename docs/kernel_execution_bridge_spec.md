# Kernel Execution Bridge Spec

## Status

Draft implementation spec referenced by the
[Aleph3 Unified Plan](aleph3_unified_plan.md).

## Purpose

This document will define how SDK trusted-subset execution converges onto the
kernel.

It should specify:

- how `ir::Node` maps or lowers into kernel execution inputs
- where the adapter lives
- how `Engine::evaluate` stops depending on `runtime::Evaluator`
- what transitional compatibility is acceptable during migration

## Scope

This spec should cover:

- bridge entrypoints
- ownership boundaries between SDK and kernel
- lowering or translation model
- runtime policy and diagnostic projection
- migration off `runtime::Evaluator`

## Required Sections

1. bridge architecture
2. `ir::Node` to kernel representation strategy
3. evaluation entrypoint used by `Engine`
4. policy and binding propagation
5. diagnostic projection back to SDK types
6. transitional adapter strategy
7. migration slices and tests

## Initial Design Questions

- whether to lower `ir::Node` directly into `Expr` or evaluate through a
  narrower kernel bridge form
- how much trusted-subset validation remains SDK-owned versus kernel-owned
- where host-function invocation should attach in the kernel path

## Acceptance Criteria

This spec is sufficient when:

- the removal path for `runtime::Evaluator` is explicit
- SDK execution can be redirected without inventing a second semantic model
