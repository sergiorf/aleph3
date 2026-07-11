# Kernel Symbol Model Spec

## Status

Initial symbol metadata and definition contract is now implemented, including
explicit builtin and host ownership records plus a dispatch contract that
consults shared symbol-definition facts and an explicit runtime function
catalog.

Primary implementation:

- [`include/symbols/SymbolState.hpp`](../include/symbols/SymbolState.hpp)
- [`include/kernel/EvaluationContext.hpp`](../include/kernel/EvaluationContext.hpp)
- [`include/kernel/FunctionRegistry.hpp`](../include/kernel/FunctionRegistry.hpp)

Related lifecycle contract:

- [Kernel Registration Lifecycle Spec](kernel_registration_lifecycle_spec.md)
- [Kernel Attribute Spec](kernel_attribute_spec.md)

## Purpose

This document defines the first kernel-owned symbol model that evaluator,
registration, and rewrite work can share.

Plain-language summary:

- a symbol is a name such as `x`, `Plus`, or `Clamp`
- symbol metadata is descriptive information about that name
- a definition record answers "who currently owns behavior for this name?"
- the symbol model gives the evaluator one shared place to look instead of
  scattering ownership facts across local branches
- a function catalog answers "which registered behaviors are available in this
  engine or session right now?"
- evaluation-control attributes now also publish a small active scheduling
  contract through that same shared symbol state

Practical examples:

- `answer = 42` creates an `own_value` record for `answer`
- `f[x_] := x + 1` creates a `user_function` record for `f`
- `Length[{1,2,3}]` resolves through a registered symbolic handler
- `Clamp[x, 0, 10]` can resolve through a builtin or a host function,
  depending on which ownership records exist
- one test can register a rewrite in its own catalog without changing another
  test's catalog

## Current Contract

### Symbol Metadata

Kernel-owned symbol metadata now has an explicit shape:

- symbol name
- attribute list
- documentation string
- definition origin
- provider identifier

This is represented by `symbols::SymbolMetadata` and stored in
`symbols::SymbolMetadataTable`.

Attributes are no longer purely descriptive in every case.

Current answer:

- `HoldFirst`, `HoldRest`, and `HoldAll` are now active kernel contract facts
  for the small builtin-owned set that already relies on held arguments
- `listable` and `numeric_function` remain descriptive metadata in this slice

### Definition Records

The kernel now has a lightweight definition-record layer beyond raw value and
function maps.

`symbols::SymbolDefinitionRecord` tracks:

- definition kind
- definition origin
- provider identifier

The current definition kinds are:

- `own_value`
- `user_function`
- `special_form`
- `registered_handler`
- `builtin_function`
- `host_function`
- `rewrite_rule`

These records are stored in `symbols::SymbolDefinitionTable`.

The table also now exposes exact-kind lookup in addition to boolean presence
checks, so evaluator and tests can query specific ownership records directly.

`rewrite_rule` is now an active contract for registered normalized-head
rewrites:

- consulting a registered normalized-head rewrite records rewrite ownership for
  that head in shared kernel definition state
- this records simplification-stage extension ownership, not ordinary callable
  dispatch ownership

### Evaluation Context

`kernel::EvaluationContext` now carries:

- `symbol_values`
- `symbol_metadata`
- `definition_records`
- `function_definitions`
- a pointer to the active `FunctionRegistry`

That means symbol metadata, symbolic values, and registered-definition facts can
travel through one kernel execution context, together with the catalog of
registered behavior available for that execution.

## Registration Contract

The kernel registry now exposes the minimal symbolic registration contract that
future packs can build on.

`kernel::SymbolicFunctionSpec` includes:

- `metadata.name`
- `metadata.owning_package`
- `metadata.documentation`
- `metadata.attributes`
- `metadata.source`
- `metadata.rewrite_safe`
- callable handler

For pack-style registration, the first explicit helper is:

- `FunctionRegistry::register_pack_function(...)`

That is the current minimum contract a future pack must satisfy:

- declare the public symbol name
- declare which package owns it
- provide its handler
- optionally document it
- declare whether it is safe to use from rewrite-driven transformations

In plain terms, registration is how Aleph3 learns that a name has executable
behavior, and the function catalog is where one engine or session stores those
registrations.

Attribute note:

- symbolic registration metadata can now publish explicit hold attributes
- that metadata becomes visible through shared symbol state when the
  registration is consulted
- publishing attribute metadata does not by itself make a symbol a new
  evaluation owner

Examples:

- built-in symbolic behavior such as `StringJoin[...]` is loaded into the
  default catalog up front
- the `core-calculus` pack registers `D[...]` and `Differentiate[...]`
- an embedding app can register a host function such as `Clamp[...]`
- two SDK engines can now evaluate against different host-function sets without
  sharing runtime registration state

Lifecycle note:

- builtin and pack-backed symbolic registrations are fixed when a registry is
  created
- host-function registration remains the only public mutable registration
  surface today
