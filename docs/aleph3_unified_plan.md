# Aleph3 Unified Plan

## Status

This is the canonical planning document for Aleph3.

It replaces the previous split across:

- `kernel_refactor_urgent_plan.md`
- `kernel_refactor_backlog.md`
- `symbolic_engine_product_plan.md`
- `symbolic_evaluator_cleanup_plan.md`

Supporting architecture and contract documents remain valid, but they should
refer back to this file for roadmap, sequencing, and implementation priority.

For the stable, high-level system picture, see
[Architecture](architecture.md).

Implementation specs referenced by this plan:

- [Kernel Design Spec](kernel_design_spec.md)
- [Kernel Symbol Model Spec](kernel_symbol_model_spec.md)
- [Kernel Attribute Spec](kernel_attribute_spec.md)
- [Kernel Rewrite Spec](kernel_rewrite_spec.md)
- [Kernel Execution Bridge Spec](kernel_execution_bridge_spec.md)
- [Kernel Exact Algebra Spec](kernel_exact_algebra_spec.md)
- [Kernel Registration Lifecycle Spec](kernel_registration_lifecycle_spec.md)
- [Kernel Assumptions Spec](kernel_assumptions_spec.md)

## Purpose

Aleph3 needs one plan because its highest-level product direction, core
architecture migration, symbolic-kernel evolution, and cleanup backlog all
describe the same program.

That program is:

1. converge on one symbolic kernel
2. make the SDK a constrained consumer of that kernel
3. grow higher mathematics through serious packs
4. make the CLI a permanent interactive product over the same semantics
5. build a reusable session foundation for future IDE and workbench surfaces
6. establish digital signal processing as a serious domain pack after the
   algebra and calculus foundations are credible
7. harden the supported symbolic subset into a production-grade core

## Strategic Position

Aleph3 is a layered symbolic math system.

Near-term external positioning should stay honest:

- Aleph3 is already a strong embeddable formula and symbolic engine
- the most product-ready surface today is the trusted SDK subset plus the
  kernel-backed symbolic surface around it
- Aleph3 is not yet close enough to Mathematica-class CAS breadth to market
  itself that way without inviting the wrong comparison

Recommended near-term message:

- a safe embeddable symbolic/formula engine in C++20, with a path toward
  richer CAS features

The intended product stack is:

1. `aleph3_kernel`
   The core symbolic engine and main strategic asset.
2. `aleph3_sdk`
   The host-facing embedding layer built on top of the kernel and the first
   practical adoption surface for external developers.
3. `aleph3_packs_*`
   Domain math growth such as algebra, special functions, calculus, signal,
   and electrical engineering.
4. tools and future products
   CLI, notebook-style interactive applications, examples, hosted services,
   commercial SDK packaging, and other product surfaces.

The kernel is not just implementation detail beneath the SDK.
It is the semantic center that all product surfaces depend on.
The SDK is the primary embedding surface, but it is not the only near-term
product investment: math packs and the interactive CLI must mature alongside
it so kernel capability is both meaningful and directly usable.

## Current Repository Reality

The repository is in a transitional state.

What is already true:

- `aleph3_kernel` exists as an explicit build target
- `aleph3_sdk` already depends on `aleph3_kernel`
- the first concrete pack target now exists for polynomial algebra
- the symbolic engine already has a meaningful supported subset with parser,
  evaluation, normalization, simplification, exact rationals, and a narrow
  algebra layer
- the SDK now evaluates through the kernel and retains lowered kernel
  execution state rather than trusted-subset IR
- the public SDK surface is documented and split into stable versus
  transitional API areas
- the kernel now has an initial symbolic registration contract, symbol
  metadata/definition records, and exact-rule rewrite infrastructure
- evaluator flow now populates those kernel contracts during registered
  symbolic resolution, builtin execution, host execution, assignment, and
  user-definition registration
- the first assumptions surface now exists through temporary assumption facts,
  `Assuming[...]`, `Refine[...]`, explicit sign predicates, and a small
  derived-sign layer for simple arithmetic forms
- the first richer assumptions category now exists through explicit
  integer/rational/real symbol-domain facts with exact-literal predicate
  answers
- typed single-expression patterns now support structural type constraints
  while sequence patterns remain explicitly unsupported
- conditional typed patterns now evaluate predicates through the same bounded
  kernel context used by `MatchQ`, `Replace`, and `ReplaceRepeated`
- univariate `Factor` now preserves exact rational coefficients for the
  supported rational-root workflow
- targeted replacement now supports explicit nonnegative depth and depth-range
  controls without widening into sequence patterns or broader level syntax
- exact single-divisor multivariate polynomial division now uses explicit
  variable precedence and the canonical graded-lexicographic order
- an experimental stateful session contract now owns reusable kernel context,
  structured results, and diagnostics for the symbolic CLI REPL
- the session and CLI now provide non-mutating expression inspection and
  deterministic pack discovery and completion over registry metadata

What is still unresolved:

- the best-supported product surface is still the trusted SDK subset rather
  than a broad CAS workflow
