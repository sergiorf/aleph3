# Exact Vector Specification

## Ownership And Representation

Exact vectors are owned by the `core-algebra` pack. Public expressions use
ordinary non-empty flat lists:

```text
{1, 2, 3}
```

Vectors do not add a kernel `Expr` alternative and do not change scalar
`Plus`, `Times`, or list semantics. The pack converts supported lists to an
algebra-owned `DenseVector<ExactCoefficient>` value for vector algorithms.

## Supported Surface

- `Dot[v1, v2]` requires two vectors of equal length and returns an exact
  scalar.
- `Cross[v1, v2]` requires two three-dimensional vectors and returns an exact
  vector.
- `Norm[v]` computes the exact squared norm and returns an exact square root
  when one exists, otherwise a symbolic `Sqrt[exact_value]`.

Entries must be exact integers or normalized rationals. Results use exact
integers and rationals through the shared arbitrary-precision scalar model.
Exact vector operations never fall back to floating point.

Examples:

```text
Dot[{1, 2, 3}, {4, 5, 6}]               -> 32
Dot[{1/2, 2/3}, {3/4, 5/6}]             -> 67/72
Cross[{1, 0, 0}, {0, 1, 0}]             -> {0, 0, 1}
Cross[{1/2, 0, 0}, {0, 2/3, 0}]         -> {0, 0, 1/3}
Norm[{3, 4}]                            -> 5
Norm[{1/2, 2/3}]                        -> 5/6
Norm[{1, 1}]                            -> Sqrt[2]
```

## Exact Square Roots

The kernel exact-scalar layer preserves perfect square roots for exact integer
and rational inputs:

```text
Sqrt[9]                                  -> 3
Sqrt[25/36]                              -> 5/6
Sqrt[2]                                  -> Sqrt[2]
Sqrt[2/3]                                -> Sqrt[2/3]
Sqrt[2.0]                                -> 1.414214
```

Non-perfect exact inputs remain symbolic. Approximate inputs continue to use
the supported machine-real numeric path. Negative exact inputs keep the
existing real-domain behavior: they remain symbolic in ordinary evaluation and
report a numeric-domain diagnostic in strict runtime contexts.

## Diagnostics And Budgets

- Empty, non-list, or nested-list vectors report `kernel.invalid_form`.
- Length mismatches, non-3D `Cross` inputs, and oversized vectors report
  `kernel.domain_violation`.
- Symbolic, decimal, and complex entries report
  `kernel.unsupported_construct`.
- Vector dot, cross, and norm arithmetic charge the shared evaluation-step
  budget and report `kernel.step_budget_exhausted` when the configured budget
  is exhausted.

Inputs are limited to 4,096 elements.

## Unsupported Boundaries

The first vector surface excludes empty vectors, symbolic entries, decimal and
approximate vector algebra, complex vectors, tensors, `Angle`, `Normalize`,
`Projection`, `Distance`, sparse vectors, and implicit matrix/vector
reinterpretation. Matrices remain rectangular nested lists and are not treated
as vectors.
