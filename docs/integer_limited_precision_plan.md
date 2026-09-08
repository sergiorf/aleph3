# Integer Limited-Precision Remediation Plan

## Goal

Aleph3 should preserve exact integer and rational meaning for supported
symbolic work without silently rounding through `double`, wrapping `int64_t`,
or letting one consumer invent a private numeric model. The first remediation
goal is a kernel-owned exact-scalar design that can replace the current
checked-`int64_t` ceiling with arbitrary-precision integers and rationals in
small, independently committable slices.

Observable success:

- integer and rational literals that fit in memory parse without precision
  loss;
- exact integer/rational arithmetic, simplification, algebra helpers, dense
  matrices, session, CLI, and SDK-supported surfaces either preserve exactness
  or return deterministic diagnostics;
- no exact path demotes to `Number` or a transitional `double` polynomial path
  to avoid growth;
- compatibility and resource limits are documented, tested, and visible at
  public boundaries.

This plan advances the Unified Plan's Exact Algebra Depth milestone and the
Bounded Numerical and Finite Data MVP workstream. It is a kernel and
algebra-pack feature with session, CLI, SDK, help, and manual follow-through.

## Repository Evidence

Current contracts and implementation show three separate precision issues:

- `Expr::Number` stores machine reals as `double`, while `Expr::Rational` stores
  exact rationals as `int64_t` numerator and denominator.
- Exact algebra documents and helpers intentionally use checked `int64_t`
  coefficients, but this is now the known ceiling for larger exact work.
- Several simplification and conversion paths still cast integer-valued
  `Number` values or multiply rational numerators and denominators directly,
  creating risks of precision loss or undefined overflow before the checked
  exact-coefficient layer can report a stable failure.

Relevant current documents:

- [Unified Plan](aleph3_unified_plan.md), especially Exact Algebra Depth and
  Bounded Numerical and Finite Data MVP.
- [Feature Development Workflow](feature_development_workflow.md).
- [Architecture](architecture.md), which owns the expression representation
  overview and must be updated when `Expr` grows new numeric alternatives.
- [Kernel Exact Algebra Spec](kernel_exact_algebra_spec.md).
- [Algebra Supported Subset](algebra_supported_subset.md).
- [Exact Dense-Matrix Specification](algebra_dense_matrix_spec.md).
- [Expressions And Evaluation](manual/expressions-and-evaluation.md).
- [Trusted Subset](trusted_subset_v1.md) and
  [SDK Stable Interfaces](sdk/stable_interfaces.md) for SDK compatibility.

Likely implementation entry points:

- expression model and construction:
  `include/expr/Expr.hpp`, `include/expr/ExprUtils.hpp`;
- parser and lowering:
  `include/parser/Parser.hpp`, `src/syntax/SymbolicLowering.cpp`,
  `src/syntax/TrustedSubsetLowering.cpp`, `include/syntax/Node.hpp`;
- exact arithmetic and simplification:
  `src/evaluator/EvaluatorBuiltins.cpp`,
  `src/evaluator/SimplificationRules.cpp`, `src/kernel/Rewrite.cpp`,
  `include/normalizer/Normalizer.hpp`;
- exact algebra:
  `include/algebra/ExactPolynomial.hpp`,
  `src/algebra/ExactPolynomialConversion.cpp`,
  `src/algebra/ExactPolynomialOps.cpp`,
  `src/algebra/ExactFactorization.cpp`,
  `src/algebra/ExactRationalExpression.cpp`,
  `src/algebra/PolyUtils.cpp`;
- public algebra registration:
  `src/packs/AlgebraPack.cpp`;
- calculus order parsing:
  `src/packs/CalculusPack.cpp`;
- session and CLI rendering:
  `src/session/Session.cpp`, `include/expr/FullForm.hpp`,
  `src/expr/Expr.cpp`, `src/tooling/SymbolicCliSupport.cpp`;
- docs and help:
  `docs/architecture.md`, `docs/manual/`, `docs/*_spec.md`,
  `include/help/HelpTexts.hpp`.

## Chosen Design Direction

Introduce a kernel-owned exact scalar layer before broad algorithm migration.
The layer should provide first-class arbitrary-precision integers and
normalized exact rationals, with explicit adapters for bounded places that
still require native integers.

Preferred representation:

- add an `ExactInteger` type backed by `boost::multiprecision::cpp_int` or an
  equivalent approved header-only arbitrary-precision integer;
- add an `ExactRational` type that owns a normalized `ExactInteger` numerator
  and positive denominator;
- rename the expression-facing machine-real concept from the current
  implementation name `Number` to the public architecture concept `Real`, or
  document why the C++ type remains `Number` while the semantic expression
  alternative is called `Real`;
