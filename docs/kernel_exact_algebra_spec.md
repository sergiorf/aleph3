# Kernel Exact Algebra Spec

## Status

Draft implementation spec referenced by the
[Aleph3 Unified Plan](aleph3_unified_plan.md).

## Purpose

This document will define the exact arithmetic and algebra-facing foundations
required for stronger symbolic math.

It should specify:

- exact scalar abstractions
- polynomial coefficient strategy
- ownership boundary between kernel exact math and algebra packs
- migration away from floating-point-centered algebra internals

## Scope

This spec should cover:

- integer and rational foundation choices
- exact complex support expectations
- coefficient-ring abstractions
- algebra helper interfaces needed by the kernel and packs
- overflow and large-number strategy

## Required Sections

1. exact scalar model
2. coefficient abstractions
3. polynomial/algebra ownership boundary
4. interaction with evaluator simplification
5. migration strategy from current algebra internals
6. testing invariants

## Initial Design Questions

- whether big-integer support should be introduced immediately or staged
- which exact abstractions belong in the kernel versus algebra pack layer
- how exact coefficients interact with current polynomial APIs

## Acceptance Criteria

This spec is sufficient when:

- exact numeric ownership is explicit
- algebra growth no longer depends on unclear floating-point foundations
