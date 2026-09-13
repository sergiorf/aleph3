# Rational-Expression Domain Conditions Plan

## Status And Scope

This is the active implementation plan for the next rational-expression domain
slice referenced by the [Unified Plan](aleph3_unified_plan.md). The completed
exact division-by-zero migration is archived at
[docs/archive/exact_division_by_zero_plan.md](archive/exact_division_by_zero_plan.md).

This plan is not a broad conditional algebra system. It defines the smallest
next step for consuming the internal excluded-denominator metadata already
preserved by exact rational-expression transformations.

The work is a substantial feature under the
[Feature Development Workflow](feature_development_workflow.md) because it
changes public symbolic behavior and supported algebra boundaries. Complete
design approval is required before implementation changes.

## Goal

Expose a bounded, condition-aware way to use rational-expression domain
restrictions without weakening exactness or inventing private semantics outside
the kernel and `core-algebra` pack.

Observable success:

- users can distinguish an unconditional equivalence from an equivalence that
  requires nonzero denominator conditions;
- existing unconditional `Equivalent` results remain stable unless the approved
  design explicitly changes them;
- denominator exclusions preserved by `Cancel` and rational-expression
  normalization have one public consumer with documented output;
- unsupported identities still return `Unknown` or explicit diagnostics rather
  than falling back to numerical sampling or approximate proof;
- `Apart`, broad branch reasoning, assumptions, solving, and general theorem
  proving remain deferred.

## Roadmap Alignment

Unified Plan workstream:

- Symbolic MVP Gap Closure: consume internal rational-expression
  excluded-domain metadata from a condition-aware transformation or equivalence
  workflow before broadening rational-expression transformations.
- CAS Engine Roadmap Gap Closure: keep domain restrictions explicit before
  broad solving, integration, validation, or tutor-grade transformation
  checking.

Owner:

- Kernel owns the `DomainRestrictions` metadata carrier.
- The `core-algebra` pack owns rational-expression transformations and the
  public equivalence-facing behavior.
- CLI, session, notebook, SDK, and future web surfaces consume the shared
  kernel and pack behavior. They must not add private condition semantics.

## Current Repository Evidence

Canonical contracts:

- [Kernel Exact Algebra Spec](kernel_exact_algebra_spec.md) states that exact
  rational-expression transformations carry internal excluded-zero denominator
  metadata.
- [Algebra Supported Subset](algebra_supported_subset.md) documents `Together`,
  `Cancel`, and `Equivalent`, including the current lack of public condition
  output.
- [Algebra Equivalence Spec](algebra_equivalence_spec.md) defines `Equivalent`
  as `True`, `False`, or `Unknown` and intentionally returns `Unknown` when an
  unconditional proof would drop denominator restrictions.
- [Manual Algebra Pack Guide](manual/packs-algebra.md) explains the current
  user-visible behavior and examples.

Implementation entry points:

- `include/kernel/DomainRestrictions.hpp` stores excluded-zero expressions.
- `src/kernel/DomainRestrictions.cpp` canonicalizes, deduplicates, and renders
  restriction strings.
- `src/algebra/ExactRationalExpression.cpp` adds denominator and canceled-factor
  restrictions during rational-expression normalization and cancellation.
- `src/algebra/ExactEquivalence.cpp` compares restriction metadata when proving
  rational-expression equivalence.
- `src/packs/AlgebraPack.cpp` registers the public algebra functions.

Representative current behavior:

```text
Cancel[(x^2 - 1)/(x - 1)]               -> x + 1
Equivalent[(x^2 - 1)/(x - 1), x + 1]    -> Unknown
Equivalent[(x*y)/x, y]                  -> Unknown
Equivalent[Cancel[(x*y)/x], y]          -> True
Equivalent[1/x, 1/x]                    -> True
```

The first two `Unknown` results are intentional today: the simplified printed
expressions match only after dropping a condition such as `x - 1 != 0` or
`x != 0`.

## Design Decision To Approve

Choose exactly one first public condition surface before implementation:

1. A new pack function, such as `EquivalentConditions[expr1, expr2]`, that
   reports the conditions under which the current exact rational-expression
   proof succeeds.
2. A bounded result wrapper, such as `ConditionalExpression[result, condition]`,
   returned by a new condition-aware equivalence function.
3. A deliberately narrower inspection helper, such as
   `DomainRestrictions[expr]`, that exposes preserved denominator exclusions
   without changing proof results.

Preferred direction for the first slice: a new `core-algebra` pack function
that leaves existing `Equivalent` behavior unchanged and exposes conditional
equivalence only for the current exact rational-expression subset. This avoids
surprising users who rely on `Equivalent` returning only `True`, `False`, or
`Unknown`, and it keeps the design away from broad `ConditionalExpression`
semantics until those are specified.

Rejected directions for this slice:

- changing `Cancel` to print conditions inline;
- making `Equivalent` return a new fourth result without a compatibility
  decision;
- adding `Apart`;
- using numerical sampling as proof evidence;
- adding broad assumptions, inequality solving, quantifiers, or branch-cut
  reasoning.

## Proposed First Public Contract

This section is the starting proposal for approval, not current behavior.

Add a condition-aware equivalence helper owned by `core-algebra`:

```text
EquivalentConditions[expr1, expr2]
```

Result shape:

