# Aleph3 System Architecture

## Purpose

This document defines the target architecture for Aleph3 as a layered symbolic
system.

Architecture status note:

- this document describes the SDK/trusted-subset architecture track
- the repository also contains a symbolic-core track that is converging toward
  a single kernel
- the current SDK runtime evaluator should be treated as transitional, not as a
  second permanent kernel

Related architecture documents:

- [Class Collaboration](class_collaboration.md)
- [Symbolic Core Architecture](symbolic_core_architecture.md)
- [Kernel Refactor Urgent Plan](kernel_refactor_urgent_plan.md)
- [Kernel Refactor Backlog](kernel_refactor_backlog.md)
- [Kernel Representation Decision](kernel_representation_decision.md)
- [Layer Ownership Matrix](layer_ownership_matrix.md)

It answers four questions:

1. what layers the system has
2. what major classes each layer owns
3. how data flows from source text to evaluated result
4. whether the system should have an IR and eventually a VM

## Architectural Goals

The architecture is optimized for:

- trustworthy embedding
- stable SDK boundaries
- explicit diagnostics
- bounded evaluation
- freedom to refactor internals without breaking customers

It is not optimized for:

- broad CAS feature coverage
- notebook-style workflows
- maximum compatibility with current internals

## Top-Level Layers

The system should have six layers:

1. `sdk`
2. `frontend`
3. `ir`
4. `semantics`
5. `runtime`
6. `tooling`

These are logical layers first. They should also map to code organization.

## Layer Overview

### 1. SDK

Responsibility:

- public API for host applications
- lifecycle orchestration: compile, validate, evaluate
- public value types
- public diagnostics and runtime errors
- schema and policy configuration

Key rule:

The SDK must not expose internal AST or runtime implementation details.

### 2. Frontend

Responsibility:

- source text handling
- lexing
- parsing
- syntax diagnostics
- source span tracking

Output:

- trusted internal IR

### 3. IR

Responsibility:

- minimal internal representation of the trusted subset
- stable boundary between parse and later phases

Key rule:

The IR should model only the supported embedded language, not the full symbolic
expression universe.

### 4. Semantics

Responsibility:

- schema validation
- feature gating
- arity checks
- type checks where intentionally supported
- complexity analysis

Output:

- validated IR plus diagnostics

### 5. Runtime

Responsibility:

- evaluation
- runtime value model
- host function dispatch
- evaluation budgeting
- eventual compiled execution path

Transitional rule:

The current SDK runtime layer is allowed as a migration layer for the
trusted-subset path, but long-term symbolic semantics must converge on a single
kernel rather than persisting as a separate `runtime::Evaluator` core.

### 6. Tooling

Responsibility:

- example apps
- CLI
- benchmark harness
- developer diagnostics tools

Tooling is a consumer of the SDK, not a peer architecture owner.

## Data Flow

The intended flow is:

1. host constructs `Engine`
2. host supplies `Schema` and `Policy`
3. SDK compiles formula source
4. frontend lexes and parses source into IR
5. semantics validates IR against schema and policy
6. SDK returns `CompiledFormula` or diagnostics
7. host evaluates compiled formula with bindings
8. runtime executes IR and returns `Value` or `RuntimeError`

Migration note:

For the current repository state, this flow is still implemented separately
from the symbolic evaluator. That is a transitional condition being corrected
by the kernel refactor program.

That gives a clean lifecycle:

- compile
- validate
- evaluate

## Proposed Module Structure

```text
include/
  sdk/
    Engine.hpp
    Types.hpp
    Schema.hpp
    Policy.hpp
  frontend/
    Diagnostic.hpp
    SourceSpan.hpp
    Token.hpp
    Lexer.hpp
    Parser.hpp
  ir/
    Node.hpp
    NodeKind.hpp
    Visitor.hpp
  semantics/
    Validator.hpp
    AstAnalysis.hpp
    TypeSystem.hpp
  runtime/
    Value.hpp
    Evaluator.hpp
    HostFunction.hpp
    RuntimeError.hpp

src/
  sdk/
    Engine.cpp
    Schema.cpp
    Policy.cpp
  frontend/
    Lexer.cpp
    Parser.cpp
  ir/
  semantics/
    Validator.cpp
    AstAnalysis.cpp
    TypeSystem.cpp
  runtime/
    Evaluator.cpp
    HostFunction.cpp
```

