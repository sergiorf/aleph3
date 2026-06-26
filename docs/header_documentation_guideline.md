# Header Documentation Guideline

## Purpose

This document defines the project guideline for documentation headers in
Aleph3 header files.

## Rule

Public and architecture-significant headers should begin with a short
documentation header comment.

That comment should answer:

- what this header owns
- why it exists
- what major types or contracts it introduces
- any important boundary or usage rule

## Minimum Expectation

Add a documentation header for:

- headers under `include/kernel/`
- headers under `include/sdk/`
- headers that define core symbolic contracts under `include/evaluator/`,
  `include/expr/`, `include/parser/`, `include/packs/`, and `include/symbols/`

Optional for now:

- small internal-only helper headers where the name and contents are already
  trivially obvious

## Style

Keep the header comment short and factual.

Good content:

- subsystem purpose
- ownership boundary
- major exported type names
- migration status if transitional

Avoid:

- changelog text
- promotional wording
- line-by-line summaries of obvious code

## Example Shape

```cpp
/*
 * Kernel Evaluation Context
 * -------------------------
 * Shared execution context for symbolic kernel state and embedded runtime
 * state during the kernel unification refactor.
 */
```

## Migration Guideline

Do not try to retrofit every header in one sweep.

Apply this rule in three situations:

1. new headers
2. major refactors to existing headers
3. headers that define core project contracts

## Current Recommendation

The refactor work should treat missing documentation headers in core contract
files as normal cleanup to do while touching those headers.
