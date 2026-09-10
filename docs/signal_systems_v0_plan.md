# Signal Systems V0 Plan

## Status

This is a future design plan for an exact continuous-time signal-systems pack.
It is not the active roadmap and does not replace the local notebook MVP or the
focused finite-DSP pack in [Aleph3 Unified Plan](aleph3_unified_plan.md).

Use this plan when the roadmap activates continuous-time transfer functions,
control-system workflows, or a signal-systems pack. Before implementation,
apply the [Feature Development Workflow](feature_development_workflow.md), read
the current algebra and registration specifications, and re-check the code.

## Goal

Deliver `Signal Systems V0: Exact Continuous-Time SISO LTI` as a pack-level
capability over the existing kernel and exact algebra foundations.

The first useful outcome is exact transfer-function construction, composition,
equivalence, evaluation, and continuous-time stability analysis without adding
a second evaluation kernel.

## Roadmap Fit

- Advances the math-pack track after the local notebook foundation is stable.
- Depends on exact algebra hardening and reusable algebra-core ownership.
- Complements, but does not replace, the planned focused DSP pack for finite
  sequences, convolution, FIR filtering, direct DFT, and inverse DFT.
- Keeps engineering semantics in a signal-systems core and pack adapter rather
  than in `aleph3_kernel`.
- Leaves plotting, broad transform theory, FFT acceleration, and state-space
  workflows as later tranches.

## Repository Evidence

Current CMake ownership puts reusable algebra implementation and pack
registration in one target:

- `aleph3_pack_algebra` includes `src/algebra/*.cpp`.
- `aleph3_pack_algebra` also includes `src/packs/AlgebraPack.cpp`.
- The target links directly to `aleph3_kernel`.

This is workable today, but a future signal-systems core should depend on
algebra algorithms without depending conceptually on the algebra pack adapter.

## Target Architecture

```text
aleph3_kernel
  Expr / Evaluator / Assumptions / ExactScalar / diagnostics / budgets
        ^
        |
aleph3_algebra_core
  ExactPolynomial / ExactRationalExpression / factorization / equivalence
        ^
        |
   +----+----------------+
   |                     |
aleph3_pack_algebra  aleph3_signal_core
                         ^
                         |
                   aleph3_pack_signal
```

The kernel remains the only semantic core. Public signal-system operations are
registered pack functions over `Expr`. `aleph3_signal_core` contains
engineering algorithms and typed errors. `aleph3_pack_signal` performs Aleph
expression conversion, registration, diagnostics, and budget integration.

Do not add `TransferFunction` to the `Expr` variant for V0. Represent it
publicly as an ordinary Aleph function call and convert at pack boundaries.

## Prerequisite: Algebra Core Split

Before signal work, split the behavior-preserving algebra implementation target:

```text
aleph3_algebra_core
  include/algebra/*.hpp
  src/algebra/*.cpp

aleph3_pack_algebra
  include/packs/AlgebraPack.hpp
  src/packs/AlgebraPack.cpp
```

Expected dependencies:

```text
aleph3_algebra_core -> aleph3_kernel
aleph3_pack_algebra -> aleph3_algebra_core
aleph3_signal_core -> aleph3_algebra_core
aleph3_pack_signal -> aleph3_signal_core
```

This split should preserve user-visible algebra behavior. Verification should
run existing algebra and pack tests, plus affected SDK, CLI, notebook, and web
tests that link the algebra pack.

## V0 Public Surface

Use names that avoid conflicts with existing or planned CAS concepts.
In particular, do not use bare `Series` for system composition because local
power series is a separate planned symbolic feature.

Initial functions:

- `TransferFunction[num, den, s]`
- `TransferFunctionQ[expr]`
- `TransferFunctionNumerator[tf]`
- `TransferFunctionDenominator[tf]`
- `TransferFunctionVariable[tf]`
- `NormalizeTransferFunction[tf]`
- `CancelTransferFunction[tf]`
- `Cascade[G, H]` or another approved non-conflicting series-composition name
- `Parallel[G, H]`
- `Feedback[G, H]`
- `ScaleSystem[k, G]`
- `TransferFunctionEquivalentQ[G, H]`
- `TransferFunctionValue[G, x]`
- `DCGain[G]`
- `RouthTable[tf]`
- `RouthTable[polynomial, s]`
- `StableQ[tf]`