- represent exact integer values distinctly from machine-real `Number` values
  at the expression boundary, either by adding an `Integer` expression
  alternative or by making the existing exact rational representation carry all
  exact integer values as denominator-one rationals;
- keep `Number` as machine-real only;
- expose exact scalar operations through a narrow kernel header, not through
  ad hoc `cpp_int` use in packs or consumers;
- charge budgets or enforce size limits where operations can grow
  substantially.

The exact expression representation choice needs approval before
implementation. The recommended choice is a first-class `Integer` expression
alternative plus `Rational` using `ExactInteger` fields. This keeps `Head[3]`
as `Integer`, avoids treating all integers as rationals internally, and makes
machine-real `Number` usage explicit.

Rejected alternatives:

- keep checked `int64_t` as the long-term answer: this preserves deterministic
  overflow but does not solve the reported precision ceiling;
- add arbitrary precision only inside algebra helpers: this creates a private
  pack scalar model and leaves parser, simplifier, session, CLI, and SDK
  precision bugs in place;
- parse large integers as strings while evaluating through `double`: this fixes
  display but not semantics;
- replace all numeric behavior in one large refactor: the blast radius is too
  high and would make verification weak.

## Non-Goals

This remediation does not deliver arbitrary-precision floating-point numbers,
interval arithmetic, broad numerical analysis, algebraic-number coefficients,
general coefficient-ring algorithms, faster factorization, broad solving, or
Mathematica-compatible numeric precision tracking. Decimal literals remain
machine reals unless a later bounded-numerical design changes that contract.

Exact-size limits are still allowed. An implementation may reject inputs or
intermediates that exceed configured digit, byte, term, matrix, or evaluation
budgets.

## Implementation Slices

Each slice below should compile, pass its focused tests, update the owning
documentation, and be suitable as an individual commit. Run a baseline before
slice 1 and record any unrelated failures.

### Slice 1: Precision Audit And Guard Rails

Status: complete.

Completion notes:

- audited the current exact rational/equation paths that performed raw
  `int64_t` arithmetic in parser lowering, evaluator arithmetic,
  simplification, normalized-head rewrite helpers, transform helpers, exact
  polynomial conversion, and expression rendering;
- centralized bounded rational guard rails in the existing expression utility
  layer for checked `int64_t` add/subtract/multiply/negation, safe
  normalization across `INT64_MIN`, strict bounded integer conversion from
  `Number`, exact rational arithmetic, and exact rational comparison;
- updated exact polynomial coefficients to reuse the shared checked rational
  guard rails while preserving the current `int64_t` representation;
- added regression coverage for near-bound rational arithmetic overflow,
  minimum-integer rational normalization, exact-only inexact coefficient
  rejection, and session-level `runtime.exact_overflow` projection;
- arbitrary-precision integers and rationals remain planned for later slices.

Behavior delivered:

- identify every current exact-integer and exact-rational path that casts
  through `double`, multiplies `int64_t` fields unchecked, or normalizes values
  in a way that can overflow before diagnostics;
- add focused regression tests for current failures or unsafe boundaries while
  preserving the current `int64_t` contract;
- map all exact overflow that reaches public pack/session boundaries to
  `kernel.exact_overflow`.

Likely locations:

- `tests/evaluator/EvaluatorRationalTests.cpp`;
- `tests/algebra/ExactPolynomialOpsTests.cpp`;
- `tests/algebra/PolynomialExprConversionTests.cpp`;
- `tests/packs/*` for public algebra diagnostics;
- `src/evaluator/SimplificationRules.cpp`, `src/kernel/Rewrite.cpp`,
  `src/transforms/Transforms.cpp`, `include/expr/ExprUtils.hpp`.

Tests:

- adding or multiplying near-`int64_t` rationals reports exact overflow instead
  of wrapping;
- rational normalization handles negative denominators and minimum integers
  without undefined negation;
- exact-only algebra helpers reject unsafe inexact `Number` conversions;
- public CLI/session diagnostics keep the stable exact-overflow code.

Documentation:

- update [Expressions And Evaluation](manual/expressions-and-evaluation.md)
  and exact algebra docs only for clarified current behavior;
- keep arbitrary precision labeled as planned, not shipped.

Verification:

- `ctest --test-dir build -C Release -R aleph3_symbolic_tests --output-on-failure`;
- `git diff --check`;
- review the audit list and tests against every matched cast/multiply site.

### Slice 2: Kernel Exact-Scalar Abstraction

Status: complete.

Completion notes:

- added `kernel::ExactInteger` and `kernel::ExactRational` as a kernel-owned
  exact scalar module backed by Boost.Multiprecision `cpp_int`;
