# Representation Gap Inventory

## Purpose

This document inventories the concrete modeling gaps between:

- `Expr`
- `ir::Node`

Related documents:

- [Aleph3 Unified Plan](aleph3_unified_plan.md)
- [Kernel Representation Decision](kernel_representation_decision.md)
- [Trusted Subset V1](trusted_subset_v1.md)

Primary sources:

- [include/expr/Expr.hpp](/home/sergio/dev/aleph3/include/expr/Expr.hpp)
- [include/ir/Node.hpp](/home/sergio/dev/aleph3/include/ir/Node.hpp)

## Summary

`Expr` already models the symbolic kernel domain.

`ir::Node` models a constrained trusted-subset execution language.

The gap is not small. It is structural.

That confirms the earlier decision:

- `Expr` should evolve as the kernel representation
- `ir::Node` should remain a constrained SDK/trusted-subset form

## Direct Shape Comparison

### `Expr` currently models

- symbol
- number
- complex
- rational
- boolean
- string
- function call
- function definition
- assignment
- rule
- list
- infinity
- complex infinity
- indeterminate

### `ir::Node` currently models

- number literal
- boolean literal
- string literal
- variable
- unary operator
- binary operator
- call
- `If`

## Features Present In `Expr` But Not In `ir::Node`

These are the largest gaps.

### Symbolic Value Forms

`Expr` supports:

- `Symbol`
- `Rational`
- `Complex`
- `Infinity`
- `ComplexInfinity`
- `Indeterminate`

`ir::Node` does not directly model any of those except the narrow idea of a
variable name and `double` number literals.

Impact:

- exact arithmetic and symbolic exceptional values cannot be expressed natively
  in `ir::Node`

### Symbolic Program Forms

`Expr` supports:

- `FunctionDefinition`
- `Assignment`
- `Rule`

`ir::Node` does not model any of them.

Impact:

- symbolic definitions, rewrite rules, and assignment-driven symbolic state are
  outside the current IR language

### Structural Data Forms

`Expr` supports:

- `List`

`ir::Node` does not model list literals as a first-class syntax node.

Impact:

- symbolic structural manipulation is much broader in `Expr`

### Symbolic Fallback Semantics

`Expr` can preserve unresolved symbolic structure directly.

`ir::Node` is oriented toward concrete trusted-subset runtime execution and
errors rather than symbolic fallback as a first-class result shape.

Impact:

- unresolved calls and symbolic partial evaluation fit `Expr` naturally and fit
  `ir::Node` only awkwardly

## Features Present In `ir::Node` But Not Explicitly Present In `Expr`

These are mostly SDK/trusted-subset strengths rather than kernel strengths.

### Source Span On Every Node

`ir::Node` carries:

- `SourceSpan span`

`Expr` does not currently carry source spans in the core representation.

Impact:

- SDK diagnostics and runtime error locations are easier to maintain through
  `ir::Node`

### Narrow Operator Taxonomy

`ir::Node` models arithmetic/comparison operators explicitly as:

- `UnaryOperator`
- `BinaryOperator`

`Expr` typically represents these as function heads such as `Plus`, `Times`,
`Less`, and so on.

Impact:

- `ir::Node` is easier to validate and evaluate deterministically for the
  trusted subset
- `Expr` is better aligned with symbolic language semantics

### Fixed Trusted Conditional Shape

`ir::Node` has a dedicated `IfNode`.

`Expr` models conditionals through `FunctionCall("If", ...)`.

Impact:

- `ir::Node` is clearer for fixed trusted-subset control flow
- `Expr` remains more uniform for symbolic semantics

## Capability Matrix

| Capability | `Expr` | `ir::Node` | Notes |
| --- | --- | --- | --- |
| symbolic fallback | yes | weak / indirect | core reason `Expr` stays kernel-owned |
| exact rational values | yes | no | SDK IR uses `double` literals |
| complex values | yes | no | out of trusted subset |
| symbolic exceptional values | yes | no | `Infinity`, `Indeterminate`, etc. |
| user-defined symbolic functions | yes | no | not expressible in current IR |
| assignments | yes | no | not expressible in current IR |
| rules / rewrite data | yes | no | blocks rule-driven kernel behavior in IR |
| first-class list structure | yes | no | IR currently lacks list nodes |
| source spans everywhere | no | yes | current SDK diagnostic advantage |
| explicit trusted operator nodes | no | yes | current SDK validation/runtime advantage |
| host-facing bounded validation flow | indirect | yes | current SDK design advantage |

## Trusted Subset Fit

`ir::Node` is still the better fit for the current trusted subset because it:

- keeps the language intentionally narrow
- supports structured spans
- matches current validator and runtime expectations

But that does not make it the right kernel form.

The trusted subset is a constrained product language.
The kernel is a broader symbolic semantic system.

## Migration Implications

### What should not happen

Do not try to expand `ir::Node` until it duplicates:

- rules
- assignments
- symbolic definitions
- exact symbolic values
- unresolved symbolic fallback

That would recreate `Expr` under another name.

### What should happen

- keep `ir::Node` narrow for trusted-subset compilation and validation
- add adapters or lowering boundaries only where SDK execution needs kernel
  behavior
- evolve `Expr` and surrounding kernel structures instead of creating a second
  symbolic AST track

## Near-Term Follow-On Questions

1. Which kernel operations need a clean lowering path from `ir::Node` first?
2. Should `Expr` gain optional source-location attachment later?
3. Which trusted-subset constructs should lower directly into `Expr` and which
   should remain cached in `ir::Node` form for SDK compile artifacts?
