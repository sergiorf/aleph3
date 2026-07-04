# The Aleph3 Manual

This is the user-facing manual for Aleph3 and the future source of the Aleph3
book. It explains the current CLI, kernel, SDK, and packs while preparing the
workflow documentation for the planned local notebook.

Aleph3 is becoming a lightweight local symbolic notebook. Today the CLI makes
the focused symbolic system interactive, the SDK embeds it in host
applications, and packs add mathematical domains. All surfaces share one
kernel.

## Contents

1. [Expressions and Evaluation](expressions-and-evaluation.md)
2. [Built-in Functions](built-in-functions.md)
3. [Rewriting and Assumptions](rewriting-and-assumptions.md)
4. [Embedding with the SDK](embedding-with-the-sdk.md)
5. [The Algebra Pack](packs-algebra.md)
6. [Sessions, CLI, and the Notebook](sessions-cli-and-notebook.md)
7. [Appendix: Concepts and Terminology](concepts-and-terminology.md)

New symbolic users can read chapters 1, 2, 3, 5, and 6. Application developers
should read chapters 1, 4, and 2 first.

## Documentation Roles

- The [concepts appendix](concepts-and-terminology.md) defines vocabulary and
  the mental model alongside the workflow chapters.
- The rest of this manual teaches functions and workflows through examples.
- Specifications define exact supported behavior and remain authoritative.
- The [unified plan](../aleph3_unified_plan.md) describes unfinished work.
- The [notebook MVP design](../notebook_mvp_design.md) records the proposed
  first desktop-product contract; it does not describe shipped behavior.

Examples use the current Wolfram-like syntax and, unless stated otherwise, run
in the symbolic CLI REPL started with `aleph3_cli repl`. Output shown after
`->` is the expected canonical result, not text to type.

## Current Boundary

Aleph3 already provides exact arithmetic, symbolic fallback, persistent
definitions, bounded rewriting, assumptions, and polynomial algebra. It is not
yet a broad general-purpose CAS. Unsupported forms are preserved or rejected
according to documented contracts rather than approximated silently.

The repository also contains a headless notebook core with versioned JSON
persistence and deterministic clean `Run All`; no graphical notebook
application has shipped yet.

## Mathematical Notation

The manual uses dollar-delimited TeX for mathematical notation:

```text
Inline:  $a^2 + b^2 = c^2$
Display: $$\gcd(a,b) = \gcd(b, a \bmod b)$$
```

GitHub renders this notation in Markdown pages. Current VS Code releases can
render it in the built-in Markdown preview when `markdown.math.enabled` is
enabled. A renderer without math support will still show the TeX source.

## Build The PDF Book

The manual is also the single source for a PDF book. Install
[Pandoc](https://pandoc.org/installing.html) and a TeX distribution that
provides `xelatex` (for example, MiKTeX or TeX Live), then run from the
repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./docs/manual/build-book.ps1
```

The output is `build/docs/aleph3-manual.pdf`. The build uses the fixed chapter
order in `book.yaml`, enables dollar-delimited TeX math, and asks XeLaTeX to
typeset formulas in the PDF. Build output remains outside the documentation
source tree and is ignored by Git.
