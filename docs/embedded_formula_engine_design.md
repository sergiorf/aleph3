# Aleph3 Embedded Formula Engine

## Purpose

This document defines a product and technical design for positioning Aleph3 as
an embeddable symbolic and numeric formula engine for C++ applications. The
goal is not to replace Mathematica broadly. The goal is to win a narrow class
of use cases where teams need:

- Local deterministic formula evaluation
- A small embeddable runtime
- Mathematica-like expression syntax
- User-editable formulas inside a larger product
- Tight C++ integration without a Python dependency

This wedge is commercially stronger than a general-purpose CAS because buyers
in this segment pay for integration, reliability, and control, not for the
full breadth of a research math environment.

## Product Thesis

Aleph3 should be packaged as a formula runtime that product teams embed into
their own software.

Representative uses:

- Test and measurement tools with user-defined formulas
- Calibration and signal-processing applications
- Scientific instruments with local expression evaluation
- Engineering simulation frontends with pre-processing rules
- B2B desktop software that exposes a formula DSL to end users

Core message:

`Aleph3 is an embeddable symbolic formula engine for C++ products that need
editable math, deterministic execution, and a smaller operational footprint
than Mathematica or Python-based stacks.`

## Non-Goals

Aleph3 is not attempting to provide, in the near term:

- A notebook environment
- Broad numerical methods coverage
- Full Mathematica language compatibility
- General-purpose scientific visualization
- Large-scale data science tooling
- Arbitrary code execution

These non-goals should remain explicit in product positioning and engineering
scope. They keep the surface area commercially credible.

## Target Users

### Primary Buyer

A software team building a native engineering or scientific application.

Typical traits:

- Main application written in C++ or another native language
- Need to let users enter formulas safely
- Need symbolic simplification or normalization before evaluation
- Need formulas to be saved, validated, versioned, and replayed
- Do not want a Python runtime or a heavyweight external CAS dependency

### Primary End User

A domain expert, not a mathematician:

- Test engineer
- Controls engineer
- Applications engineer
- Lab automation engineer
- Quantitative analyst in an internal desktop workflow

They want editable formulas and predictable outputs. They do not need a large
language runtime.

## Customer Problems

Current alternatives create one of four problems:

1. Hardcoded formulas in application code
   Changes require engineering releases.

2. Ad hoc expression evaluators
   Numeric-only engines cannot represent symbolic structure, simplification, or
   reusable expression trees.

3. Python and SymPy embedding
   Operationally heavier, harder to sandbox, and awkward in native products.

4. Mathematica
   Powerful but expensive, broad, operationally heavy for embedding, and often
   misaligned with product integration needs.

## Why Aleph3 Can Win

Aleph3 already has the right conceptual core:

- Expression tree representation
- Parser for human-written formulas
- Evaluation context with variables and user-defined functions
- Basic symbolic transforms such as simplification and expansion
- Rules, strings, lists, and exact rational support

This is enough to define a focused embedded product if the engine is hardened
around API stability, safety, diagnostics, and host integration.

## Product Scope

### Version 1 Scope

Aleph3 Embedded Engine v1 should support:

- Parse formula strings into an AST
- Validate formulas against a host-defined environment
- Evaluate formulas with supplied variables
- Simplify and normalize expressions
- Persist formulas as source text and canonicalized form
- Report structured errors with source locations
- Register host-provided functions
- Disable unsupported language features by policy

### Version 1.5 Scope

- Rule-based rewrites for domain normalization
- Precompiled formulas for repeated evaluation
- Deterministic numeric mode configuration
- Rich metadata on symbols and functions
- Audit logging hooks

### Version 2 Scope

- Code generation for C and C++
- Dimension and units checking
- Sandboxed policy profiles by deployment mode
- Incremental compilation and caching
- Stable C ABI for non-C++ hosts

## Product Surface

The product should be split into three layers.

### 1. Core Engine

Responsibilities:

- AST representation
- Parsing
- Evaluation
- Simplification
- Function registry
- Host bindings

This corresponds to the current repo's main value.

### 2. Embedding SDK

Responsibilities:

- Public stable API
- Validation API
- Error types
- Host function registration
- Serialization and canonicalization
- Policy configuration
- Threading and lifecycle rules

This layer is currently missing and should become the primary product surface.

### 3. Tooling

Responsibilities:

- CLI for testing formulas
- Conformance tests
- Benchmark harness
- Formula linting
- Migration and compatibility tools

The existing REPL can evolve into developer tooling, but it should not remain
the primary product in this strategy.

## Example Use Cases

### Use Case 1: Instrument Configuration

A laboratory instrument allows administrators to define output formulas such as:

`(gain * raw_signal + offset) / reference`

The application embeds Aleph3 to:

- Parse and validate the formula before saving
- Ensure only approved variables are used
- Evaluate the formula locally during acquisition
- Preserve the formula source for auditing

### Use Case 2: Calibration Workflow

An engineering desktop tool lets users define transforms like:

`If[temp > threshold, a * x^2 + b * x + c, x]`

Aleph3 provides:

- User-editable expressions
- Validation of variable names and functions
- Deterministic local evaluation
- Future symbolic normalization for diagnostics

### Use Case 3: Formula-Driven Product Logic

A native desktop application exposes formulas for scoring or ranking. The host
uses Aleph3 to:

- Evaluate formulas on local records
- Register custom functions such as `Clamp`, `Normalize`, or `Lookup`
- Restrict unsafe constructs
- Produce user-facing diagnostics

## Technical Requirements

### Functional Requirements

- Parse expressions from strings
- Support variables, numeric constants, strings, booleans, lists, and function
  calls
- Evaluate expressions against a provided symbol table
- Allow host registration of custom functions
- Return structured parse, validation, and runtime errors
- Support canonical serialization for persistence and diffs
- Support optional symbolic simplification before evaluation

### Non-Functional Requirements

- Deterministic results for a fixed engine version and config
- No arbitrary host code execution by default
- No network dependency
- No Python runtime dependency
- Small deployable footprint
- Fast cold start
- Stable documented API
- Thread-safe usage model or clearly scoped per-engine contexts

## Proposed Public API

The current internal API is expression-centric. The embedded product needs a
higher-level API that is safer and easier for host applications to adopt.

Illustrative C++ API:

```cpp
namespace aleph3 {

struct EngineOptions {
    bool enable_user_functions = false;
    bool enable_assignments = false;
    bool enable_strings = true;
    bool enable_lists = true;
    bool simplify_before_eval = true;
};

struct Diagnostic {
    enum class Severity { Error, Warning };
    Severity severity;
    std::string code;
    std::string message;
    size_t line;
    size_t column;
};

struct ValidationResult {
    bool ok;
    std::vector<Diagnostic> diagnostics;
};

class Engine {
public:
    explicit Engine(EngineOptions options = {});

    ValidationResult validate(
        std::string_view source,
        const Schema& schema) const;

    Result<CompiledFormula, std::vector<Diagnostic>> compile(
        std::string_view source,
        const Schema& schema) const;

    Result<Value, RuntimeError> evaluate(
        const CompiledFormula& formula,
        const InputBindings& bindings) const;

    void register_function(const HostFunctionSpec& spec);
};

}
```

Key design principle:

The host should not need to manipulate raw AST nodes for common workflows.

## Schema and Validation Model

The product needs an explicit schema model. This is the boundary between the
engine and the host application's domain.

Example schema concepts:

- Allowed variable names
- Variable types
- Allowed built-in functions
- Host-defined functions
- Allowed constants
- Feature flags such as assignments or user-defined functions
- Numeric policy such as exact rational preservation vs floating evaluation

Validation should detect:

- Unknown symbols
- Unknown functions
- Wrong arity
- Disallowed features
- Type mismatches where inferable
- Unsupported constructs for a given deployment profile

## Safety Model

Safety is a product feature, not just an implementation detail.

### Threats

- Unbounded expression growth
- Expensive recursive evaluation
- Malicious formulas entered by users
- Unexpected host callbacks
- Runtime instability from unchecked edge cases

### Required Controls

- Maximum AST depth
- Maximum node count
- Maximum function nesting
- Maximum string and list sizes
- Per-evaluation operation budget
- Host function registration with explicit metadata
- Feature gating by profile

### Policy Profiles

Suggested built-in profiles:

- `strict_numeric`
- `formula_dsl`
- `engineering_default`
- `developer_full`

This lets the same engine serve both end-user and internal workflows.

## Architecture Changes Needed

The current codebase is a strong prototype, but it is not yet packaged as an
embedding-first engine. The main work items are below.

### 1. Separate Public SDK From CLI

Today the CLI in `src/main.cpp` is a primary entry point. The product should
move toward:

- `aleph3_core`
- `aleph3_sdk`
- `aleph3_cli`

The CLI becomes a consumer of the SDK, not the main face of the system.

### 2. Add Structured Diagnostics

The parser and evaluator currently rely mainly on exceptions and plain strings.
The SDK needs:

- Error codes
- Source spans
- Validation diagnostics
- Runtime diagnostics distinct from parse errors

### 3. Introduce a Stable Value Layer

The current `Expr` variant is an internal representation. The SDK should
present a cleaner `Value` abstraction for host-side input and output.

Reasons:

- Reduces coupling to internal AST choices
- Simplifies FFI later
- Makes SDK docs clearer

### 4. Add Compilation Boundary

The product should distinguish:

- Parse
- Validate
- Compile
- Evaluate

This enables caching, repeated execution, benchmarking, and clearer contracts.

### 5. Add Host Function API

Host callbacks need a constrained interface with:

- Name
- Arity
- Type expectations
- Purity or side-effect metadata
- Error contracts

### 6. Add Determinism Controls

For commercial embedding, numerical behavior must be predictable. The engine
needs explicit settings for:

- Floating-point mode expectations
- Rational-to-float coercion
- Simplification behavior
- Constant treatment

### 7. Add Serialization and Canonicalization

Commercial users care about:

- Formula storage
- Formula comparison
- Change tracking
- Auditability

The engine should expose canonical source or canonical AST serialization.

## Current Codebase Fit