- added normalization, sign, zero/one checks, comparison, decimal rendering,
  decimal integer parsing, arithmetic, and checked adapters to the current
  bounded `int64_t` representation;
- kept the public expression model unchanged: `Expr::Rational` still uses the
  bounded storage until the expression-model migration slice;
- made Boost.Multiprecision an explicit build dependency through system Boost
  when available or modular CMake `FetchContent` fallback otherwise.

Behavior delivered:

- add a kernel-owned exact scalar module with `ExactInteger` and
  `ExactRational` operations;
- keep the expression model unchanged in this slice by providing lossless
  adapters to and from current `int64_t` rationals where representable;
- centralize normalization, comparison, sign, zero/one checks, native-integer
  conversion, and stable string rendering;
- replace low-level helper calls that only need scalar arithmetic but do not
  require an expression-model change.

Likely locations:

- new `include/kernel/ExactScalar.hpp` and `src/kernel/ExactScalar.cpp`, or an
  equivalent expression-owned scalar module if that better matches local
  layering;
- `include/expr/ExprUtils.hpp`;
- tests under `tests/kernel/` or `tests/evaluator/` depending on existing test
  organization.

Tests:

- large `ExactInteger` construction, comparison, sign normalization, and
  string round trip;
- rational normalization for large common factors;
- checked conversion to `int64_t` succeeds at bounds and fails outside bounds;
- denominator zero remains a domain error before expression construction.

Documentation:

- update [Kernel Exact Algebra Spec](kernel_exact_algebra_spec.md) with the new
  internal scalar module while stating that public expressions still have the
  old bounded storage until later slices.

Verification:

- focused exact-scalar tests;
- `ctest --test-dir build -C Release -R aleph3_symbolic_tests --output-on-failure`;
- `git diff --check`.

### Slice 3: Lossless Integer Literal Frontend

Status: complete.

Completion notes:

- added a distinct integer token and syntax node that preserve decimal source
  text without calling `std::stod`;
- kept decimal literals with a dot as machine-real `Number` input;
- added bounded syntax lowering adapters so current symbolic expressions and
  trusted SDK IR continue accepting standalone integers that can be represented
  exactly by the current `Number` storage while reporting source-spanned
  diagnostics for integers outside that boundary;
- preserved exact rational literal parsing for integer-token numerators and
  denominators, with oversized rationals still rejected before the
  expression-model arbitrary-precision slice.

Behavior delivered:

- parse integer tokens into a lossless syntax representation rather than a
  `double`;
- preserve the source spelling needed to distinguish decimal integers from
  decimal machine reals;
- keep trusted-subset validation compatible with existing accepted integer
  forms while adding explicit diagnostics for integers rejected by current SDK
  host-value limits, if any.

Likely locations:

- `include/syntax/Node.hpp`;
- `src/syntax/Lexer.cpp`, `src/syntax/Parser.cpp`;
- `src/syntax/SymbolicLowering.cpp`;
- `src/syntax/TrustedSubsetLowering.cpp`;
- legacy parser compatibility in `include/parser/Parser.hpp` if still used by
  symbolic paths.

Tests:

- large integer literals keep exact decimal text through parse, lowering, and
  rendering;
- `1/large_integer` and `large_integer/large_integer` normalize exactly once
  the expression model supports it, or fail with the current bounded diagnostic
  before that model lands;
- decimal literals such as `1.0` still lower to machine real `Number`;
- source-aware diagnostics still point at the numeric token.

Documentation:

- update [Trusted Subset](trusted_subset_v1.md) and
  [Expressions And Evaluation](manual/expressions-and-evaluation.md) for the
  frontend distinction between exact integers and machine reals.

Verification:

- parser and symbolic lowering tests;
- SDK frontend tests if trusted lowering changes;
- `ctest --test-dir build -C Release --output-on-failure` when parser node
  shapes affect both symbolic and SDK tests.

### Slice 4: Expression Model For Arbitrary-Precision Exact Scalars

Status: complete. Implemented after explicit approval. This slice changed the
kernel expression representation and public numeric taxonomy.

Behavior delivered:

- make exact integers and rationals first-class expression values using the
  approved representation;
- settle the architecture-visible numeric taxonomy as exact `Integer`, exact
  `Rational`, and machine `Real`;
- keep the C++ machine-real struct named `Number` in this slice unless a
  separate mechanical rename is approved. Documentation should describe the
  semantic head as `Real` and call out `Number` only as a transitional C++
  implementation name where needed;
- update constructors, structural equality, canonical rendering, `FullForm`,
  `Head`, `IntegerQ`, `RationalQ`, `NumberQ`, serialization where present, and
  pattern matching for `_Integer` and `_Rational`;