- exact rational internals are ahead of exact value exposure in the public SDK
  runtime surface
- polynomial and algebra internals still mix narrow exact support with
  floating-point foundations
- higher math boundaries are still blended into evaluator-oriented code
- symbol definition and extension contracts are not yet strong enough for the
  intended pack model
- assumptions and domain semantics are still intentionally narrow and mostly
  sign- and boolean-oriented rather than a broader domain engine
- the new engine-scoped registration catalog is in place, but long-term pack
  loading, unload boundaries, and stronger thread-safety guarantees still need
  to be made explicit
- the architecture writing is ahead of the finished product surface in several
  areas
- CI and repository hygiene still need product-grade hardening
- several advanced roadmap areas still depend on stronger kernel contracts

The main remaining architectural risk is not the old runtime split anymore.
It is whether the kernel grows a clean extension model before more symbolic
surface area gets added.

## Accepted Core Decisions

### One Kernel

Aleph3 must converge on one kernel.
There must not be two permanent peer evaluator cores.

### Representation Decision

`Expr` is the long-term kernel semantic representation.

`ir::Node` remains useful, but only as an SDK/trusted-subset representation for
parsing, validation, compile artifacts, or lowering inputs.
It is not a second kernel AST.

### SDK Position

The SDK owns:

- `Engine`
- `Schema`
- `Policy`
- host-facing types and diagnostics
- trusted-subset validation
- deployment controls and budgets

The SDK does not own a separate permanent semantic runtime.

The SDK should be treated as the first practical product surface:

- a small, stable, well-documented embedding API
- the easiest open-source adoption path for external developers
- a bridge from host applications into kernel-backed symbolic evaluation
- a validation layer for whether kernel contracts are usable from the outside
- the first monetizable surface Aleph3 should make excellent before broader
  CAS or notebook claims

### Near-Term Product Honesty

Until exact algebra, assumptions, richer matching, and pack extensibility are
materially stronger, Aleph3 should be positioned as:

- an embeddable symbolic/formula engine
- a kernel-first system with a safe trusted SDK subset
- a project on the road to richer CAS features

It should not yet be positioned as:

- a modern Mathematica-like CAS with near-parity expectations

### Syntax Strategy

Aleph3 should not tie its long-term identity to being only a clone of another
system's surface syntax.

Current Wolfram-like syntax is useful because:

- it is compact for symbolic work
- it makes early experimentation faster
- it lowers friction for users who already know that style

But product strategy should remain independent from that syntax.

Accepted direction:

- Aleph3 may support a Wolfram-like frontend as a compatibility or migration
  surface
- Aleph3 should reserve the option to introduce and prefer a clearer
  Aleph3-native surface syntax over time
- parser and evaluation architecture should treat syntax as a frontend choice,
  not as the product identity
- documentation and positioning should describe expressions in Aleph3 terms
  first, not as "Mathematica but in C++"

### Pack Position

Higher math growth should move through pack registration on top of the kernel,
not through unchecked evaluator branching.

### Product Surface Position

Interactive products such as a notebook-like application belong above the
kernel and SDK.

They may eventually supersede parts of the CLI surface, but they must remain
consumers of unified kernel semantics rather than becoming a third execution
center.

## Kernel Functional Scope

The kernel must eventually own the semantic functionality required for Aleph3
to behave like one coherent symbolic engine.

### Kernel-Owned Now Or Immediately Next

- expression and value representation centered on `Expr`
- symbolic evaluation loop and fallback behavior
- structural normalization and bounded simplification
- exact scalar arithmetic contracts, especially integers, rationals, and
  complex numbers
- symbol lookup, symbol values, and current user-defined function handling
- built-in metadata needed for evaluation behavior
- shared diagnostics and evaluation context
- registration interfaces used by symbolic functions, packs, and SDK-facing
  host integration

### Kernel Capabilities That Need To Be Expanded

- richer symbol definition storage beyond narrow call-definition handling
- explicit attribute model for evaluation control and symbol behavior
- general pattern matching
- rewrite and traversal APIs
- assumptions and domain-aware transformation hooks
- exact algebra-facing abstractions that higher symbolic math can depend on
- canonical unsupported-feature and partial-support behavior
- convergence path for SDK trusted-subset lowering into kernel execution

### Kernel Boundaries

The kernel should own:

- semantics
- expression transformation
- exact math foundations
- symbolic diagnostics
- extension registration contracts

The kernel should not own:

- host deployment policy
- schema allowlists
- UI concerns
- notebook/session presentation behavior
- product packaging ergonomics
- domain packs whose logic can live above stable kernel contracts

## Plan And Spec Split

This document is the canonical plan.

It answers:

- what Aleph3 is trying to become
- what work is most important
- what order the work should happen in
- which major milestones define progress

The implementation specs answer:

- how the kernel is decomposed
- what interfaces each subsystem owns
- how execution should flow
- how migration should proceed in detail

Rule:

- roadmap and priority changes belong here
- subsystem design detail belongs in the referenced specs