- `True` when equivalence is unconditional in the current exact subset;
- `False` when non-equivalence is proven in the current exact subset;
- a list of nonzero conditions when the expressions are equivalent only under
  preserved denominator exclusions;
- `Unknown` when the current proof methods cannot decide.

Condition rendering:

```text
EquivalentConditions[(x^2 - 1)/(x - 1), x + 1] -> {x - 1 != 0}
EquivalentConditions[(x*y)/x, y]               -> {x != 0}
EquivalentConditions[1/x, 1/x]                 -> True
EquivalentConditions[x + 1, x + 2]             -> False
EquivalentConditions[Sin[x]^2 + Cos[x]^2, 1]   -> Unknown
```

Open design points that must be resolved before code:

- whether Aleph3 already has or should add a first-class `Unequal`/`NotEqual`
  expression, or whether this helper should use a narrower condition form;
- whether a one-condition result is a singleton list or a dedicated condition
  expression;
- whether conditions should be returned only when both sides reduce to the same
  numerator and denominator after cancellation, or also when one side is an
  explicit `Cancel[...]` result with lost metadata;
- how exact zero denominators discovered during conversion report
  `runtime.division_by_zero`;
- whether this helper belongs in the current notebook MVP tranche or should
  wait until after the remaining session/reset and lexical-binding work.

## Implementation Slices

### 1. Design Approval And Spec Update

Define the durable public contract in
[Algebra Equivalence Specification](algebra_equivalence_spec.md) and
[Algebra Supported Subset](algebra_supported_subset.md).

Completion criteria:

- function name, result shape, examples, and unsupported boundaries are
  approved;
- the spec states whether existing `Equivalent` behavior changes;
- the spec states exactness, diagnostics, and non-goals;
- no implementation files are changed before this approval.

### 2. Internal Proof Result Carrier

Extend the exact equivalence helper so it can return enough information for a
condition-aware public surface without changing existing `Equivalent`.

Likely locations:

- `include/algebra/ExactEquivalence.hpp`
- `src/algebra/ExactEquivalence.cpp`
- focused tests under `tests/algebra`

Behavior to prove:

- unconditional equivalent;
- proven not equivalent;
- conditionally equivalent with a stable ordered set of excluded-zero
  expressions;
- unknown when unsupported methods or mismatched restrictions prevent a result.

Completion criteria:

- current `Equivalent` tests still pass;
- new internal tests prove conditional metadata is preserved and ordered;
- exact zero denominator failures still map to `runtime.division_by_zero`.

### 3. Public Pack Function

Register the approved public function in the `core-algebra` pack and expose it
through shared help/discovery metadata.

Likely locations:

- `src/packs/AlgebraPack.cpp`
- `include/help/HelpTexts.hpp`
- pack and help tests

Completion criteria:

- arity validation uses existing diagnostics;
- examples from the spec evaluate exactly as documented;
- unsupported methods return `Unknown` rather than approximate evidence;
- existing `Equivalent` behavior remains unchanged unless the approved design
  says otherwise.

### 4. Documentation And Manual Examples

Update current-behavior documentation in the same implementation change.

Likely locations:

- `docs/manual/packs-algebra.md`
- `docs/algebra_equivalence_spec.md`
- `docs/algebra_supported_subset.md`
- `docs/kernel_exact_algebra_spec.md` if the metadata carrier contract changes
- `docs/README.md` only if document navigation changes

Completion criteria:

- user docs explain why `Cancel[(x^2 - 1)/(x - 1)]` is valid only away from
  `x = 1`;
- manual examples are covered by tests or executed against the CLI/test
  harness;
- stale claims about absent condition output are removed or scoped to
  `Equivalent`.

### 5. Verification And Diff Review

Run the feature workflow gates.

Focused commands should include the algebra tests covering exact equivalence,
rational-expression transformations, and help text. Broader verification should
include the affected symbolic test target and any CLI/session smoke coverage
used for manual examples.

Completion criteria:

- baseline failures, if any, are recorded before implementation;
- focused and affected broader tests pass or are explicitly accounted for;
- final diff has no duplicate condition semantics in CLI, session, SDK, or UI
  layers;
- documentation examples match executable behavior.

## Compatibility And Unsupported Boundaries

Must preserve:

- `Equivalent` remains proof-bearing and deterministic;
- exact arithmetic and exact rational-expression conversion do not silently
  demote to approximate arithmetic;
- known denominator zero reports the stable division-by-zero diagnostic;
- `Infinity` and `Indeterminate` remain symbolic or calculus objects, not
  ordinary exact division results.

Out of scope:

- `Apart` and partial fractions;
- broad `ConditionalExpression` semantics;
- assumptions-driven discharge of conditions;
- solving `condition` expressions;
- numerical sampling as proof;
- trigonometric, logarithmic, square-root, and branch-cut identities;
- general theorem proving, quantifiers, and implication checking;
- broad validation or tutor-grade transformation checking.

## Completion Report Requirements

The final implementation report should include:

1. the approved public condition surface;
2. production files changed;
3. where conditional equivalence metadata now flows;
4. old behavior versus new behavior for representative examples;
5. tests added or changed;
6. focused and broader verification commands with results;
7. documentation pages updated and how examples were verified;
8. remaining unsupported boundaries and intentionally deferred follow-ups.
