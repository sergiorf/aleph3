# Kernel Rewrite Spec

## Status

Initial rewrite infrastructure is now implemented as an exact structural rule
engine.

Primary implementation:

- [include/kernel/Rewrite.hpp](/home/sergio/dev/aleph3/include/kernel/Rewrite.hpp)
- [src/kernel/Rewrite.cpp](/home/sergio/dev/aleph3/src/kernel/Rewrite.cpp)

## Purpose

This document defines the first kernel-owned rewrite contract.

It is intentionally smaller than the long-term target. The current goal is to
establish rewrite ownership, traversal semantics, and bounded repeated
application before adding patterns.

## Current Contract

### Rule Scope

The current rewrite engine accepts exact `Rule` expressions:

- left-hand side must match structurally
- right-hand side is substituted structurally

There is no pattern language yet.
This is exact tree rewriting only.

### Equality Model

`kernel::structurally_equal(...)` compares expressions structurally across:

- atoms
- function calls
- rules
- assignments
- function definitions
- lists

This is the equality contract used by the first rewrite slice.

### Traversal Semantics

`rewrite_once(...)` performs:

- top-level exact match first
- recursive traversal into child expressions otherwise
- whole-tree reconstruction when any child rewrite happens

`rewrite_repeated(...)` performs repeated application up to a caller-provided
rewrite bound.

### Bounded Behavior

The first repeated entrypoint is explicitly bounded:

- caller provides `max_rewrites`
- traversal stops when no rewrite occurs
- traversal also stops once the bound is reached

## What This Enables Now

The rewrite subsystem can already support:

- exact local transformation tests
- rule-driven structural cleanup experiments
- migration of selected hardcoded transforms toward kernel-owned rewrite APIs

## What Is Not Implemented Yet

- symbolic patterns
- named pattern variables
- conditional rules
- rewrite strategy selection
- integration with assumptions
- evaluator/rewrite scheduling policy

## Next Steps

- add the first minimal pattern language
- define rewrite budgeting relative to evaluation budgets
- decide which existing simplifications should move behind rewrite entrypoints