### Feature Planning

Use the [Feature Development Workflow](feature_development_workflow.md) to turn
substantial roadmap work into researched designs, decision-complete
implementation plans, and verified delivery. That workflow does not replace
this plan or the focused subsystem specifications. Small fixes remain
lightweight.

## Product Sequencing Rule

Aleph3 should remain kernel-first for the current phase.

Kernel-first does not mean product-last. Work should proceed as three
coordinated tracks:

1. kernel and SDK foundations
2. math-pack capability, beginning with algebra and followed by focused
   calculus
3. interactive experience, beginning with the CLI and a reusable session
   layer

After algebra proves exact pack workflows and calculus proves assumption-aware
transformation, digital signal processing becomes the next planned domain-pack
track. It must reuse those contracts rather than introducing private numeric,
expression, or session semantics.

Each track must consume the same kernel contracts. Pack or interactive work
that reveals a missing contract should feed that requirement back into the
kernel rather than create private semantics.

The project should avoid large standalone GUI, hosted-product, solver, or broad
CAS investments while foundational contracts remain transitional. It should
not defer narrow user-facing slices that exercise stable contracts. A useful
tranche therefore delivers one foundation improvement, one mathematical
capability, and one way for users to experience or inspect it.

## End State

The program is complete when all of the following are true:

- `aleph3_kernel` is the only semantic core
- SDK evaluation routes through the kernel
- the legacy runtime evaluator path is removed
- kernel-owned contracts exist for evaluation context, diagnostics, symbol
  definitions, and registration
- higher math growth follows pack boundaries instead of accumulating in the
  evaluator core
- algebra and focused calculus workflows are available through registered
  packs
- the CLI remains a supported product surface over unified kernel semantics
- CLI and future IDE/workbench consumers share one stateful session contract
- the supported symbolic subset is documented as a product contract, including
  the small coefficient-layer basis class and algebra-aware exponent class
- tests are organized by layer and validate semantic invariants

## Program Rules

These rules apply to all implementation work until the migration is complete:

- no new feature may deepen both evaluator stacks independently
- SDK constraints should be implemented as validation or policy over kernel
  semantics, not as a second semantic system
- external positioning should describe Aleph3 as an embeddable symbolic/formula
  engine until the broader CAS foundation is materially stronger
- broad differentiation beyond the focused calculus pack, integration, solver
  work, or large special-function expansion should wait until the kernel
  boundary is stronger
- bug fixes, regression tests, documentation cleanup, and narrow contract
  hardening are allowed in parallel
- new semantic behavior must declare whether it belongs to kernel, SDK, or pack
- each active tranche should include a kernel/SDK contract task, a math-pack
  task, and a CLI or IDE-foundation task
- CLI and IDE consumers must reuse kernel evaluation, diagnostics,
  registration, and budgets rather than introducing another runtime
- every new concept that becomes user-visible or architecturally important must
  be documented in plain language with small practical examples
- every created or modified header should carry a short descriptive file header
  that explains what the interface owns and why it exists

## Priority Order

The coordination priorities are:

1. keep docs and repo messaging aligned with the one-kernel direction
2. finish collapsing SDK execution into the kernel
3. define the symbol and extension model the kernel will grow through
4. establish the CLI and shared session layer as permanent kernel consumers
5. add general rewrite infrastructure
6. strengthen the algebra pack on exact foundations and expose its workflows
   through SDK and CLI surfaces
7. add assumptions and domain-aware semantics needed by packs
8. decide the durable registration and extension lifetime model for embedding
   and interactive sessions
9. make the syntax strategy explicit and keep it separate from kernel
   semantics
10. implement a focused calculus pack as the next extension-model proof
11. harden SDK, CLI, scripts, and structured tooling output as supported
    product surfaces
12. add an initial IDE or workbench consumer over the shared session protocol
13. deliver a focused digital signal processing pack through SDK, CLI, and
    session surfaces

## Milestones

Completed foundation:

- canonical plan and one-kernel direction are in place
- kernel boundary and representation choice are explicit
- shared execution contracts exist for evaluation context, diagnostics, and
  registration
- SDK evaluation is kernel-backed and no longer acts as a peer runtime
- the SDK is substantially complete as the first external adoption layer

Active milestone:

### M5. Kernel Extensibility And Interactive CLI Foundations

Outcome:

- symbol definition model exists
- registration and metadata replace more evaluator branching
- rewrite infrastructure begins to support symbolic growth
- the CLI is established as a permanent product surface
- a stateful session boundary begins to unify definitions, evaluation,
  diagnostics, and inspection

### M6. Serious Algebra Workflows Through SDK And CLI

Outcome:

- exact algebra backbone is in place
- algebra is a serious registered pack rather than packaging alone
- supported algebra workflows are reachable through both SDK and CLI paths
- exactness, unsupported boundaries, and pack ownership are tested end to end

### M7. Calculus Pack And Reusable IDE Session Protocol

Outcome:

- a focused calculus transformation pack uses shared rewrite, exactness, and
  assumption contracts
