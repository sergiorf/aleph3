# Trusted Subset V1

## Purpose

This document defines the exact feature subset the rewritten Aleph3 embedded
engine must support in its first trustworthy version.

This subset is intentionally narrow.

The goal is not to preserve the current language breadth. The goal is to ship
a small, reliable engine for embeddable formula evaluation in native products.

If a feature is not listed here as supported, it should be treated as
unsupported for v1.

## Product Intent

Trusted Subset V1 is designed for host applications that need:

- user-editable formulas
- deterministic local evaluation
- host-controlled variables and functions
- save-time validation
- bounded runtime behavior

It is not designed to be a general symbolic mathematics language.

## Supported Concepts

V1 supports only the following conceptual building blocks:

- literals
- variables
- arithmetic expressions
- comparisons
- conditional expressions via `If`
- host-approved function calls

That is the entire trusted language core.

## Supported Value Types

The runtime value model for v1 supports:

- number
- boolean
- string

Optional for late v1 only if needed by a concrete host use case:

- homogeneous list of values

Not supported in v1:

- complex numbers
- rational as a first-class public runtime type
- symbolic unevaluated expressions as public results
- arbitrary nested symbolic structures as host-visible values

Notes:

- Internally the runtime may represent values however it wants.
- Public SDK behavior should expose a clean value model, not legacy AST detail.

## Supported Syntax

## 1. Literals

Supported:

- decimal integers: `0`, `1`, `42`
- decimal floating-point numbers: `3.14`, `0.5`, `-2.75`
- booleans: `True`, `False`
- double-quoted strings: `"abc"`

Notes:

- Unary minus is part of expression syntax, not a separate numeric token
  requirement.
- String escaping support may be minimal in v1, but whatever is supported must
  be explicitly documented and tested.

## 2. Variables

Supported:

- identifiers matching the v1 identifier rules

Recommended identifier rules:

- first character: ASCII letter or `_`
- remaining characters: ASCII letter, digit, or `_`

Notes:

- Unicode identifiers are intentionally out of scope for v1.
- Variable validity is controlled by schema, not by syntax alone.

## 3. Arithmetic Operators

Supported infix operators:

- `+`
- `-`
- `*`
- `/`
- `^`

Supported unary operators:

- unary `-`
- unary `+`

Minimum precedence expectations:

1. unary operators
2. `^`
3. `*`, `/`
4. `+`, `-`

Notes:

- `^` should be right-associative.
- The evaluator may initially restrict exponent behavior if needed for
  determinism and simplicity.

## 4. Comparison Operators

Supported:

- `==`
- `!=`
- `<`
- `<=`
- `>`
- `>=`

Notes:

- Comparison results are booleans.
- Chained comparisons such as `0 < x < 1` are out of scope for v1 unless
  implemented explicitly and tested explicitly.

## 5. Parentheses

Supported:

- grouping with `(` and `)`

Examples:

- `(x + 1) * y`
- `If((x + y) > 0, x, y)`

## 6. Function Calls

Supported syntax:

- `Name[arg1]`
- `Name[arg1, arg2]`
- `Name[arg1, arg2, ...]`

Function calls are allowed only if the function is:

- built into the trusted subset, or
- explicitly registered and permitted by the host schema

## 7. Conditional Expression

Supported syntax:

- `If[condition, then_expr, else_expr]`

Requirements:

- exactly three arguments
- `condition` must evaluate to a boolean

Notes:

- No multi-branch or optional-argument `If` variants in v1.

## Built-In Functions In Scope

V1 should keep built-ins minimal.

### Required Built-In Form

- `If`

### Optional Small Built-In Set

These may be included only if they clearly support the embedding use case and
do not complicate the trusted core disproportionately:

- `Min`
- `Max`
- `Abs`

Default recommendation:

- keep built-ins to `If` plus host-registered functions unless a concrete use
  case forces more

## Host Function Model

Host functions are first-class in the product model.

V1 host function requirements:

- registration is engine-scoped
- each function has a name
- each function has explicit arity metadata or arity rules
- host functions can be allowlisted in schema
- runtime failures from host functions are surfaced as structured runtime errors

Host functions should be pure from the engine’s perspective.

That means:

