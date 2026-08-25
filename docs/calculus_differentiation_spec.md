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
- `D[expr, {x, n}]` and `Differentiate[expr, {x, n}]` compute the `n`th
  derivative by repeated application of the same first-derivative contract.
  The order `n` must be a nonnegative integer-valued numeric order within the
  supported order limit.
- `D[expr, {x, 0}]` evaluates `expr` and returns that value unchanged.
- Constants, exact and approximate numbers, booleans, strings, infinities, and
  symbols other than `x` differentiate to `0`.
- `D[x, x]` returns `1`.
- `Plus` differentiates term by term.
- `Times` uses the finite product rule and treats factors independent of `x`
  as constants.
- `Divide[a, b]` differentiates by first using the calculus-local reciprocal
  product form `a * b^-1`; `Divide[1, b]` uses `b^-1`. This reuses the same
  finite product, numeric-power, and chain-rule contracts rather than adding a
  separate quotient-rule surface.
- `Power[u, n]` supports exact or approximate numeric exponents when `u`
  depends on the differentiation variable. The base derivative is computed
  recursively through the same differentiation contract, so
  `D[u^n, x] = n*u^(n-1)*D[u, x]` inside this focused subset.
- The first chain-rule surface covers unary `Sin`, `Cos`, `Exp`, `Log`, and
  `Sqrt`.

Results are normalized through the existing kernel arithmetic and exactness
contracts where no held unsupported derivative remains. The focused
differentiation contract does not require later algebraic recombination of
reciprocal-product terms into a single quotient.

## Diagnostics And Budgets

The second argument must be a symbol or `{symbol, nonnegative integer order}`.
Malformed derivative specifications, negative orders, non-integer orders,
symbolic orders, and orders above the supported limit report
`kernel.invalid_form`. Recursive differentiation consumes the shared
evaluation-step budget and reports `runtime.step_budget_exhausted` when
exhausted.

Unsupported dependent calls remain unevaluated as `D[expr, x]`. This is a
deliberate preservation contract, not a claim that the derivative is zero.

## Unsupported Boundaries

This slice excludes `Piecewise`, `Sum`, `Product`, compact partial-derivative
notation, assumptions-driven branch simplification, integration, limits, broad
special functions, and broad symbolic simplification beyond the already
supported arithmetic surface.

## Planned Differentiation Follow-Ups

The symbolic MVP follow-up should keep extending differentiation before adding
broader calculus.

Planned next surface:

- simple partial derivatives by composing first derivatives over distinct
  symbols, such as `D[D[expr, x], y]`, with any compact multi-variable syntax
  specified before exposure.

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
