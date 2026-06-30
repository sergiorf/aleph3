# Kernel Symbol Definition Precedence

## Purpose

This document defines the current precedence rules for symbolic name and
function resolution in the Aleph3 kernel refactor.

Related documents:

- [Aleph3 Unified Plan](aleph3_unified_plan.md)
- [Class Collaboration](class_collaboration.md)
- [System Architecture](system_architecture.md)

## Scope

This is the current precedence contract for the symbolic `Expr` evaluator.

It does not yet define a fully unified precedence model for:

- future assumptions-driven transformations
- future richer symbol metadata such as upvalues/downvalues

Those remain later steps.

## Current Symbol Value Precedence

For a symbolic `Symbol`, the current evaluator resolves in this order:

1. cycle guard
2. `symbol_values` lookup in the evaluation context
3. unresolved symbolic fallback

That means:

- a symbol with a bound symbolic value evaluates through that value
- an unbound symbol remains symbolic
- recursive self-reference falls back to the symbol itself rather than looping

## Current Function Call Precedence

For a symbolic `FunctionCall`, the current evaluator resolves in this order.

### First-Level Dispatch

1. structural dispatch
2. general function dispatch

Meaning:

- structural functions such as `List` are handled structurally
- everything else enters general function resolution through the shared symbol
  registry and builtin precedence rules

### General Function Resolution

Inside the general function path, the current order is:

1. special forms
2. kernel symbolic registry functions
3. builtin evaluator functions
4. user-defined functions
5. host functions
6. unresolved symbolic fallback

Meaning:

- special forms such as `If`, `And`, and `Or` win first
- registered symbolic built-ins and pack functions win before user-defined
  functions
- builtin evaluator functions win before user-defined functions
- user-defined functions are only used when no earlier symbolic dispatch owns
  the name
- host functions are resolved through the shared kernel context after symbolic
  function ownership checks
- if nothing owns the name, the function call remains symbolic

Plain-language examples:

- if a user defines `Plus[x_, y_] := 99`, `Plus[1, 2]` still uses builtin
  arithmetic and returns `3`
- if a user defines `Length[x_] := 99`, `Length[{1,2,3}]` still uses the
  registered symbolic handler and returns `3`
- if a host app registers `Clamp`, it is only used when no earlier symbolic
  owner wins for that name

## Registered Normalized-Head Rewrite Scheduling

Registered normalized-head rewrites do not participate in the ordinary
function-call precedence above.

Current answer:

- they are a simplification-stage extension point, not a callable dispatch
  owner
- they are consulted only after normalization inside the existing
  simplification flow
- they run in deterministic priority order per head
- the first matching rewrite wins

That means:

- ordinary function-call dispatch still follows special forms, registered
  symbolic handlers, builtin evaluator functions, user-defined functions, host
  functions, then symbolic fallback
- registered normalized-head rewrites do not make an otherwise unknown head
  callable
- list-aware arithmetic, domain-sensitive `Power` behavior, and special-
  function shortcuts remain evaluator-owned even when a head also has
  registered rewrites

## Current Assignment And Definition Rules

- `Assignment` writes evaluated values into `symbol_values`
- delayed function definitions store the raw body
- immediate function definitions evaluate the body first and then store it
- user-defined function bodies evaluate in a local copied context with bound
  parameters

## Current Host Function Position

Host functions now resolve through the shared kernel context during SDK
evaluation.

## Explicit Current Answers

### Built-in versus user-defined function with same name

Current answer:

- built-in or registered symbolic function wins

### Pack-registered function versus user-defined function with same name

Current answer:

- pack-registered symbolic function wins

### Unknown symbolic function

Current answer:

- remains symbolic rather than erroring by default

### Unknown symbolic variable

Current answer:

- remains symbolic rather than erroring by default

### Unknown SDK variable

Current answer:

- runtime error, because SDK evaluation requires concrete bindings or constants

## Next Follow-On Steps

1. define richer symbol metadata and attribute-driven dispatch
2. reduce the remaining evaluator-local execution paths after owner selection
3. decide how broader future rewrite categories should register without
   becoming a second function-call dispatcher
