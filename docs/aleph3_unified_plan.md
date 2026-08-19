# Aleph3 Unified Plan

## Status and Scope

This is Aleph3's sole active implementation roadmap. It contains unfinished
work, sequencing, and completion criteria. Current behavior belongs in the
[manual](manual/README.md), architectural ownership belongs in
[Architecture](architecture.md), and testable contracts belong in the focused
specifications listed by the [documentation index](README.md). Completed work
is intentionally removed from this file; Git history and current-behavior
documentation preserve that record.

Substantial features follow the
[Feature Development Workflow](feature_development_workflow.md). A roadmap item
does not override a specification, and completing an item includes updating
the owning specification and user documentation.

## Direction

Aleph3 is becoming a lightweight, local-first symbolic notebook and
computation environment written in modern C++. The kernel is its only semantic
core. The SDK is the stable host-embedding boundary, the CLI is the permanent
scripting and diagnostic surface, the session is shared interactive
infrastructure, and math grows through registered packs.

The first usable product is the local symbolic notebook: a small, coherent
environment for exact symbolic work, bounded numerical approximation, session
state, examples, and deterministic diagnostics. The first DSP pack follows
that notebook foundation and uses shared kernel contracts rather than moving
DSP-specific semantics into the kernel. Broader symbolic mathematics,
accelerated DSP, and advanced transform work remain later tranches.

The near-term product claim must remain narrower than a general-purpose CAS:
today's strongest surfaces are the SDK, CLI, session, and a bounded symbolic
subset. The desktop notebook, broader calculus, and wider domain mathematics
remain planned work.

The current Wolfram-like syntax is a frontend rather than the product identity.
Parser and printer work must keep syntax separate from kernel semantics so a
future Aleph3-native frontend remains possible.

Commercial and repository decisions are governed by
[IP and Repository Strategy](ip_and_repo_strategy.md). The free or
low-friction notebook should become useful before paid packs or hosted
services receive substantial investment.

## Non-Negotiable Boundaries

- `aleph3_kernel` owns expression meaning, evaluation, exactness, rewriting,
  assumptions, shared diagnostics, budgets, and extension contracts.
- `Expr` is the semantic representation. SDK `ir::Node` is a validated
  frontend form and must not become a peer evaluator representation.
- The SDK adds schemas, policies, host-facing types, validation, and
  deployment controls over kernel behavior.
- Packs own domain algorithms and register them through shared kernel
  contracts; they do not add private evaluators.
- CLI, session, notebook, and later IDE surfaces consume shared semantics.
  They do not add parser rules, simplifications, or fallback behavior.
- Exactness, unsupported behavior, diagnostics, compatibility, and resource
  limits remain explicit at every public boundary.
- Dynamic loading must not be claimed until registration lifetime, mutation,
  unload, and thread-safety rules are specified and tested.

## Priority Order

1. Finish the remaining interactive definition, session reset, completion,
   help, and cross-surface conformance work needed for coherent notebook use.
2. Close the smallest symbolic MVP gaps that are not already implemented:
   minimal lexical binding, rational-expression helpers, coefficient
   extraction, and first calculus follow-ups.
3. Fill bounded numerical and finite-list gaps that make the notebook useful
   for sampled data and exact-or-approximate exploration without weakening the
   exact symbolic default.
4. Choose a desktop toolkit from measured spikes and deliver the first visible
   notebook create/edit/evaluate/display/save/reopen/`Run All` loop.
5. Add notebook cancellation, restart or reset, definition clearing flows,
   discoverability, example gallery, packaging, and interactive budget
   enforcement around that loop.
6. Continue exact algebra hardening required by calculus, bounded solving,
   finite summation, and future DSP work.
7. Specify the shared kernel prerequisites for the first DSP pack without
   adding DSP-specific semantics to the kernel.
8. Deliver a focused DSP pack for finite sequences, convolution, FIR filtering,
   and direct DFT/inverse DFT.
