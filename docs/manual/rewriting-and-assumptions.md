# Rewriting And Assumptions

## Rules And Patterns

A rule replaces a matching structure:

```text
Replace[f[x], f[a_] -> g[a]]            -> g[x]
MatchQ[f[x, x], f[a_, a_]]              -> True
MatchQ[f[x, y], f[a_, a_]]              -> False
MatchQ[3, _Integer]                      -> True
```

`a_` matches one expression and binds it. Reusing a binder requires the same
structure. Typed patterns restrict the matched expression. Sequence patterns
and general rule lists remain outside the current contract.

## Targeting A Depth

Depth zero is the whole expression. Its arguments are at depth one and their
arguments are at depth two. Heads are not traversed.

```text
Replace[f[f[x], x], x -> y, 1]          -> f[f[x], y]
Replace[f[f[x], x], x -> y, 2]          -> f[f[y], x]
Replace[f[f[x], x], x -> y, {1, 2}]     -> f[f[y], y]
```

The range is inclusive. Omitting it uses whole-expression traversal. Negative
levels and broader level specifications are unsupported.

## Repeated And Conditional Rewriting

`ReplaceRepeated` applies a rule until no change occurs or a bound is reached:

```text
ReplaceRepeated[f[f[x]], f[a_] -> a, 0] -> x
```

Repeated work consumes the evaluation budget. Conditional patterns accept a
match only when their predicate evaluates exactly to `True`:

```text
Replace[3, Condition[n_Integer, Positive[n]] -> g[n]] -> g[3]
```

## Assumptions And Refine

Assumptions supply temporary facts that justify selected behavior:

```text
Refine[Sqrt[x^2], x >= 0]                -> x
Refine[Abs[x], x >= 0]                   -> x
Assuming[x > 0, If[x > 0, 1, 2]]         -> 1
```

The current model covers direct booleans, comparisons, signs, nonzero facts,
and narrow integer/rational/real domains, plus limited derived signs for simple
exact forms. It is not a general theorem prover. Temporary facts do not leak
after `Refine` or `Assuming` finishes.

See the [rewrite specification](../kernel_rewrite_spec.md) and
[assumptions specification](../kernel_assumptions_spec.md) for exact limits.