Representative session:

```text
g = TransferFunction[1, s + 1, s]
h = TransferFunction[1, s + 2, s]
Cascade[g, h]
TransferFunction[1, s^2 + 3*s + 2, s]

Feedback[g, h]
TransferFunction[s + 2, s^2 + 3*s + 3, s]

StableQ[%]
True
```

## Core Representation

The first internal model is exact continuous-time SISO LTI:

```cpp
namespace aleph3::signal {

struct TransferFunction {
    algebra::ExactPolynomial numerator;
    algebra::ExactPolynomial denominator;
    std::string variable;
};

}
```

The `variable` field represents one validated Aleph symbol. If symbol identity
becomes richer than a string, use that shared representation instead.

Do not spread conversion logic through `SignalPack.cpp`. Keep it in dedicated
conversion code:

```cpp
Expected<TransferFunction, SignalError> from_expr(const ExprPtr& expr);
ExprPtr to_expr(const TransferFunction& transfer_function);
```

The pack adapter maps typed `SignalError` values to kernel diagnostics.

## Invariants

A valid V0 transfer function must have:

- one symbolic variable;
- exact integer or rational polynomial coefficients;
- a nonzero denominator;
- nonnegative integer polynomial powers;
- deterministic polynomial ordering;
- explicit continuous-time semantics.

Reject these in V0:

```text
TransferFunction[Sin[s], s + 1, s]
TransferFunction[1.2*s + 1, s + 1, s]
TransferFunction[1, s^x + 1, s]
TransferFunction[1, 0, s]
TransferFunction[1, s + t, s]
```

Use deterministic diagnostics for unsupported or invalid forms. Do not silently
sample, approximate, or infer broader signal semantics.

## Normalization And Cancellation

Keep representation normalization separate from mathematical cancellation.

`NormalizeTransferFunction` may:

- remove zero polynomial terms;
- normalize exact rational coefficients;
- enforce deterministic denominator sign;
- optionally make the denominator leading coefficient one;
- normalize scalar content shared between numerator and denominator.

It must preserve pole-zero factors. For example, it may turn:

```text
TransferFunction[2*s + 2, 2*s^2 + 6*s + 4, s]
```

into:

```text
TransferFunction[s + 1, s^2 + 3*s + 2, s]
```

but it must not cancel the common `s + 1` factor.

`CancelTransferFunction` is the explicit operation that may produce:

```text
TransferFunction[1, s + 2, s]
```

This preserves engineering structure when users need to reason about supplied
models, while still offering mathematical simplification explicitly.

## Exact Composition

For:

```text
G = N_G / D_G
H = N_H / D_H
```

implement:

```text
Cascade[G, H]  = (N_G N_H) / (D_G D_H)
Parallel[G, H] = (N_G D_H + N_H D_G) / (D_G D_H)
Feedback[G, H] = (N_G D_H) / (D_G D_H + N_G N_H)
```

Use exact polynomial arithmetic. Normalize scalar content, but do not cancel
common factors unless the operation explicitly requests cancellation.

## Exact Equivalence

`TransferFunctionEquivalentQ[G, H]` should avoid numerical sampling.

For:

```text
G = N_1 / D_1
H = N_2 / D_2
```

return true when:

```text
N_1 D_2 - N_2 D_1 == 0
```

This should report:

```text
TransferFunctionEquivalentQ[
  TransferFunction[s + 1, (s + 1)*(s + 2), s],
  TransferFunction[1, s + 2, s]
]
True
```

without floating-point evaluation.

## Evaluation And DC Gain

`TransferFunctionValue[G, x]` first supports exact real values. `DCGain[G]` is
evaluation at zero.

When the denominator is zero at the requested value, use existing Aleph
non-finite or indeterminate semantics where available. Do not invent
signal-specific numeric atoms.