9. Extend DSP with bounded transform functionality such as a small unilateral
   Z-transform, while leaving acceleration and broad transform theory for later.

Each product tranche should pair a contract improvement with a mathematical
capability and a user-visible or cross-surface way to exercise it. Missing
shared behavior feeds back into the kernel or session rather than being
implemented privately in a pack or UI.

## Active Milestones

### Durable Extensibility

Outcome: registered behavior and symbol definitions are suitable for
long-lived embedding and future pack loading.

Remaining work:

- move remaining host and richer builtin execution behind registry- or
  definition-backed contracts where appropriate;
- define richer definition categories and their lookup/precedence rules;
- prove another nontrivial behavior through shared definition state rather
  than evaluator-local branching;
- grow beyond the implemented registry lifecycle contract only when runtime
  pack loading or unload becomes an active slice;
- keep evaluation-control attributes deliberately bounded until their hooks
  and precedence are tested.

### Interactive Definitions and Session Semantics

Outcome: notebook, CLI, session, and SDK consumers that admit symbolic input
use one predictable state model for assignments, user functions, cleanup,
completion, and reset behavior.

The implemented `Set`, `SetDelayed`, user function, `Clear`, `Unset`, and
precedence foundations are owned by the symbol model, definition precedence,
and manual documents rather than tracked here as unfinished work.

Remaining work:

- specify and implement explicit session reset or restart behavior for CLI and
  notebook consumers, including definition clearing and cached-output effects;
- close any remaining gaps in the bounded definition subset contract across
  CLI, session, SDK where applicable, and notebook fixtures;
- keep completion and help output consistent for builtins, pack functions,
  host functions, and session-local user definitions;
- make cross-surface tests prove that provider-owned behavior still wins over
  user definitions according to the shared precedence contract;
- keep `With` as a planned lexical binding construct, not a replacement for
  assignments, user functions, cleanup, or session reset.

Full Mathematica assignment semantics, upvalues/downvalues, dynamic scoping,
and unrestricted evaluation-control attributes remain outside this milestone.

### Exact Algebra Depth

Outcome: algebra growth relies on explicit exact coefficient and ordering
contracts rather than a floating-point-centered legacy path.

Remaining work:

- introduce or finish exact coefficient-ring abstractions and a documented
  large-integer/overflow strategy;
- remove growth-facing dependence on `double` polynomial internals;
- deepen exact multivariate division, GCD, and factorization only in that
  order, with explicit monomial ordering and canonical-form invariants;
- extend bounded exact matrix work only where the shared scalar layer can
  preserve exactness and diagnose unsupported cases;
- keep the supported algebra subset and manual examples synchronized with
  every expansion.

General multivariate GCD/factorization, broad approximate matrices,
eigenvalue workflows, and advanced decompositions are outside this milestone
unless separately planned.

### Focused Differentiation and Notebook UI Decision

Outcome: the first calculus pack proves assumption-aware transformation, and
a measured toolkit decision unblocks the graphical notebook.

Remaining work:

- run bounded Qt and webview/native-wrapper spikes against the same notebook
  and session fixture;
- record the toolkit decision and packaging implications in Architecture.

### Symbolic MVP Gap Closure

Outcome: the first public CLI/notebook experience has the small symbolic
operations users naturally reach for while staying narrower than a
general-purpose CAS.

Remaining work:

- introduce one minimal lexical binding construct, preferably `With`, before
  broader `Module` or `Block` semantics;
- extend exact algebra with `Cancel`, `Together`, `Numerator`, and
  `Denominator` over a bounded rational-expression subset, leaving `Apart`
  until the partial-fraction contract is credible;
- follow the focused differentiation pack with higher-order `D[expr, {x, n}]`
  and simple partial-derivative workflows;
- specify a small exact `Solve` tranche after the notebook MVP unless a
  dedicated architecture pass proves it belongs earlier. The first credible
  subset is one equation in one variable for exact linear and quadratic cases,
  with explicit rejection of multivariate, transcendental, inequality,
  numerical, and general polynomial solving;