## Core Public SDK Classes

## Engine

Header:

- `include/sdk/Engine.hpp`

Responsibility:

- main public facade
- owns or references engine-scoped configuration
- composes frontend, semantics, and runtime services

Likely interface:

```cpp
class Engine {
public:
    explicit Engine(EngineOptions options = {});

    CompileResult compile(
        std::string_view source,
        const Schema& schema,
        const Policy& policy = Policy::default_policy()) const;

    ValidationResult validate(
        std::string_view source,
        const Schema& schema,
        const Policy& policy = Policy::default_policy()) const;

    EvaluationResult evaluate(
        const CompiledFormula& formula,
        const Bindings& bindings) const;

    void register_function(HostFunctionSpec spec);
};
```

## EngineOptions

Header:

- `include/sdk/Types.hpp`

Responsibility:

- configure high-level engine behavior
- mostly construction-time concerns

Examples:

- numeric mode selection
- diagnostic verbosity defaults
- default built-in set policy

## CompiledFormula

Header:

- `include/sdk/Types.hpp`

Responsibility:

- opaque handle returned by `compile`
- holds validated IR and internal metadata
- reused across evaluations

Key rule:

Host code should not access raw IR directly.

Likely implementation:

- pimpl
- opaque shared state
- internal pointer to `ir::Node`

## Value

Header:

- `include/sdk/Types.hpp`
- runtime backing in `include/runtime/Value.hpp`

Responsibility:

- public result and input value model

Supported v1 variants:

- number
- boolean
- string

Late v1 optional:

- list

## Diagnostic

Header:

- `include/sdk/Types.hpp`
- syntax details backed by `include/frontend/Diagnostic.hpp`

Responsibility:

- machine-readable compile or validation issue

Fields should include:

- severity
- code
- message
- source span

## Schema

Header:

- `include/sdk/Schema.hpp`

Responsibility:

- define what names and features a host allows

Contents:

- allowed variables
- allowed built-ins
- allowed host functions
- optional type metadata

## Policy

Header:

- `include/sdk/Policy.hpp`

Responsibility:

- define runtime and structural limits

Fields:

- max AST depth
- max node count
- max evaluation steps
- enabled feature flags

## HostFunctionSpec

Header:

- `include/runtime/HostFunction.hpp`
- public exposure via SDK headers as needed

Responsibility:

- declarative description of a host-provided function

Fields:

- name
- arity
- callable
- optional type expectations
- purity expectations

## Frontend Classes

## SourceSpan

Header:

- `include/frontend/SourceSpan.hpp`

Responsibility:

- track source ranges for diagnostics

Fields:

- byte offset start
- byte offset end
- line
- column

## Token

Header:

- `include/frontend/Token.hpp`

Responsibility:

- lexed token unit

Fields:

- token kind
- lexeme or value payload
- source span

## Lexer

Header:

- `include/frontend/Lexer.hpp`

Responsibility:

- convert source text to tokens

Key behavior:

- no semantic decisions
- detect invalid characters and malformed literals

## Parser

Header:

- `include/frontend/Parser.hpp`

Responsibility:

- consume tokens
- produce IR
- emit syntax diagnostics

Recommended parser style:

- Pratt parser or precedence-climbing parser

Reason:

- good fit for arithmetic-heavy expression syntax

## IR Classes

The IR should be intentionally smaller than the full symbolic `Expr`.

## NodeKind

Header:

- `include/ir/NodeKind.hpp`

Possible kinds:

- `NumberLiteral`
- `BooleanLiteral`
- `StringLiteral`
- `Variable`
- `UnaryOp`
- `BinaryOp`
- `Call`
- `IfExpr`

## Node

Header:

- `include/ir/Node.hpp`

Responsibility:

- base representation for parsed and validated expressions

Design options:

1. tagged union with structs
2. polymorphic hierarchy

Recommendation:

- use tagged union style for v1

Reason:

- smaller
- easier to reason about
- lower allocation and ownership complexity

Example shape:

```cpp
struct Node {
    NodeKind kind;
    SourceSpan span;
    Payload payload;
};
```

Or a variant-based design:

```cpp
using Node = std::variant<
    NumberLiteralNode,
    BooleanLiteralNode,
    StringLiteralNode,
    VariableNode,
    UnaryOpNode,
    BinaryOpNode,
    CallNode,
    IfNode
>;
```

Recommendation:

- prefer explicit structs plus `std::variant`

## Semantics Classes

## Validator

Header:

- `include/semantics/Validator.hpp`

Responsibility:

- validate IR against `Schema` and `Policy`

Checks:

- unknown variables
- unknown functions
- disallowed features
- arity mismatches
- optional simple type rules

## AstAnalysis

Header:

- `include/semantics/AstAnalysis.hpp`

Responsibility:

- compute structural properties

Metrics:

- node count
- max depth
- feature usage

Used for:

- policy enforcement
- diagnostics

## TypeSystem

Header:

- `include/semantics/TypeSystem.hpp`

Responsibility:

- optional lightweight type reasoning

V1 scope:

- enough to validate obvious errors
- not a full static type inference engine

Examples:

- `If` condition should be boolean
- known function argument types when explicitly declared

## Runtime Classes

## Runtime Value

Header:

- `include/runtime/Value.hpp`

Responsibility:

- internal and public-facing runtime value representation

Variants:

- number
- boolean
- string

## RuntimeError

Header:

- `include/runtime/RuntimeError.hpp`

Responsibility:

- machine-readable runtime failure

Examples:

- division by zero
- invalid host function call
- budget exhaustion
- type mismatch at runtime

## Evaluator

Header:

- `include/runtime/Evaluator.hpp`

Responsibility:

- execute validated IR

Input:

- compiled formula
- bindings
- engine function registry
- evaluation budget

Output:

- `Value` or `RuntimeError`

Key rule:

- evaluator must not consult process-global mutable state

## FunctionRegistry

Header:

- internal to runtime layer, not the old singleton model

Responsibility:

- hold engine-scoped host function specs

Important:

- this is not the current global singleton design
- this is a per-engine registry owned by `Engine`

## EvaluationBudget

Header:

- internal runtime support type

Responsibility:

- track allowed work during evaluation

Fields:

- max steps
- steps used

Behavior:

- decrement or increment on each operation
- abort deterministically when exhausted

## Recommended Ownership Model

## Engine Ownership

`Engine` should own:

- host function registry
- default configuration
- service objects or factories as needed

`CompiledFormula` should own:

- validated IR
- compile-time metadata
- source hash or canonical source if useful

Evaluation should be:

- per call
- binding-driven
- free of global mutable runtime state

## Threading Model

Recommended v1 model:

- `Engine` is immutable after configuration except for explicit registration
  steps
- `CompiledFormula` is read-only after creation
- evaluation uses per-call bindings and budget state

This enables a future model where:

- multiple threads can evaluate the same compiled formula safely
- separate engines can host different function sets

Exact guarantees should be documented separately once implemented.

## Should There Be An IR?

Yes.

The system should absolutely have an IR.

Reason:

- it separates parsing from validation and runtime
- it gives the SDK a stable internal compilation boundary
- it enables future optimization and alternate execution paths
- it avoids reparsing on repeated evaluation

For v1, the IR should be:

- structural
- simple
- very close to the supported syntax

It should not yet try to be a full optimization IR.

## Should There Be A VM?

Eventually: yes, probably.

