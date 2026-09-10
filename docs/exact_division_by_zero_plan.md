# Exact Division-By-Zero Implementation Plan

Status: active implementation plan. Current behavior remains owned by the
focused specifications and manual until this plan is implemented and those
documents are updated.

## Goal

Standardize ordinary exact division by known zero across Aleph3.

Observable success:

- exact `Integer` and `Rational` division by a known zero denominator reports
  `runtime.division_by_zero`;
- `1/0`, `0/0`, `1/(2-2)`, and `(1/2)/(3-3)` no longer differ by expression
  shape;
- ordinary exact division does not construct `Infinity` or `Indeterminate`;
- valid exact rational arithmetic remains normalized and arbitrary precision;
- symbolic denominators such as `1/x` and `x/(x+1)` remain symbolic or follow
  existing rational-expression contracts until the denominator is known to be
  zero;
- polynomial scalar division, dense exact matrix operations, session recovery,
  SDK validation/runtime boundaries, and calculus uses of `Infinity` and
  `Indeterminate` keep their current intended behavior.

This advances the Unified Plan's Exact Algebra Depth and Quality workstreams.
The owner is the kernel exact arithmetic and evaluation path, with parser,
symbolic lowering, algebra-pack, session, CLI, SDK, and documentation
follow-through.

## Repository Evidence

Relevant canonical documents:

- [Unified Plan](aleph3_unified_plan.md), especially Exact Algebra Depth and
  Quality and Documentation.
- [Feature Development Workflow](feature_development_workflow.md).
- [Kernel Exact Algebra Spec](kernel_exact_algebra_spec.md).
- [Algebra Supported Subset](algebra_supported_subset.md).
- [Dense Matrix Spec](algebra_dense_matrix_spec.md).
- [Trusted Subset](trusted_subset_v1.md).
- [Expressions And Evaluation](manual/expressions-and-evaluation.md).

Initial code search shows the inconsistency is spread across multiple paths:

- parser literal rational handling in `include/parser/Parser.hpp`;
- source-aware symbolic lowering helpers in `src/syntax/SymbolicLowering.cpp`;
- exact simplification and divide handling in
  `src/evaluator/SimplificationRules.cpp`;
- builtin numeric dispatch and runtime divide diagnostics in
  `src/evaluator/EvaluatorBuiltins.cpp`;
- exact rational helpers in `include/expr/ExprUtils.hpp`;
- algebra conversion and pack-facing division-by-zero mapping in
  `src/algebra/*` and `src/packs/AlgebraPack.cpp`;
- existing tests in `tests/evaluator`, `tests/session`, `tests/sdk`,
  `tests/algebra`, `tests/notebook`, parser tests, and structural tests for
  symbolic infinity objects.

The old behavior is not a single evaluator rule. Literal exact forms can be
lowered to `Infinity` or `Indeterminate` before the evaluator sees a
`Divide`, while evaluated-zero denominators can reach the runtime
division-by-zero path.

## Chosen Design

Adopt this semantic distinction:

- exact ordinary arithmetic: `exact_nonzero / 0` and `0 / 0` are runtime
  division-by-zero errors;
- inexact machine-real arithmetic: preserve the current intended runtime
  policy after inspection, and do not broaden this plan into a redesign of
  IEEE-754 or approximate-number behavior;
- symbolic and calculus objects: keep `Infinity`, `ComplexInfinity`, and
  `Indeterminate` available where they are valid symbolic objects, including
  calculus or limit-oriented code.

Preferred implementation shape:

- centralize the exact zero-denominator decision in the smallest existing
  kernel/evaluator helper that fits local ownership;
- detect exact zero after normal operand evaluation for ordinary `Divide`;
- route known exact zero denominators through
  `kernel::ErrorCode::division_by_zero` and the existing runtime diagnostic
  message;
- preserve `Rational` invariants: denominator positive, denominator nonzero,
  normalized numerator and denominator, denominator-one values canonicalized
  to `Integer` where the current scalar model does so;
- leave symbolic denominators unresolved rather than throwing because they
  could become zero later.

Rejected alternatives:

- keep `1/0 -> Infinity` and `0/0 -> Indeterminate` for literal exact input;
- represent invalid exact rationals with denominator zero and interpret them
  later;
- remove `Infinity` or `Indeterminate` from the expression model;
- change floating-point divide-by-zero policy as a side effect.

## Implementation Slices

### 1. Baseline And Red Tests

Record current behavior for:

- direct exact forms: `1/0`, `-1/0`, `42/0`, `0/0`, and a large exact
  integer divided by zero;
- evaluated denominators: `1/(1-1)`, `1/(2-2)`, `1/(2*0)`, `1/(0+0)`, and a
  large exact cancellation;
- rational forms: `(1/2)/0`, `(1/2)/(3-3)`, `1/(0/3)`, and
  `1/((2/6)-(1/3))` where the parser and evaluator admit those forms;
- valid exact division: `2/4`, `2/(-4)`, `0/17`, `1/(2/3)`,
  `(1/2)/(3/4)`, and existing arbitrary-precision rational normalization
  cases.

Completion criterion: focused tests fail for the current shape-dependent
behavior and identify the expected diagnostic code for each public layer.

### 2. Syntax And Lowering

Update parser and symbolic lowering paths that currently construct
`Infinity` or `Indeterminate` from zero-denominator exact rationals.

