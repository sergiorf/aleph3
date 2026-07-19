# Kernel List And Structural Operations Spec

## Status

This document defines the symbolic MVP contract for structural inspection and
finite-list operations. The first structural slice is implemented for
`FullForm`, `Head`, and one-level `Part`; the first finite-list slice is
implemented for `Map`, `Apply`, `Select`, and `Cases` over explicit finite
lists. Nested traversal, levels, heads traversal, broad predicates, and scoping
remain planned or deferred as marked below.

## Purpose

The MVP needs a small, predictable surface for inspecting expression trees and
working with finite lists. The goal is interactive usefulness in the CLI and
notebook, not Mathematica compatibility.

This contract builds on existing `Expr` trees, lists, predicates, rewrite, and
evaluation budgets. It must not add a second evaluator or make functions
implicitly list-aware.

## Implemented Structural Inspection

- `Head[expr]` returns the public head name for atoms, lists, rules,
  assignments, function definitions, and function calls.
- `Part[expr, index]` returns the one-based child at `index` for supported
  lists, function calls, and rules.

Planned later:

- `Part[expr, {i, j, ...}]` may follow only after nested traversal and
  diagnostics are specified.

`FullForm[expr]` is the public structural rendering builtin. It evaluates
`expr`, renders that evaluated expression through the shared FullForm renderer,
and returns the rendering as an exact string. The output is diagnostic text,
not a stable serialization format.

CLI and session full-form inspection, including `symbolic-fullform` and the
full-form portion of `:inspect`, remains parsed-form inspection before normal
evaluation. These inspection commands should use the same renderer as
`FullForm[expr]` while documenting their different evaluation timing.

Required diagnostics:

- invalid indexes, including zero and out-of-range indexes
- non-integer indexes
- unsupported part extraction from atoms
- malformed nested part specifications

## Implemented Finite-List Operations

- `Map[f, {a, b, c}]` evaluates to `{f[a], f[b], f[c]}`.
- `Apply[f, {a, b}]` evaluates to `f[a, b]`.
- `Select[list, predicate]` keeps elements for which the predicate evaluates
  exactly to `True`; unresolved predicate calls are treated as non-matches in
  the first slice.
- `Cases[list, pattern]` returns elements matching the current supported
  pattern language.

The first slice operates only on explicit finite lists. Broader expression-tree
traversal for `Map`, levels, heads, rule lists, and sequence patterns remain
outside the initial contract.

Required diagnostics:

- non-list inputs where a list is required
- predicates that do not evaluate to exact `True` or `False` where `Select`
  requires a boolean decision
- unsupported patterns inherited from the rewrite matcher
- traversal or result sizes that exceed kernel budgets or configured limits

## Binding And Scoping Direction

The symbolic MVP should add one minimal lexical binding construct before broad
scoping. The preferred first contract is `With`, implemented as capture-safe
lexical substitution over an explicit binding list and body.

Deferred:

- `Module`
- `Block`
- dynamic scoping
- generated-symbol hygiene beyond the chosen `With` surface
- broad local assignment semantics

The binding contract must build on the existing `FreeVariables`,
`BoundVariables`, `DependsOn`, and capture-safe substitution model.

## Attribute Boundary

The implemented list operations do not make general `Listable` active. `Map`,
`Apply`, `Select`, and `Cases` are explicit operations over finite lists.
Scalar functions remain scalar unless their own supported contract says
otherwise.

Likewise, `Flat` and `Orderless` do not become general matcher semantics for
this slice.