The current repo already aligns well with this direction:

- `include/expr/Expr.hpp`: internal expression model
- `include/parser/Parser.hpp`: parser
- `include/evaluator/Evaluator.hpp`: evaluation logic
- `include/evaluator/FunctionRegistry.hpp`: good base for host registration
- `include/evaluator/EvaluationContext.hpp`: execution context
- `include/transforms/Transforms.hpp`: symbolic transforms

Gaps relative to the embedded product:

- No stable SDK boundary
- No structured diagnostics
- Limited policy controls
- No compile/cache abstraction
- No schema-driven validation API
- No documented threading model
- No packaging or ABI strategy

## Packaging Strategy

The product should ship in tiers.

### Community Edition

- Core parser and evaluator
- CLI
- Basic built-ins
- Source build via CMake

Purpose:

- Adoption
- Credibility
- Developer familiarity

### Commercial SDK

- Stable API
- Long-term support releases
- Embedding documentation
- Validation and diagnostics
- Policy profiles
- Precompiled formula support
- Priority bug fixes

Purpose:

- Monetizable layer

### Enterprise Add-Ons

- C ABI
- Code generation
- Compliance documentation
- Extended audit features
- Integration support

Purpose:

- Higher ACV for regulated or operationally demanding teams

## Licensing Direction

The repo is currently MIT. That is good for adoption but weak for direct SDK
monetization if the commercial value sits entirely in the same code.

Possible paths:

1. Open core plus commercial SDK
   Keep the engine open and sell the hardened SDK, tooling, support, and
   packaged binaries.

2. Open core plus proprietary enterprise modules
   Keep core parsing and evaluation open; sell code generation, audit tooling,
   and advanced validation.

3. Dual licensing
   Viable only if the project ownership and contribution model are managed
   carefully from the start.

Recommended starting point:

Open core plus commercial SDK and support.

This matches the likely buying behavior in the embedding market.

## Competitive Positioning

### Against Mathematica

Message:

`Use Aleph3 when you need a formula engine inside your product, not a complete
research computing platform.`

Advantages:

- Smaller surface area
- Native C++ integration
- Local deterministic runtime
- Lower cost and simpler operational model

### Against SymPy

Message:

`Use Aleph3 when Python is not the right runtime boundary for your product.`

Advantages:

- Better fit for native applications
- Smaller deployable footprint
- Easier product embedding story

### Against Generic Expression Evaluators

Message:

`Use Aleph3 when numeric-only expression parsing is too weak and you need
symbolic structure, transforms, or richer expression semantics.`

Advantages:

- Symbolic representation
- Formula normalization
- Rules and richer semantics

## Roadmap

### Phase 0: Positioning and Product Surface

- Publish embedded-engine design
- Define supported feature subset
- Rename docs still referring to Mathix
- Split CLI concerns from engine concerns

### Phase 1: SDK Foundation

- Add structured diagnostics
- Add schema validation API
- Add `Engine` facade
- Add compile and evaluate lifecycle
- Add host function registration contract

### Phase 2: Hardening

- Add limits and policy controls
- Add benchmarking
- Add compatibility tests
- Document determinism and threading

### Phase 3: Commercial Packaging

- Ship versioned binaries
- Write embedding guides
- Add support SLAs and release policy
- Define paid distribution terms

### Phase 4: Expansion

- Add code generation
- Add richer type system
- Add units and dimensions
- Add stable C ABI

## Success Metrics

Product success should not be measured first by total downloads. For this
strategy, the useful metrics are:

- Time to embed in a host application
- Median evaluation latency for repeated formulas
- Number of supported host integrations
- Conversion from open-source users to paid SDK customers
- Reduction in customer-owned formula bugs after adoption
- Support burden per customer

## Risks

### Technical Risks

- Internal AST may leak into public API
- Exceptions and diagnostics may remain too ad hoc
- Feature growth may outrun engine hardening
- Floating-point behavior may surprise enterprise users

### Market Risks

- Product may be perceived as "too small" if positioned as a Mathematica
  replacement too early
- SymPy may be "good enough" for many prospects
- Customers may prefer custom DSLs if the SDK is not opinionated enough

### Execution Risks

- Chasing broad CAS features instead of embedding quality
- Underinvesting in docs and examples
- Keeping the public API unstable for too long

## Immediate Next Steps

1. Treat this embedded-engine wedge as the primary product strategy.
2. Refactor the repo narrative to describe Aleph3 as an embeddable engine first
   and a CLI second.
3. Define the v1 SDK boundary before adding more math breadth.
4. Build one end-to-end sample host application that embeds the engine.
5. Add structured diagnostics and schema validation before adding major new CAS
   features.

## Recommendation

Aleph3 should pursue the embedded formula engine niche as its first
commercialization path. The current architecture already contains the core
concepts required for that wedge. The work now is less about adding more math
and more about turning the prototype into a reliable product boundary:

- Stable SDK
- Validation
- Diagnostics
- Policy controls
- Packaging

If executed well, Aleph3 does not need to beat Mathematica at being broad. It
needs to be better at being embed-friendly, predictable, and operationally
light for native software teams.