- no implicit access to engine internals
- no ambient mutable global state requirement
- no assumptions of side-effect ordering as a language contract

## Validation Rules

V1 validation must reject:

- unknown variables
- unknown functions
- disallowed functions
- wrong arity when known
- unsupported syntax features
- formulas exceeding policy limits

V1 validation does not need to provide:

- advanced static typing for every expression
- deep algebraic reasoning
- symbolic simplification guarantees

## Runtime Rules

V1 runtime must guarantee:

- deterministic results for the same engine version, schema, policy, formula,
  and bindings
- explicit host-provided variable bindings
- bounded execution through policy controls

V1 runtime does not need to support:

- symbolic unevaluated fallback
- user-defined function environments
- session-style mutable definitions

## Explicitly Unsupported In V1

The following features are out of scope and should be rejected by validation or
parse as unsupported:

- assignments such as `x = 2`
- user-defined functions such as `f[x_] := x + 1`
- delayed or immediate definitions
- rules such as `a -> b`
- lists unless explicitly promoted into late v1
- string concatenation operators beyond plain string literal/function support
- logical infix operators such as `&&` and `||`
- pattern matching
- replacement rules
- symbolic simplification as a required user-visible contract
- polynomial manipulation functions
- exact rational syntax as a public product contract
- complex arithmetic
- implicit multiplication such as `2x`
- Mathematica compatibility beyond the documented subset

Two exclusions are especially important:

1. Implicit multiplication is out.
   Reason:
   It complicates parsing and introduces ambiguity with little value for the
   first embedding wedge.

2. User-defined functions are out.
   Reason:
   They create substantial semantic and safety surface area that the embedded
   v1 does not need.

## Error Model Expectations

V1 must distinguish:

- syntax errors
- validation errors
- runtime errors

Each category must be structured and machine-readable.

At minimum, diagnostics and runtime errors should include:

- stable code
- human-readable message
- source location when relevant

## Canonical Examples

These examples should work in v1:

- `x + 1`
- `(gain * raw_signal + offset) / reference`
- `If[temp > threshold, scale * x, x]`
- `Clamp[x, 0, 1]` when `Clamp` is host-registered and allowed
- `Max[0, value]` if `Max` is included as a built-in or host function

These examples should be rejected in v1:

- `f[x_] := x + 1`
- `x = 2`
- `2x + 1`
- `a -> b`
- `{1, 2, 3}` unless lists are explicitly promoted into scope
- `x && y`

These exclusions are about v1 scope control, not permanent product direction.
Likely post-v1 candidates include:

- more built-in functions with explicit contracts
- improved polynomial manipulation
- symbolic differentiation
- carefully scoped integral support

## Parser Simplicity Rules

To keep the trusted core small, parser design should follow these rules:

- parse only documented syntax
- reject ambiguous shorthand
- prefer explicit syntax over convenience syntax
- avoid special-case language growth unless tied to a product use case

This means the parser should be intentionally less ambitious than the current
prototype parser.

## Runtime Simplicity Rules

To keep the runtime trustworthy, evaluation design should follow these rules:

- no process-global engine state
- no hidden variable resolution
- no implicit symbolic fallback for unsupported constructs
- no dependence on output formatting for semantics
- no feature execution outside schema and policy controls

## Promotion Rules For Future Features

A feature may be promoted beyond v1 only if all of the following are true:

1. it supports the embedded product wedge directly
2. it can be expressed cleanly in the SDK and schema model
3. it has clear error behavior
4. it has bounded runtime behavior
5. it does not destabilize the trusted core disproportionately

This rule exists to prevent the rewrite from drifting back into a broad CAS.

## Release Gate For Trusted Subset V1

Trusted Subset V1 is complete only when:

- all supported syntax in this document compiles and evaluates correctly
- all unsupported syntax in this document is rejected intentionally
- schema validation works for variables and functions
- runtime behavior is deterministic
- safety policies are enforced
- SDK contracts pass without exposing internal AST

## Recommendation

Use this document as the scope boundary for:

- parser implementation
- validator implementation
- runtime implementation
- test design
- product messaging for the first trustworthy engine

If a proposed feature is not in this document, it should default to `no` until
there is a strong product reason to add it.