For v1: no, not as the first execution path.

## Recommended Execution Evolution

### Phase 1: Tree Interpreter

Compile source into IR, then interpret IR directly.

Why:

- simplest trustworthy implementation
- easiest to test
- lowest architecture risk

### Phase 2: Lowered Execution Plan

Introduce a lower-level internal form for repeated evaluation.

Possible forms:

- compact instruction list
- register-based plan
- stack-based bytecode

Why:

- better repeated execution performance
- easier budget accounting
- future optimization opportunities

### Phase 3: VM

Add a dedicated VM only if one or more of these become true:

- repeated evaluation performance matters materially
- host functions and built-ins need cheaper dispatch
- deterministic execution accounting needs tighter control
- code generation or alternate backends become important

## Recommended VM Position

If a VM is added later, it should sit under the runtime layer:

```text
source -> lexer -> parser -> IR -> validator -> lowered IR/bytecode -> VM
```

That means:

- parser still produces structural IR
- semantics still validate structural IR
- runtime may optionally lower validated IR into bytecode

This is the correct separation.

The parser should not compile directly to VM bytecode as the primary path.

Reason:

- validation and diagnostics want the higher-level structure
- structural IR is easier to inspect and debug
- SDK compile/validate contracts are cleaner that way

## Candidate Future VM Classes

If the project reaches that stage, likely classes would be:

- `runtime::LoweringPass`
- `runtime::Instruction`
- `runtime::BytecodeProgram`
- `runtime::VirtualMachine`
- `runtime::Frame`

### LoweringPass

Responsibility:

- convert validated IR into an executable lower-level form

### Instruction

Responsibility:

- represent one VM operation

Examples:

- load constant
- load binding
- call function
- compare
- jump if false
- return

### BytecodeProgram

Responsibility:

- immutable sequence of instructions plus constant pool metadata

### VirtualMachine

Responsibility:

- execute the lowered program under budget and policy constraints

## Architectural Decision On VM

Decision:

- adopt IR now
- do not adopt VM in the first trustworthy implementation
- design `CompiledFormula` so a future lowering-to-VM path can be added without
  changing the SDK

This is the right compromise between pragmatism and long-term extensibility.

## Example Compile and Evaluate Lifecycle

```cpp
Engine engine;
Schema schema;
Policy policy;

schema.allow_variable("x");
schema.allow_function("Clamp");

engine.register_function(make_clamp_function());

auto compiled = engine.compile("Clamp[x + 1, 0, 10]", schema, policy);
if (!compiled.ok()) {
    return compiled.diagnostics();
}

auto result = engine.evaluate(
    compiled.value(),
    Bindings{{"x", Value(3.0)}});
```

Internal flow:

1. `Engine::compile`
2. `Lexer`
3. `Parser`
4. IR constructed
5. `Validator`
6. `AstAnalysis`
7. `CompiledFormula` created
8. `Engine::evaluate`
9. `Evaluator`
10. host function registry used if needed
11. `Value` returned

## Architecture Boundaries That Must Be Preserved

These boundaries are important and should not be blurred during implementation.

### Boundary 1

SDK must not expose internal IR.

### Boundary 2

Frontend must not depend on runtime.

### Boundary 3

Validation must not rely on executing expressions.

### Boundary 4

Runtime must not depend on singleton global state.

### Boundary 5

Tooling must use the SDK, not internal shortcuts.

## Recommended First Implementation Sequence

1. define SDK public types
2. define IR node set
3. implement lexer and parser
4. implement validator and AST analysis
5. implement tree interpreter runtime
6. add example integration
7. consider lowered IR and VM only after repeated-evaluation needs are real

## Recommendation

Aleph3 should be built around:

- a small public SDK
- a simple structural IR
- a validation layer
- a tree-interpreter runtime first

Yes, the architecture should have an IR.

No, the first trustworthy implementation should not start with a VM.

But the architecture should leave room for a later lowering step and VM without
changing the SDK contract.
