# Kernel Rewrite Spec

## Status

Initial rewrite infrastructure is now implemented as a structural rule engine
with the first minimal pattern language.

Primary implementation:

- [include/kernel/Rewrite.hpp](/home/sergio/dev/aleph3/include/kernel/Rewrite.hpp)
- [src/kernel/Rewrite.cpp](/home/sergio/dev/aleph3/src/kernel/Rewrite.cpp)

## Purpose

This document defines the first kernel-owned rewrite contract.

It is intentionally smaller than the long-term target. The current goal is to
establish rewrite ownership, traversal semantics, bounded repeated
application, and the first reusable pattern contract.

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

The current rewrite engine accepts structural `Rule` expressions and a first
pattern language:

- exact structural rules still work
- symbols ending in `_` act as named pattern binders
- repeated use of the same named binder must match the same expression
- the right-hand side substitutes named binders by bare symbol name

Examples:

- `f[x] -> g[x]`
- `f[a_] -> g[a]`
- `f[a_, a_] -> same[a]`

This is still intentionally small.
There are no typed patterns, predicates, conditions, sequence patterns, or
attribute-aware matcher rules yet.

### Equality Model

`kernel::structurally_equal(...)` compares expressions structurally across:

- atoms
- function calls
- rules
- assignments
- function definitions
- lists

This is the equality contract used by structural matching and by repeated
named-pattern consistency checks.

### Traversal Semantics

`rewrite_once(...)` performs:

- top-level structural or pattern match first
- recursive traversal into child expressions otherwise
- whole-tree reconstruction when any child rewrite happens

`rewrite_repeated(...)` performs repeated application up to a caller-provided
rewrite bound.

`rewrite_repeated(..., EvaluationContext&, max_rewrites)` uses the same
traversal semantics, but each successful rewrite also consumes one kernel
evaluation step through the supplied context.

### Bounded Behavior

Rewrite is bounded in two layers:

- caller provides `max_rewrites` as the local rewrite-loop cap
- traversal stops when no rewrite occurs
- traversal also stops once the bound is reached
- when an `EvaluationContext` is supplied, each successful rewrite consumes one
  step from `policy().budget().max_evaluation_steps`

This intentionally avoids creating a second independent policy budget just for
rewrite.
The current contract is:

- `max_rewrites` bounds a specific rewrite loop
- evaluation-step budget bounds total kernel work when rewrite participates in
  runtime execution

## Scheduling Relative To Evaluation And Normalization

Current scheduling rules are intentionally conservative:

- generic evaluation remains evaluator-owned
- builtin simplification remains evaluator-owned
- rewrite is a caller-directed transformation API, not an implicit global pass
- normalization remains a separate kernel concern

That means rewrite does not automatically:

- normalize before matching
- normalize after substitution
- run after every evaluator reduction

Callers that need canonical ordering should normalize explicitly before or
after rewrite, depending on the intended contract.

Today the general evaluator still does:

1. normalize once at evaluation entry
2. run evaluator-owned dispatch and builtin simplification
3. preserve symbolic fallback when no owner reduces the form

General rewrite integration beyond explicit callers is still future work and
should only happen where the scheduling contract is precise enough to avoid
hidden semantic drift.

## What This Enables Now

The rewrite subsystem can already support:

- exact local transformation tests
- first pattern-based substitution tests
- rule-driven structural cleanup experiments
- migration of selected hardcoded transforms toward kernel-owned rewrite APIs

Example of what works now:

- rule: `f[x] -> g[x]`
- input: `f[f[x]]`
- repeated rewrite output: `g[g[x]]`

## What Is Not Implemented Yet

- conditional rules
- typed or predicate-based patterns
- sequence patterns
- rewrite strategy selection
- integration with assumptions
- broad evaluator/rewrite scheduling integration

## Next Steps

- decide which existing simplifications should move behind rewrite entrypoints
- define whether selected rewrite-driven flows should normalize pre-match,
  post-substitution, or both
