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

1. Harden parser/printer round trips, canonical rendering, diagnostics,
   budgets, and explicit unsupported behavior.
2. Finish symbol, definition, registration, rewrite, and assumptions contracts
   needed by packs and long-lived embedding.
3. Remove remaining floating-point-centered dependencies from growth-facing
   exact algebra and deepen the bounded algebra pack.
4. Deliver a focused differentiation pack over shared kernel contracts.
5. Specify and deliver the kernel prerequisites needed for a small DSP pack
   without adding DSP-specific semantics to the kernel.
6. Choose a desktop toolkit from measured spikes and deliver the first visible
   notebook create/edit/evaluate/display/save/reopen/`Run All` loop.
7. Enforce conformance across kernel, packs, session, CLI, notebook fixtures,
   and the SDK where its trusted subset permits.
8. Add a focused DSP pack only after its algebra, calculus, and kernel
   prerequisite contracts are credible.

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
- specify registry mutation, lifetime, thread safety, and eventual pack
  loading/unload boundaries;
- keep evaluation-control attributes deliberately bounded until their hooks
  and precedence are tested.

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

- specify a narrow differentiation subset, including variables, constants,
  supported heads, chain/product rules, simplification, budgets, and failure
  behavior;
- implement it as a registered pack using shared rewrite, assumptions,
  exactness, and diagnostic contracts;
- expose it consistently through session and CLI, and through the SDK only if
  the trusted-subset contract permits;
- run bounded Qt and webview/native-wrapper spikes against the same notebook
  and session fixture;
- record the toolkit decision and packaging implications in Architecture.

### DSP Kernel Prerequisites

Outcome: the kernel can support a small educational DSP pack without hardcoding
DSP-specific functions, objects, or simplifications into the kernel.

Remaining work:

- specify first-class `Piecewise` representation, evaluation, diagnostics, and
  assumption-aware simplification boundaries;
- define finite `Range`, `Linspace`, and `Sample` contracts that preserve
  exactness and report unsupported numeric, symbolic, or oversized ranges
  explicitly;
- introduce held or bound-variable constructs for a focused `Sum` surface, and
  leave `Product` to follow only after the same binding contract is proven;
- specify `FreeVariables`, `BoundVariables`, `DependsOn`, and capture-safe
  substitution so summation, calculus, and rewrite rules share one binding
  model;
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
document, and run verified examples.

Remaining work:

- build the graphical application over the existing headless document,
  persistence, and clean `Run All` foundations;
- add input/text/output presentation, canonical-text fallback, and structured
  diagnostic display without UI-owned semantics;
- verify save/reopen and stale cached-output presentation in the application;
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
- specify finite discrete sequences, convolution, FIR filtering, exactness,
  budgets, and unsupported forms;
- implement those operations through existing kernel and pack contracts;
- add rational transfer-function and z-domain work only after dependencies on
  algebra, calculus, assumptions, and the DSP kernel prerequisites are
  explicit;
- expose supported operations consistently through shared consumers.

FFT acceleration, streaming audio, codecs, real-time scheduling, hardware
integration, and image processing are outside the first DSP pack.

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

1. Specify and implement the focused differentiation slice.
2. Close the registration lifetime/mutation/thread-safety design needed by
   long-lived embedding and future packs.
3. Specify the focused DSP kernel prerequisite slice, including piecewise,
   finite sampling, binding, substitution, conditional rewrites, and bounded
   linear inequality reasoning.
4. Run and record the notebook toolkit spikes against one shared fixture.
5. Build the first graphical notebook vertical slice.
6. Continue exact algebra hardening required by calculus and future DSP work.
7. Expand cross-surface conformance and packaging verification around each
   delivered slice.

## Deferred Work

Defer until an active milestone establishes the required contracts:

- broad integration and differential-equation solving;
- broad symbolic summation and product simplification beyond the focused
  bound-variable contracts;
- nonlinear or general-purpose inequality solving;
- general sequence patterns and unbounded traversal;
- broad special-function expansion;
- solver infrastructure;
- plotting beyond a separately specified bounded data/display contract;
- full natural-language explanation traces beyond focused rule and
  transformation metadata;
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
- exactness, budgets, diagnostics, compatibility, and unsupported boundaries
  are explicit;
- focused and affected broader tests pass;
- user-visible behavior has accurate manual documentation and verified
  examples;
- local documentation links, CLI help, and indexes remain consistent;
- the final diff has been reviewed and contains no stale plan claims.

The overall program succeeds when the graphical notebook provides the narrow
local workflow above, higher mathematics grows through durable registered
packs, all consumers share one kernel and session model, and supported behavior
is documented and tested as a product contract rather than inferred from
implementation details.