- keep `Limit`, local power-series expansion, finite `Sum`, and infinite
  summation as separate contracts rather than one broad calculus bucket.

Broad scoping, general functional programming, general-purpose simplification,
solver infrastructure, broad integration, arbitrary precision, and full
Mathematica compatibility remain outside this milestone.

### CAS Engine Roadmap Gap Closure

Outcome: Aleph3 grows from a bounded symbolic notebook engine toward a more
capable CAS without weakening the kernel ownership, exactness, diagnostics,
assumption, domain, and unsupported-behavior contracts that make the smaller
system reliable.

This roadmap records the engine gaps identified against the desired CAS
checklist. It is not part of the Web MVP launch scope, and it must not cause
the browser, BFF, CLI, session, SDK, or packs to invent private symbolic
semantics. Each item below needs a focused specification before implementation
unless an existing specification already owns the contract.

Already implemented foundations to preserve:

- shared `Expr` representation for symbols, exact integers and rationals,
  machine numbers, complex values, calls, lists, assignments, rules, and user
  definitions;
- normalization, structural equality, canonical ordering, bounded rewriting,
  capture-safe substitution, free and bound variable inspection, and limited
  like-term and exponent normalization;
- exact checked-rational arithmetic, explicit overflow diagnostics, and
  opt-in machine-real approximation through the current `N` surface;
- bounded algebra pack support for `Expand`, `Factor`, `Collect`, `GCD`,
  `PolynomialQuotient`, `Coefficient`, `CoefficientList`, and exact dense
  matrix operations;
- narrow assumption and domain facts through `Assuming`, `Refine`, sign
  predicates, nonzero facts, and integer/rational/real predicates;
- focused differentiation through the calculus pack.

Near-term engine gaps after the Web MVP:

- finish rational-expression helpers: `Cancel`, `Together`, `Numerator`, and
  `Denominator`, with domain restrictions and singularities represented or
  diagnosed instead of erased;
- add a first mathematical equivalence contract, such as
  `Equivalent[expr1, expr2]`, with proof-bearing methods limited to structural
  equality after normalization, supported algebraic reduction, polynomial
  comparison, and `expr1 - expr2 -> 0` inside the exact supported subset;
- keep any numerical sampling fallback separate from proof results and label
  it as evidence, not equivalence;
- define a domain-restriction carrier for transformations that can introduce,
  remove, or preserve excluded points, starting with rational-expression
  cancellation and square-root or logarithm rewrites;
- extend focused differentiation with higher-order derivatives and simple
  partial-derivative workflows before broad calculus.

Mid-term CAS tranche:

- specify and implement the first exact `Solve` subset for one equation in one
  variable covering linear and quadratic cases, with explicit solution sets,
  domain restrictions, and rejection of unsupported multivariate,
  transcendental, inequality, numerical, and general polynomial solving;
- specify local truncated `Series` after differentiation and exact algebra
  helpers are stable, including expansion variable, expansion point, order,
  order-term representation, truncation arithmetic, and conversion back to an
  ordinary expression;
- specify simple algebraic and known-function `Limit` separately from series,
  including finite and infinite target forms, one-sided limits as deferred
  unless explicitly included, and deterministic unsupported diagnostics;
- add a deliberately small rule-based `Integrate` subset only after
  assumptions, domains, and transformation metadata can prevent invalid
  antiderivative claims;
- broaden exact dense linear algebra only where checked exact coefficients,
  diagnostics, and resource budgets remain explicit.

Longer-term CAS tranche:

- introduce arbitrary-precision integer and rational arithmetic only through an
  approved exact-scalar compatibility design that accounts for storage,
  printing, SDK exposure, overflow replacement, and performance budgets;
