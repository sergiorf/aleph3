# Symbolic Core Gap Analysis

## Purpose

This document compares Aleph3's current symbolic engine with the kind of core
semantics exposed by the Wolfram Language / Mathematica symbolic engine.

The goal is not to promise full parity. The goal is to identify the main
architecture and semantic gaps, then separate:

- what belongs in the pure symbolic kernel
- what belongs in optional math packs built on top of that kernel

## Framing

A Mathematica-class symbolic engine is not mainly a large pile of math
functions. Its strength comes from a small number of deep core capabilities:

- uniform symbolic expression representation
- symbol-based evaluation with definitions and attributes
- pattern matching and rule application
- exact arithmetic and symbolic-preserving evaluation
- assumptions and domain-sensitive transformations
- package-style extension of higher math on top of the kernel

Aleph3 already has part of that story:

- expression trees
- parser
- evaluator with narrow explicit semantics
- canonicalization and simplification contracts
- exact rational arithmetic
- a small algebra layer

The main gap is that Aleph3 is still stronger at direct built-in evaluation
than at general symbolic transformation driven by definitions, patterns, and
assumptions.

## Current Aleph3 Position

Strong relative areas:

- exact rational arithmetic for evaluator-visible cases
- explicit normalization and simplification contracts
- narrow but coherent evaluation-control semantics
- stable symbolic fallback for unsupported cases
- regression-tested algebra subset

Weak relative areas:

- no general pattern matcher
- no rule engine comparable to replacement-rule workflows
- no definition model comparable to downvalues/upvalues/subvalues
- no assumptions/domain framework
- no exact polynomial coefficient infrastructure
- no calculus layer
- shallow special-function semantics
- no package boundary between kernel and higher math domains

## Gap Analysis Versus A Mathematica-Class Core

### 1. Symbol And Definition System

Mathematica-class behavior depends on symbols carrying definitions and
evaluation metadata, not just on evaluator-side dispatch tables.

Aleph3 today:

- built-ins are mostly evaluator-dispatched
- user-defined functions exist, but only in a narrow call-definition model
- there is no general symbol-definition stack or value-table architecture

Gap:

- no general ownvalue/downvalue/upvalue-style design
- no clean precedence model between built-ins, user definitions, and rules
- no symbol-local metadata model beyond the current semantics registry

Impact:

- limits extensibility
- makes evaluator branching accumulate ad hoc behavior
- makes package-style higher-level math harder to build cleanly

### 2. Pattern Matching And Rules

Mathematica's symbolic power relies heavily on symbolic patterns and rewrite
rules, not only on evaluator-recognized built-ins.

Aleph3 today:

- has structural expressions and some rules as data
- does not have a general purpose expression pattern matcher
- does not have broad replace/rewrite traversal primitives

Gap:

- no `Blank`-style matcher model
- no conditional rules
- no repeated rewrite engine with explicit termination strategy
- no traversal API for targeted replacement

Impact:

- simplification stays narrow
- symbolic calculus and many algebra features cannot be built compositionally
- package authors would need evaluator changes instead of declarative rules

### 3. Attributes And Evaluation Semantics

Aleph3 has made good progress here, but it remains narrow relative to a full
symbolic kernel.

Aleph3 today:

- explicit registry for listability and selected evaluation properties
- `If`, `And`, and `Or` have explicit contracts
- `Flat` and `Orderless` are recognized in the supported subset

Gap:

- semantics are still mostly function-registry facts, not a broader symbolic
  attribute system
- no hold variants beyond the explicitly supported subset
- no sequence-holding/splicing behavior
- no evaluation hooks driven by symbol definitions

Impact:

- evaluator control is still kernel-internal rather than language-extensible

### 4. Assumptions, Domains, And Contextual Simplification

Mathematica's symbolic core is not only rewrite-driven. It also uses domains
and assumptions to decide what transformations are valid.

Aleph3 today:

- mostly uses local numeric-domain checks
- preserves symbolic fallback when it cannot justify a reduction

Gap:

- no assumptions environment
- no domain lattice for real/complex/integer/polynomial regimes
- no assumption-aware simplification or transformation API

Impact:

- simplification must remain conservative
- many mathematically valid rewrites cannot be expressed safely

### 5. Exact Algebra Infrastructure