- the interactive session surface provides structured evaluation, diagnostics,
  completion, inspection, and pack discovery
- CLI and future IDE consumers do not need private execution logic

### M8. Polished CLI And Initial IDE Or Workbench

Outcome:

- the CLI supports interactive sessions, scripts, machine-readable results,
  tracing, timing, and pack operations as stable workflows
- an initial IDE, editor integration, or workbench consumes the shared session
  protocol
- the CLI remains supported alongside the richer interactive surface

### M9. Focused Digital Signal Processing Pack

Outcome:

- `aleph3_pack_dsp` is a registered domain pack over the same kernel
- the first supported workflows cover finite discrete sequences, exact or
  mixed-exact convolution, and FIR filtering
- a second stage adds rational transfer-function and z-domain workflows only
  after their algebra and assumptions requirements are explicit
- supported DSP operations are reachable through SDK, CLI, and session
  consumers with structured unsupported-case diagnostics
- FFT acceleration, streaming audio, file codecs, real-time scheduling,
  hardware integration, and image processing remain outside the first pack

## Workstreams

### A. Documentation And Direction Freeze

Goals:

- keep the repo explicit about the architecture direction
- keep focused contract docs separate from the roadmap
- avoid stale transitional wording

Tasks:

- keep `README.md` and architecture docs aligned with the one-kernel message
- point all roadmap references to this document
- keep supported-subset and unsupported-area docs explicit
- keep terminology practical and explain new concepts with short examples rather
  than internal jargon
- keep syntax-facing docs honest about what is Aleph3-native versus what is
  currently Wolfram-like compatibility syntax

Success criteria:

- contributors can identify the canonical plan without inference
- readers can tell which docs are contracts and which doc is the roadmap

### B. Kernel Boundary And Build Graph

Goals:

- make the build graph and ownership model reflect the intended architecture

Remaining tasks:

- keep target ownership clear as code moves
- continue separating kernel-owned, SDK-owned, and pack-owned code
- align test grouping with those boundaries

Success criteria:

- the build graph no longer suggests two long-term semantic cores

### C. Shared Kernel Contracts

Goals:

- define the kernel contracts that both symbolic and SDK-facing execution use

Success criteria:

- embedded and symbolic evaluation can be expressed through one kernel context
  model

### D. SDK V1 Adoption Track

Goals:

- make the trusted SDK subset the first polished commercial and adoption-ready
  embedding surface without crowding out pack and interactive investment

Tasks:

- keep the stable SDK subset small, explicit, and well-documented
- improve host-integration examples and fast-start embedding documentation
- keep public runtime exactness boundaries honest and explicit
- defer broader CAS positioning until the kernel can support it credibly

Success criteria:

- Aleph3 can be presented confidently as an embeddable formula/symbolic engine
- SDK adoption does not depend on unfinished broader CAS claims
- SDK contracts are exercised by pack-backed capabilities where those
  capabilities belong in the trusted subset

### D2. Syntax And Language Surface

Goals:

- separate kernel semantics from frontend syntax choices
- avoid positioning Aleph3 as only a clone of another system's language

Tasks:

- document the current Wolfram-like syntax as the current input surface, not as
  the whole product identity
- decide whether Aleph3 should support dual frontends, with compatibility
  syntax and a future Aleph3-native syntax
- keep parser, printer, and docs structured so syntax can evolve without
  changing kernel semantics
- add practical examples when syntax behavior is documented or changed

Success criteria:

- syntax decisions are explicit rather than accidental
- Aleph3 can differentiate product identity from compatibility syntax

### E. Symbol Definition And Extension Model

Goals:

- move from narrow evaluator dispatch toward a symbol-centric kernel

Current status:

- initial registration metadata is implemented
- initial symbol metadata and definition records are implemented
- evaluator dispatch now syncs and consults symbol-definition and registration
  contracts for registered symbolic handlers, builtin evaluator functions, user
  functions, and host functions
- builtin and host ownership are now explicit in kernel definition records
- evaluator dispatch now derives a primary owner from shared symbol and
  registration facts, and builtin execution is now registry-backed
- `If`, `And`, and `Or` now execute through an explicit registry-backed
  special-form category with shared definition ownership
- runtime evaluation now reads registrations from an explicit function catalog
  carried by the current engine or evaluation context instead of consulting
  singleton global state
- the first serious pack-backed symbolic surface now registers polynomial
  algebra handlers through that same registry path

Remaining tasks:

- move the remaining host and richer builtin execution semantics behind
  registry- or definition-backed contracts
- decide how much attribute metadata should control dispatch versus remain
  descriptive once the first evaluation-control slice settles
- define a staged richer definition model beyond the current narrow
  own-value and user-function contracts:
  - make lookup categories explicit
  - define which definition kinds are symbol-local versus registry-backed
  - prove at least one new symbolic behavior can be expressed through shared
    definition state rather than evaluator-local branching
