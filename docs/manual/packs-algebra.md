# The Algebra Pack

The `core-algebra` pack owns the polynomial surface. It uses shared kernel
expressions, exact arithmetic, diagnostics, and registration rather than a
private evaluator. See [Polynomial Vocabulary](concepts-and-terminology.md#polynomial-vocabulary)
for monomials, total degree, and variable precedence.

## Expand And Collect

```text
Expand[(x + 1)*(x + 2)]                  -> x^2 + 3 * x + 2
Expand[(1/2)*(x + y)]                    -> 1/2 * x + 1/2 * y
Collect[y*x + x^2 + z*x, x]              -> x^2 + x * y + x * z
```

Exact rational coefficients remain exact. `Collect` returns a canonical
expanded expression, not a coefficient map.

## Factor

`Factor` supports monomial content and a focused univariate rational-root path:

```text
Factor[x^5 - x^3]                        -> x^3 * (x - 1) * (x + 1)
Factor[1/2*x^2 + x + 1/2]                -> 1/2 * (x + 1) * (x + 1)
```

General multivariate factorization is not yet supported.

## Polynomial GCD

```text
GCD[x^2 - 1, x - 1, x]                   -> x - 1
```

Aleph3 also supports a bounded exact multivariate case when the variable list
is explicit and at least one nonzero operand is a single monomial:

```text
GCD[x*y + x, x, {x, y}]                  -> x
GCD[x^2*y, x*y^2, {x, y}]                -> x * y
GCD[0, 2*x + 2*y, {x, y}]                -> x + y
```

For a polynomial $p$, the valuation $\nu_x(p)$ is the smallest exponent
of $x$ among its nonzero terms. The supported result uses
$\min(\nu_x(a),\nu_x(b))$ for every selected variable and normalizes its
coefficient to one. This finds shared monomial content; it is not a general
multivariate GCD algorithm.

Inferred multivariate selectors, decimal coefficients, two multi-term nonzero
operands, and `GCD[0, 0, {x, y}]` remain unsupported or invalid as appropriate.

## Quotient And Remainder

```text
PolynomialQuotient[x^2 + 1, x + 1, x]    -> {x - 1, 2}
```

The result satisfies `dividend = divisor*quotient + remainder`.

```text
Expand[(x + 1) * (x - 1) + 2]          -> x^2 + 1
```

Exact single-divisor multivariate division requires explicit precedence:

```text
PolynomialQuotient[x^2*y + x*y^2 + y, x*y, {x, y}]
    -> {x + y, y}
```

The variable list controls leading terms under graded lexicographic order.
Different precedence can change quotient and remainder while preserving the
reconstruction identity.

For the example above:

```text
x*y * (x + y) + y                      -> x^2*y + x*y^2 + y
```

Decimal multivariate division, multiple divisors, configurable orders,
general multivariate GCD, and broad multivariate factorization remain
unsupported.
Exact `int64_t` coefficient overflow is reported instead of wrapped.

Examples outside the current boundary include:

```text
GCD[x^2 - 1, y - 1]                    -> unsupported multivariate input
GCD[x + y, x - y, {x,y}]               -> unsupported two-multi-term input
PolynomialQuotient[x*y, x]             -> explicit selector required
PolynomialQuotient[0.5*x*y, x, {x,y}]  -> unsupported inexact division
```

The authoritative boundary is the
[supported algebra subset](../algebra_supported_subset.md).
