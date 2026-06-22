# Current Engine Audit

## Purpose

This document audits the current Aleph3 engine as a potential foundation for
the embedded formula-engine product. It is not a style review and it is not a
feature inventory. It is a decision document.

Each subsystem is classified as one of:

- `salvage now`
- `salvage later`
- `reference only`
- `discard`

The classification is based on one question:

`Does this subsystem improve the odds of shipping a small, trustworthy embedded
engine, or does it increase risk and ambiguity?`

## Executive Summary

The current codebase is a credible prototype, not a trustworthy product core.

What is clearly good:

- the repo captures the right product direction at a conceptual level
- there is a real expression model, parser, evaluator, and test suite
- the project already separates library code from the CLI at the build level

What is clearly risky:

- semantics are inconsistent across parsing, evaluation, normalization, and
  simplification
- the engine uses global mutable state in the function registry
- error handling is exception-first and stringly typed
- the current public surface is implementation-driven rather than product-driven
- the supported language is much broader than the embedded v1 needs

Conclusion:

The current engine should not be treated as the trusted implementation path for
the embedded product. It should be treated as:

- a source of ideas
- a source of tests to audit and selectively reuse
- a source of small isolated utilities where they are easy to verify

It should not be treated as the product foundation.

## Overall Classification

### Salvage Now

- product direction and docs
- some expression and value-model ideas
- selected utility helpers
- selected tests after review
- existing CMake workspace and repo structure

### Salvage Later

- pretty-printing concepts
- parts of algebra support not needed for v1
- selected normalization ideas if narrowed and re-verified

### Reference Only

- current parser
- current evaluator
- current simplification and transform pipeline
- current built-in function behavior
- current CLI behavior

### Discard

- global singleton function registry as a product design
- using current AST breadth as the default public surface
- preserving current language breadth for compatibility reasons

## Subsystem Audit

## 1. Expression Model

Files:

- [include/expr/Expr.hpp](/home/sergio/dev/aleph3/include/expr/Expr.hpp)
- [include/expr/ExprUtils.hpp](/home/sergio/dev/aleph3/include/expr/ExprUtils.hpp)
- [src/expr/Expr.cpp](/home/sergio/dev/aleph3/src/expr/Expr.cpp)

Classification:

- `salvage later`

What looks useful:

- There is a clear attempt to model symbolic objects explicitly.
- The expression tree covers numbers, rationals, booleans, strings, lists,
  calls, definitions, assignments, rules, and special values.
- The helpers in `ExprUtils.hpp` show what common constructors the engine wants.

What is risky:

- The current `Expr` type is too broad for the embedded v1 and too tightly
  coupled to internal semantics.
- `ExprPtr` is used everywhere, so ownership, mutation expectations, and public
  boundary concerns are all mixed together.
- The current AST includes many language features that the first embedded
  product should not expose.

Decision:

- Do not use `Expr` as the new public SDK model.
- Rebuild a smaller internal IR for the trusted subset.
- Reuse ideas or limited utilities only if they are clearly isolated.

## 2. Pretty Printing and Stringification

Files:

- [src/expr/Expr.cpp](/home/sergio/dev/aleph3/src/expr/Expr.cpp)

Classification:

- `salvage later`

What looks useful:

- The pretty-printing code is readable.
- It captures infix display rules and canonical-ish formatting choices that may
  still be useful for tooling.

What is risky:

- Display behavior is mixed with semantic assumptions.
- Formatting logic is not a safe basis for canonical persistence contracts yet.
- The string output may encode legacy semantics that should not be preserved in
  the rewrite.

Decision:

- Keep as reference for future display and debugging.
- Do not tie the new engine’s contracts to current output strings.

## 3. Parser

Files:

- [include/parser/Parser.hpp](/home/sergio/dev/aleph3/include/parser/Parser.hpp)
- parser tests in [tests/parser](/home/sergio/dev/aleph3/tests/parser)

Classification:

- `reference only`

What looks useful:

- The parser handles a broad and ambitious language surface.
- It already covers arithmetic, comparisons, strings, lists, assignments,
  function definitions, and some special forms.
- There is a substantial test suite around parser behavior.

What is risky:

- The parser is implemented as a single large header with many responsibilities.
- Parse errors are exception-based and not structured for embedding.
- There is no clean source-span diagnostic model.
- The parser is solving a broader language than the embedded v1 needs.
- Special cases such as `If`, definitions, assignment, and implicit
  multiplication are interwoven into one path, which raises maintenance risk.