## Routh-Hurwitz Stability

Continuous-time `StableQ` should begin with exact Routh-Hurwitz analysis, not
numerical root finding.

Core shape:

```cpp
struct RouthTable {
    std::vector<std::vector<kernel::ExactRational>> rows;
};
```

For a polynomial:

```text
D(s) = a_n s^n + ... + a_1 s + a_0
```

the first rows are:

```text
row 0 = a_n,     a_(n-2), ...
row 1 = a_(n-1), a_(n-3), ...
```

remaining rows use:

```text
r[i][j] =
  (r[i-1][0] * r[i-2][j+1] - r[i-2][0] * r[i-1][j+1])
  / r[i-1][0]
```

Handle and test:

- zero first-column element;
- entire zero row;
- degree zero;
- zero polynomial;
- symbolic coefficients;
- degree or table-size budget exhaustion.

Reject symbolic-coefficient Routh tables in V0.

## Budgets

Signal operations can grow polynomial degree and term counts quickly. Use
kernel evaluation budgets through the pack adapter and explicit signal-core
limits for core algorithms.

V0 should specify limits for:

- maximum polynomial degree;
- maximum polynomial terms;
- maximum composition depth;
- maximum Routh order;
- maximum response samples when numerical response data is later added.

Budget failures should be explicit diagnostics. They must not trigger
approximate fallback.

## Diagnostics

The focused specification should assign stable diagnostic codes for at least:

- invalid transfer-function arity;
- non-symbol transfer variable;
- numerator or denominator not polynomial in the selected variable;
- non-exact coefficient;
- denominator zero polynomial;
- variable mismatch between systems;
- unsupported symbolic coefficients;
- unsupported Routh special case if not implemented in the active slice;
- budget exhaustion.

## Tests

Prioritize property-like identities and exact edge cases:

```text
Cascade[G, H] == Cascade[H, G] by TransferFunctionEquivalentQ
Parallel[G, H] == Parallel[H, G] by TransferFunctionEquivalentQ
Cascade[Cascade[G, H], K] == Cascade[G, Cascade[H, K]]
Parallel[Parallel[G, H], K] == Parallel[G, Parallel[H, K]]
Cascade[G, TransferFunction[1, 1, s]] == G
Parallel[G, TransferFunction[0, 1, s]] == G
Feedback[G, TransferFunction[0, 1, s]] == G
NormalizeTransferFunction[NormalizeTransferFunction[G]] == NormalizeTransferFunction[G]
TransferFunctionValue[Cascade[G, H], x] == TransferFunctionValue[G, x] * TransferFunctionValue[H, x]
```

Stability regressions:

```text
StableQ[TransferFunction[1, s + 1, s]]            -> True
StableQ[TransferFunction[1, s^2 + 2*s + 1, s]]    -> True
StableQ[TransferFunction[1, s^2 - 1, s]]          -> False
StableQ[TransferFunction[1, s^3 + 2*s^2 + 3*s + 4, s]] -> True
```

Use exact coefficients beyond `int64_t` in targeted stress tests.

## Documentation Requirements

Treat documentation as a first-class deliverable, not a post-implementation API
reference. Signal systems introduce engineering vocabulary that many symbolic
math users will not already know, so V0 should ship with both runnable Aleph
examples and foundational explanations.

Every user-visible slice must update:

- a new manual chapter, likely `docs/manual/packs-signal-systems.md`;
- `docs/manual/README.md` and `docs/manual/book.yaml` so the chapter appears
  in the browsable manual and PDF book;
- `docs/manual/concepts-and-terminology.md` for vocabulary users need outside
  the chapter;
- the focused signal-systems specification created from this plan;
- CLI help and completion metadata for every registered function;
- the documentation index;
- the unified plan only if roadmap priority or milestone status changes.

Manual examples must be covered by tests or executed through the built CLI.

### Foundational Manual Content

The signal-systems manual chapter should teach enough background for a user to
understand the supported operations without leaving the Aleph documentation.
It should be concise, mathematical, and honest about scope.

