# The Algebra Pack

The `core-algebra` pack owns the polynomial surface. It uses shared kernel
expressions, exact arithmetic, diagnostics, and registration rather than a
private evaluator. See [Polynomial Vocabulary](../concepts.md#polynomial-vocabulary)
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

The current GCD contract is univariate. Multivariate GCD is planned.

## Quotient And Remainder

```text
PolynomialQuotient[x^2 + 1, x + 1, x]    -> {x - 1, 2}
```

The result satisfies `dividend = divisor*quotient + remainder`.

Exact single-divisor multivariate division requires explicit precedence:

```text
PolynomialQuotient[x^2*y + x*y^2 + y, x*y, {x, y}]
    -> {x + y, y}
```

The variable list controls leading terms under graded lexicographic order.
Different precedence can change quotient and remainder while preserving the
reconstruction identity.

Decimal multivariate division, multiple divisors, configurable orders,
multivariate GCD, and broad multivariate factorization remain unsupported.
Exact `int64_t` coefficient overflow is reported instead of wrapped.

The authoritative boundary is the
[supported algebra subset](../algebra_supported_subset.md).
