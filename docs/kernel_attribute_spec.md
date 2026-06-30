# Kernel Attribute Spec

## Status

The first evaluation-control attribute slice is now implemented.

This slice makes `HoldFirst`, `HoldRest`, and `HoldAll` explicit kernel
contract facts for the small builtin-owned set that already relies on held
arguments today.

## Purpose

This document defines what symbol attributes mean today in Aleph3, and what
they do not mean yet.

Plain-language summary:

- an attribute is metadata attached to a symbol such as `If` or `Assuming`
- some attributes are still descriptive only
- the first active attribute contract now covers evaluation-control behavior
- attributes still do not form a new general dispatcher

## Current Active Attributes

The active evaluation-control attributes in this slice are:

- `HoldFirst`
- `HoldRest`
- `HoldAll`

Current builtin-owned heads using that contract:

- `If` uses `HoldRest`
- `And` uses `HoldAll`
- `Or` uses `HoldAll`
- `Assuming` uses `HoldFirst`
- `Refine` uses `HoldRest`

These attributes are now visible through shared symbol metadata in the
evaluation context.

## What Active Means In This Slice

In this slice, active means:

- the attribute is part of the documented kernel contract
- builtin or registered symbolic ownership can publish it through shared
  symbol metadata
- evaluator scheduling for the supported heads continues to honor the same
  held-argument behavior as before

Examples:

```text
If[True, 1, 1/0] -> 1
And[False, 1/0] -> False
Or[True, 1/0] -> True
Assuming[x > 0, If[x > 0, 1, 2]] -> 1
Refine[Abs[x], x >= 0] -> x
```

## Registration Contract

Symbolic registration metadata can now declare explicit symbol attributes.

That means:

- builtin symbolic handlers can publish hold metadata through registration
- future pack handlers may carry attribute metadata too
- attribute metadata alone does not grant held evaluation semantics outside the
  supported builtin-owned slice

In practical terms:

- `Assuming` and `Refine` now publish their hold metadata through the symbolic
  registration path
- ordinary pack registrations remain attribute-free unless they explicitly
  declare attributes

## Current Limits

Still intentionally out of scope:

- general attribute-driven dispatch
- hold behavior for user-defined functions
- hold behavior for host functions
- broad pack-defined held evaluation semantics
- broader attribute families such as orderless or flat becoming execution
  owners

## Descriptive-Only Attributes For Now

These attributes remain descriptive in this slice:

- `listable`
- `numeric_function`

They still appear in shared symbol metadata, but they do not become a new
general ownership or dispatch system here.

## Precedence And Ownership Notes

- special forms still own special-form dispatch
- registered symbolic handlers still own registered symbolic execution
- builtin evaluator functions still own builtin evaluator execution
- evaluation-control attributes shape argument scheduling inside those existing
  owners
- attributes do not make an otherwise unknown symbol callable

## Next Growth Areas

Likely follow-on work:

- deciding how far attributes should influence dispatch versus remain metadata
- defining whether any pack-owned hold semantics should ever become active
- expanding the shared attribute model only after precedence and ownership
  rules stay clear
