# Algebra Equivalence Specification

## Status

Current implementation contract referenced by the
[Aleph3 Unified Plan](aleph3_unified_plan.md).

## Purpose

`Equivalent` is the first proof-bearing mathematical equivalence surface. It is
intentionally bounded: it proves only identities that follow from current
canonical expression normalization, exact polynomial comparison, or exact
rational-expression comparison without losing domain restrictions.

This is not a general theorem prover and not a numerical identity checker.

## Ownership

The `core-algebra` pack owns the public `Equivalent` function. The function is
registered through the shared kernel function registry and uses existing `Expr`,
diagnostic, exact-algebra, and rational-expression contracts.

The kernel owns the `DomainRestrictions` metadata carrier used by exact
rational-expression transformations. Other consumers must not add private
equivalence semantics in the CLI, session, notebook, SDK, or web layers.

## Supported Surface

```text
Equivalent[expr1, expr2]
```

The result is one of:

- `True`, when Aleph3 proves equivalence in the supported subset;
- `False`, when Aleph3 proves non-equivalence in the supported exact algebra
  subset;
- `Unknown`, when the current proof methods cannot produce an unconditional
  result.

`Unknown` is a proof outcome, not an error. It means the expressions might be
equivalent, might be inequivalent, or might be equivalent only under side
conditions that this public surface does not yet render.

## Proof Methods

`Equivalent` may prove equality by:

- comparing canonical structural output after ordinary evaluation;
- converting both sides to exact polynomials over the merged supported variable
  set and comparing the exact normalized difference with zero;
- converting both sides to exact rational expressions, applying the supported
  exact cancellation contract, and comparing numerator, denominator, and
  preserved denominator-exclusion metadata.

Examples:

```text
Equivalent[x + 1, 1 + x]                     -> True
Equivalent[x^2 + 2*x + 1, x^2 + x + x + 1]  -> True
Equivalent[x + 1, x + 2]                     -> False
Equivalent[1/x, 1/x]                         -> True
```

Rational-expression transformations are domain-sensitive. If two expressions
reduce to the same printed expression only after dropping denominator
restrictions, `Equivalent` returns `Unknown` in this slice:

```text
Equivalent[(x^2 - 1)/(x - 1), x + 1]         -> Unknown
Equivalent[(x*y)/x, y]                       -> Unknown
Equivalent[Cancel[(x*y)/x], y]               -> True
```

The last example is `True` because the explicit `Cancel` result is already the
expression being compared; the internal side-condition metadata is not attached
to the printed expression.

## Diagnostics And Exactness

Malformed calls report the standard arity diagnostic. Exact coefficient
overflow reports `runtime.exact_overflow`. Denominator zero reached during
rational-expression conversion reports the existing division-by-zero diagnostic.

Unsupported proof methods return `Unknown` when possible rather than inventing
a private fallback. Exact algebra operations must not silently demote to
approximate arithmetic.

## Unsupported Boundaries

The first equivalence slice does not support:

- trigonometric identities such as `Sin[x]^2 + Cos[x]^2 == 1`;
- numerical sampling as a proof method;
- public conditional-equivalence output;
- broad branch, square-root, or logarithm domain reasoning;
- general theorem proving;
- solving, quantifiers, or implication checking;
- broad special-function identities;
- general multivariate rational simplification beyond the current exact
  rational-expression subset.

Examples:

```text
Equivalent[Sin[x]^2 + Cos[x]^2, 1]           -> Unknown
Equivalent[Sqrt[x^2], x]                     -> Unknown
```