- preserve machine-real `Number` for decimals and approximate results.

Recommended slice-local design:

- add a first-class `Integer` expression alternative backed by
  `kernel::ExactInteger`;
- replace `Rational` fields with `kernel::ExactRational`, preserving a
  compatibility constructor from bounded `int64_t` values for existing call
  sites;
- canonical expression construction returns `Integer` for denominator-one exact
  rational values at expression boundaries, except where a caller is
  intentionally constructing a `Rational` expression to test rational storage;
- exact integer literals lower directly to `Integer` from preserved decimal
  text, including unary minus, without `double` conversion or bounded
  `int64_t` rejection;
- exact rational literals lower through `ExactRational` normalization, and
  denominator-one forms such as `6/3` render and behave as `Integer`;
- decimal literals with a decimal point continue to lower to machine-real
  `Number` and report public head `Real`;
- the SDK trusted subset keeps its current bounded public host-number model in
  this slice. It may continue rejecting decimal integer tokens that cannot be
  represented exactly as the SDK's public `Value::number`. First-class SDK
  exact-scalar transport is left to slice 8.

Non-goals for this slice:

- migrating all arithmetic, simplification, polynomial coefficients, dense
  matrices, or transport formats to arbitrary precision; slices 5 through 8
  own those migrations;
- introducing arbitrary-precision floating-point numbers or precision tracking;
- changing decimal syntax, approximate elementary functions, complex storage,
  SDK `Value` variants, notebook file format, CLI JSON output, or web API
  numeric envelopes except where they already carry canonical text;
- deleting all checked-`int64_t` helpers. They remain required adapters for
  bounded consumers until later slices remove or narrow them.

Completion notes:

- `Expr::Integer` now carries `kernel::ExactInteger`, and `Expr::Rational`
  carries arbitrary-precision exact numerator and denominator values.
- Exact integer and rational lowering no longer rejects large symbolic
  integers at the expression boundary; decimal literals remain machine-real
  `Number` values with public head `Real`.
- Denominator-one rational construction canonicalizes to `Integer`.
- `Head`, `IntegerQ`, `RationalQ`, `RealQ`, exact sign predicates, typed
  patterns, structural equality/hash/order, rendering, and `FullForm` are
  updated for the new exact scalar alternatives.
- Bounded adapters remain in native-size consumers such as string ranges,
  part indexes, rewrite levels, derivative orders, matrix dimensions, and SDK
  host-number projection.
- The implementation also migrated exact arithmetic, simplification, exact
  polynomial coefficients, and dense matrix scalar paths onto the shared exact
  scalar model where the existing supported subset already had exact
  semantics.

Pre-implementation repository evidence that this slice superseded:

- `include/expr/Expr.hpp` currently defines `Expr` as a variant containing
  `Number` and bounded `Rational`, with no `Integer` alternative.
- `src/syntax/SymbolicLowering.cpp` preserves integer token text, then rejects
  standalone integers outside current expression bounds and builds exact
  rationals from bounded `int64_t` numerators and denominators.
- `src/expr/Expr.cpp` and `include/expr/FullForm.hpp` render bounded rationals
  and integral machine reals as if they were exact integers in several places.
- `src/evaluator/BuiltInFunctions.cpp` reports `Head[3] -> Integer` by
  inspecting whether a `Number(double)` is integral; this should become an
  actual `Integer` expression case.
- `src/kernel/Assumptions.cpp`, `src/kernel/Rewrite.cpp`, and pattern helpers
  classify numeric literals by looking at `Number` and bounded `Rational`.
- `include/kernel/ExactScalar.hpp` and `src/kernel/ExactScalar.cpp` already
  provide arbitrary-precision `ExactInteger` and `ExactRational`, including
  normalization, comparison, rendering, and bounded adapters.

Likely locations:

- `include/expr/Expr.hpp`;
- `include/expr/ExprUtils.hpp`;
- `include/expr/ExprStructural.hpp`;
- `src/expr/Expr.cpp`;
- expression structural implementation under `src/expr/`;
- `include/expr/FullForm.hpp`;
- `src/syntax/SymbolicLowering.cpp`;
- `include/parser/Parser.hpp` only if the legacy compatibility parser still
  bypasses shared symbolic lowering for integer construction;
- `src/evaluator/BuiltInFunctions.cpp`;
- `src/evaluator/EvaluatorBuiltins.cpp` only for preserving bounded current
  arithmetic behavior around new `Integer` atoms;
- `src/session/Session.cpp`;
- `src/kernel/Assumptions.cpp`;
- `src/kernel/Rewrite.cpp`;
- pattern and predicate code under evaluator/kernel;
- `include/help/HelpTexts.hpp`;
- tests under `tests/frontend/`, `tests/evaluator/`, `tests/session/`,
  `tests/tooling/`, and `tests/expr/` if structural tests are split there.

