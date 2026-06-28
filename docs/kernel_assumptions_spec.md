# Kernel Assumptions Spec

## Status

Initial assumptions support is now implemented for the first narrow symbolic
workflow.

## Purpose

This document describes the first assumption-aware kernel contract in plain
language.

The goal is not full Mathematica-style assumptions yet.
The goal is a small, explicit surface that already helps symbolic workflows.

## What An Assumption Means

An assumption is extra context that says something should be treated as true
while evaluating or refining an expression.

Practical examples:

- `flag` means treat `flag` as `True`
- `Not[flag]` means treat `flag` as `False`
- `x > 0` means treat `x` as positive
- `x >= 0` means treat `x` as nonnegative
- `Positive[x]` means treat `x` as positive
- `NonPositive[x]` means treat `x` as nonpositive

## User-Facing Surface

### `Assuming[assumptions, expr]`

Evaluate `expr` in a temporary context with extra facts.

Examples:

```text
Assuming[x > 0, If[x > 0, 1, 2]] -> 1
Assuming[flag, If[flag, "ok", "no"]] -> "ok"
Assuming[Positive[x], Positive[x]] -> True
```

### `Refine[expr]`

Refine an expression using assumptions already present in the current
evaluation context.

This matters more for internal callers today than for raw CLI usage.

### `Refine[expr, assumptions]`

Refine an expression with temporary extra facts.

Examples:

```text
Refine[Abs[x], x >= 0] -> x
Refine[Sqrt[x^2], x <= 0] -> -x
Refine[x > 0, And[x >= 0, NotEqual[x, 0]]] -> True
Refine[Positive[x], x > 0] -> True
Refine[NonZeroQ[x], Positive[x]] -> True
```

### Sign Predicates

The kernel now also supports a small family of explicit sign queries:

- `Positive[x]`
- `Negative[x]`
- `NonNegative[x]`
- `NonPositive[x]`
- `ZeroQ[x]`
- `NonZeroQ[x]`

These are useful when callers want to ask a direct domain question instead of
rewriting it as a comparison with zero.

Examples:

```text
Positive[3/2] -> True
ZeroQ[0] -> True
Refine[NonPositive[x], x <= 0] -> True
```

## Supported Assumption Forms In This Slice

This first slice intentionally supports only a small set of forms:

- bare boolean symbols such as `flag`
- `Not[flag]`
- `And[a, b, ...]`
- lists of assumptions such as `{x >= 0, NotEqual[x, 0]}`
- direct comparisons:
  - `Equal`
  - `NotEqual`
  - `Less`
  - `LessEqual`
  - `Greater`
  - `GreaterEqual`
- direct sign predicates on symbols:
  - `Positive`
  - `Negative`
  - `NonNegative`
  - `NonPositive`
  - `ZeroQ`
  - `NonZeroQ`

## What The Kernel Currently Uses Assumptions For

The current kernel uses those facts in a few specific places:

- resolving unknown boolean symbols such as `flag`
- resolving direct assumed comparisons such as `x > 3` when asked again
- resolving sign-based comparisons against zero
- resolving direct sign predicates such as `Positive[x]`
- refining `Abs[x]` from sign facts
- refining `Sqrt[x^2]` from sign facts

This keeps the slice practical without pretending there is already a full
domain engine underneath it.

## Important Limits

Still intentionally out of scope:

- conditions and typed pattern assumptions
- quantified logic
- interval arithmetic beyond simple sign facts
- contradiction solving
- broad domain propagation across arbitrary algebra
- assumption-aware global simplification scheduling

Examples that are still out of scope:

- proving `x > 3` implies `x > 0`
- proving `x != 2` changes a larger algebraic expression
- rich real/complex/integer domain tracking
- predicate assumptions over arbitrary expressions such as `Positive[x + 1]`

## Contract Notes

- assumptions are temporary unless explicitly stored by the caller in an
  `EvaluationContext`
- unsupported assumption forms fail explicitly instead of being silently
  ignored
- this slice is designed for predictable behavior, not broad inference

## Next Growth Areas

Likely follow-on work:

- richer domain categories
- assumption-aware rewrite hooks
- stronger contradiction handling
- pack-facing domain queries over the same kernel contract
