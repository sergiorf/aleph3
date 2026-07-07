# The Calculus Pack

The `core-calculus` pack currently provides a focused symbolic derivative
surface. It uses the shared expression, exactness, diagnostics, budget, and
registration contracts rather than private evaluator behavior.

## Differentiation

Use `D[expr, x]` to differentiate with respect to a symbol. `Differentiate` is
the long-form alias.

```text
D[x, x]                                  -> 1
D[y, x]                                  -> 0
D[x^2 + 3*x, x]                          -> 2 * x + 3
D[x*y, x]                                -> y
D[Sin[x], x]                             -> Cos[x]
D[Exp[x^2], x]                           -> 2 * x * (Exp[x^2])
```

The first supported rules cover constants, variables, sums, finite products,
numeric powers of the differentiated symbol, and chain rules for `Sin`, `Cos`,
`Exp`, `Log`, and `Sqrt`.

Unsupported dependent function calls are preserved as derivatives instead of
being guessed:

```text
D[f[x], x]                               -> D[f[x], x]
```

The differentiation variable must be a symbol:

```text
D[x, x + 1]                              -> kernel.invalid_form
```

The authoritative boundary is the
[focused differentiation specification](../calculus_differentiation_spec.md).
