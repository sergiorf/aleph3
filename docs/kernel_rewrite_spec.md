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

## What "Rewrite" Means

In Aleph3, a rewrite is a rule-based symbolic transformation.

Example rule:

```text
f[x] -> g[x]
```

Example input:

```text
f[f[x]]
```

Example output after repeated rewriting:

```text
g[g[x]]
```

That is different from ordinary numeric evaluation.

Numeric evaluation asks:

- what is `2 + 3`?

Rewrite asks:

- how should one expression shape be transformed into another expression
  shape?

This matters because symbolic algebra depends heavily on transformations such
as:

- replacing one form with an equivalent form
- distributing or factoring structure
- canonicalizing expressions
- applying user or pack rules without hardcoding every case into evaluator
  branches

## Current Contract

### Rule Scope

The current rewrite engine accepts exact `Rule` expressions:

- left-hand side must match structurally
- right-hand side is substituted structurally

There is no pattern language yet.
This is exact tree rewriting only.

That means the left-hand side must already match the expression tree exactly.
The current engine does not yet understand placeholders such as "any
expression" or named pattern variables.

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

Example of what works now:

- rule: `f[x] -> g[x]`
- input: `f[f[x]]`
- repeated rewrite output: `g[g[x]]`

Example of what does not work yet:

- rule: `f[a_] -> g[a]`

Pattern variables such as `a_` are not implemented yet.

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