Task 1: expression representation and construction.

Deliver:

- introduce `struct Integer { kernel::ExactInteger value; }` and add it to
  `Expr`;
- update `Rational` to hold `kernel::ExactRational`, with accessors or helper
  functions for numerator and denominator rather than exposing bounded fields
  as the durable API;
- add helper constructors such as `make_integer`, `make_rational`, and
  `make_exact_scalar_expr`, where the last one canonicalizes denominator-one
  rationals to `Integer`;
- keep existing bounded constructors or adapters only as compatibility shims,
  and make new expression code prefer the exact helpers.

Tests:

- direct construction of small and large `Integer` values preserves decimal
  text through `to_string`;
- `make_exact_scalar_expr(ExactRational(6, 3))` returns an integer expression;
- zero and sign-normalized rationals render as `0`, `-1/2`, and `2/3`
  according to the public contract;
- existing small-number tests still compile through compatibility constructors.

Documentation:

- add the expression taxonomy decision to
  [Architecture](architecture.md) and
  [Kernel Exact Algebra Spec](kernel_exact_algebra_spec.md).

Verification:

- focused expression tests;
- `ctest --test-dir build -C Release -R "(FullForm|Normalizer|Evaluator)" --output-on-failure`.

Task 2: symbolic lowering and legacy parser compatibility.

Deliver:

- lower `IntegerLiteralNode` directly to `Integer` using
  `ExactInteger::from_decimal_string`;
- lower unary-minus integer literals by constructing a negative
  `ExactInteger`, including values below `INT64_MIN`;
- lower exact fraction literals through `ExactRational` and canonicalize
  denominator-one results to `Integer`;
- preserve decimal literals such as `1.0` as machine-real `Number`;
- keep trusted-subset lowering behavior unchanged unless it explicitly enters
  symbolic `Expr` construction after SDK validation.

Tests:

- `123456789012345678901234567890` parses, lowers, evaluates, and renders as
  the same integer;
- `-9223372036854775809` lowers and renders exactly;
- `9007199254740993` is no longer rejected by symbolic lowering;
- `9007199254740993/3` lowers without precision loss and renders as
  `3002399751580331`;
- `1.0` remains machine-real and `Head[1.0] -> Real`;
- SDK trusted parser tests still report the documented
  `frontend.parser.integer_out_of_range` boundary for oversized host numbers.

Documentation:

- update [Expressions And Evaluation](manual/expressions-and-evaluation.md)
  to remove the pre-slice symbolic lowering limitation while keeping the SDK
  boundary explicit;
- update [Trusted Subset](trusted_subset_v1.md) only to clarify that the SDK
  public host-number range remains bounded in this slice.

Verification:

- parser and symbolic lowering tests;
- `aleph3_sdk_tests` if trusted lowering or parser compatibility changes.

Task 3: rendering, full form, heads, and predicates.

Deliver:

- render `Integer` with exact decimal text and `Rational` as normalized
  `numerator/denominator`;
- update unary-minus and subtraction formatting so negative exact integers and
  rationals do not require bounded negation;
- update `FullForm` so `Integer` emits the integer decimal text and `Rational`
  emits `Rational[n, d]`;
- update `Head` to dispatch on `Integer` directly, while `Number` always
  reports `Real`;
- decide and document `NumberQ`: recommended contract is true for exact
  integers, exact rationals, machine reals, and complex numeric atoms if
  `NumberQ` exists in the implemented surface; otherwise do not add it in this
  slice;
- update `IntegerQ`, `RationalQ`, `RealQ`, sign predicates, and zero tests so
  exact integers and rationals are answered without `double` conversion.

Tests:

- `Head[3] -> Integer`, `Head[1.5] -> Real`,
  `Head[123456789012345678901234567890] -> Integer`;
- `IntegerQ[large] -> True`, `RationalQ[large] -> True`, and
  `RealQ[large] -> True`;
- `IntegerQ[3/2] -> False`, `RationalQ[3/2] -> True`,
  `RationalQ[0.5]` keeps the documented existing behavior or is updated with
  matching docs in the same task;
- `Positive`, `Negative`, `ZeroQ`, and `NonZeroQ` work for large exact
  integers and rationals;
- `FullForm[large]` and `FullForm[1/2]` match documented output.

Documentation:

- update [Built-in Functions](manual/built-in-functions.md),
  [Concepts And Terminology](manual/concepts-and-terminology.md), and help
  entries for `Head`, predicates, `N`, and numeric exactness notes.

Verification:

- evaluator predicate and head tests;
- session help tests if help text changes.