Mathematica-class algebra rests on exact representations first, then on
algorithms built over them.

Aleph3 today:

- exact rationals exist at the expression layer
- polynomial helpers still use `double` internally
- exact rational coefficients are rejected in algebra helpers

Gap:

- no exact polynomial coefficient ring abstraction
- no modular or exact factorization pipeline
- no exact multivariate GCD/division backbone

Impact:

- algebra remains product-usable only for a narrow subset
- higher algebra packs would rest on the wrong foundation

### 6. Calculus And Transformation Packs

Mathematica's calculus features sit on top of strong rewrite and algebra
infrastructure.

Aleph3 today:

- no differentiation or integration layer

Gap:

- no derivative representation contract
- no rewrite-driven differentiation rules
- no simplification/algebra feedback loop for calculus output

Impact:

- no meaningful path yet toward symbolic calculus packs

### 7. Special Functions And Analytic Contracts

Mathematica's special-function layer depends on domain rules, branch choices,
exact special values, recurrence, asymptotics, and simplification identities.

Aleph3 today:

- early Gamma work
- basic transcendental support

Gap:

- no general analytic contract framework
- branch behavior is still shallow
- special-function simplification rules are sparse

Impact:

- special functions will remain fragile unless they are built on a stronger
  kernel and assumptions model

## What Belongs In The Pure Symbolic Core

The pure symbolic core should be the minimal engine that makes symbolic math
possible, not the whole math library.

It should own:

- expression/value representation
- parser and structural printers
- symbol table and definition storage
- attribute and evaluation-semantics model
- evaluation loop and symbolic fallback
- pattern matcher
- rule application and traversal engine
- exact scalar arithmetic primitives
- canonicalization and normalization
- bounded simplification framework
- assumptions/domain context
- error and diagnostics model
- package/extension registration API

It should not directly own every advanced math domain algorithm.

## What Belongs In Math Packs

Math packs should be layered libraries on top of the symbolic core.

Candidate packs:

- elementary functions pack
- algebra pack
- exact polynomial pack
- linear algebra pack
- calculus pack
- equation solving pack
- special functions pack
- discrete math / combinatorics pack

These packs should depend on the kernel contracts, not widen the kernel by
embedding domain logic into evaluator branches.

## Recommended Architecture Strengthening

### Kernel Layer

Introduce a clearer kernel boundary:

- `expr`: symbolic representation
- `symbols`: symbol metadata, definitions, and attributes
- `eval`: core evaluator and control semantics
- `rewrite`: pattern matching and rule execution
- `exact`: exact scalar arithmetic types and helpers
- `normalize`: canonical structural normalization
- `assumptions`: domain and context facts

### Pack Layer

Move domain math behind pack-like modules:

- `pack_elementary`
- `pack_algebra`
- `pack_calculus`
- `pack_special_functions`

### Registration Model

Prefer registration and declarative metadata over evaluator branching:

- kernel registers primitive semantics
- packs register symbols, rules, exact values, and algorithms
- evaluator asks the kernel symbol/definition model first, not scattered
  special cases

## Recommended Future Phases

These are the meaningful post-documentation phases if Aleph3 wants to move
toward a Mathematica-class symbolic core without losing rigor.

### Phase 12. Kernel Boundary And Symbol Definition Model

Goal:

- define the pure symbolic kernel boundary
- introduce a symbol-definition architecture that can grow beyond hardcoded
  evaluator dispatch

### Phase 13. Pattern Matching And Rewrite Engine

Goal:

- add the general symbolic rule machinery that higher symbolic features depend
  on

### Phase 14. Assumptions And Domain Semantics

Goal:

- let transformations depend on explicit mathematical context instead of only
  local syntax and numeric checks

### Phase 15. Exact Algebra Backbone

Goal:

- replace the current floating-point polynomial internals with exact algebra
  infrastructure suitable for real symbolic growth

### Phase 16. Math Packs And High-Level Domains

Goal:

- move algebra, calculus, special functions, and future higher math into pack
  layers built on the stronger kernel

## Strategic Rule

Aleph3 should not try to imitate Mathematica by accreting more built-in
functions into evaluator dispatch alone.

The scalable path is:

1. strengthen the symbolic kernel
2. make transformation semantics extensible
3. build math packs on top of that kernel