- stage the first real evaluation-control attribute tranche explicitly:
  - keep current listability and numeric-function metadata
  - `HoldFirst`, `HoldRest`, and `HoldAll` are now durable kernel contracts
    for the current builtin-owned held heads
  - keep broader attribute families out until precedence and evaluation hooks
    are tested clearly
- document the remaining mutation and thread-safety rules for registry-backed
  embedding
- decide how future pack loading and unload boundaries should work on top of
  the engine-scoped catalog

Success criteria:

- new symbolic behavior can increasingly be added through definitions and
  registration rather than evaluator branching
- the extension model is credible for long-lived embedding and future packs
- precedence between symbol-owned definitions, registered handlers, builtin
  execution, rewrite-owned simplification, and host functions is explicit
  enough to support broader symbolic growth safely

### F. Pattern Matching And Rewrite Engine

Goals:

- add the general symbolic transformation machinery higher-level features need

Current status:

- exact structural rule rewriting is implemented
- the first named-binder pattern language is implemented
- bounded repeated rewrite entrypoints exist
- the first evaluator-facing rewrite-owned simplification slice now covers
  fixed-arity arithmetic identity rewrites
- that dedicated n-ary arithmetic rewrite contract is now implemented for
  scalar `Plus` and `Times` neutral elimination, scalar `Times` annihilator
  handling, and scalar numeric/rational bucket folding
- a first symbolic coefficient layer now exists for normalized `Plus` over
  single-symbol and single-power bases with numeric and exact-rational
  coefficients
- a first algebra-aware layer now exists for same-symbol exponent accumulation
  in normalized `Times` and nested numeric `Power` forms
- typed single-expression patterns and bounded `Condition[pattern, predicate]`
  rules are implemented; targeted depth-controlled traversal is implemented,
  while sequence patterns remain open

Near-term tasks:

- keep the symbolic coefficient contract limited to single-symbol and
  single-power bases until stronger exact algebra exists
- keep the algebra-aware layer limited to same-symbol exponent accumulation and
  numeric nested-power collapse until stronger exact algebra exists
- keep division cancellation,
  list-aware arithmetic, and special-function shortcuts evaluator-owned until
  stronger kernel contracts exist
- the registered normalized-head rewrite ownership slice is now completed:
  - registered normalized-head rewrites are explicit parts of the shared
    symbol and extension model, while remaining a simplification-stage
    contract rather than ordinary function-call dispatch
- define the next matcher and rewrite ladder explicitly rather than leaving it
  as one broad future bucket:
  1. richer single-expression pattern classes
  2. targeted traversal and replacement APIs
  3. only later, if still justified, broader sequence-pattern machinery

Success criteria:

- symbolic transforms no longer depend only on hardcoded evaluator cases
- at least one nontrivial symbolic workflow beyond arithmetic cleanup can be
  expressed through rewrite contracts rather than evaluator-local branching

Detailed guidance for the next rewrite migration slice:

- do not wait for general sequence patterns before moving broader arithmetic
  cleanup out of evaluator-local code
- use a dedicated n-ary contract for normalized `Plus` and `Times`
- treat that contract as head-aware variadic reduction, not as a generic
  matcher feature
- migrate only reductions that preserve current scalar arithmetic contracts
  without depending on list semantics or stronger algebra metadata
- move like-term collection into a separate symbolic coefficient contract
  instead of broadening arithmetic rewrite
- keep exponent merging in the separate algebra-aware layer rather than
  broadening arithmetic rewrite
- keep rewrite families distinct:
  - structural replacement rules
  - simplification-stage normalized-head rewrites
  - future symbol-bound transformation rules
- require each new matcher feature to declare:
  - how it is bounded
  - whether it participates in normal evaluation, explicit replacement, or
    simplification only
  - what unsupported pattern forms must still preserve or reject explicitly

### G. Exact Algebra Backbone

Goals:

- replace floating-point-centered symbolic algebra internals with exact
  foundations

Tasks:

- introduce exact coefficient-ring abstractions
- support exact rational polynomial coefficients
- harden multivariate polynomial algorithms
- define an explicit staged path for multivariate algebra growth:
  - keep today’s current multivariate support limited to safe expand,
    collect, and narrow factor-content workflows
  - move multivariate GCD, division, and factorization growth only after exact
    coefficient and monomial-ordering contracts are explicit
  - add exact multivariate term-order, content, and normalization invariants
    before claiming broader algebra support
- make overflow and large-integer strategy explicit
- remove growth-facing dependence on `double` polynomial internals
- make downstream consumers explicit:
  - rewrite-owned symbolic transforms
  - future calculus output cleanup
  - pack-owned algebra and special-function rules that need exact coefficient
    preservation

Near-term multivariate algebra plan:

- do not market current polynomial algebra as broad multivariate CAS support
- treat multivariate exact arithmetic as the next real algebra-foundation
  expansion after the first pack extraction
- active implementation outcome:
  - exact multivariate coefficient preservation is now being made explicit for
    `Expand` and `Collect` through a public exact polynomial layer, while
    multivariate `GCD`, division, and broader factorization remain out of
    scope
