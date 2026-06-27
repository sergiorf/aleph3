# Kernel Rewrite Spec

## Status

Draft implementation spec referenced by the
[Aleph3 Unified Plan](aleph3_unified_plan.md).

## Purpose

This document will define the rewrite subsystem required for general symbolic
transformation.

It should specify:

- pattern model
- matcher scope
- traversal semantics
- rewrite application rules
- bounded repeated-rewrite behavior

## Scope

This spec should cover:

- pattern representation
- match environment representation
- rule representation
- traversal APIs
- rewrite budgets and termination rules
- interaction with evaluator-driven reductions

## Required Sections

1. initial pattern language scope
2. match result representation
3. traversal order semantics
4. single-pass rewrite semantics
5. repeated rewrite semantics and budgets
6. conditional rule support
7. interaction with assumptions and symbol definitions
8. testing invariants

## Initial Design Questions

- what the minimal first pattern language should be
- whether rewrites run before, after, or interleaved with builtin evaluation
- how to avoid non-termination while staying useful
- which current simplifications should migrate into rewrite-driven behavior

## Acceptance Criteria

This spec is sufficient when:

- traversal semantics are explicit
- rule application order is defined
- bounded rewrite behavior is testable
