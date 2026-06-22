# Symbolic Evaluator Cleanup Plan

## Purpose

This plan turns the symbolic evaluator cleanup into concrete repository work.

The objective is not to redesign the symbolic engine from scratch.
The objective is to take the current evaluator path and make it:

- readable
- modular
- contract-driven
- easier to test
- ready for new symbolic capabilities such as differentiation

The main debt today is concentrated in:

- [include/evaluator/Evaluator.hpp](/home/sergio/dev/aleph3/include/evaluator/Evaluator.hpp)
- [src/evaluator/Evaluator.cpp](/home/sergio/dev/aleph3/src/evaluator/Evaluator.cpp)

## Current Problems

### 1. The main evaluator mixes too many responsibilities

The current symbolic evaluator path combines:

- expression traversal
- special-form semantics
- built-in math dispatch
- symbolic fallback behavior
- user-defined function evaluation
- registry dispatch
- normalization-sensitive behavior
- algebra-specific dispatch

That makes it hard to reason about semantic boundaries.

### 2. Most behavior is encoded as condition-heavy branching

The implementation is effective but structurally fragile.

The evaluator is not yet organized around explicit semantic categories.
Instead, much of the behavior is driven by large clusters of `if`/`else`
checks and tables living close to the main evaluation entrypoint.

### 3. Evaluate and simplify boundaries are not crisp enough

The symbolic product needs a clearer rule for:

- what `evaluate` owns
- what `simplify` owns
- what should stay symbolic when reduction is incomplete

Without that split, new features will keep increasing semantic overlap.

### 4. Error construction is too ad hoc

The symbolic path still leans on direct `std::runtime_error` creation with
scattered message strings.

That makes error behavior:

- harder to keep consistent
- harder to test
- harder to evolve later into structured diagnostics

### 5. Algebra dispatch is not isolated enough

The polynomial tier has improved, but evaluator-level routing still knows too
much about algebra-specific behavior and argument handling.

That needs a clearer boundary before differentiation or deeper CAS features
are added.

## Cleanup Goal

After this cleanup, the symbolic evaluator should look like an orchestrator.

The main entrypoint should do only a few things:

1. inspect the expression kind
2. route to the right semantic subsystem
3. rebuild symbolic output when no rule applies
4. surface consistent errors

The main entrypoint should not remain the place where all evaluation logic
lives.

## Target Architecture

The symbolic evaluator should be split into the following internal areas.

### Evaluator Core

Owns:

- top-level `evaluate`
- expression-kind dispatch
- symbolic fallback and reconstruction

Recommended files:

- `include/evaluator/Evaluator.hpp`
- `src/evaluator/EvaluatorCore.cpp`

### Special Forms

Owns:

- `If`
- any future lazy or non-standard-evaluation constructs

Recommended files:

- `include/evaluator/EvaluatorSpecialForms.hpp`
- `src/evaluator/EvaluatorSpecialForms.cpp`

### Built-In Numeric And Symbolic Functions

Owns:

- unary/binary numeric built-ins
- comparison dispatch
- known symbolic constants and identity cases
- domain checks that belong to built-in evaluation

Recommended files:

- `include/evaluator/EvaluatorBuiltins.hpp`
- `src/evaluator/EvaluatorBuiltins.cpp`

### Algebra Dispatch

Owns:

- polynomial function routing
- symbolic argument policy for algebra functions
- variable selector handling

Recommended files:

- `include/evaluator/EvaluatorAlgebra.hpp`
- `src/evaluator/EvaluatorAlgebra.cpp`

This can wrap the current polynomial implementation in:

- [include/algebra/PolyUtils.hpp](/home/sergio/dev/aleph3/include/algebra/PolyUtils.hpp)
- [src/algebra/PolyUtils.cpp](/home/sergio/dev/aleph3/src/algebra/PolyUtils.cpp)

### User-Defined Function Dispatch

Owns:

- argument binding
- delayed vs immediate evaluation semantics
- arity validation
- recursive/self-referential behavior boundaries