Decision:

- Do not harden the current parser into the trusted product path.
- Rebuild a new parser for the smaller trusted subset.
- Reuse parser tests only after classifying which behaviors belong in v1.

## 4. Evaluator

Files:

- [include/evaluator/Evaluator.hpp](/home/sergio/dev/aleph3/include/evaluator/Evaluator.hpp)
- [src/evaluator/Evaluator.cpp](/home/sergio/dev/aleph3/src/evaluator/Evaluator.cpp)
- [include/evaluator/EvaluationContext.hpp](/home/sergio/dev/aleph3/include/evaluator/EvaluationContext.hpp)

Classification:

- `reference only`

What looks useful:

- The evaluator is real, not scaffolding.
- It already demonstrates variable binding, function application, symbolic and
  numeric cases, and some special forms.

What is risky:

- Evaluation logic is concentrated in header code and large condition-heavy
  paths.
- Semantic scope is too broad for the embedded v1 and not cleanly separated by
  policy.
- Runtime behavior relies on plain exceptions rather than structured runtime
  errors.
- There is no obvious evaluation budget or bounded-execution model.
- The current evaluator is tightly coupled to the current AST and function
  registry model.

Specific semantic risk observed from code:

- Boolean semantics are inconsistent. `simplify` can return `Symbol("True")`
  and `Symbol("False")`, while other parts of the engine use the dedicated
  `Boolean` variant and helpers like `get_boolean_value`.
- This kind of inconsistency is exactly the sort of issue that makes the engine
  hard to trust as a product core.

Decision:

- Rebuild the evaluator for the trusted subset.
- Do not preserve current evaluator behavior as an implicit compatibility goal.

## 5. Function Registry

Files:

- [include/evaluator/FunctionRegistry.hpp](/home/sergio/dev/aleph3/include/evaluator/FunctionRegistry.hpp)
- [src/evaluator/BuiltInFunctions.cpp](/home/sergio/dev/aleph3/src/evaluator/BuiltInFunctions.cpp)

Classification:

- `discard` as a product design

What looks useful:

- The basic idea of decoupling function handlers from evaluation is correct.

What is risky:

- The registry is a singleton with process-global mutable state.
- Built-ins are registered globally.
- This is a poor fit for embedding, isolation, concurrency, and multi-instance
  use.

Decision:

- Do not carry the singleton registry into the rewrite.
- Replace it with engine-scoped host function registration.

## 6. Built-In Functions

Files:

- [src/evaluator/BuiltInFunctions.cpp](/home/sergio/dev/aleph3/src/evaluator/BuiltInFunctions.cpp)
- built-in logic inside [include/evaluator/Evaluator.hpp](/home/sergio/dev/aleph3/include/evaluator/Evaluator.hpp)

Classification:

- `reference only`

What looks useful:

- There is already a catalog of desirable built-ins and host-function patterns.
- Several useful embedded-engine ideas are present: strings, `Length`, `N`,
  logical functions.

What is risky:

- Built-ins are split across multiple implementation locations.
- Function semantics are broader than the v1 subset requires.
- Some behaviors are chosen for convenience rather than product clarity.

Decision:

- Re-implement only the small built-in set needed by the trusted subset.
- Treat the current built-in library as a menu of ideas, not as a contract.

## 7. Simplification, Normalization, and Transforms

Files:

- [src/transforms/Transforms.cpp](/home/sergio/dev/aleph3/src/transforms/Transforms.cpp)
- [include/normalizer/Normalizer.hpp](/home/sergio/dev/aleph3/include/normalizer/Normalizer.hpp)
- [tests/SimplifyTests.cpp](/home/sergio/dev/aleph3/tests/SimplifyTests.cpp)
- [tests/ExpandTests.cpp](/home/sergio/dev/aleph3/tests/ExpandTests.cpp)

Classification:

- `reference only`

What looks useful:

- There is meaningful work here, especially around simple simplification and
  polynomial-style expansion.
- It demonstrates the kinds of transforms users may eventually want.

What is risky:

- Normalization, simplification, and evaluation semantics are not clearly
  layered.
- The transform logic mixes algebraic intent with representation-specific
  shortcuts.
- Complex number normalization and symbolic rewrites are sufficiently subtle
  that they should not be trusted without focused re-verification.

Decision:

- Exclude this subsystem from the trusted embedded v1.
- Revisit later only after the new core is stable.

