# Kernel Registration Interfaces

## Purpose

This document defines the first shared registration interface for kernel
functions during the Aleph3 refactor.

Related documents:

- [Kernel Refactor Urgent Plan](kernel_refactor_urgent_plan.md)
- [Kernel Refactor Backlog](kernel_refactor_backlog.md)
- [Kernel Evaluation Context](kernel_evaluation_context.md)
- [Kernel Diagnostic Taxonomy](kernel_diagnostic_taxonomy.md)

Primary implementation:

- [include/kernel/FunctionRegistry.hpp](/home/sergio/dev/aleph3/include/kernel/FunctionRegistry.hpp)

## Problem

The repository currently has multiple extension paths:

- symbolic built-ins registered through the pack registry
- symbolic pack functions looked up through the same registry
- SDK host functions registered through `Engine::register_function`

Those paths serve different execution tracks, but they should not live as
unrelated registry concepts forever.

## Current Shared Contract

The kernel now owns the shared registration surface:

- `aleph3::kernel::FunctionRegistry`

It currently provides:

- symbolic function registration and lookup
- host function map registration helpers
- host function map lookup helpers

## Current Projection Layers

Compatibility aliases remain during migration:

- `aleph3::packs::PackRegistry` aliases the kernel registry
- `aleph3::FunctionRegistry` aliases the kernel registry

This keeps existing symbolic call sites compiling while moving the ownership
boundary into the kernel.

## Current Rules

- symbolic built-ins and pack-style symbolic functions should register through
  the kernel registry
- SDK host functions should register through kernel registry helpers even if
  they still live in engine-owned maps today
- new extension paths should not create additional registry concepts outside
  the kernel

## Current Non-Goals

This step does not yet mean:

- there is one evaluator for `Expr` and `ir::Node`
- user-defined functions are registered through the same API as built-ins
- pack lifecycle, enablement, or unloading is fully defined
- host functions are stored globally

Those are later steps.

## Next Follow-On Steps

1. define symbol definition precedence across built-ins, user definitions, and
   packs
2. push more evaluator dispatch through the kernel registry instead of ad hoc
   branches
3. decide whether host function state remains engine-owned or moves behind a
   richer kernel-owned registration object