Recommended files:

- `include/evaluator/EvaluatorFunctions.hpp`
- `src/evaluator/EvaluatorFunctions.cpp`

### Evaluator Error Helpers

Owns:

- standardized evaluator error creation
- centralized message wording
- optional future error categorization

Recommended files:

- `include/evaluator/EvaluatorErrors.hpp`
- `src/evaluator/EvaluatorErrors.cpp`

## Refactor Phases

## Phase 0. Freeze The Current Contract

Before moving logic, define the current intended product behavior.

Deliverables:

- a short evaluator contract section added to
  [docs/symbolic_engine_product_plan.md](/home/sergio/dev/aleph3/docs/symbolic_engine_product_plan.md)
  or a companion contract doc
- explicit rules for:
  - when evaluation reduces numerically
  - when evaluation returns a symbolic form
  - which functions evaluate arguments eagerly
  - which functions preserve raw symbolic arguments
  - when runtime errors are expected

Test gate:

- add focused regression tests for the current symbolic product surface before
  structural extraction starts

## Phase 1. Add Evaluator Contract Tests

This phase exists to make the later refactor safe.

Add or strengthen tests in:

- [tests/evaluator/EvaluatorTests.cpp](/home/sergio/dev/aleph3/tests/evaluator/EvaluatorTests.cpp)
- [tests/evaluator/EvaluatorFunctionsTests.cpp](/home/sergio/dev/aleph3/tests/evaluator/EvaluatorFunctionsTests.cpp)
- [tests/SimplifyTests.cpp](/home/sergio/dev/aleph3/tests/SimplifyTests.cpp)
- [tests/SymbolicCliSupportTests.cpp](/home/sergio/dev/aleph3/tests/SymbolicCliSupportTests.cpp)

Required contract cases:

- numeric arithmetic still reduces correctly
- `If` evaluates only the selected branch when the condition is boolean
- unresolved symbolic function calls remain symbolic instead of collapsing
- polynomial functions use symbolic arguments where required
- invalid variable selectors and bad arity paths fail consistently
- boolean and comparison behavior stays stable
- CLI symbolic commands preserve current product-facing behavior

Success condition:

- there is a compact evaluator regression net covering product behavior rather
  than only incidental examples

## Phase 2. Extract Algebra Dispatch

This is the safest first extraction because the product boundary is already
partly visible.

Work:

- move algebra-specific evaluator routing out of the generic evaluator path
- keep [src/evaluator/Evaluator.cpp](/home/sergio/dev/aleph3/src/evaluator/Evaluator.cpp)
  focused on dispatch, not polynomial semantics
- formalize helpers like variable extraction and argument policy inside the
  algebra dispatch layer

Likely moves:

- current `evaluate_polynomial_function(...)`
- current variable-selector helpers
- current polynomial function name checks

Success condition:

- algebra routing is isolated behind one narrow evaluator-facing interface

## Phase 3. Extract Special Forms

Work:

- move `If` handling and any other special-case non-eager evaluation behavior
  out of the main evaluator path
- define a clear lazy-evaluation policy per special form

Success condition:

- the core evaluator no longer contains special-form semantics inline

## Phase 4. Extract Built-In Dispatch

Work:

- move unary/binary built-in tables and comparison dispatch out of the main
  evaluator header
- separate:
  - numeric built-ins
  - comparison built-ins
  - symbolic identity tables
  - domain checking

Important constraint:

- this phase must not casually change built-in semantics

Success condition:

- built-in function logic is no longer mixed into the top-level evaluator entry

## Phase 5. Extract User-Defined Function Dispatch

Work:

- move user-function application and binding logic into its own module
- centralize delayed/immediate argument semantics
- centralize arity failures

Success condition:

- user-defined function behavior has one implementation home instead of being
  spread across evaluator control flow

## Phase 6. Centralize Error Construction

Work:

- replace scattered evaluator `runtime_error` strings with internal error
  helper functions
