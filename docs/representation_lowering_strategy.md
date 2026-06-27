# Representation Lowering Strategy

## Purpose

This document defines how Aleph3 moves from the SDK trusted-subset
representation to the kernel semantic representation after the decision that:

- `Expr` is the kernel semantic form
- `ir::Node` is a constrained SDK/frontend form only

Related documents:

- [Kernel Representation Decision](kernel_representation_decision.md)
- [Representation Gap Inventory](representation_gap_inventory.md)
- [Layer Ownership Matrix](layer_ownership_matrix.md)

## Decision

Aleph3 should use one-way lowering from validated trusted-subset `ir::Node`
trees into kernel `Expr` trees when SDK execution needs kernel semantics.

This is a lowering strategy, not a promotion of `ir::Node` into a second
kernel AST.

## Ownership Boundary

### `ir::Node` remains SDK-owned

`ir::Node` continues to own:

- trusted-subset parser output
- source spans for frontend diagnostics
- validator input
- compile-time shape for schema and policy checks

`ir::Node` must not become:

- the place where new symbolic semantics are added
- the representation for user definitions, rules, or symbolic fallback
- a permanent peer to `Expr`

### `Expr` remains kernel-owned

`Expr` owns:

- semantic execution shape
- symbolic fallback behavior
- normalized functional heads such as `Plus`, `Times`, `Power`, and `If`
- future symbolic growth such as rewrite rules, assumptions, and domain packs

## Lowering Boundary

The lowering seam should live in a kernel-owned migration adapter:

- input: validated `ir::Node`
- output: kernel `Expr`

The repository may also keep a temporary staging object that carries both:

- trusted-subset `ir::Node` for frontend and validation ownership
- lowered `Expr` for future kernel-backed execution

That seam exists so:

- SDK compile and validation stay narrow
- kernel execution can converge on one semantic path
- migration code is explicit and temporary rather than spread through the repo

## Initial Lowering Contract

The first lowering set should cover the entire current trusted subset used by
the SDK runtime:

- number literals -> `Number`
- boolean literals -> `Boolean`
- string literals -> `String`
- variables -> `Symbol`
- unary plus -> `Plus[arg]`
- unary minus -> `Times[-1, arg]`
- binary add -> `Plus[left, right]`
- binary subtract -> `Plus[left, Times[-1, right]]`
- binary multiply -> `Times[left, right]`
- binary divide -> `Divide[left, right]`
- binary power -> `Power[left, right]`
- comparisons -> `Equal`, `NotEqual`, `Less`, `LessEqual`, `Greater`,
  `GreaterEqual`
- calls -> `FunctionCall(callee, args...)`
- `IfNode` -> `If[condition, then, else]`

This mapping intentionally targets symbolic heads rather than reproducing a
separate SDK operator-specific execution tree.

## `CompiledFormula` Strategy

During the transition, `CompiledFormula` may continue to cache validated
trusted-subset `ir::Node` trees because:

- validation already runs on that form
- source spans remain useful there
- the SDK API keeps `CompiledFormula` opaque

However, the long-term direction should be:

1. compile and validate into `ir::Node`
2. lower into `Expr` before execution
3. optionally cache the lowered `Expr` inside `CompiledFormula`
4. treat the cached `ir::Node` as frontend metadata, not execution truth

That means `CompiledFormula` may temporarily hold both:

- validated `ir::Node` for frontend concerns
- lowered `Expr` for execution

If both are cached, only `Expr` is the semantic execution representation.

## What Is Explicitly Temporary

The lowering adapter is migration scaffolding.

It is temporary in these senses:

- it exists because the repo still has SDK-owned frontend and validation forms
- it should stay narrow and translation-oriented
- it should not accumulate host policy logic
- it should not become a rich second interpreter API
- if a staging object carries both forms, that object is transitional as well

## What Should Not Be Added Here

Do not use the lowering layer to:

- add new symbolic language constructs first in `ir::Node`
- preserve source spans by threading them into kernel semantics prematurely
- recreate runtime-specific operator semantics that disagree with `Expr`
- encode host evaluation policy into expression structure

## Near-Term Follow-On Work

1. add a kernel-owned lowering module for trusted-subset nodes
2. route SDK execution through that module before symbolic evaluation collapse
3. decide whether `CompiledFormula` should cache lowered `Expr`, `ir::Node`, or
   both during the transition
4. simplify the remaining bridge once kernel execution owns the trusted subset
