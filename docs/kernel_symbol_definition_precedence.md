# Kernel Symbol Definition Precedence

## Purpose

This document defines the current precedence rules for symbolic name and
function resolution in the Aleph3 kernel refactor.

Related documents:

- [Aleph3 Unified Plan](aleph3_unified_plan.md)
- [Kernel Registration Interfaces](kernel_registration_interfaces.md)
- [Kernel Evaluation Context](kernel_evaluation_context.md)

## Scope

This is the current precedence contract for the symbolic `Expr` evaluator.

It does not yet define a fully unified precedence model for:

- SDK host functions versus symbolic functions
- future rewrite rules
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

1. algebra dispatch
2. structural dispatch
3. general function dispatch

Meaning:

- algebra entrypoints such as `Expand` and `Factor` are intercepted before the
  general function path
- structural functions such as `List` are handled structurally
- everything else enters general function resolution

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
- builtin evaluator branches win before user-defined functions
- user-defined functions are only used when no earlier symbolic dispatch owns
  the name
- host functions are resolved through the shared kernel context after symbolic
  function ownership checks
- if nothing owns the name, the function call remains symbolic

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

1. define how SDK host functions and symbolic registrations relate long-term
2. define precedence for future rewrite rules
3. define richer symbol metadata and attribute-driven dispatch