- broaden complex arithmetic from the current representation and arithmetic
  into exact symbolic constants, conjugation, real and imaginary part,
  magnitude, argument, and branch-sensitive simplification;
- add first-class `Piecewise` before broad limits, integration, summation, or
  solving workflows depend on case splits;
- grow equation solving beyond the first exact subset only through separate
  contracts for polynomial systems, inequalities, transcendental equations, and
  numerical fallback;
- expand matrices beyond the current dense exact surface only after symbolic,
  approximate, inverse, rank, eigenvalue, and decomposition contracts are
  specified.

Tutor-grade equivalence and step validation are architectural capabilities, not
optional polish. The first validation surface should be designed before broad
solving or integration becomes active:

- define a `ValidateTransformation`-style contract over a before expression,
  after expression, and assumptions;
- return structured outcomes such as `VALID`, `INVALID`,
  `VALID_WITH_CONDITIONS`, `LOSES_SOLUTIONS`, `INTRODUCES_SOLUTIONS`, and
  `UNKNOWN`, with machine-readable conditions and diagnostics;
- distinguish implication direction for equations and transformations, so a
  step like `x^2 = 4` to `x = 2` can be reported as incomplete or
  solution-losing rather than simply false;
- reuse shared assumptions, domain restrictions, equivalence checking,
  polynomial comparison, and exact solving subsets instead of building a
  separate tutor evaluator;
- attach transformation metadata to simplification, algebra, calculus, and
  rewrite results where the metadata is needed for validation or explanation.

Broad CAS behavior remains incremental. No milestone may claim general
Mathematica or SymPy parity, broad theorem proving, broad integration, broad
solving, or broad branch analysis until focused contracts, tests, diagnostics,
manual pages, help entries, and cross-surface fixtures make those claims true.

### Bounded Numerical and Finite Data MVP

Outcome: the notebook can mix exact symbolic work with opt-in machine-real
approximation and finite data operations without silently abandoning exactness.

Already implemented numeric and list behavior remains documented in the manual
and focused specifications. Roadmap work here is only for the remaining gaps.

Remaining work:

- specify the bounded approximate-evaluation contract around exact integers
  and rationals by default, decimal literals, machine reals, `N[expr]`, mixed
  exact and machine-real arithmetic, supported constants, elementary functions,
  overflow, invalid domains, non-finite values, and budget limits;
- specify and implement remaining finite-list and iteration needs such as
  `Range`, `Table`, and `Total`, with exactness preservation, size budgets, and
  deterministic errors for invalid indexes, unsupported symbolic bounds, and
  oversized results;
- decide which operations are kernel structural contracts and which belong in
  a core pack, while keeping DSP-specific sequence conventions in the DSP pack;
- preserve implemented `List`, `Length`, one-level `Part`, `Map`, `Apply`,
  `Select`, `Cases`, and current `N` behavior as current-contract foundations,
  not future work.

Arbitrary precision, broad numerical analysis, interval arithmetic,
arbitrary-precision transcendental evaluation, lazy sequences, sequence
patterns, and unbounded traversal remain deferred.

### Focused Series and Summation

Outcome: later notebook-adjacent symbolic math distinguishes local truncated
series, finite symbolic summation, and infinite series instead of conflating
them.

Remaining work:

- after focused differentiation, specify bounded local power-series expansion
  such as `Series[Exp[x], {x, 0, 5}]` with expansion variable, expansion point,
  truncation order, first-class truncated-series representation, bounded
  addition and multiplication, conversion back to an ordinary expression, and
  clear order-term rendering;
- specify finite symbolic `Sum` over the shared binding, substitution,
  assumptions, budget, and diagnostic contracts. The first subset should cover
  exact finite integer bounds and only explicitly supported families such as
  constants, arithmetic progressions, simple polynomial sums, and finite
  geometric sums;
- keep infinite series out of the notebook MVP. A later bounded tranche may
  cover geometric series, finite modifications of geometric series, simple
  telescoping cases, and possibly basic p-series recognition, but only with
  explicit convergence conditions.

