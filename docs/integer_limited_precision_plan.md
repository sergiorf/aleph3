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

Behavior delivered:

- make exact integers and rationals first-class expression values using the
  approved representation;
- settle the architecture-visible numeric taxonomy: exact `Integer`, exact
  `Rational`, machine `Real`, and the relationship between semantic names and
  C++ implementation names;
- update constructors, structural equality, canonical rendering, `FullForm`,
  `Head`, `IntegerQ`, `RationalQ`, `NumberQ`, serialization where present, and
  pattern matching for `_Integer` and `_Rational`;
- preserve machine-real `Number` for decimals and approximate results.

Likely locations:

- `include/expr/Expr.hpp`;
- `src/expr/Expr.cpp`;
- `include/expr/FullForm.hpp`;
- `src/session/Session.cpp`;
- `src/kernel/Assumptions.cpp`;
- `src/kernel/Rewrite.cpp`;
- pattern and predicate code under evaluator/kernel.

Tests:

- `Head[3] -> Integer`, `IntegerQ[large] -> True`, and
  `NumberQ[large]` follows the documented predicate contract;
- large exact integers render canonically and round-trip through session/CLI;
- exact rational signs and denominator-one values render deterministically;
- `_Integer` and `_Rational` pattern constraints match the new values.

Documentation:

- update [Architecture](architecture.md) with the new `Expr` numeric
  alternatives and ownership rules for `Integer`, `Rational`, and machine
  `Real`;
- update manual concepts, built-ins, expressions/evaluation, help entries, and
  focused specs that currently say exact coefficients use `int64_t`;
- keep SDK behavior explicit where host APIs still expose bounded numeric
  values.

Verification:

- `aleph3_symbolic_tests`;
- focused parser, evaluator, assumptions, rewrite, and session tests;
- `aleph3_sdk_tests` if shared syntax or predicates affect SDK lowering.

### Slice 5: Exact Arithmetic And Simplification Migration

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

## Open Decisions Before Slice 4

- Confirm whether the expression model should add a first-class `Integer`
  alternative or represent integers as denominator-one rationals.
- Confirm whether the C++ `Number` type is renamed to `Real` in the same slice
  or kept as an implementation name with `Real` as the documented semantic
  type.
- Choose the exact scalar dependency: `boost::multiprecision::cpp_int`,
  vendored header-only code, or another approved internal type.
- Set initial scalar-size budgets and decide whether they are global kernel
  budgets or operation-local guards.
- Decide SDK exact-scalar transport: reject oversized exact values at the SDK
  boundary, expose tagged decimal strings, or add first-class SDK exact scalar
  host types.
- Decide JSON transport for CLI, notebook caches, and web/engine APIs before
  changing persisted formats.