Task 4: structural identity, ordering, hashing, and pattern matching.

Deliver:

- update `structural_equal`, `structural_hash`, and `structural_less` to cover
  `Integer` and arbitrary-precision `Rational` values without string-based
  comparison in the hot path unless no better scalar comparator exists;
- define deterministic type ordering among numeric atoms. Recommended order:
  `Integer`, `Rational`, `Number`/machine `Real`, `Complex`, then existing
  nonnumeric alternatives in the current relative order;
- update pattern matching so `_Integer` matches only the new `Integer`
  alternative, `_Rational` matches exact rationals and not integers unless the
  current documented contract explicitly treats integers as rationals, and
  named patterns preserve bindings by structural equality;
- update rewrite traversal and substitution helpers only where variant
  visitation requires a new case.

Tests:

- structurally equal large integers hash equally;
- unequal large integers and rationals sort deterministically;
- `MatchQ[large, _Integer] -> True`;
- `MatchQ[large/2, _Rational] -> True`;
- `MatchQ[large, _Rational]` follows the documented chosen contract;
- repeated named-pattern constraints such as
  `MatchQ[f[large, large], f[n_Integer, n_Integer]] -> True` and a one-digit
  difference returns `False`;
- `Replace[f[large], f[n_Integer] -> g[n]]` preserves the exact value.

Documentation:

- update pattern and structural matching sections in the manual only for
  user-visible predicate and pattern behavior.

Verification:

- rewrite and evaluator pattern tests;
- `ctest --test-dir build -C Release -R "(Rewrite|Evaluator)" --output-on-failure`.

Task 5: bounded adapters and current behavior preservation.

Deliver:

- add exact-to-bounded helper functions for call sites that still require
  `int`, `int64_t`, `std::size_t`, or `double`, and make those helpers return
  deterministic diagnostics or `std::optional` rather than silently narrowing;
- update existing bounded consumers such as `Part` indexes, rewrite levels,
  string ranges, calculus derivative orders, matrix dimensions, and algebra
  conversion gates to call the adapters explicitly;
- preserve slice-4 arithmetic scope: existing arithmetic may continue to use
  bounded paths where not yet migrated, but it must not turn a large exact
  integer into a rounded machine real merely because it is now representable;
- ensure `N[large exact]` is the only explicit path in this slice that may
  approximate a large exact integer or rational to machine real, subject to the
  existing non-finite result diagnostics.

Tests:

- `Part[{a}, large]` reports the existing invalid-index diagnostic rather than
  narrowing;
- derivative/order and rewrite-level controls reject oversized exact integers
  with stable invalid-form diagnostics;
- evaluating a large standalone exact integer returns the same exact integer;
- unsupported arithmetic with large exact values remains symbolic or reports
  the documented current diagnostic until slice 5 migrates arithmetic;
- `N[large]` produces a machine-real result or a documented runtime diagnostic
  when finite conversion is impossible.

Documentation:

- document the temporary boundary that arbitrary-precision expression storage
  precedes full arithmetic/algebra migration, and point to later slices for the
  remaining work.

Verification:

- affected evaluator, calculus, rewrite, and pack tests;
- targeted CLI smoke tests for representative diagnostics.

Task 6: session, CLI, help, and documentation consistency.

Deliver:

- verify session canonical text and diagnostics carry large exact integers and
  rationals without precision loss;
- update CLI symbolic evaluation and inspection paths that rely on
  `to_string`, `to_string_raw`, `FullForm`, or `Head`;
- update help metadata for integer/rational exactness and any predicate
  examples;
- search changed docs and help for stale claims that exact coefficients or
  public expressions are limited to checked 64-bit integers;
- leave SDK, notebook, CLI JSON, and web transport expansion to slice 8 unless
  a current text-only path already works automatically through canonical text.

Tests:

- session execute of a large integer returns exact canonical text;
- CLI symbolic evaluation smoke covers a large integer and a large normalized
  rational;
- help tests still find manual-backed examples and exactness notes;
- notebook cached-output behavior is unchanged unless canonical text tests
  already cover it.

Documentation:

- update manual examples and local links for the new expression model;
- update `docs/aleph3_unified_plan.md` only if this slice changes roadmap
  state after implementation, not while merely planning.

Verification:

- `aleph3_symbolic_tests`;
- `aleph3_sdk_tests` when SDK parser or bridge tests are touched;
- `aleph3_notebook_tests` only if cached canonical text changes;
- `git diff --check`;
- final diff review for duplicate numeric semantics and stale
  checked-`int64_t` claims.

### Slice 5: Exact Arithmetic And Simplification Migration

Status: complete.

Completion notes:

- exact integer/rational `Plus`, `Times`, division, unary minus, comparison,
  and normalization use the shared exact-scalar model across the evaluator and
  normalized-head arithmetic rewrites;
- exact `Power` now evaluates exact integer powers of exact integer and
  rational bases through exact-scalar exponentiation before any finite-double
  fallback, preserving large integer powers and rational positive or negative
  integer powers;
- exact powers with unsupported oversized exponents remain symbolic instead of
  demoting to approximate machine-real evaluation;
- exact power growth consumes the runtime evaluation-step budget in strict
  execution contexts;
- focused regression coverage now proves large exact integer powers, exact
  rational powers, exact comparisons after power evaluation, and strict-budget
  exhaustion for exact power growth;
- manual arithmetic and help text document large exact powers, rational powers,
  exactness boundaries, and budget behavior.

Behavior delivered:

- migrate exact integer/rational arithmetic in `Plus`, `Times`, `Power`,
  division, unary minus, comparison, and normalization to the exact-scalar
  module;
- remove unchecked numerator/denominator multiplication from simplification
  and rewrite paths;
- define and test budget or size-limit diagnostics for exact arithmetic growth.

Likely locations:

- `src/evaluator/EvaluatorBuiltins.cpp`;
- `src/evaluator/SimplificationRules.cpp`;
- `src/kernel/Rewrite.cpp`;
- `include/normalizer/Normalizer.hpp`;
- `src/transforms/Transforms.cpp`;
- `include/evaluator/EvaluatorSemantics.hpp` if numeric metadata needs
  clarification.

Tests:

- large exact integer addition, multiplication, powers, rational addition, and
  rational multiplication preserve exact output;
- mixed exact and machine-real arithmetic still produces approximate `Number`
  only through documented inexact paths;
- canonical ordering compares exact rationals without converting to `double`;
- unsupported or over-budget growth reports deterministic diagnostics.

Documentation:

- update manual arithmetic examples with at least one large exact integer and
  one large rational example;
- update help text for `N`, numeric predicates, and arithmetic exactness notes
  where needed.

Verification:

- focused evaluator, simplification, rewrite, and CLI/session tests;
- `aleph3_symbolic_tests`;
- `git diff --check`.

### Slice 6: Exact Algebra Coefficients

Behavior delivered:

- migrate `ExactCoefficient` to the shared exact-rational representation;
- keep `ExactPolynomial`, rational-expression transformations, polynomial
  division, GCD, coefficient extraction, and factorization exact without
  `int64_t` coefficient overflow;
- retain algorithmic budgets for term growth and expensive divisor searches.

Likely locations:

- `include/algebra/ExactPolynomial.hpp`;
- `src/algebra/ExactPolynomialConversion.cpp`;
- `src/algebra/ExactPolynomialOps.cpp`;
- `src/algebra/ExactRationalExpression.cpp`;
- `src/algebra/ExactFactorization.cpp`;
- `src/algebra/PolyUtils.cpp`;
- `src/packs/AlgebraPack.cpp`.

Tests:

- `Expand`, `Collect`, `Coefficient`, `CoefficientList`,
  `PolynomialQuotient`, `PolynomialRemainder`, `GCD`, `Together`, `Cancel`,
  and supported `Factor` preserve large exact coefficients;
- formerly overflowing exact coefficient cases now succeed when within budgets;
- intentionally oversized factorization candidate spaces fail with a stable
  budget or unsupported diagnostic rather than hanging;
- inexact polynomial inputs remain on the documented inexact path.

Documentation:

- update [Kernel Exact Algebra Spec](kernel_exact_algebra_spec.md),
  [Algebra Supported Subset](algebra_supported_subset.md), and
  [packs-algebra manual](manual/packs-algebra.md);
- replace stale checked-`int64_t` claims with exact-scalar size-budget claims.

Verification:

- algebra-focused tests;
- `aleph3_symbolic_tests`;
- CLI examples from the algebra manual.

### Slice 7: Exact Dense Matrices

Behavior delivered:

- migrate dense matrix scalar entries to the shared exact-rational type;
- preserve current shape, element-count, singularity, and step-budget
  contracts;
- add scalar-size or intermediate-growth diagnostics where elimination can
  expand large rationals.

Likely locations:

- dense matrix value type and operations under `include/algebra/` and
  `src/algebra/`;
- public pack adapters in `src/packs/AlgebraPack.cpp`.

Tests:

- `Det`, `RowReduce`, and `LinearSolve` preserve large exact integer and
  rational entries;
- shape and unsupported symbolic/decimal diagnostics remain unchanged;
- oversized scalar growth reports the documented diagnostic.

Documentation:

- update [Exact Dense-Matrix Specification](algebra_dense_matrix_spec.md) and
  [packs-algebra manual](manual/packs-algebra.md).