- supported univariate rational-root factorization now clears exact rational
  denominators and restores exact scalar content without using inexact roots
- exact monomials now have explicit lexicographic, graded-lexicographic, and
  graded-reverse-lexicographic ordering policies with variable precedence;
  canonical algebra rendering initially uses graded lexicographic order
- exact single-divisor multivariate division now uses explicit selector order
  as precedence under fixed graded lexicographic order; multivariate GCD and
  broader factorization remain unsupported
- target early exact multivariate milestones in this order:
  1. exact multivariate coefficient preservation
  2. explicit monomial ordering and canonical form rules
  3. supported multivariate division contracts
  4. supported multivariate GCD contracts
  5. only then broader multivariate factorization growth

Success criteria:

- algebra growth no longer rests on the current narrow floating polynomial core
- no serious higher symbolic pack depends on the floating polynomial path for
  its core correctness story

### H. Assumptions And Domain Semantics

Goals:

- make simplification and transformation context-aware where justified

Current status:

- assumption storage now exists in kernel evaluation context
- the first temporary-assumption surface now exists through `Assuming` and
  `Refine`
- the first richer domain category now exists through narrow
  integer/rational/real symbol facts
- the current supported assumption subset covers:
  - direct boolean symbol facts such as `flag`
  - direct boolean negation as `Not[flag]`
  - direct comparison facts
  - direct sign predicates such as `Positive[x]` and `NonZeroQ[x]`
  - direct symbol-domain predicates such as `IntegerQ[n]` and `RealQ[x]`
  - sign facts around zero used by comparisons, `Abs`, and `Sqrt[x^2]`
  - simple derived-sign reasoning for `-x`, exact numeric scaling, and exact
    integer powers such as `x^2`
  - exact numeric-domain answers for the current integer/rational/real
    predicate family

Tasks:

- make selected transforms assumption-aware
- define a pack-facing domain query surface over the same kernel contract
- stage richer domain facts in an explicit order:
  1. stronger exact sign and zero facts for simple forms
  2. explicit contradiction handling
  3. selected assumption-aware rewrite hooks
  4. only later, broader domain categories beyond integer/rational/real
- keep unsupported cases explicit

Success criteria:

- targeted domain-sensitive rewrites can happen without ad hoc unsafe behavior
- at least one rewrite-owned or pack-owned transform uses shared assumptions
  queries instead of evaluator-local domain checks

### I. Math Packs And Product Surface Growth

Goals:

- move higher math out of the kernel core while keeping semantics unified
- treat math-pack depth as an active product investment, not only an
  architecture demonstration

Tasks:

- split elementary functions, algebra, calculus, and special functions into
  clearer pack-style modules over time
- define pack registration interfaces
- keep kernel contracts small and stable while allowing math-surface growth
- choose one serious early pack that proves the extension model with meaningful
  workflows rather than only placeholder packaging
- use the algebra pack as the proving ground for staged multivariate algebra
  growth once exact algebra contracts are stronger
- expose supported algebra workflows coherently through SDK and CLI entrypoints
- keep vertical domains such as electrical engineering out of the kernel and
  implement them only once kernel extension points are stable
- tighten what counts as a serious pack proof:
  - it must register behavior through kernel contracts only
  - it must rely on shared rewrite, exact, or assumptions contracts where
    mathematically required
  - it must carry explicit unsupported boundaries and ownership tests
  - packaging alone does not count as proof of extensibility
- likely serious-pack progression:
  - algebra remains the current proving ground for exactness and ownership
  - a focused calculus transformation pack is the next proof and must use
    shared rewrite, exactness, and assumptions contracts
  - a focused digital signal processing pack follows calculus and proves that
    an applied domain can compose exact algebra, complex values, assumptions,
    registration, and interactive session contracts
- stage the DSP pack deliberately:
  1. define finite discrete-sequence representation and indexing conventions
     without adding a second expression model
  2. implement deterministic finite convolution and FIR filtering
  3. add rational transfer-function and z-domain simplification over shared
     algebra contracts
  4. add frequency-response workflows over shared complex-number semantics
  5. consider optimized FFT and streaming APIs only after correctness,
     budgets, numeric policy, and performance baselines are explicit
- keep DSP presentation and I/O outside the pack core: plotting, audio files,
  devices, and editor widgets belong to product adapters

Success criteria:

- higher math growth follows pack boundaries instead of evaluator sprawl
- at least one serious pack proves that meaningful domain behavior can be added
  without modifying evaluator internals
- that proof pack exercises more than packaging: it demonstrates rule
  registration, exact/domain usage where needed, and explicit unsupported-case
  behavior
- algebra workflows are usable and documented through SDK and CLI surfaces
- the calculus pack proves a second domain can extend the kernel without
  evaluator-local branches
- the DSP pack proves an applied domain can compose multiple kernel and pack
  contracts while retaining explicit exact/inexact and unsupported boundaries

### J. Interactive Applications And Product Surfaces

Goals:

- keep the CLI as a permanent first-class interface
- provide richer end-user interaction without fragmenting semantics
- create one reusable session foundation for CLI and future IDE/workbench
  consumers

