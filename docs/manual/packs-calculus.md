# The Calculus Pack

The `core-calculus` pack currently provides a focused symbolic derivative
surface. It uses the shared expression, exactness, diagnostics, budget, and
registration contracts rather than private evaluator behavior.

## Differentiation

Use `D[expr, x]` to differentiate with respect to a symbol. Use
`D[expr, {x, n}]` for the `n`th derivative with respect to `x`.
`Differentiate` is the long-form alias.

```text
D[x, x]                                  -> 1
D[y, x]                                  -> 0
Differentiate[x^2, x]                    -> 2 * x
D[x^2 + 3*x, x]                          -> 2 * x + 3
D[x*y, x]                                -> y
D[Sin[x], x]                             -> Cos[x]
D[Exp[x^2], x]                           -> 2 * x * (Exp[x^2])
D[x^3, {x, 2}]                           -> 6 * x
```

Mathematically, `D[expr, x]` computes the derivative
$\frac{d}{dx}\,\text{expr}$ inside the current supported symbolic subset. A
symbol that is not the differentiation variable is treated as independent of
that variable:

```text
D[42, x]                                 -> 0
D[1/3, x]                                -> 0
D["label", x]                            -> 0
D[a, x]                                  -> 0
D[x, x]                                  -> 1
```

The sum rule is applied term by term:

$$
\frac{d}{dx}(a + b + \cdots) =
\frac{da}{dx} + \frac{db}{dx} + \cdots
$$

```text
D[x^2 + 3*x + y, x]                      -> 2 * x + 3
```

For finite products, factors that do not depend on `x` are pulled out as
constants. When multiple factors depend on `x`, Aleph3 applies the product
rule:

$$
\frac{d}{dx}(u v) = u'v + uv'
$$

```text
D[3*x, x]                                -> 3
D[x*y, x]                                -> y
D[x*y*x, x]                              -> 2 * x * y
```

Division is differentiated by the same machinery. The calculus pack treats
`a / b` as `a * b^-1` and `1 / b` as `b^-1` for differentiation, then reuses
the product rule, power rule, and chain rule:

```text
D[1/x, x]                                -> -(x^-2)
D[x/(x + 1), x]                          -> -1 * x * (x + 1)^-2 + (x + 1)^-1
D[7/(x^2 + 3), x]                        -> -14 * x * (x^2 + 3)^-2
D[1/(Sin[x] + 2), x]                     -> -1 * (Cos[x]) * ((Sin[x]) + 2)^-2
D[Sin[x/(x + 1)], x]                     -> (-1 * x * (x + 1)^-2 + (x + 1)^-1) * (Cos[x / (x + 1)])
```

Aleph3 does not currently promise quotient-form simplification for these
results. For example, the derivative of `x/(x + 1)` is returned in
reciprocal-product form rather than recombined as `1/(x + 1)^2`.

Numeric powers use the power rule. When the base is composite, Aleph3
recursively differentiates the base through the same focused derivative
machinery:

$$
\frac{d}{dx}x^n = n x^{n-1}
$$

$$
\frac{d}{dx}u(x)^n = n u(x)^{n-1} u'(x)
$$

The exponent may be an exact integer or rational number.

```text
D[x^5, x]                                -> 5 * x^4
D[x^(3/2), x]                            -> 3/2 * x^1/2
D[(x + 1)^5, x]                          -> 5 * (x + 1)^4
D[(x^2 + 1)^5, x]                        -> 10 * x * (x^2 + 1)^4
D[(x*y + 1)^3, x]                        -> 3 * y * (x * y + 1)^2
D[(y^2 + 1)^5, x]                        -> 0
```

The first chain rules are available for `Sin`, `Cos`, `Exp`, `Log`, and
`Sqrt`. For a supported one-argument function $f$, Aleph3 uses
$\frac{d}{dx}f(u(x)) = f'(u(x))u'(x)$:

```text
D[Sin[x^2], x]                           -> 2 * x * (Cos[x^2])
D[Cos[x], x]                             -> -(Sin[x])
D[Exp[x^2], x]                           -> 2 * x * (Exp[x^2])
D[Log[x], x]                             -> x^-1
D[Sqrt[x], x]                            -> 1/2 * x^-1/2
```

Higher-order derivatives repeat the same focused first-derivative contract.
The order must be a nonnegative integer-valued numeric order inside the
supported limit. Order zero evaluates the input expression and returns it
unchanged.

```text
D[x^3, {x, 0}]                           -> x^3
D[x^3, {x, 1}]                           -> 3 * x^2
D[x^3, {x, 2}]                           -> 6 * x
D[x^3, {x, 3}]                           -> 6
D[x^3, {x, 4}]                           -> 0
Differentiate[x^3, {x, 2}]               -> 6 * x
D[Sin[x], {x, 2}]                        -> -(Sin[x])
```

Exact rational exponents and coefficients remain exact where the existing
arithmetic path supports them. Decimal inputs remain decimal expressions when
they participate in arithmetic.

Unsupported dependent function calls are preserved as derivatives instead of
being guessed:

```text
D[f[x], x]                               -> D[f[x], x]
```

This is intentional. The kernel does not invent a derivative for an unknown
head, and the calculus pack does not add private assumptions about user-defined
functions.

The differentiation variable must be a symbol:

```text
D[x, x + 1]                              -> kernel.invalid_form
D[x, {x + 1, 2}]                         -> kernel.invalid_form
D[x, {x, -1}]                            -> kernel.invalid_form
D[x, {x, 1/2}]                           -> kernel.invalid_form
D[x, {x, n}]                             -> kernel.invalid_form
```

The current calculus boundary excludes `Piecewise`, `Sum`, `Product`,
compact partial-derivative notation, integration, limits, branch-sensitive
assumption simplification, and broad special functions.
General symbolic exponent differentiation is also excluded: expressions such
as `D[u(x)^v(x), x]` and broad logarithmic differentiation remain unsupported
unless they fit the numeric-exponent power rule above.

The authoritative boundary is the
[focused differentiation specification](../calculus_differentiation_spec.md).
