# Kernel Variable Analysis Specification

## Status

Initial variable dependency and capture-safe substitution contract is
implemented.

Primary implementation:

- [`include/kernel/VariableAnalysis.hpp`](../include/kernel/VariableAnalysis.hpp)
- [`src/kernel/VariableAnalysis.cpp`](../src/kernel/VariableAnalysis.cpp)

## Purpose

This document defines the kernel-owned contract for answering which symbols an
expression depends on and for performing bounded internal substitution without
capturing variables.

The first goal is deliberately narrow: provide shared infrastructure for
rewrite, calculus, future summation, and DSP prerequisite work without
introducing broad lexical scoping or a general binder language.

## Supported Surface

The public symbolic functions are:

- `FreeVariables[expr]`
- `BoundVariables[expr]`
- `DependsOn[expr, x]`

`FreeVariables` returns a canonical list of symbol names that occur free in
`expr`. `BoundVariables` returns a canonical list of supported binders.
`DependsOn[expr, x]` returns `True` exactly when `x` is free in `expr`.

These functions inspect held expression structure. They do not evaluate the
first argument, so calls such as `FreeVariables[a = x]` do not perform the
assignment.

## Binding Model

The supported binders in this slice are:

- function-definition parameters, such as `x` in `f[x_] := x + y`
- named rule-pattern binders, such as `a` in `f[a_] -> g[a]`
- typed named pattern binders, such as `n` in `n_Integer`

Assignments are not lexical binders. The target name in `a = x` is definition
state, not a scoped variable.

Examples:

```text
FreeVariables[x + y^2]                  -> {x, y}
FreeVariables[f[a_] -> g[a, y]]         -> {y}
BoundVariables[f[a_] -> g[a, y]]        -> {a}
DependsOn[f[a_] -> g[a, y], a]          -> False
DependsOn[f[a_] -> g[a, y], y]          -> True
```

Results are sorted by symbol name for deterministic output.

The kernel API also analyzes `FunctionDefinition` and `Assignment` expression
nodes directly. Current surface syntax can parse those forms as top-level
inputs, but cannot nest them as the argument of `FreeVariables` or
`BoundVariables`; user-facing examples therefore use rules.

## Capture-Safe Substitution

The kernel API also exposes `substitute_symbols_capture_safe(...)` for internal
callers.

The first substitution contract is conservative:

- symbols already bound in the current lexical scope are not replaced
- a replacement whose free variables would be captured by the current scope is
  skipped
- binders are not renamed in this slice

For example, substituting `y -> x` into `f[x_] := y` leaves `y` unchanged
because inserting `x` would make it captured by the function parameter.

## Unsupported Boundaries

This slice does not introduce:

- `Sum`, `Product`, lambda, local modules, or broader scoped constructs
- sequence patterns
- predicate patterns beyond the existing rewrite surface
- binder renaming or alpha-conversion
- broad variable dependency analysis for heads as first-class expressions
- SDK trusted-subset exposure

Unsupported syntax outside the existing parser and pattern contract remains
unsupported here too.
