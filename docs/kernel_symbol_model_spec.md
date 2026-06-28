# Kernel Symbol Model Spec

## Status

Initial symbol metadata and definition contract is now implemented, including
explicit builtin and host ownership records plus a first dispatch contract that
consults shared symbol-definition facts.

Primary implementation:

- [include/symbols/SymbolState.hpp](/home/sergio/dev/aleph3/include/symbols/SymbolState.hpp)
- [include/kernel/EvaluationContext.hpp](/home/sergio/dev/aleph3/include/kernel/EvaluationContext.hpp)
- [include/kernel/FunctionRegistry.hpp](/home/sergio/dev/aleph3/include/kernel/FunctionRegistry.hpp)

## Purpose

This document defines the first kernel-owned symbol model that evaluator,
registration, and rewrite work can share.

Plain-language summary:

- a symbol is a name such as `x`, `Plus`, or `Clamp`
- symbol metadata is descriptive information about that name
- a definition record answers "who currently owns behavior for this name?"
- the symbol model gives the evaluator one shared place to look instead of
  scattering ownership facts across local branches

Practical examples:

- `answer = 42` creates an `own_value` record for `answer`
- `f[x_] := x + 1` creates a `user_function` record for `f`
- `Length[{1,2,3}]` resolves through a registered symbolic handler
- `Clamp[x, 0, 10]` can resolve through a builtin or a host function,
  depending on which ownership records exist

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
- `registered_handler`
- `builtin_function`
- `host_function`
- `rewrite_rule`

These records are stored in `symbols::SymbolDefinitionTable`.

The table also now exposes exact-kind lookup in addition to boolean presence
checks, so evaluator and tests can query specific ownership records directly.

### Evaluation Context

`kernel::EvaluationContext` now carries:

- `symbol_values`
- `symbol_metadata`
- `definition_records`
- `function_definitions`

That means symbol metadata, symbolic values, and registered-definition facts can
travel through one kernel execution context.

## Registration Contract

The kernel registry now exposes the minimal symbolic registration contract that
future packs can build on.

`kernel::SymbolicFunctionSpec` includes:

- `metadata.name`
- `metadata.owning_package`
- `metadata.documentation`
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
behavior.

Examples:

- built-in symbolic behavior such as `StringJoin[...]` is registered up front
- a future pack could register `Differentiate[...]`
- an embedding app can register a host function such as `Clamp[...]`

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

Execution still remains partly evaluator-owned once an owner is selected,
especially for host execution and some remaining builtin behavior families.

## What Is Implemented Versus Deferred

Implemented now:

- kernel-owned symbol metadata records
- kernel-owned definition records
- registry metadata for symbolic functions
- explicit pack-registration metadata path
- evaluator-side population of metadata/definition records for registered
  symbolic handlers, builtin evaluator functions, host functions,
  user-defined functions, and assignments
- evaluator dispatch ownership selection derived from shared symbol-definition
  and registration facts

Deferred:

- attribute-driven evaluation control
- ownvalue/downvalue-style lookup
- mutation semantics beyond current tables
- fully registry- or definition-driven execution for the remaining host and
  richer builtin behavior paths once an owner is selected

## Next Steps

- decide how the small symbolic coefficient contract should attach to
  symbol-definition metadata if it grows beyond the current builtin-owned
  implementation
- define how rewrite rules register against the same symbol contract
- decide how far attribute metadata should influence dispatch and argument
  evaluation
