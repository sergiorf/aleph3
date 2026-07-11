# Focused Differentiation Specification

## Ownership And Scope

Focused symbolic differentiation is owned by the `core-calculus` pack. The
pack registers `D` and `Differentiate` through the shared kernel function
registry and operates on existing `Expr` trees. It does not add a new
expression alternative and does not introduce evaluator-private calculus
semantics.

## Supported Surface

- `D[expr, x]` differentiates `expr` with respect to symbol `x`.
- `Differentiate[expr, x]` is an alias for `D[expr, x]`.
- Constants, exact and approximate numbers, booleans, strings, infinities, and
  symbols other than `x` differentiate to `0`.
- `D[x, x]` returns `1`.
- `Plus` differentiates term by term.
- `Times` uses the finite product rule and treats factors independent of `x`
  as constants.
- `Power[x, n]` supports exact or approximate numeric exponents.
- The first chain-rule surface covers unary `Sin`, `Cos`, `Exp`, `Log`, and
  `Sqrt`.

Results are normalized through the existing kernel arithmetic and exactness
contracts where no held unsupported derivative remains.

## Diagnostics And Budgets

The second argument must be a symbol. Other forms report `kernel.invalid_form`.
Recursive differentiation consumes the shared evaluation-step budget and
reports `runtime.step_budget_exhausted` when exhausted.

Unsupported dependent calls remain unevaluated as `D[expr, x]`. This is a
deliberate preservation contract, not a claim that the derivative is zero.

## Unsupported Boundaries

This slice excludes `Piecewise`, `Sum`, `Product`, higher-order derivatives,
partial-derivative notation, assumptions-driven branch simplification,
integration, limits, broad special functions, and broad symbolic
simplification beyond the already supported arithmetic surface.

## Planned Differentiation Follow-Ups

The symbolic MVP follow-up should extend differentiation before adding broader
calculus.

Planned next surface:

- `D[expr, {x, n}]` for nonnegative exact integer `n`, implemented as repeated
  application of the existing first-derivative contract.
- simple partial derivatives by composing first derivatives over distinct
  symbols, such as `D[D[expr, x], y]`, with any compact multi-variable syntax
  specified before exposure.
- diagnostics for negative, non-integer, symbolic, or oversized derivative
  orders.

Deferred beyond this follow-up:

- integration, definite or indefinite
- broad `Limit`
- broad `Series`, `Normal`, and `SeriesCoefficient`
- residues and singularity analysis
- vector calculus
- symbolic or numerical differential equations
- broad special-function introduction

`Limit` and `Series` may become later MVP-adjacent slices only after their
input forms, exactness, truncation objects, diagnostics, and interaction with
assumptions are specified in focused contracts.