Broad asymptotic, Puiseux, Laurent, branch-analysis, analytic-continuation,
special-function summation, infinite products, and general convergence
analysis remain deferred.

### DSP Kernel Prerequisites

Outcome: the kernel can support a small educational DSP pack without hardcoding
DSP-specific functions, objects, or simplifications into the kernel.

Remaining work:

- specify first-class `Piecewise` representation, evaluation, diagnostics, and
  assumption-aware simplification boundaries;
- build on shared finite data contracts for `Range` and related list
  generation instead of treating them as private DSP prerequisites; keep
  `Linspace` and `Sample` specified where their numeric or signal-processing
  conventions are actually needed;
- introduce held or bound-variable constructs for a focused `Sum` surface, and
  leave `Product` to follow only after the same binding contract is proven;
- build on the implemented `FreeVariables`, `BoundVariables`, `DependsOn`, and
  capture-safe substitution contract when adding summation, calculus, or
  rewrite features that need shared binding;
- add only the linear inequality normalization and bounded integer-range
  counting needed by the first DSP and summation workflows;
- add conditional rewrite-rule support through the shared assumptions query
  surface, with bounded matching and deterministic diagnostics;
- attach rule and transformation metadata sufficient for future explanation
  traces without promising broad natural-language proof output;
- record the printer/export direction for LaTeX or richer display formats
  without making rich formatting part of the immediate product claim.

Broad summation, nonlinear inequality solving, sequence-pattern matching,
general product simplification, rich proof explanation, and plotting remain
outside this milestone unless a focused specification brings them in.

### Local Notebook MVP

Outcome: a user can create and edit a local document, evaluate supported input
through one shared session, understand results or failures, save and reopen the
document, run all inputs, and launch an installable Windows-first desktop
package without a development environment.

Remaining work:

- build the graphical application over the existing headless document,
  persistence, and clean `Run All` foundations;
- add input/text/output presentation, queued/running/completed/cancelled/failed
  statuses, canonical-text fallback, and structured diagnostic display without
  UI-owned semantics;
- verify save/reopen, explicit execution-order semantics, stale cached-output
  presentation, and recovery after evaluation or application failure in the
  application;
- add cancellation, restart or reset, definition-clearing flows, and
  interactive enforcement of evaluation budgets through shared session/kernel
  contracts;
- provide minimal help and function discovery for supported operations,
  including accepted forms, examples, exactness behavior, unsupported
  boundaries, and owning component where useful;
- ship a tested example gallery covering current arithmetic, rewriting,
  assumptions, polynomial algebra, matrices when documented, and focused
  differentiation when delivered;
- add packaging smoke tests and a keyboard-only workflow check;
- harden CLI tracing, timing, reproducibility, and pack diagnostics where they
  support the same workflows.

The complete behavioral contract and non-goals are in
[Notebook MVP Design](notebook_mvp_design.md).

### Focused Digital Signal Processing Pack

Outcome: a registered `aleph3_pack_dsp` supports a deliberately small,
well-diagnosed exact or mixed-exact workflow.

Remaining work:

- depend on the focused DSP kernel prerequisite contracts for piecewise forms,
  finite sampling, binding, substitution, conditional rules, and bounded
  inequality reasoning instead of adding private pack semantics;
- specify finite discrete sequences, shared-list or sequence indexing,
  convolution, FIR filtering, exactness, mixed-exact behavior, budgets, and
  unsupported forms;
- include direct DFT and inverse DFT in the first DSP MVP, using a simple
  `O(N^2)` implementation if needed. The specification must define forward and
  inverse conventions, normalization, finite numerical inputs, small bounded
  exact or symbolic inputs, roots-of-unity representation, result ordering,
  input-size budgets, and cross-surface behavior;