- `CompiledFormula` does not own host-function registrations; later evaluation
  consults the engine's current host-function set

## Current Precedence Facts

Current evaluator precedence is still:

1. special forms
2. registered symbolic handlers
3. builtin evaluator functions
4. user-defined functions
5. host functions
6. symbolic fallback

The new symbol model now participates in that precedence directly by making
callable ownership explicit and allowing dispatch to derive a primary owner
from shared symbol-definition and registration facts.

`If`, `And`, and `Or` now use the explicit `special_form` definition kind and
must be present in the active function catalog. Their held evaluation and
short-circuit behavior is registry-backed rather than an implicit process-wide
evaluator branch.

Execution still remains partly evaluator-owned once an owner is selected,
especially for host execution and some remaining builtin behavior families.

## What Is Implemented Versus Deferred

Implemented now:

- kernel-owned symbol metadata records
- kernel-owned definition records
- registry metadata for symbolic functions
- explicit pack-registration metadata path
- explicit registry-backed special-form specs for `If`, `And`, and `Or`
- shared attribute metadata sync for active held builtins and registered
  symbolic handlers that declare hold attributes
- shared rewrite-rule metadata and definition-record sync for registered
  normalized-head rewrites
- evaluator-side population of metadata/definition records for registered
  symbolic handlers, builtin evaluator functions, host functions,
  user-defined functions, and assignments
- evaluator dispatch ownership selection derived from shared symbol-definition
  and registration facts
- session-local cleanup through `Clear` and `Unset`, plus session lifecycle
  reset through `session::Session::reset()`

Deferred:

- broader attribute-driven evaluation control beyond the current held builtin
  slice
- ownvalue/downvalue-style lookup
- mutation semantics beyond the current session-local cleanup and reset surface
- fully registry- or definition-driven execution for the remaining host and
  richer builtin behavior paths once an owner is selected
- runtime pack loading, pack unload, and broader registry mutation semantics

## Implemented Session Cleanup Surface

`Clear` and `Unset` are the MVP state-cleanup functions for interactive
sessions. They are kernel-owned mutation operations over the current session
context, not registry mutation or pack lifecycle tools.

Implemented contract:

- `Clear[symbol]` removes a symbol's current own value and any user function
  definitions for that name in the active session.
- `Unset[symbol]` removes the directly assigned own value for that symbol.
- both functions take an unevaluated symbol argument, so `Clear[x]` targets
  `x` even when `x` currently has an own value.
- `Unset[f]` leaves user function definitions intact in this MVP slice.
- clearing an unknown user symbol is a no-op with deterministic success, so
  cleanup scripts remain idempotent.
- clearing builtin, special-form, registered pack, or host-function ownership
  is rejected with an explicit diagnostic and must not remove symbol metadata,
  registry entries, or provider-owned definition records.
- cleanup never mutates another session, a compiled formula, the default
  function catalog, pack registrations, or host registrations.
- both functions return the target symbol on success because the public
  expression model does not currently expose `Null`.

The implementation updates symbol values, user function tables, and
session-owned definition records together. If a symbol has both session-owned
state and provider-owned behavior, cleanup removes only the session-owned
records and leaves provider behavior visible through normal precedence.

Implemented session lifecycle reset:

- `session::Session::reset()` discards the active session-local evaluation
  context and recreates it against the same function registry.
- reset removes session-local own values, user function definitions,
  definition records, assumptions, learned symbol metadata, and runtime
  counters.
- reset preserves provider catalogs, builtins, registered packs, and the
  default registry. It does not mutate notebook documents or persisted files.
- the CLI REPL exposes this lifecycle operation as `:reset`; it is not a
  symbolic builtin and does not replace `Clear` or `Unset`.

Implemented interactive discovery contract:

- session completion reports names from the active function registry and the
  current session-local definition state.
- provider-owned builtin, special-form, and pack entries keep discovery
  precedence over same-named session-local records, matching evaluator
  precedence.
- session-local own values are reported as `symbol`; session-local user
  functions are reported as `function`.
- session help uses the same registry and session-local state, with static
  help metadata providing accepted forms, concise descriptions, short examples,
  exactness notes, unsupported boundaries, and manual anchors where available.
- focused help is deterministic prefix/name/package lookup. Fuzzy search,
  natural-language help, and rich in-product documentation search remain
  outside this contract.

Deferred cleanup syntax:

- Mathematica-style `x =.` syntax is not implemented.
- A downvalue-specific or function-definition-specific `Unset` syntax remains
  deferred until its exact parser and ownership contract is specified.

## Next Steps

- decide how the small symbolic coefficient contract should attach to
  symbol-definition metadata if it grows beyond the current builtin-owned
  implementation
- grow rewrite registration on top of this shared rewrite-rule ownership
  contract without widening rewrite semantics prematurely
- specify any function-definition-specific cleanup syntax before exposing it
  publicly
- decide how far attribute metadata should influence dispatch and argument
  evaluation after the first held-builtin contract
