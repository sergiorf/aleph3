# Exact Vector Algebra Plan

## Status And Scope

Status: complete.

This plan records the implemented direction for a small exact vector-algebra
surface in the `core-algebra` pack. The durable current behavior now belongs
in the supported algebra subset, the vector specification, the manual, help
text, and tests.

The milestone adds:

- `Dot[v1, v2]`
- `Cross[v1, v2]`
- `Norm[v]`
- exact integer and rational square-root preservation needed by `Norm`

It deliberately does not add `Angle`, `Normalize`, `Projection`, `Distance`,
symbolic vectors, complex vectors, approximate vector algebra, tensors, or
private evaluator semantics.

## Alignment

This work advances the Unified Plan's Exact Algebra Depth and Bounded
Numerical and Finite Data MVP tracks. The owning subsystem is the
`core-algebra` pack for vector algorithms and public pack functions, with a
kernel exact-scalar improvement for square-root preservation. The kernel
remains the only semantic core; packs continue to register behavior through
the shared function registry.

The implementation must preserve the current exactness rule:

```text
exact input -> exact or symbolic output
approximate input -> approximate output only under an explicit supported contract
```

For this milestone, vector inputs are exact-only. Approximate vector algebra is
deferred even though approximate scalar `Sqrt` remains supported.

## Repository Research Checklist

Before editing implementation files, inspect:

- `src/packs/AlgebraPack.cpp`
- `include/algebra/DenseMatrix.hpp`
- `include/algebra/ExactPolynomial.hpp`
- `include/kernel/ExactScalar.hpp`
- `src/kernel/ExactScalar.cpp`
- `src/evaluator/EvaluatorBuiltins.cpp`
- `src/evaluator/EvaluatorSemantics.cpp`
- `tests/packs/AlgebraPackTests.cpp`
- `tests/session/SessionTests.cpp`
- `tests/evaluator/EvaluatorTests.cpp`
- `tests/evaluator/EvaluatorFunctionsTests.cpp`
- `docs/algebra_dense_matrix_spec.md`
- `docs/algebra_supported_subset.md`
- `docs/manual/packs-algebra.md`
- `docs/manual/built-in-functions.md`
- `include/help/HelpTexts.hpp`

Confirm the current representation of `ExactCoefficient`,
`ExactInteger`, and `ExactRational`; how exact integers and rationals are
converted from `Expr`; how matrix operations consume the shared evaluation
budget; how runtime diagnostics are mapped; how pack functions are registered;
how exact `Power` is implemented; and how `Sqrt` currently handles exact and
approximate inputs.

## Design

Public vectors remain ordinary Aleph lists:

```text
{1, 2, 3}
```

No kernel `Expr` variant is added for vectors.

Add an algebra-layer vector value under `include/algebra`, preferably
`DenseVector.hpp`:

```cpp
template <typename Scalar>
class DenseVector {
public:
    explicit DenseVector(std::vector<Scalar> values);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const Scalar& operator[](std::size_t index) const;
    [[nodiscard]] Scalar& operator[](std::size_t index);
    [[nodiscard]] const std::vector<Scalar>& values() const noexcept;

private:
    std::vector<Scalar> values_;
};
```

Keep vector algorithms independent of `Expr`. `AlgebraPack.cpp` should handle
argument evaluation, conversion, diagnostics mapping, evaluation-budget
hookup, and conversion back to expressions.

Use the current 4,096-element algebra limit for vectors unless repository
evidence reveals a more appropriate shared limit.

## Exact Scalar And Square Root

Rename the matrix-specific scalar conversion helper in `AlgebraPack.cpp` from
`exact_matrix_scalar` to a reusable exact algebra scalar helper. The accepted
subset remains exact integers and exact rationals only. This refactor must not
broaden matrix semantics.

Add exact square-root helpers at the kernel exact-scalar layer, not in the
algebra pack. The helpers should support arbitrary-precision integers and
normalized exact rationals:

- an exact integer square root exists only for nonnegative perfect squares
- an exact rational square root exists only when the normalized numerator and
  denominator are perfect squares
- implementations must not convert to `double`
- returned roots must be verified by `root * root == value`

Update builtin `Sqrt` so exact integer and rational inputs return:

```text
Sqrt[0]      -> 0
Sqrt[1]      -> 1
Sqrt[9]      -> 3
Sqrt[25/36]  -> 5/6
Sqrt[2]      -> Sqrt[2]
Sqrt[2/3]    -> Sqrt[2/3]
Sqrt[2.0]    -> approximate machine-real result
```

Preserve the current negative exact input policy. Do not add complex-number
support in this milestone.

## Vector Conversion

In the algebra pack, add:

```cpp
using ExactVector = algebra::DenseVector<ExactCoefficient>;
ExactVector exact_vector_from_expr(const ExprPtr& expr);
ExprPtr exact_vector_to_expr(const ExactVector& vector);
```

`exact_vector_from_expr` accepts only non-empty flat lists of exact integers
or rationals. It rejects:

- `{}` as invalid form
- nested lists such as `{{1, 2}, {3, 4}}` as invalid vector form
- symbolic entries such as `{1, x, 3}` as unsupported constructs
- decimal entries such as `{1, 2.5, 3}` as unsupported constructs
- oversized vectors as domain violations

Do not treat a matrix as a vector.

## Algebra Operations

Add pure algebra-layer operations:

```cpp
template <typename Scalar>
Scalar dot_product(
    const DenseVector<Scalar>& left,
    const DenseVector<Scalar>& right,
    const AlgebraWork& work = {});

template <typename Scalar>
DenseVector<Scalar> cross_product(
    const DenseVector<Scalar>& left,
    const DenseVector<Scalar>& right,
    const AlgebraWork& work = {});

template <typename Scalar>
Scalar squared_norm(
    const DenseVector<Scalar>& vector,
    const AlgebraWork& work = {});
```

If useful, generalize `MatrixWork` to `AlgebraWork`; avoid churn if a
compatible alias is enough.

`dot_product` requires equal lengths and consumes shared evaluation work
consistently with matrix scalar updates. Length mismatch is a domain violation.

`cross_product` supports only three-dimensional vectors. Non-3D inputs are
domain violations.

`squared_norm` is internal and may delegate to `dot_product(vector, vector,
work)`. Do not expose `SquaredNorm` publicly in this milestone.

## Public Behavior

Register these pack functions:

```text
Dot
Cross
Norm
```

Expected exact outputs:

```text
Dot[{1,2,3}, {4,5,6}]                         -> 32
Dot[{1/2,2/3}, {3/4,5/6}]                     -> 67/72
Dot[{-1,2,-3}, {4,-5,6}]                      -> -32
Dot[{9223372036854775808}, {2}]               -> 18446744073709551616

Cross[{1,0,0}, {0,1,0}]                       -> {0,0,1}
Cross[{0,1,0}, {1,0,0}]                       -> {0,0,-1}
Cross[{1,2,3}, {2,4,6}]                       -> {0,0,0}
Cross[{1/2,0,0}, {0,2/3,0}]                   -> {0,0,1/3}

Norm[{3,4}]                                   -> 5
Norm[{1/2,2/3}]                               -> 5/6
Norm[{0,0,0}]                                 -> 0
Norm[{1,1}]                                   -> Sqrt[2]
Norm[{1,2,2}]                                 -> 3
```

`Norm` computes the exact squared norm, then returns an exact square root when
one exists, otherwise a symbolic `Sqrt[exact_value]`. It must not call an
approximate square-root path for exact vector inputs.

## Diagnostics

Follow existing runtime diagnostic conventions exactly.

Invalid form examples:

```text
Dot[{}, {}]
Norm[{}]
Dot[{{1,2}}, {{3,4}}]
```