The desired syntax behavior is:

- exact zero denominators in ordinary expressions are preserved as `Divide`
  forms or reported through the standard runtime path, according to the
  owning layer's current error model;
- parse/lowering validation remains distinct from runtime evaluation where the
  existing layer already makes that distinction;
- nonzero exact rational literals continue to normalize immediately.

Completion criterion: literal `1/0` and `0/0` no longer bypass runtime exact
division semantics by becoming symbolic infinity objects during parsing or
lowering.

### 3. Evaluator Exact Divide Semantics

Update ordinary `Divide` evaluation and simplification so exact evaluated
denominators are checked by value, not by original syntax.

Cover at least:

- `Integer / Integer`;
- `Integer / Rational`;
- `Rational / Integer`;
- `Rational / Rational`;
- nested evaluated exact denominators.

Keep valid exact canonicalization unchanged.

Completion criterion: all exact scalar combinations with a known zero
denominator throw the existing division-by-zero runtime error, and valid exact
division still produces canonical exact results.

### 4. Algebra And Symbolic Boundaries

Verify polynomial and rational-expression behavior at public algebra
boundaries.

Required preservation:

- `Expand[2*x/3]`, `Expand[(2*x)/3]`, `Expand[2*(x/3)]`, and
  `Expand[(x + 1)/3]` keep exact rational coefficients;
- `Expand[x/0]` and `Expand[(x+1)/(2-2)]` report the same runtime
  division-by-zero diagnostic;
- `Expand[x/(x+1)]` keeps its current unsupported polynomial-subset behavior,
  not division by zero;
- rational-expression forms such as `x/(x+1)` and `1/x` keep existing
  symbolic/domain behavior.

Completion criterion: exact scalar zero denominators fail consistently without
turning symbolic rational functions into eager runtime errors.

### 5. Cross-Surface Regressions

Update or add coverage for:

- session and REPL recovery after `1/0` and `0/0`;
- assignment-introduced zero denominators such as `a = 0; 1/a` and
  `a = 2; b = a-a; 1/b`;
- short-circuiting forms that must not evaluate dead `1/0` branches;
- SDK validation/runtime expectations, preserving
  `semantics.validator.division_by_zero` where validation owns the failure and
  `runtime.division_by_zero` where runtime owns it;
- dense exact matrix smoke cases: determinant, row reduction, and linear solve
  over rational entries;
- existing structural, pretty-print, full-form, and calculus tests involving
  `Infinity`, `ComplexInfinity`, and `Indeterminate`.

Completion criterion: public consumers share the same kernel semantics, while
their layer-specific diagnostic codes remain intentional.

### 6. Documentation And Help

Update current-behavior documentation in the same implementation change:

- `docs/kernel_exact_algebra_spec.md`: exact scalar division-by-zero contract;
- `docs/manual/expressions-and-evaluation.md`: examples showing exact
  division by zero as a diagnostic and clarifying exact versus approximate
  number boundaries;
- `docs/algebra_supported_subset.md`: polynomial scalar division by zero and
  symbolic denominator boundaries if the existing text is incomplete;
- `include/help/HelpTexts.hpp`: only if the current `Divide` help needs an
  explicit diagnostic or exactness note.

Completion criterion: manual examples match executable behavior, focused specs
state the durable contract, and no affected doc claims exact `1/0` evaluates
to `Infinity` or exact `0/0` evaluates to `Indeterminate`.

## Verification Plan

Use the feature workflow gates:

1. Baseline: run the closest current evaluator, algebra, session, SDK, and
   parser tests and record unrelated failures.
2. Red: add focused failing tests for exact direct and evaluated zero
   denominators.
3. Green: implement each slice with focused verification after it.
4. Broader verification: run affected evaluator, parser, algebra, session,
   notebook, SDK, calculus, and dense-matrix suites.
5. Review: inspect the diff for duplicate divide-by-zero semantics, accidental
   floating-point changes, stale documentation, and unintended removal of
   symbolic infinity objects.

Representative acceptance expressions:

```text
1/0
0/0
-1/0
42/0
1/(2-2)
(1/2)/(3-3)

2/4
(-2)/(-4)
2/(-4)
0/17
1/(2/3)
(1/2)/(3/4)

Expand[2*x/3]
Expand[(x + 1)/3]
Expand[(x + 1/3)^2 - (x^2 + 2*x/3 + 1/9)]
Expand[x/0]

1/0
1/2 + 1/2
```

Expected high-level results:

- every known exact zero denominator reports division by zero;
- valid exact arithmetic remains exact and normalized;
- `Expand[x/0]` reports division by zero;
- evaluation after an error still succeeds.

## Completion Report Requirements

The final implementation report should include:

1. the root cause of the old inconsistency;
2. every production file changed;
3. where exact zero-denominator semantics now live;
4. old behavior versus new behavior;
5. tests modified and added;
6. focused and broader verification commands with results;
7. observed floating-point behavior and whether it was intentionally left
   unchanged;
8. confirmation that `Infinity` and `Indeterminate` remain available for
   symbolic or calculus use;
9. follow-up semantic issues discovered and intentionally kept out of scope.

When this implementation is complete and the canonical specs/manual own the
new behavior, move this file under `docs/archive/` or remove it if it no
longer carries useful migration context.
