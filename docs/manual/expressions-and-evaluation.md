# Expressions And Evaluation

## Expressions

Aleph3 represents symbolic meaning as expressions. A call has a **head** and
arguments. In `Clamp[x, 0, 10]`, `Clamp` is the head. Arithmetic uses the same
structure, so `x + 2` has the internal form `Plus[x, 2]`.

```text
FullForm[x + 2]          -> Plus[x, 2]
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
x + y + x                -> y + 2 * x
x*x^2                    -> x^3
```

Canonical does not mean “the form every mathematician prefers.” It means a
stable representation for equality, matching, and algorithms.

## Exact And Approximate Numbers

```text
1/2 + 1/3                -> 5/6
1/2 + 2                  -> 5/2
1/2 + 0.5                -> approximate Number
```

Exact coefficients use checked 64-bit integer storage. Overflow is reported;
arbitrary-precision integers are future work.

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
