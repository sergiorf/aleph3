# Symbolic Evaluator Cleanup Plan

## Purpose

This plan tracks only the remaining evaluator-semantic cleanup work for the
symbolic core.

Completed phase history is intentionally removed here. Stable contracts that
are already delivered should live in focused product docs, not in a lingering
execution plan.

## Current Status

Completed and now treated as product contract:

- canonical forms and normalization
- evaluation-control semantics
- simplification and rewrite boundaries
- algebra supported-subset hardening

Current algebra contract:

- [Algebra Supported Subset](algebra_supported_subset.md)

Current strategic gap analysis:

- [Symbolic Core Gap Analysis](symbolic_core_gap_analysis.md)

Current architecture contract:

- [Symbolic Core Architecture](symbolic_core_architecture.md)
- [Kernel Refactor Urgent Plan](kernel_refactor_urgent_plan.md)

## Active Phase

### Phase 11. Documentation Pass

Goal:

- simplify engine-facing docs
- remove outdated transitional wording
- keep the symbolic-core-first architecture explicit

Work:

- align the README and symbolic planning docs around the current architecture:
  symbolic engine core, SDK on top, future products above that
- point readers at focused contract docs instead of keeping stale completed
  phase writeups in planning documents
- document evaluator, simplification, and algebra subsystem boundaries in the
  docs that describe current behavior
- document the kernel-versus-pack split explicitly and keep it separate from
  SDK/runtime concerns
- document the supported subset and explicitly unsupported areas
- keep UDF coverage expectations visible in the production-readiness story
- keep the VM track documented as a later architecture track rather than part
  of current semantic stabilization

Success condition:

- docs describe the current architecture and semantic boundaries clearly
- readers can tell which symbolic behaviors are product contract today
- remaining roadmap items are future-facing rather than mixed with completed
  stabilization work

## Future Track: Aleph3 VM

This remains a future architecture track, not a blocking symbolic-semantics
phase.

Start the VM track only after the supported symbolic subset is stable enough
that lowering and runtime work will not churn with semantic changes.

## Additional Implementation Steps

1. Gamma function support toward Mathematica-level behavior
   - numeric support for positive and negative non-integer reals
   - numeric support for complex inputs
   - explicit pole behavior at non-positive integers
   - regression coverage for half-integers, recurrence, and conjugation
   - remaining:
     - clearer analytic-domain and branch-behavior contract for symbolic cases

2. Complex transcendental and special-function expansion
   - extend unary transcendental support beyond basic arithmetic over complex
     values
   - define complex-domain contracts for powers, logarithms, roots, and special
     functions
   - add symbolic fallback and branch-behavior tests for the supported subset
   - include follow-on Gamma-family and complex special-function interactions

## Future Phases After Documentation

These phases are not active yet, but they are now the right roadmap if Aleph3
intends to move toward a stronger Mathematica-class symbolic core.

### Phase 12. Kernel Boundary And Symbol Definition Model

Goal:

- separate the pure symbolic kernel from higher math packs
- move toward a symbol-centric definition and attribute model

Priority:

- highest after Phase 11
- should begin before broad new algebra, calculus, rewrite, or special-function
  expansion

Work:

- define kernel-owned components versus pack-owned components
- introduce a symbol-definition storage model beyond narrow UDF call
  definitions
- define precedence between built-ins, user definitions, and future rewrite
  rules
- make registration and metadata the main extensibility path

### Phase 13. Pattern Matching And Rewrite Engine

Goal:

- add general symbolic transformation machinery

Priority:

- second after Phase 12
- should start before major rewrite-heavy symbolic features are added

Work:

- introduce expression pattern matching
- add replace/rewrite traversal APIs
- support conditional rewrite rules
- define bounded repeated-rewrite semantics and testing strategy

### Phase 14. Assumptions And Domain Semantics

Goal:

- make simplification and transformation context-aware

Priority:

- third
- can begin once symbol-definition and rewrite groundwork exists

Work:

- define assumptions storage in evaluation context
- define core domain categories and propagation rules
- make selected transforms assumption-aware
- keep unsupported assumption cases explicit

### Phase 15. Exact Algebra Backbone

Goal:

- replace narrow floating polynomial internals with exact algebra foundations

Priority:

- fourth
- should begin before substantial differentiation or solver growth over the
  current algebra layer

Work:

- introduce exact coefficient-ring abstractions
- support exact rational polynomial coefficients
- harden multivariate polynomial algorithms
- make overflow and large-integer strategy explicit

### Phase 16. Math Packs And High-Level Domains

Goal:

- layer higher math on top of the kernel rather than embedding it into the
  evaluator core

Priority:

- fifth, but early pack boundaries should start during Phase 12 and Phase 15
- algebra should be the first explicit early-pack migration target

Work:

- split elementary functions, algebra, calculus, and special functions into
  clearer pack-style modules
- define pack registration interfaces
- keep kernel contracts small and stable while allowing math-surface growth

## Migration Recommendation

Yes: this architecture migration should be treated as the next major program
after documentation, and it should start before broad feature expansion in the
affected areas.

Recommended order:

1. Phase 11. Documentation pass
2. Phase 12. Kernel boundary and symbol definition model
3. Phase 13. Pattern matching and rewrite engine
4. Phase 15. Exact algebra backbone
5. Phase 14. Assumptions and domain semantics
6. Phase 16. Math packs and high-level domains

Rationale:

- Phase 12 must come first because ownership and registration boundaries are
  prerequisites for the rest
- Phase 13 follows because many Mathematica-class symbolic features depend on
  rewrite machinery more than on more evaluator branches
- Phase 15 must happen before major algebra, differentiation, or solver growth
  hardens the wrong numeric foundation
- Phase 14 is important, but its payoff is much stronger once rewrite and
  symbol infrastructure exist
- Phase 16 formalizes the pack split after the kernel contracts are strong
  enough to support it cleanly

Allowed in parallel:

- bug fixes
- regression tests
- documentation cleanup
- narrow builtin contract hardening

Should mostly wait until this program begins:

- broad differentiation work
- large solver efforts
- significant special-function expansion beyond targeted contract hardening
- major algebra growth on top of the current floating polynomial core
