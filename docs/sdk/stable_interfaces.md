# Stable Interfaces

This document marks the interfaces that should now be treated as the SDK
boundary. They are stable enough to build against, but not yet feature-complete.

## Status Table

| Surface | Status | Notes |
| --- | --- | --- |
| `sdk/Types.hpp` | provisional stable | Public value model, diagnostics, opaque `CompiledFormula`, and host function metadata/contracts |
| `sdk/Schema.hpp` | provisional stable | Host allowlists for variables, functions, and constants, including optional constant values |
| `sdk/Policy.hpp` | provisional stable | Structural/runtime limits and feature flags |
| `sdk/Engine.hpp` | provisional stable | Main facade; `validate`, `compile`, and trusted-subset `evaluate` are live |
| `ir/Node.hpp` | internal stable | Trusted-subset IR for parser, validation, and runtime work |
| `frontend/Lexer.hpp` + `frontend/Parser.hpp` | internal stable | Trusted-subset syntax frontend with structured diagnostics |
| `semantics/Validator.hpp` | internal stable | Schema, arity, feature-gate, and composed-expression type validation for the trusted subset |
| `runtime/Evaluator.hpp` | internal stable | Trusted-subset tree interpreter with bindings, `If`, comparisons, and host dispatch |

## Public SDK Boundary

```mermaid
classDiagram
    class Engine {
        +compile(source, schema, policy) CompileResult
        +validate(source, schema, policy) ValidationResult
        +evaluate(formula, bindings) EvaluationResult
        +register_function(spec)
    }

    class Schema {
        +allow_variable(variable)
        +allow_function(function)
        +allow_constant(name)
    }

    class Policy {
        +default_policy()
        +strict_numeric()
    }

    class Value
    class CompiledFormula
    class HostFunctionSpec

    Engine --> Schema
    Engine --> Policy
    Engine --> CompiledFormula
    Engine --> Value
    Engine --> HostFunctionSpec
```

## Stable Design Rules

- Host applications must not depend on internal symbolic AST types such as `Expr`.
- The public SDK must remain usable without including parser or evaluator headers.
- `CompiledFormula` remains opaque even though the frontend and runtime are now live.
- `ir::Node` is for internal SDK/runtime layers only and must not leak into the SDK.
- Engine-scoped host function registration replaces the old global registry model.
- The SDK build now depends on `aleph3_kernel` even though the public SDK
  boundary still must not expose internal symbolic AST types.

## Current Guarantees

- SDK headers compile independently of symbolic-engine internal headers.
- The engine facade links with a concrete implementation.
- Validation produces structured diagnostics for syntax, schema, arity, feature-gates, branch compatibility, schema-valued constants, constant runtime traps, and obvious type failures.
- Compile produces reusable opaque `CompiledFormula` handles on successful parse + validation.
- Evaluate executes the trusted subset for literals, bindings, arithmetic, comparisons, `If`, and registered host calls.
- Optional built-ins can be enabled by policy for `Abs`, `Min`, `Max`, `Clamp`, `Floor`, `Ceil`/`Ceiling`, `Round`, and `Sqrt`.
- Schema-valued constants can participate in validation and runtime evaluation without host bindings.
- Non-finite numeric arithmetic inputs/results fail with structured runtime errors instead of leaking raw floating-point behavior.
- Numeric equality and ordering reject non-finite inputs instead of exposing raw floating-point comparison behavior.
- Invalid numeric power domains such as `0 ^ 0` and negative-base fractional powers fail with structured runtime errors.
- Built-in numeric domain failures such as `Sqrt[-1]` or `Clamp[x, high, low]` fail with structured runtime errors.
- Zero-valued numeric results are canonicalized to positive zero for deterministic output behavior.
- Equality comparisons require comparable concrete value types and reject mixed-type equality.
- Registered host functions enforce arity/parameter metadata at registration and argument/return contracts at runtime.
- The IR only models the trusted subset, not the full prototype language.

## Known Gaps

- No explicit source canonicalization or serialization yet.
- CLI evaluation currently supports numbers, booleans, and string bindings through `--var name=value`.
- Optional built-ins and richer host-function tooling ergonomics are still limited on the tooling path.
