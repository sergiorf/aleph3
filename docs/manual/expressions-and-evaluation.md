# Expressions And Evaluation

## Expressions

Aleph3 represents symbolic meaning as expressions. A call has a **head** and
arguments. In `Clamp[x, 0, 10]`, `Clamp` is the head. Arithmetic uses the same
structure, so `x + 2` has the internal form `Plus[x, 2]`.

```text
FullForm[x + 2]          -> "Plus[x, 2]"
:inspect f[x + 1]
```

This uniform model lets evaluation, matching, assumptions, and packs operate
on the same representation.

## Current Input Syntax

The shared syntax frontend accepts explicit calls, infix arithmetic and
comparison operators, lists, strings, rules, patterns, and assignments used by
the supported kernel surface:

```text
f[x, 2]
(x + 1) * (x - 1)
If[x >= 0, x, -x]
{x, y, 1/2}
f[a_] -> g[a]
```

Function calls use square brackets. The symbolic session accepts the current
compatibility conveniences used by existing examples, including implicit
multiplication such as `2x`. The trusted SDK subset remains narrower and
requires explicit syntax accepted by its validator. The current syntax is a
frontend choice; expression meaning belongs to the kernel.

The symbolic parser also preserves existing exact rational and complex
shorthand:

```text
1/2                      -> 1/2
-2/-3                    -> 2/3
I                        -> 1*I
3 + 4*I                  -> 3 + 4*I
```

These conveniences are part of the symbolic/session surface. SDK trusted
parsing starts from the same source-aware syntax frontend, but rejects syntax
outside the documented trusted subset such as `2x`, assignments, definitions,
rules, and patterns.

Decimal integer tokens are preserved by the shared syntax frontend and lower
to exact arbitrary-precision `Integer` expressions. Decimal machine-real
literals such as `1.0` lower to approximate `Real` values. Exact rational
literals normalize through the shared arbitrary-precision rational model;
denominator-one rationals canonicalize to `Integer`. The trusted SDK subset
still projects accepted integer source text through its bounded host-number
model and reports `frontend.parser.integer_out_of_range` outside that boundary.

## Evaluation And Symbolic Fallback

Evaluation applies known meanings:

```text
2 + 3                    -> 5
If[3 < 4, 10, 20]        -> 10
1/2 + 1/3                -> 5/6
```

A valid call without an applicable definition is preserved:

```text
Mystery[x]               -> Mystery[x]
```

Preservation is not failure. A later definition, assumption, rule, or pack may
make progress. The trusted SDK is stricter and may reject names absent from its
schema.

## Definitions And Session State

The symbolic session retains assignments and function definitions:

```text
a = 2
a + 3                    -> 5
f[x_] := x + 1
f[4]                     -> 5
```

Separate sessions isolate state. One-shot CLI commands are ephemeral.

## Normalization

Normalization gives equivalent structures a deterministic shape:

```text
0 + x                    -> x
x + y + x                -> 2 * x + y
x*y + y*x                -> 2 * x * y
Sin[x] + Sin[x]          -> 2 * (Sin[x])
x*x^2                    -> x^3
Sin[x]*Sin[x]            -> (Sin[x])^2
1*x*y                    -> x*y
```

Canonical does not mean “the form every mathematician prefers.” It means a
stable representation for equality, matching, and algorithms.

## Exact And Approximate Numbers

```text
1/2 + 1/3                -> 5/6
1/2 + 2                  -> 5/2
12345678901234567890 + 1 -> 12345678901234567891
9007199254740993/3       -> 3002399751580331
1/2 + 0.5                -> approximate Number
```

Exact integer and rational expressions use arbitrary-precision scalar storage
and are not rounded through machine reals. Bounded adapters remain explicit for
places that require native sizes, indexes, exponents, SDK host-number values,
or budgeted algorithms. Values outside those local bounds are rejected at that
boundary rather than wrapped or silently approximated.

Use decimals only when approximation is intended:

```text
N[1/3]                   -> approximate Number
```

## Diagnostics And Budgets

Parsing, validation, and runtime failures use structured codes and source
locations when available. Examples include invalid forms, unsupported
constructs, division by zero, exact overflow, and exhausted budgets. Budgets
bound work such as evaluation steps and repeated rewrites, which is essential
for safe embedding.

Unknown symbolic calls and actual failures are different contracts:

```text
UnknownHead[x]           -> UnknownHead[x]
1/0                      -> diagnostic
```

For the deeper model, see [Concepts and Terminology](concepts-and-terminology.md).
