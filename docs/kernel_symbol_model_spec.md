# Kernel Symbol Model Spec

## Status

Draft implementation spec referenced by the
[Aleph3 Unified Plan](aleph3_unified_plan.md).

## Purpose

This document will define the kernel symbol model in enough detail to drive
implementation.

It should specify:

- definition kinds
- attributes
- precedence
- lookup rules
- mutation rules
- interaction with built-ins, user definitions, rewrites, and packs

## Scope

This spec should cover:

- symbol identity and metadata
- symbol values and callable definitions
- attribute model
- precedence and resolution rules
- definition storage APIs
- evaluation-facing lookup contracts

## Required Sections

1. symbol identity and naming model
2. definition kinds and data structures
3. attribute set and meaning
4. precedence between built-ins, user definitions, pack definitions, and
   rewrites
5. lookup algorithm
6. mutation/update rules
7. cycle and recursion handling
8. testing invariants

## Initial Design Questions

- which definition categories should exist first
- whether to model ownvalue/downvalue-style behavior directly or through a
  narrower intermediate design
- how user-defined functions should migrate from current evaluator structures
- how pack registration participates in lookup without bypassing kernel rules

## Acceptance Criteria

This spec is sufficient when:

- symbol resolution order is explicit
- mutation and lookup semantics are unambiguous
- evaluator and rewrite subsystems can depend on the same symbol contract