Verification:

- dense matrix tests;
- `aleph3_symbolic_tests`;
- manual matrix examples.

### Slice 8: Cross-Surface Compatibility And SDK Exposure

Behavior delivered:

- settle exact scalar exposure through SDK schemas, host values, JSON, CLI
  machine-readable output, session responses, web/engine APIs, and notebook
  cached outputs;
- preserve backward compatibility for machine-real host APIs while adding
  string or tagged exact-scalar transport where needed;
- document which consumers can carry arbitrary-size exact values and which
  intentionally reject them.

Likely locations:

- `include/sdk/Types.hpp`, `include/sdk/Schema.hpp`, `src/sdk/Engine.cpp`;
- `src/tooling/aleph3_cli.cpp`, `src/tooling/SymbolicCliSupport.cpp`;
- `src/session/Session.cpp`;
- `include/notebook/Notebook.hpp`, `src/notebook/Notebook.cpp`;
- `src/web/*Api.cpp` where persisted or JSON results carry numeric values.

Tests:

- CLI plain text and JSON output preserve large exact integers as exact text or
  tagged exact values;
- notebook save/load round trips cached large exact outputs without evaluation;
- SDK trusted parsing either accepts exact values through an approved transport
  or rejects them with a stable diagnostic;
- web/engine tests remain compatible with the paused web surface.

Documentation:

- update SDK stable interfaces, SDK guide, notebook manual sections, CLI
  help, and any JSON/API examples affected by exact scalar transport.

Verification:

- `aleph3_sdk_tests`;
- `aleph3_notebook_tests`;
- web/engine API tests if numeric JSON changes;
- full `ctest --test-dir build -C Release --output-on-failure`.

### Slice 9: Documentation And Compatibility Cleanup

Behavior delivered:

- remove stale `int64_t` product claims from user docs and focused specs;
- update examples so at least one large exact integer/rational case is covered
  by tests or executable verification;
- keep the deferred-work boundaries honest for arbitrary-precision floats,
  interval arithmetic, broad algebra, and solving.

Likely locations:

- `docs/manual/expressions-and-evaluation.md`;
- `docs/manual/concepts-and-terminology.md`;
- `docs/manual/built-in-functions.md`;
- `docs/manual/packs-algebra.md`;
- `docs/architecture.md`;
- `docs/algebra_supported_subset.md`;
- `docs/kernel_exact_algebra_spec.md`;
- `docs/algebra_dense_matrix_spec.md`;
- `docs/sdk/stable_interfaces.md`;
- `include/help/HelpTexts.hpp`;
- `docs/aleph3_unified_plan.md` only if roadmap state or priority changes.

Tests:

- documentation examples covered by focused tests, CLI fixture tests, or
  executable smoke commands;
- local links to changed docs still resolve.

Verification:

- affected test suites from earlier slices;
- Markdown link check if available, otherwise targeted manual link review;
- `git diff --check`;
- final diff review for duplicate numeric semantics.

## Cross-Cutting Acceptance Checklist

The complete remediation is done only when:

- exact scalar ownership is kernel-owned and used by packs and consumers;
- architecture, focused specs, manual pages, and help agree on the public
  numeric type names `Integer`, `Rational`, and machine `Real`;
- integer literals no longer pass through `double`;
- exact rationals normalize with arbitrary-precision numerator and denominator
  storage;
- exact arithmetic and algebra operations preserve exactness within documented
  budgets;
- inexact decimal behavior remains explicit and unchanged except where a
  separately approved bounded-numerical design says otherwise;
- all public diagnostics for unsupported, invalid, overflow, and budget cases
  are stable and documented;
- manual examples, help catalog entries, focused specs, SDK docs, notebook
  persistence, and CLI output agree with the implementation;
- focused positive/negative tests and affected broader suites pass.

## Open Decisions Before Slice 4 Implementation

The detailed slice 4 plan recommends the following decisions, but
implementation still requires explicit approval because the expression model is
a public kernel contract:

- add a first-class `Integer` expression alternative backed by
  `kernel::ExactInteger`;
- migrate `Rational` expression storage to `kernel::ExactRational`;
- keep the C++ machine-real struct named `Number` during slice 4 while
  documenting the public expression head as `Real`;
- keep SDK exact-scalar host transport, JSON transport, notebook persisted
  format changes, and full exact arithmetic migration for later slices.

Before implementation, confirm the recommended contracts for:

- whether `_Rational` and `RationalQ` should treat exact integers as rational
  values or reserve `Rational` strictly for non-integer exact fractions;
- whether this slice should introduce scalar-size budgets for expression
  construction itself, or defer size budgets until arithmetic growth in
  slice 5.