Tasks:

- define a stateful interactive session service above the kernel and SDK
- preserve definitions and selected session configuration across evaluations
- provide structured requests and results for evaluation, diagnostics,
  completion, expression inspection, and pack discovery
- evolve the CLI around that service with interactive sessions, script
  execution, machine-readable output, tracing, timing, and pack operations
- keep presentation concerns such as terminal formatting, cell rendering, and
  editor UI outside the kernel
- make a future IDE, editor extension, notebook, or workbench another consumer
  of the same session service
- ensure every interactive consumer reuses kernel evaluation, diagnostics,
  registration, and budgets
- defer commitment to a GUI toolkit, editor host, or transport encoding until
  an initial protocol consumer requires that choice

Current status:

- the first experimental session owns one kernel evaluation context
- evaluation, simplification, and full-form requests return structured results
  and diagnostics
- inspection requests return head, full form, symbols, node count, and depth
  without evaluating or mutating the expression
- pack-discovery requests return deterministic package and symbol records from
  the shared function registry
- completion requests merge registry metadata with session-defined symbols and
  functions, and the CLI exposes them through Tab and `:complete`
- the symbolic CLI REPL reuses one session, preserving assignments and user
  definitions across inputs
- the CLI consumes those operations through `:inspect` and `:packs`
- one-shot CLI commands remain intentionally ephemeral

Priority:

- CLI and session foundations proceed now in narrow slices over stable kernel
  contracts
- a large standalone GUI waits until the reusable session contract has a real
  CLI consumer

Success criteria:

- the CLI remains useful and supported even after richer tools exist
- CLI and IDE/workbench consumers share session semantics and structured
  results
- no interactive surface creates a third semantic center

### K. Tests, Validation, And Product Hardening

Goals:

- move from feature-example testing toward product-contract testing

Current coverage audit:

- the repository has broad example and regression coverage across parser,
  evaluator, SDK, algebra, pack ownership, session, and CLI layers
- the Release suite currently executes 485 Catch2 test cases, but CI exposes
  them through two coarse test executables and does not publish line or branch
  coverage
- algebra and evaluator behavior have the deepest coverage; the new session,
  typed-pattern, pack-lifecycle, and process-level CLI contracts are thinner
- this audit is a contract-coverage review, not a measured source-coverage
  claim; measured coverage must be added before percentage targets are set

Tasks:

- group tests by kernel, SDK, transitional compatibility, and packs
- add invariant-oriented regression coverage
- strengthen UDF, cross-subsystem, diagnostics, and unsupported-case coverage
- keep supported-subset behavior documented and testable
- add proof-oriented symbolic workflow coverage as core contracts strengthen:
  - rewrite-driven symbolic transformation beyond arithmetic cleanup
  - assumptions-aware transformation
  - exact algebra cleanup after symbolic transforms
  - serious-pack ownership and no-evaluator-branching assertions
- expand polynomial contract coverage around the current supported surface:
  - round-trip and invariant tests for `Expand`, `Collect`, `Factor`, `GCD`,
    and `PolynomialQuotient` on supported inputs
  - canonical-form assertions for term ordering, coefficient normalization,
    sign handling, and zero/one behavior
  - exact-rational boundary tests that prove both supported behavior and
    explicit rejection paths
  - explicit multivariate boundary tests that separate supported expansion and
    collection from unsupported broader algebra inference
  - pack-ownership tests proving polynomial algebra remains registry-backed and
    pack-owned rather than drifting back into evaluator-local branching
- expand CI beyond the current basic matrix with Debug, Clang, sanitizers,
  formatting, and lightweight performance checks over time
- add contract tests for session persistence, structured diagnostics,
  machine-readable CLI output, inspection, completion, and pack discovery as
  those interfaces land
- close the following P0 contract gaps before widening the affected surfaces:
  - session diagnostic codes for parse, evaluation, unsupported, and budget
    failures; state preservation after failure; per-request budget reset;
    isolation of definitions and registry state between sessions
  - every advertised typed pattern in anonymous, named, nested, repeated-name,
    `MatchQ`, `Replace`, and `ReplaceRepeated` forms; malformed constraints;
    sequence rejection; and the documented policy for ordinary identifiers
    containing underscores
  - exact rational `Factor` invariants for zero, constants, rational content,
    repeated roots, irreducible remainder, canonical sign/order, expand-factor
    round trips, multivariate rejection, and checked overflow at denominator
    and coefficient boundaries
  - CLI one-shot ephemerality, REPL recovery after errors, nonzero failure exit
    codes, diagnostic output, EOF handling, mode-switch state behavior, and
    Windows/Linux argument and pipe behavior
  - registration collision, duplicate registration, independent registry,
    ownership metadata, and lifetime behavior before pack unload or dynamic
    loading is claimed
- add a P1 cross-surface conformance matrix that runs the same supported
  expressions through direct kernel, session, CLI, and SDK surfaces where the
  SDK subset permits them, comparing values, canonical rendering, diagnostics,
  and budget outcomes
