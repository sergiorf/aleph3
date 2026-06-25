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
