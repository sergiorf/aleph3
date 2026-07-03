# The Aleph3 Manual

This is the user-facing manual for Aleph3 and the future source of the Aleph3
book. It explains the system through workflows and examples while keeping
normative limits in the focused specifications.

Aleph3 is both an embeddable formula engine and a focused symbolic system. Both
uses share one kernel; the SDK constrains it for host applications, sessions
make it interactive, and packs add mathematical domains.

## Contents

1. [Expressions and Evaluation](expressions-and-evaluation.md)
2. [Built-in Functions](built-in-functions.md)
3. [Rewriting and Assumptions](rewriting-and-assumptions.md)
4. [Embedding with the SDK](embedding-with-the-sdk.md)
5. [The Algebra Pack](packs-algebra.md)
6. [Sessions, CLI, and the Notebook](sessions-cli-and-notebook.md)

New symbolic users can read chapters 1, 2, 3, 5, and 6. Application developers
should read chapters 1, 4, and 2 first.

## Documentation Roles

- The [concepts guide](../concepts.md) defines vocabulary and the mental model.
- This manual teaches functions and workflows through examples.
- Specifications define exact supported behavior and remain authoritative.
- The [unified plan](../aleph3_unified_plan.md) describes unfinished work.

Examples use the current Wolfram-like syntax and, unless stated otherwise, run
in the symbolic CLI REPL.

## Current Boundary

Aleph3 already provides exact arithmetic, symbolic fallback, persistent
definitions, bounded rewriting, assumptions, and polynomial algebra. It is not
yet a broad general-purpose CAS. Unsupported forms are preserved or rejected
according to documented contracts rather than approximated silently.