- normalize wording for:
  - bad arity
  - bad variable selectors
  - unsupported domains
  - unknown function and unsupported symbolic cases where applicable

Success condition:

- evaluator failures can be updated consistently and tested by category

## Phase 7. Shrink The Core Entry Point

Once extraction is complete:

- reduce the top-level `evaluate` implementation to orchestration only
- delete duplicated helper logic that became redundant during extraction
- move any remaining internal-only helpers into the right module

Success condition:

- `evaluate` becomes short enough that a reader can understand the dispatch
  model in one pass

## Concrete File Plan

### Existing Files To Keep

- [include/evaluator/Evaluator.hpp](/home/sergio/dev/aleph3/include/evaluator/Evaluator.hpp)
- [src/evaluator/Evaluator.cpp](/home/sergio/dev/aleph3/src/evaluator/Evaluator.cpp)
- [include/evaluator/EvaluationContext.hpp](/home/sergio/dev/aleph3/include/evaluator/EvaluationContext.hpp)

### New Files To Add

- `include/evaluator/EvaluatorAlgebra.hpp`
- `src/evaluator/EvaluatorAlgebra.cpp`
- `include/evaluator/EvaluatorSpecialForms.hpp`
- `src/evaluator/EvaluatorSpecialForms.cpp`
- `include/evaluator/EvaluatorBuiltins.hpp`
- `src/evaluator/EvaluatorBuiltins.cpp`
- `include/evaluator/EvaluatorFunctions.hpp`
- `src/evaluator/EvaluatorFunctions.cpp`
- `include/evaluator/EvaluatorErrors.hpp`
- `src/evaluator/EvaluatorErrors.cpp`

These names are recommended, not mandatory.
The real rule is one semantic responsibility per module.

## Test Strategy

Each phase should end with a test pass.

Minimum required command after each phase:

```bash
ctest --test-dir build --output-on-failure -R aleph3_symbolic_tests
```

Recommended test additions by area:

### Core Evaluator

- symbolic fallback reconstruction
- numbers, rationals, booleans, and symbols
- stable handling of unknown calls

### Special Forms

- `If[True, a, b]`
- `If[False, a, b]`
- symbolic `If` condition fallback
- branch non-evaluation where applicable

### Built-Ins

- numeric unary/binary happy paths
- domain failures
- comparison behavior across numeric and exact forms
- known symbolic constant reductions

### Algebra

- `Expand`
- `Factor`
- `Collect`
- `GCD`
- `PolynomialQuotient`
- bad variable selectors
- unsupported multivariate paths

### User-Defined Functions

- arity mismatch
- delayed definitions
- immediate definitions
- symbolic argument preservation where intended

## Risks And Constraints

### Do not change product behavior casually

The cleanup is structural first.
Semantic changes should happen only when:

- the current behavior is clearly wrong, or
- the contract is being intentionally tightened with tests and docs

### Do not merge SDK concerns back into the symbolic evaluator

The symbolic evaluator cleanup should improve the symbolic layer.
It should not pull SDK runtime constraints into the older symbolic path unless
that is an intentional shared abstraction.

### Do not let differentiation start before extraction

Differentiation will touch:

- special forms
- built-ins
- symbolic fallback
- algebra interaction

If it lands before cleanup, it will make the evaluator tangle worse.

## Recommended Immediate Next Tasks

1. Add this plan to the symbolic product docs index.
2. Write a short evaluator contract section in the symbolic product plan.
3. Add evaluator contract regression tests for:
   - special forms
   - symbolic fallback
   - algebra dispatch
4. Start Phase 2 by extracting algebra dispatch first.

## Definition Of Done

The symbolic evaluator cleanup is done when all of the following are true:

- evaluator semantics are documented in product terms
- main evaluator flow is orchestration rather than implementation
- algebra, special forms, built-ins, and user-function handling are isolated
- evaluator errors are centralized enough to be tested consistently
- symbolic regression tests protect the cleaned structure
- the project is ready to add symbolic differentiation without expanding a
  monolithic evaluator further