- implement those operations through existing kernel and pack contracts;
- add a bounded unilateral Z-transform tranche either at the end of the first
  DSP MVP or immediately after it, only after algebra, calculus, assumptions,
  and DSP prerequisite dependencies are explicit. Regions of convergence must
  be represented or diagnosed explicitly, and a rational expression alone must
  not be treated as a complete bilateral transform result;
- support a bounded frequency-response workflow by evaluating rational
  transfer functions on the unit circle without claiming a general DTFT;
- expose supported operations consistently through shared consumers.

FFT acceleration, general symbolic DTFT, continuous Fourier transform, inverse
continuous Fourier transform, Fourier series, broad transform tables,
distribution theory, multidimensional Fourier transforms, signal plotting,
streaming audio, codecs, real-time scheduling, hardware integration, and image
processing are outside the first DSP pack.

## Cross-Cutting Workstreams

### Syntax and Diagnostics

- harden supported-input round trips and canonical rendering;
- improve source-aware parse diagnostics and deterministic rejection;
- document compatibility syntax honestly and evaluate dual frontends only as
  a separately approved design;
- never change syntax and semantic contracts accidentally in the same slice.

### Rewrite and Assumptions

- add richer single-expression pattern classes only when their bounds and
  evaluation phase are explicit;
- add conditional rules only through the shared assumptions surface, with
  deterministic failure behavior and no private pack predicate engine;
- keep sequence-pattern machinery deferred until a demonstrated workflow
  requires it;
- expand domain queries and assumption-aware hooks through the shared kernel
  surface;
- broaden domain categories beyond integer/rational/real only after sign,
  zero, contradiction, and transformation behavior is solid;
- keep structural replacement, simplification-stage normalized-head rewrites,
  and future symbol-bound transformations distinct.

### SDK and Consumer Conformance

- keep the stable SDK subset small, explicit, and adoption-ready;
- improve fast-start embedding and host-integration examples;
- expose exact values publicly only with an approved compatibility design;
- maintain shared semantic fixtures across kernel, session, CLI, notebook,
  packs, and SDK where schemas/policies admit the expression;
- keep CLI scripting deterministic and machine-readable without adding
  CLI-only semantics.

### Discoverability

- provide a minimal CLI and notebook help catalog for supported operations,
  accepted forms, one or two examples, exactness or approximation behavior,
  unsupported boundaries, and owning pack or component where useful;
- fill out structured, manual-backed help entries for the existing supported
  surface, prioritizing currently sparse catalog entries before adding richer
  product-shell search. Each entry should include accepted forms, short
  examples covered by manual text or tests, exactness notes where relevant,
  unsupported boundaries, owning component or pack, and a local manual anchor;
- keep completions deterministic across builtins, pack functions, host
  functions, and session-local definitions;
- defer broad documentation search, rich tutorials in the product shell, and
  natural-language help until the supported surface and packaging are stable.

### Quality and Documentation

- organize tests by ownership layer and preserve affected broader coverage;
- add negative tests for unsupported forms, budget exhaustion, malformed
  persisted input, and cross-surface diagnostic stability;
- keep build/CI coverage honest across supported configurations;
- document every user-visible capability in the manual with runnable examples
  and explicit unsupported boundaries;
- update focused specifications, the supported-subset contract, CLI help,
  README, index, or this roadmap only when each document owns the change;
- validate manual examples and local documentation links before completion.

### Transitional Cleanup

- remove compatibility aliases and adapters only after dependents migrate;
- continue moving domain logic out of evaluator-oriented code without changing
  behavior silently;
- keep target ownership and dependency direction visible as directories move;
- treat stale transitional wording as a defect rather than preserving it as
  history in current documentation.

## Immediate Action Queue

Use this order unless a regression or dependency changes it:

1. Finish the remaining interactive definition/session semantics work:
   session reset, definition clearing flows, completion/help consistency, and
   cross-surface fixtures. As part of the help/discovery follow-through, fill
   sparse structured help entries with manual-backed examples and unsupported
   boundaries for the current supported surface.
