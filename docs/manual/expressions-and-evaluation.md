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

## Diagnostics And Budgets

Parsing, validation, and runtime failures use structured codes. Examples
include invalid forms, unsupported constructs, division by zero, exact
overflow, and exhausted budgets. Budgets bound work such as evaluation steps
and repeated rewrites, which is essential for safe embedding.

For the deeper model, see [Concepts and Terminology](../concepts.md).
