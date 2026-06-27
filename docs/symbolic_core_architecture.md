# Symbolic Core Architecture

## Purpose

This document defines the intended architecture boundary between:

- the pure symbolic kernel
- math packs layered on top of that kernel
- the SDK and other product surfaces above both

This is the architecture contract Aleph3 should clean toward.

Related documents:

- [Class Collaboration](class_collaboration.md)
- [Aleph3 Unified Plan](aleph3_unified_plan.md)
- [Kernel Evaluation Context](kernel_evaluation_context.md)
- [Kernel Diagnostic Taxonomy](kernel_diagnostic_taxonomy.md)
- [Kernel Registration Interfaces](kernel_registration_interfaces.md)
- [Kernel Symbol Definition Precedence](kernel_symbol_definition_precedence.md)

## Top-Level Layers

Aleph3 should be understood as four layers:

1. Pure symbolic kernel
2. Math packs
3. SDK / host embedding layer
4. Future products and services

The key rule is simple:

- the kernel owns symbolic semantics
- math packs extend symbolic capability using kernel contracts
- the SDK embeds and constrains the engine for host use
- products sit above those layers without redefining core semantics

## Pure Symbolic Kernel

The pure symbolic kernel is the minimal engine required to support symbolic
math coherently.

It should own:

- expression/value representation
- parser and structural printers
- symbol metadata and definition storage
- attribute and evaluation-semantics model
- evaluation loop and symbolic fallback
- exact scalar arithmetic
- normalization and canonicalization
- bounded simplification framework
- pattern matching and rewrite execution
- assumptions/domain context
- package registration interfaces
- core diagnostics and error taxonomy

It should not own:

- most domain-specific algebra algorithms
- special-function libraries as evaluator-side special cases
- calculus, solver, or linear algebra policy
- host-facing policy and validation concerns

## Math Packs

Math packs are symbolic libraries built on top of kernel contracts.

They should own:

- domain-specific built-ins
- algebra algorithms
- special functions
- calculus rules
- solver strategies
- domain-specific simplification rules
- exact-domain metadata that depends on a specific math area

Candidate packs:

- elementary functions pack
- algebra pack
- special functions pack
- calculus pack
- solver pack
- linear algebra pack
- combinatorics/discrete pack

The main rule for packs:

- packs should register capabilities with the kernel
- packs should not bypass the kernel by embedding ad hoc semantics across the
  evaluator core

## SDK Layer

The SDK is not part of the symbolic kernel.

It should own:

- host-facing types and stable APIs
- validation and policy enforcement
- compilation boundaries for trusted-subset execution
- runtime budgets and operational controls
- host function bridging
- embedding ergonomics

It should not redefine symbolic semantics that belong to the kernel.

## Current Repository Mapping

Current code is only partially aligned with the target split.

Closest current kernel-owned areas:

- `include/expr`, `src/expr`
- `include/parser`, `src/frontend`, `include/frontend`
- `include/evaluator`, `src/evaluator`
- `include/normalizer`
- `include/transforms`, `src/transforms`
- `include/semantics`, `src/semantics`

Current blended areas:

- `include/algebra`, `src/algebra`
  These are currently treated as part of the symbolic engine surface, but they
  are really early math-pack code sitting too close to evaluator internals.

Clearly non-kernel areas:

- `include/sdk`, `src/sdk`
- `include/runtime`, `src/runtime`
- `include/ir`
- `include/tooling`, `src/tooling`

## Recommended Ownership By Subsystem

### `expr`

Owns:

- symbolic value model
- structural construction helpers
- structural formatting / full-form support

### `parser` and `frontend`

Own:

- tokenization
- parsing
- syntax diagnostics
- source-to-expression construction

### `symbols`

Target subsystem not yet properly separated.

Should own:

- symbol metadata
- definition tables
- attribute storage
- precedence between built-ins, user definitions, and rewrite-driven behavior

### `evaluator`

Should own:

- evaluation orchestration
- evaluation order
- symbolic fallback
- interaction with definitions, attributes, rewrite engine, and assumptions

Should not become the home for all domain math semantics.

### `normalize`

Owns:

- canonical structural normalization
- deterministic commutative ordering
- structural idempotence contracts

### `rewrite`

Target subsystem not yet properly separated.

Should own:

- pattern matching
- rule application
- traversal-driven replacement
- bounded repeated rewrite semantics

### `exact`

Target subsystem not yet properly separated.

Should own:

- exact scalar arithmetic
- exact coefficient abstractions
- overflow and exactness-boundary utilities

### `assumptions`

Target subsystem not yet properly separated.

Should own:

- assumption context
- domain facts
- assumption queries used by transforms and packs

### `packs`

Target subsystem not yet present as a clear repo boundary.

Should own:

- algebra algorithms
- elementary function families beyond kernel primitives
- special functions
- calculus
- solver libraries

## Clean Split Rules

When deciding whether code belongs in the kernel or in a pack, use these
rules.

A feature belongs in the kernel if:

- many math domains depend on it
- it changes evaluation semantics globally
- it defines expression meaning rather than only computing a domain result
- it is needed for user-extensible symbolic programming

A feature belongs in a pack if:

- it is a domain algorithm over stable symbolic primitives
- it can be registered declaratively through kernel contracts
- it should be swappable, expandable, or versioned independently
- it does not need to redefine the evaluator architecture

## Anti-Patterns To Avoid

Avoid these architecture mistakes:

- putting new symbolic power only into evaluator `if` branches
- using built-in function dispatch as a substitute for a symbol-definition
  system
- tying special functions directly to evaluator internals without a pack
  boundary
- expanding algebra features on top of floating polynomial internals when the
  long-term target is exact symbolic algebra
- letting SDK/runtime concerns leak backward into symbolic semantics

## Recommended Near-Term Cleanup

The current architecture cleanup should focus on documentation and clearer
ownership first, then code movement.

Near-term cleanup targets:

- describe the pure kernel explicitly in product docs
- describe algebra as an early pack-like subsystem instead of permanent kernel
  evaluator logic
- identify missing target subsystems: `symbols`, `rewrite`, `exact`,
  `assumptions`, `packs`
- keep evaluator code framed as orchestration rather than semantic landfill

## Recommended Future Code Organization

A plausible long-term direction is:

- `include/kernel/expr`
- `include/kernel/parser`
- `include/kernel/symbols`
- `include/kernel/eval`
- `include/kernel/normalize`
- `include/kernel/rewrite`
- `include/kernel/exact`
- `include/kernel/assumptions`
- `include/packs/algebra`
- `include/packs/elementary`
- `include/packs/special_functions`
- `include/packs/calculus`

This does not need to happen immediately as a directory rename.

The important thing is to align ownership and interfaces first, then move code
once boundaries are stable.

## Code Migration

Migration sequencing and implementation priority now live in the
[Aleph3 Unified Plan](aleph3_unified_plan.md).

This document stays focused on architecture boundaries and ownership rather
than carrying its own separate roadmap.
- large rewrite-heavy features without a rewrite subsystem
- deeper algebra expansion on top of the current floating polynomial core

### Success Condition

The migration is successful when:

- new symbolic power lands in kernel subsystems or packs instead of evaluator
  branches
- algebra and special functions no longer read as evaluator-owned semantics
- exact arithmetic and symbol-definition responsibilities have clear homes
- directory layout and interfaces finally tell the same architecture story