2. Specify and implement the remaining symbolic MVP gap-closure items across
   rational-expression helpers, lexical binding, and calculus follow-ups.
3. Specify and fill the remaining bounded numerical and finite-list gaps,
   especially `Range`, `Table`, `Total`, budget behavior, and unsupported
   diagnostics.
4. Run and record the notebook toolkit spikes against one shared fixture.
5. Build the first graphical notebook vertical slice, then add cancellation,
   reset, help, example gallery, packaging, and keyboard workflow checks.
6. Continue exact algebra hardening required by calculus, bounded solving,
   finite summation, future DSP work, and the CAS engine roadmap gap-closure
   tranche.
7. After the Web MVP is stable, start the CAS engine gap-closure sequence with
   rational-expression helpers, mathematical equivalence, and
   domain-restriction contracts before broad solving, integration, or
   validation.
8. Specify the focused DSP kernel prerequisite slice, including piecewise,
   finite sampling, binding, substitution, conditional rewrites, and bounded
   linear inequality reasoning.
9. Deliver the first DSP pack for finite sequences, convolution, FIR
   filtering, direct DFT, and inverse DFT.
10. Specify the bounded Z-transform and later transform tranche, keeping FFT
   acceleration and broad Fourier work deferred.

## Deferred Work

Defer until an active milestone establishes the required contracts:

- broad integration and differential-equation solving;
- broad CAS parity beyond the staged CAS engine roadmap gap-closure contracts;
- tutor-grade step or transformation validation beyond a separately specified
  equivalence, domain-restriction, and validation contract;
- broad symbolic summation and product simplification beyond the focused
  bound-variable contracts;
- infinite symbolic summation beyond a later bounded, convergence-aware tranche;
- broad lexical and dynamic scoping beyond the first `With`-style MVP binding
  construct;
- general functional-programming libraries beyond the finite-list MVP
  operations;
- lazy sequences and unbounded iteration;
- nonlinear or general-purpose inequality solving;
- general sequence patterns and unbounded traversal;
- solver infrastructure beyond a separately specified exact one-variable
  linear/quadratic subset;
- broad local-series, asymptotic-series, branch-analysis, analytic-continuation,
  and special-function expansion;
- arbitrary-precision integer, rational, and big-float arithmetic beyond the
  current checked exact coefficient strategy;
- broad numerical analysis, interval arithmetic, and arbitrary-precision
  transcendental evaluation;
- plotting beyond a separately specified bounded data/display contract;
- full natural-language explanation traces beyond focused rule and
  transformation metadata;
- rich in-product documentation search and natural-language help;
- FFT acceleration, general DTFT, continuous Fourier transforms, Fourier
  series, broad transform tables, distribution theory, multidimensional
  transforms, and advanced DSP workflows;
- rich export, collaboration, cloud execution, provider-backed AI features,
  and a marketplace;
- paid engineering, education, finance, or code-generation packs;
- large GUI/IDE investment beyond the notebook MVP;
- broad native-syntax design before current input is predictable and well
  diagnosed.

## Completion Standard

A roadmap item is complete only when:

- ownership and public/architectural behavior are reflected in the relevant
  specification;
- exactness or approximation semantics, budgets, diagnostics, compatibility,
  and unsupported boundaries are explicit;
- focused positive and negative tests plus affected broader tests pass;
- shared semantic fixtures cover cross-surface behavior where kernel, packs,
  session, CLI, notebook, or SDK consumers share the feature;
- user-visible behavior has accurate manual documentation and verified
  examples;
- CLI and notebook examples, local documentation links, help catalogs, and
  indexes remain consistent;
- the final diff has been reviewed and contains no stale plan claims.

The overall program succeeds when the graphical notebook provides the narrow
local workflow above, higher mathematics grows through durable registered
packs, all consumers share one kernel and session model, and supported behavior
is documented and tested as a product contract rather than inferred from
implementation details.