Cover at least:

- what a continuous-time signal is in the V0 context;
- what SISO means and why V0 excludes MIMO systems;
- what an LTI system is, including linearity and time invariance at a high
  level;
- what the complex variable `s` represents in transfer-function notation;
- what a transfer function $G(s)=N(s)/D(s)$ is and what numerator and
  denominator polynomials mean;
- the difference between exact rational polynomial algebra and numerical
  simulation;
- poles, zeros, and why V0 initially exposes their polynomials before broad
  root solving;
- why pole-zero cancellation is explicit rather than automatic;
- cascade, parallel, scaling, and feedback interconnections with diagrams or
  algebraic identities;
- DC gain as evaluation at $s=0$ and the meaning of a pole at zero for that
  query;
- continuous-time stability as a property of denominator roots lying in the
  open left half-plane;
- Routh-Hurwitz as an exact tabular test for that stability property without
  computing roots;
- the Routh table construction for low-degree examples;
- special Routh cases and how Aleph reports unsupported or indeterminate cases;
- exactness boundaries, budget limits, and when numerical frequency-response
  work belongs to a later tranche.

The Routh-Hurwitz section should include a worked example such as:

```text
D(s) = s^3 + 2*s^2 + 3*s + 4

Routh first rows:
s^3:  1   3
s^2:  2   4
s^1:  1   0
s^0:  4
```

Then connect the result to the rule users need: for the supported
continuous-time case, all first-column entries being positive proves
asymptotic stability.

### Examples And Unsupported Boundaries

The manual chapter should include runnable examples for each V0 function group:

- constructing and inspecting `TransferFunction`;
- validating bad inputs with stable diagnostics;
- normalizing without cancellation;
- explicitly cancelling common factors;
- composing systems with cascade, parallel, and feedback;
- checking exact equivalence;
- evaluating at exact values and computing `DCGain`;
- building a `RouthTable`;
- using `StableQ` on stable and unstable examples.

Every example should state whether the result is exact, approximate,
unsupported, or intentionally preserved as supplied engineering structure.

Unsupported examples should be visible, not hidden in diagnostics-only tests:

```text
TransferFunction[Sin[s], s + 1, s]
TransferFunction[1.2*s + 1, s + 1, s]
TransferFunction[1, s + t, s]
RouthTable[a*s^2 + b*s + c, s]
```

### Specification And Help Content

The focused signal-systems specification should own durable contracts:

- representation invariants;
- exactness and coefficient requirements;
- normalization versus cancellation;
- composition formulas;
- stability conventions;
- Routh-Hurwitz special-case handling;
- diagnostic codes;
- budgets;
- cross-surface behavior.

CLI help for each registered function should include accepted forms, one short
example, exactness notes, unsupported boundaries, and a manual anchor. Help
text should not try to teach all signal theory; it should point to the manual
chapter for concepts.

## Deferred Work

Leave these out of V0 unless a later approved plan changes scope:

- Laplace transform and inverse Laplace transform;
- Z-transform and inverse Z-transform;
- FFT acceleration;
- continuous Fourier transforms;
- plotting;
- Bode plotting, except returning data in a later bounded tranche;
- numerical poles and zeros;
- broad root solving;
- state-space systems;
- discrete-time systems and Jury or Schur stability;
- FIR, IIR, convolution, impulse response, and step response;
- natural-language explanation traces.

## Later Tranches

After V0 is stable, consider this order:

1. Numerical frequency response returning data, not plots.
2. Exact or explicitly unsupported poles and zeros, plus separate `NPoles` and
   `NZeros` for numerical roots.
3. Bode data generation with bounded sample counts.
4. State-space systems over exact dense matrices.
5. Discrete-time transfer functions and exact Jury or Schur stability.
6. FIR/IIR adapters over discrete transfer functions.
7. Bounded transform workflows only after assumptions, convergence regions,
   piecewise behavior, and diagnostics are specified.

The durable differentiator is exact transfer-function algebra plus exact
stability analysis. Preserve that contract before adding approximate or
display-oriented features.