## 8. Algebra and Polynomial Layer

Files:

- [include/algebra/Polynomial.hpp](/home/sergio/dev/aleph3/include/algebra/Polynomial.hpp)
- [src/algebra/Polynomial.cpp](/home/sergio/dev/aleph3/src/algebra/Polynomial.cpp)
- [src/algebra/PolyUtils.cpp](/home/sergio/dev/aleph3/src/algebra/PolyUtils.cpp)

Classification:

- `salvage later`

What looks useful:

- The polynomial code is relatively isolated compared with the parser/evaluator.
- It may eventually be valuable for niche engineering use cases.

What is risky:

- It is not on the critical path for the v1 embedded product.
- It adds semantic breadth and maintenance load before the core runtime is
  trustworthy.
- Several algorithms are intentionally limited, such as univariate-first
  handling.

Decision:

- Keep it out of the rewrite critical path.
- Re-audit later once the minimal embedded core is shipped.

## 9. CLI

Files:

- removed legacy CLI entrypoint (`src/main.cpp`)

Classification:

- `reference only`

What looks useful:

- The CLI proves the engine is meant to be interactive and user-facing.
- It may still be useful as developer tooling later.

What is risky:

- The CLI currently carries product identity weight that should belong to the
  SDK.
- It directly reflected symbolic-engine semantics and special cases.
- It is not the right architectural center for an embedding-first product.

Decision:

- Keep SDK tooling centered on [src/tooling/aleph3_cli.cpp](/home/sergio/dev/aleph3/src/tooling/aleph3_cli.cpp).
- Do not reintroduce the removed prototype CLI as the implementation center of the rewrite.

## 10. Tests

Files:

- [tests](/home/sergio/dev/aleph3/tests)

Classification:

- `salvage now`, selectively

What looks useful:

- There is broad coverage across parsing, evaluation, simplification, complex
  numbers, rationals, and polynomial features.
- The suite can help identify intended behaviors and corner cases.

What is risky:

- Many tests validate current behavior, not necessarily correct product
  contracts.
- The test suite spans many features outside the embedded v1 scope.
- Some tests are about the broader symbolic language breadth that the rewrite should
  intentionally avoid.

Decision:

- Mine the tests aggressively, but do not inherit them wholesale.
- Promote only tests that match the trusted subset and the new SDK contracts.

## Key Product Risks Found

## 1. Semantic Inconsistency

Different parts of the engine use overlapping but inconsistent models for:

- booleans
- normalization
- evaluation
- pretty-printing

This is the strongest reason not to build the product on the current core.

## 2. Global Mutable State

The singleton function registry is incompatible with trustworthy embedding.

## 3. No Structured Diagnostics

The current engine is built around exceptions and string messages, not the
diagnostic model required by host applications.

## 4. Scope Is Too Broad

The current language surface is much larger than what the embedded product
needs. That increases rewrite cost and verification cost without helping the
first commercial wedge.

## 5. Product Boundary Is Missing

The current library shape is organized around internal mechanics, not around
the compile/validate/evaluate lifecycle that the embedded SDK needs.

## Salvage Matrix

| Subsystem | Classification | Short Reason |
|---|---|---|
| Product direction/docs | salvage now | Strong conceptual fit for the niche |
| Expression ideas | salvage later | Useful concepts, wrong boundary |
| Pretty-printing | salvage later | Helpful for tooling, not contracts |
| Parser | reference only | Too broad, weak diagnostics, tightly coupled |
| Evaluator | reference only | Real implementation, but not trustworthy enough |
| Function registry | discard | Global mutable state is the wrong design |
| Built-ins | reference only | Useful ideas, wrong implementation boundary |
| Simplify/normalize/transforms | reference only | Semantically subtle and nonessential for v1 |
| Algebra/polynomials | salvage later | Isolated but not critical to v1 |
| CLI | reference only | Tooling value only |
| Tests | salvage now, selectively | Valuable only after contract filtering |

## Recommendation

Proceed with the controlled rewrite.

Use the current engine as:

- a prototype to study
- a place to mine small utilities
- a source of edge cases and tests

Do not use it as the trusted product path.

The next tasks should be:

1. harden the SDK validation path beyond the current constant-condition,
   schema-constant, and constant-runtime-trap checks, with end-to-end contract
   coverage
2. keep narrowing product-facing docs and examples around the SDK path, not the
   previous prototype
3. finish packaging and naming cleanup once the SDK contracts are stable enough
   to freeze
