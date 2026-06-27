# Kernel Symbol Model Spec

## Status

Initial symbol metadata and definition contract is now implemented.

Primary implementation:

- [include/symbols/SymbolState.hpp](/home/sergio/dev/aleph3/include/symbols/SymbolState.hpp)
- [include/kernel/EvaluationContext.hpp](/home/sergio/dev/aleph3/include/kernel/EvaluationContext.hpp)
- [include/kernel/FunctionRegistry.hpp](/home/sergio/dev/aleph3/include/kernel/FunctionRegistry.hpp)

## Purpose

This document defines the first kernel-owned symbol model that evaluator,
registration, and rewrite work can share.

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
- `rewrite_rule`

These records are stored in `symbols::SymbolDefinitionTable`.

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

## Current Precedence Facts

Current evaluator precedence is still:

1. special forms
2. registered symbolic handlers
3. builtin evaluator functions
4. user-defined functions
5. host functions
6. symbolic fallback

The new symbol model does not replace that precedence yet.
It makes metadata and definition ownership explicit so precedence can migrate
away from evaluator branching.

## What Is Implemented Versus Deferred

Implemented now:

- kernel-owned symbol metadata records
- kernel-owned definition records
- registry metadata for symbolic functions
- explicit pack-registration metadata path
- evaluator-side population of metadata/definition records for registered
  symbolic handlers, user-defined functions, and assignments

Deferred:

- attribute-driven evaluation control
- ownvalue/downvalue-style lookup
- mutation semantics beyond current tables
- evaluator dispatch driven primarily from symbol-definition records rather than
  evaluator-local precedence code

## Next Steps

- make evaluator dispatch consult richer symbol-definition facts
- migrate more builtin identity and precedence knowledge out of evaluator
  branches
- define the first symbolic coefficient contract for supported monomial-shaped
  terms and decide how it should attach to symbol-definition metadata
- define how rewrite rules register against the same symbol contract
