# The Algebra Pack

The `core-algebra` pack owns the polynomial surface. It uses shared kernel
expressions, exact arithmetic, diagnostics, and registration rather than a
private evaluator. See [Polynomial Vocabulary](concepts-and-terminology.md#polynomial-vocabulary)
for monomials, total degree, and variable precedence.

## Exact Coefficient Boundary

Supported exact polynomial helpers use checked `int64_t` integer and rational
coefficients. Exact rational coefficients stay exact in the documented
polynomial subset:

```text
Expand[(1/3*x + 1/6)*6]                 -> 2 * x + 1
GCD[x^2 - 1/9, x - 1/3, x]              -> x - 1/3
PolynomialQuotient[x^2 - 1/9, x - 1/3, x]
    -> {x + 1/3, 0}
PolynomialRemainder[x^2 + 1, x + 1, x]  -> 2
LeadingCoefficient[(1/2)*x^2 + x, x]    -> 1/2
```

If an exact coefficient intermediate overflows the checked representation,
Aleph3 reports an exact-overflow diagnostic. It does not wrap and does not
fall back to approximate arithmetic. Decimal inputs are inexact; they only use
the documented transitional inexact polynomial paths and are rejected by
exact-only helpers such as rational-expression transformations and exact
multivariate division.

## Expand And Collect

```text
Expand[(x + 1)*(x + 2)]                  -> x^2 + 3 * x + 2
Expand[(x + 1)^3]                        -> x^3 + 3 * x^2 + 3 * x + 1
Expand[(1/2)*(x + y)]                    -> 1/2 * x + 1/2 * y
Collect[y*x + x^2 + z*x, x]              -> x^2 + x * y + x * z
```

Exact rational coefficients remain exact. `Collect` returns a canonical
expanded expression, not a coefficient map.

Algebra helpers evaluate expression operands through the shared session state,
but explicit variable selectors remain symbolic. This lets assigned polynomial
names participate in algebra while selectors are not replaced by their current
values:

```text
p = (x + 1)^2                            -> p
Expand[p]                                -> x^2 + 2 * x + 1
x = 99                                   -> x
Collect[x^2 + 1, x]                      -> x^2 + 1
Collect[p, x]                            -> x^2 + 2 * x + 1
```

## Factor

`Factor` supports monomial content and a focused univariate rational-root path:

```text
Factor[x^5 - x^3]                        -> x^3 * (x - 1) * (x + 1)
Factor[1/2*x^2 + x + 1/2]                -> 1/2 * (x + 1) * (x + 1)
```

General multivariate factorization is not yet supported.
Supported univariate integer and rational coefficients are handled by the
exact polynomial path with checked `int64_t` arithmetic. Decimal inputs remain
outside this exact factorization subset.

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
PolynomialRemainder[x^2 + 1, x + 1, x]   -> 2
```

The result satisfies `dividend = divisor*quotient + remainder`.

```text
Expand[(x + 1) * (x - 1) + 2]          -> x^2 + 1
```

Exact single-divisor multivariate division requires explicit precedence:

```text
PolynomialQuotient[x^2*y + x*y^2 + y, x*y, {x, y}]
    -> {x + y, y}
PolynomialRemainder[x^2*y + x*y^2 + y, x*y, {x, y}]
    -> y
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
PolynomialRemainder[x*y, x]            -> explicit selector required
PolynomialQuotient[0.5*x*y, x, {x,y}]  -> unsupported inexact division
```

## Polynomial Inspection

`PolynomialDegree` and `LeadingCoefficient` inspect the current exact
univariate polynomial subset:

```text
PolynomialDegree[3*x^2 + 2*x + 1, x]   -> 2
PolynomialDegree[7, x]                 -> 0
LeadingCoefficient[3*x^2 + 2*x + 1, x] -> 3
LeadingCoefficient[(1/2)*x^2 + x, x]   -> 1/2
```

The selector must be one symbol. Decimal coefficients, symbolic coefficients,
non-polynomial inputs, unsupported variables outside the selected univariate
polynomial, negative or symbolic exponents, and exact coefficient overflow are
rejected explicitly. The zero polynomial is a domain violation in this slice;
Aleph3 does not yet expose a first-class negative-infinity degree value.

## Coefficient Extraction

`Coefficient` and `CoefficientList` read exact coefficients from the current
polynomial subset. They do not collect symbolic coefficients and they do not
simplify rational expressions.

```text
Coefficient[3*x^2 + 2*x + 1, x]       -> 2
Coefficient[3*x^2 + 2*x + 1, x, 2]    -> 3
Coefficient[3*x^2 + 2*x + 1, x, 0]    -> 1
CoefficientList[(1/2)*x^2 + x, x]     -> {0, 1, 1/2}
```

`Coefficient[poly, x]` means the coefficient of `x^1`.
`Coefficient[poly, x, n]` requires `n` to be a non-negative integer.
`CoefficientList[poly, x]` returns coefficients from degree zero through the
largest exponent of `x`.

The selector must be a single symbol. Decimal coefficients, symbolic
coefficients, non-polynomial inputs, unsupported variables outside the selected
univariate polynomial, negative or symbolic exponents, and exact coefficient
overflow are rejected explicitly.

## Rational Expression Parts

`Numerator` and `Denominator` expose the exact polynomial numerator and
denominator of the current bounded rational-expression subset:

```text
Numerator[1/2]                          -> 1
Denominator[1/2]                        -> 2
Numerator[(1/2)*x]                      -> x
Denominator[(1/2)*x]                    -> 2
Numerator[x/(x + 1)]                    -> x
Denominator[x/(x + 1)]                  -> x + 1
```

The helpers clear exact rational coefficients, so `(1/2)*x` is treated as
`x/2`. Supported inputs are exact integers, exact rationals, supported exact
polynomial expressions, products of supported rational-expression factors, and
explicit division with a nonzero supported denominator.

Decimal coefficients, symbolic coefficients outside the selected polynomial
variables, unsupported powers, and denominator zero are rejected explicitly.

## Rational Expression Transformations

`Together` combines a supported exact rational expression into one fraction:

```text
Together[1/x + 1/y]                     -> (x + y) / (x * y)
Together[1/2 + 1/x]                     -> (x + 2) / (2 * x)
Together[x/(x + 1) + 1/(x + 1)]         -> (x + 1) / (x + 1)
```

`Together` does not cancel common polynomial factors. Use `Cancel` when the
shared factor is inside the current exact polynomial GCD subset:

```text
Cancel[(x^2 - 1)/(x - 1)]               -> x + 1
Cancel[(1/2*x)/(1/4)]                   -> 2 * x
Cancel[(x*y)/x]                         -> y
```

Cancellation is valid on the original expression's nonzero-denominator
domain. Aleph3 now preserves supported denominator exclusions internally for
condition-aware operations, but the printed result is still only the simplified
expression. Decimal coefficients, symbolic coefficients outside the selected
polynomial variables, unsupported powers, denominator zero, and general
multivariate cancellation such as `Cancel[(x*y + x)/(x + 1)]` are rejected
explicitly. `Apart` remains outside the supported subset.

## Equivalence

`Equivalent[expr1, expr2]` proves equivalence only inside the current exact
algebra subset:

```text
Equivalent[x + 1, 1 + x]                         -> True
Equivalent[x^2 + 2*x + 1, x^2 + x + x + 1]       -> True
Equivalent[x + 1, x + 2]                         -> False
Equivalent[1/x, 1/x]                             -> True
```

The result `Unknown` means Aleph3 does not have an unconditional proof in this
slice. It does not mean the statement is false.

Rational-expression equivalence is domain-sensitive:

```text
Equivalent[(x^2 - 1)/(x - 1), x + 1]             -> Unknown
Equivalent[(x*y)/x, y]                           -> Unknown
Equivalent[Cancel[(x*y)/x], y]                   -> True
```

The first two comparisons simplify to the same printed expression only after
dropping a nonzero-denominator condition. Because Aleph3 does not yet expose a
public conditional-equivalence result, they return `Unknown`.

Trigonometric identities, numerical sampling proofs, broad branch reasoning,
solving, quantifiers, and general theorem proving are outside this subset:

```text
Equivalent[Sin[x]^2 + Cos[x]^2, 1]               -> Unknown
Equivalent[Sqrt[x^2], x]                         -> Unknown
```

## Exact Dense Matrices

Matrices are written as rectangular nested lists. The algebra pack validates
their shape and computes with checked exact integers and rationals:

```text
MatrixAdd[{{1, 1/2}, {2, 3}}, {{4, 1/2}, {5, 6}}] -> {{5, 1}, {7, 9}}
MatrixMultiply[{{1, 2, 3}}, {{1}, {0}, {2}}]       -> {{7}}
IdentityMatrix[2]                                   -> {{1, 0}, {0, 1}}
Transpose[{{1, 2, 3}, {4, 5, 6}}]                  -> {{1, 4}, {2, 5}, {3, 6}}
Det[{{1, 2}, {3, 4}}]                              -> -2
RowReduce[{{1, 2}, {3, 4}}]                        -> {{1, 0}, {0, 1}}
LinearSolve[{{2, 1}, {1, -1}}, {5, 1}]             -> {2, 1}
```

Rows must be non-empty and equally sized. A matrix may contain at most 4,096
entries. Decimal, symbolic, complex, empty, and ragged matrices are rejected;
`LinearSolve` currently supports only square systems with one unique exact
solution. Matrix multiplication and elimination use the shared evaluation
budget. There is no implicit list or tensor interpretation.

The authoritative boundary is the
[supported algebra subset](../algebra_supported_subset.md).