Unsupported construct examples:

```text
Dot[{x,1}, {2,3}]
Dot[{1.2,2}, {3,4}]
```

Domain violation examples:

```text
Dot[{1,2}, {3,4,5}]
Cross[{1,2}, {3,4}]
Cross[{1,2,3,4}, {5,6,7,8}]
```

Suggested messages:

```text
Dot requires vectors of equal length
Cross requires two three-dimensional vectors
```

## Deferred Angle

Do not implement `Angle` in this milestone.

`Angle[a, b]` would naturally require:

```text
ArcCos[Dot[a,b] / (Norm[a] * Norm[b])]
```

That raises separate contracts for zero-vector domains, exact symbolic
trigonometric values, `ArcCos`, products of square roots, and possible
approximate fallback. A later `Angle` tranche should specify exact parallel
and perpendicular simplifications, symbolic fallback, and approximate behavior
explicitly before implementation.

## Tests

Add algebra-layer tests for:

- dot product integer, rational, orthogonal, signed, and arbitrary-precision
  cases
- cross product basis, anticommutative, parallel, and rational cases
- squared norm where useful

Add pack or session-level tests for:

- all public examples above
- malformed vectors, unsupported entries, domain violations, and arity
- registration metadata for `Dot`, `Cross`, and `Norm`
- session discovery and completion ordering
- help entry presence and rich metadata
- exact `Sqrt` behavior for perfect and non-perfect integer/rational inputs
- approximate `Sqrt[2.0]` continuing to work
- evaluation-budget exhaustion for a sufficiently constrained large vector
  calculation

Add invariant tests:

```text
Dot[Cross[{1,2,3},{4,5,6}], {1,2,3}] -> 0
Dot[Cross[{1,2,3},{4,5,6}], {4,5,6}] -> 0
```

Also test representative commutativity for `Dot[a,b] == Dot[b,a]` and
anticommutativity for `Cross[a,b] == -Cross[b,a]` using explicit expected
vectors.

## Documentation

Add `docs/algebra_vector_spec.md` when implementing the feature. It should
document representation, supported scalar types, size limits, `Dot`, `Cross`,
`Norm`, exactness guarantees, diagnostics, unsupported symbolic and
approximate cases, and evaluation-budget behavior.

Update:

- `docs/algebra_supported_subset.md`
- `docs/manual/packs-algebra.md`
- `docs/manual/built-in-functions.md` for exact `Sqrt`
- `include/help/HelpTexts.hpp`
- `docs/README.md` to link the new vector specification

Do not document planned behavior as current behavior before implementation.

## Validation

Final validation for the implementation should include:

1. Build the project.
2. Run focused algebra, evaluator, pack, and session tests.
3. Run all existing tests when feasible.
4. Verify manual/help examples against executable behavior or tests.
5. Check local documentation links touched by the change.
6. Review the final diff for duplicated semantics, stale docs, and accidental
   approximate fallback.
7. Confirm no exact vector operation converts through `double`.
8. Confirm existing matrix behavior is unchanged.

Manual REPL checks:

```text
Dot[{1,2,3},{4,5,6}]
Dot[{1/2,2/3},{3/4,5/6}]
Dot[{9223372036854775808},{2}]

Cross[{1,0,0},{0,1,0}]
Cross[{0,1,0},{1,0,0}]
Cross[{1/2,0,0},{0,2/3,0}]

Norm[{3,4}]
Norm[{1/2,2/3}]
Norm[{1,1}]
Norm[{0,0,0}]

Sqrt[9]
Sqrt[25/36]
Sqrt[2]
Sqrt[2.0]
```

Expected important results:

```text
32
67/72
18446744073709551616

{0,0,1}
{0,0,-1}
{0,0,1/3}

5
5/6
Sqrt[2]
0

3
5/6
Sqrt[2]
approximately 1.414213...
```
