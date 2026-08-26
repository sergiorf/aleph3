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
D[x/(x + 1), x]                          -> -(x * (x + 1)^-2) + (x + 1)^-1
D[7/(x^2 + 3), x]                        -> -14 * x * (x^2 + 3)^-2
D[1/(Sin[x] + 2), x]                     -> -((Cos[x]) * ((Sin[x]) + 2)^-2)
D[Sin[x/(x + 1)], x]                     -> ((x + 1)^-1 - x * (x + 1)^-2) * (Cos[x / (x + 1)])
```

Aleph3 does not currently promise quotient-form simplification for these
results. For example, the derivative of `x/(x + 1)` is returned in
reciprocal-product form rather than recombined as `1/(x + 1)^2`.

Powers are differentiated according to which parts depend on the
differentiation variable. When the exponent is independent of the variable,
Aleph3 uses the compact power rule. When the base is composite, Aleph3
recursively differentiates the base through the same focused derivative
machinery:

$$
\frac{d}{dx}x^n = n x^{n-1}
$$

$$
\frac{d}{dx}u(x)^n = n u(x)^{n-1} u'(x)
$$

The exponent may be numeric or symbolic as long as it is independent of the
differentiation variable.

```text
D[x^5, x]                                -> 5 * x^4
D[x^(3/2), x]                            -> 3/2 * x^1/2
D[x^a, x]                                -> a * x^(-1 + a)
D[(x + 1)^5, x]                          -> 5 * (x + 1)^4
D[(x^2 + 1)^5, x]                        -> 10 * x * (x^2 + 1)^4
D[(x*y + 1)^3, x]                        -> 3 * y * (x * y + 1)^2
D[(y^2 + 1)^5, x]                        -> 0
```

When the exponent depends on the differentiation variable, Aleph3 applies the
formal logarithmic differentiation rule:

$$
\frac{d}{dx}u(x)^{v(x)}
= u(x)^{v(x)}
\left(v'(x)\log(u(x)) + v(x)u'(x)u(x)^{-1}\right)
$$

If only the exponent depends on the variable, this simplifies to the
constant-base form:

$$
\frac{d}{dx}a^{v(x)} = a^{v(x)}\log(a)v'(x)
$$

```text
D[2^x, x]                                -> 2^x * (Log[2])
D[3^(x^2), x]                            -> 2 * x * 3^(x^2) * (Log[3])
D[x^x, x]                                -> (x * x^-1 + (Log[x])) * x^x
Assuming[x != 0, D[x^x, x]]              -> ((Log[x]) + 1) * x^x
D[x^Sin[x], x]                           -> x^Sin[x] * (x^-1 * (Sin[x]) + (Cos[x]) * (Log[x]))
D[(Sin[x])^Cos[x], x]                    -> ((Cos[x])^2 * (Sin[x])^-1 - (Log[Sin[x]]) * (Sin[x])) * (Sin[x])^(Cos[x])
```

These are formal symbolic derivatives. Aleph3 does not currently attach
branch conditions for `Log`, prove that the base is positive or nonzero, or
emit excluded-domain metadata for the introduced reciprocal of the base. For
that reason, products such as `x * x^-1` may remain visible until a shared
assumption such as `x != 0` is available.
Results may stay in reciprocal-product form and are not automatically
recombined into quotient form or transformed with trigonometric identities.
Generic multiplication canonicalization still removes identity factors,
collects numeric coefficients, combines repeated factors such as
`Cos[x] * Cos[x]`, and avoids unnecessary leading `-1 *` text.

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

The authoritative boundary is the
[focused differentiation specification](../calculus_differentiation_spec.md).
