# Kernel Registration Lifecycle Spec

## Status

Implemented contract referenced by the
[Aleph3 Unified Plan](aleph3_unified_plan.md).

## Purpose

This document defines the current lifecycle and thread-safety contract for
Aleph3's registry-backed symbolic extension model.

It covers:

- default registry bootstrap versus per-engine registry copies
- pack registration timing and ownership
- SDK host-function registration visibility
- compile-time versus evaluate-time registration behavior
- current mutation limits
- supported thread-safety boundaries

## Current Model

Aleph3 currently has two distinct registration surfaces:

1. kernel function registries
2. engine-scoped host-function registration

Plain-language summary:

- the default kernel registry is bootstrapped once from builtin symbolic
  handlers, builtin evaluator handlers, builtin rewrites, and the current
  built-in packs such as `core-algebra`
- each SDK `Engine` creates its own function registry from that default
  bootstrap set
- host functions are not stored in the kernel function registry today
- host functions are stored per engine and are the only public mutable
  registration surface

## Registry Bootstrap Contract

### Default Registry

`kernel::create_default_function_registry()` builds a fresh registry that
contains:

- builtin symbolic handlers
- builtin evaluator handlers
- builtin head rewrites
- builtin pack registrations performed during bootstrap

`kernel::default_function_registry()` exposes one process-local default
registry instance for contexts that do not need engine-specific isolation.

Current implication:

- builtin and pack-backed symbolic registrations are fixed when a registry is
  created
- later SDK host-function registration does not mutate that kernel registry

### Engine Registry Copies

Each `Engine` owns its own `kernel::FunctionRegistry`, initialized from
`create_default_function_registry()`.

Current implication:

- engines do not share mutable symbolic registration state
- one engine can evolve its host-function set without mutating another engine's
  registry-backed symbolic surface

## Pack Registration Contract

Pack registration is currently bootstrap behavior, not a runtime lifecycle.

Current contract:

- packs such as `core-algebra` register into a registry while that registry is
  being constructed
- pack-backed symbolic handlers then behave like fixed registry contents for
  that registry instance
- no public API exists for runtime pack load, unload, or replacement

Practical implication:

- this slice documents pack ownership and lifecycle honestly without claiming a
  plugin-style runtime pack system that does not exist yet

## Host Function Registration Contract

`Engine::register_function(...)` is the only public mutable registration API
today.

Current behavior:

- registration mutates the target engine's host-function map only
- registration does not mutate process-global kernel state
- later `Engine::evaluate(...)` calls observe host functions registered on that
  same engine after `register_function(...)` returns
- other engines do not observe those host-function registrations

Current non-features:

- no unregister API
- no host-function registration through `CompiledFormula`
- no public mutation API for builtin symbolic handlers, pack symbolic handlers,
  or rewrites

## Compile-Time Versus Evaluate-Time Visibility

Trusted-subset compilation stages formulas into kernel expressions, but it does
not snapshot engine host registrations into `CompiledFormula`.

Current contract:

- `CompiledFormula` stores lowered kernel expression state plus compile-time
  policy and schema constant state
- `Engine::evaluate(...)` reads the engine's current host-function map at
  evaluation time
- a formula compiled before host registration can evaluate successfully after
  that host function is later registered on the same engine
- the same compiled formula can evaluate differently across different engines
  because host registration remains engine-scoped

This is intentional in the current design because host registration is runtime
engine state, not compile-time formula state.

## Thread-Safety Contract

The current supported thread-safety boundary is narrow and explicit.

### Supported Now

- separate `Engine` instances can register different host-function sets without
  sharing registration state
- `CompiledFormula` is read-only after successful compilation
- multiple threads may call `Engine::evaluate(...)` concurrently on the same
  engine and the same compiled formula
- concurrent evaluation does not require or imply global mutable registry state

### Current Limits

- `Engine::register_function(...)` is synchronized for mutation of that
  engine's host-function map, but this slice does not promise lock-free or
  wait-free behavior
- evaluations already in progress are not required to observe a concurrent
  registration change
- this slice does not define a broader concurrent mutation model for symbolic
  registry contents, pack loading, or unload

Practical implication:

- registration changes become visible to later evaluations after registration
  completes
- in-flight evaluations may continue using the host-function snapshot they
  already copied

## Non-Goals In This Slice

This spec does not introduce:

- dynamic pack loading
- pack unload
- unregister support
- mutable runtime rewrite catalogs
- mutable builtin symbolic registration APIs

## Tests Required

This contract is covered when tests prove:

- builtin and pack-backed symbolic handlers come from registry bootstrap
- a compiled formula does not snapshot host registrations
- host registrations remain engine-scoped
- later evaluations observe later host registrations on the same engine
- concurrent evaluation of one compiled formula on one engine is supported