- add P1 property and round-trip tests for parser/printer stability, expression
  normalization idempotence, algebra identities, and deterministic pack
  registration order; use fixed seeds and retain minimized regressions
- add DSP pack contract coverage with the pack itself:
  - indexing, empty/singleton signals, unequal lengths, exact and mixed-exact
    convolution, FIR state conventions, canonical output, and invalid forms
  - linearity, commutativity and associativity where mathematically applicable,
    impulse identity, and direct-versus-optimized implementation equivalence
  - transfer-function normalization, poles/zeros boundary behavior, complex
    frequency response, pack ownership, and cross-surface workflows
- evolve CI in an explicit order: split/label suites for useful failures, add
  Debug and Clang, add ASan/UBSan on Linux, publish measured coverage, then add
  parser/session fuzzing and lightweight performance regression thresholds

Success criteria:

- the supported symbolic subset behaves like a documented product contract
- engineering quality is credible for a C++ math runtime, not just for a
  prototype
- every supported pack has ownership, invariant, unsupported-boundary, and
  cross-surface tests before its milestone is considered complete
- coverage reports identify unexecuted production paths, while contract tests
  remain the acceptance authority rather than a percentage alone
- future claims of “serious symbolic capability” are backed by end-to-end
  symbolic workflow tests, not only helper-level unit coverage

### L. Transitional Removal And Cleanup

Goals:

- finish the migration instead of letting the transitional architecture linger

Tasks:

- remove stale plan files and duplicated roadmap text
- remove duplicate runtime paths once kernel execution is live
- clean up docs, tests, and class-collaboration descriptions that imply two
  lasting semantic cores
- remove tracked build artifacts and keep `.gitignore` aligned with actual
  generated directories such as `build-sdk/` and `build-test/`

Success criteria:

- the repository no longer normalizes the transitional dual-core state
- repository hygiene matches a maintained product codebase

## Immediate Action Queue

Three balanced tranches have now delivered typed and conditional patterns,
depth-controlled replacement, exact rational factorization, explicit
multivariate ordering and division, and a stateful CLI session with inspection,
pack discovery, and completion. The next active tranche should be:

1. **Kernel and SDK:** add explicit assumption-contradiction handling and the
   first selected assumption-aware rewrite hook
2. **Math packs:** specify the first supported exact multivariate GCD contract
   over the shared division and ordering invariants
3. **CLI and IDE foundation:** add script execution and a narrow
   machine-readable result mode over the existing session contract
4. **Cross-cutting validation:** continue closing P0 diagnostic, overflow,
   registry-collision, failure-recovery, and process-level CLI gaps before
   declaring these experimental interfaces stable

Every following tranche should preserve this three-track shape. Tasks may be
small, but no track should disappear from the active program.

## Deferred Work

The following should mostly wait until the kernel program is further along:

- broad differentiation and calculus breadth beyond the focused
  transformation pack
- DSP breadth beyond finite sequences, convolution, FIR filtering, and the
  later focused transfer-function workflow
- optimized FFT, streaming media, codecs, devices, and real-time DSP execution
  before numeric policy and performance contracts exist
- large solver efforts
- major symbolic special-function expansion beyond targeted contract hardening
- significant algebra growth on top of the current floating algebra core
- new domain verticals implemented directly inside evaluator branches
- a large standalone GUI before the shared session contract has a CLI consumer
- notebook-style product work that introduces execution semantics outside the
  kernel

## Allowed Parallel Work

These can proceed without waiting for the whole program:

- bug fixes
- regression tests
- documentation cleanup
- narrow builtin contract hardening
- limited symbolic-domain corrections that reduce future migration risk
- SDK tutorials, host-integration examples, and other adoption-facing polish
- exact algebra-pack slices over stable contracts
- CLI session, scripting, inspection, diagnostics, and structured-output work
  over existing kernel behavior
- IDE protocol design validated through the CLI, without committing to a GUI
  toolkit or editor host
- DSP representation and contract design after algebra/calculus dependencies
  are explicit, without prematurely adding streaming or hardware APIs

## Product Standard

The target is not vague symbolic usefulness.
The target is a production-grade symbolic engine for an explicit supported
subset.

That requires:

- stable semantic contracts
- predictable fallback behavior
- canonical forms where the subset intends them
- explicit unsupported behavior
- coherent diagnostics
- regression coverage based on semantic invariants

## Success Test

This unified plan is succeeding if:

- contributors stop asking which plan is authoritative
- architecture decisions stop being duplicated across roadmap docs
- new code follows the kernel/SDK/pack split consistently
- SDK and symbolic execution visibly converge
- SDK, math packs, and interactive tooling all receive visible incremental
  investment
- algebra and focused calculus capabilities reach users through supported
  surfaces
- focused DSP workflows reach users through a registered pack and the same
  SDK, CLI, and session semantics
- CLI and future IDE/workbench consumers share one session and execution model
- future product growth depends on stronger kernel contracts, not on more
  duplicate evaluator logic
