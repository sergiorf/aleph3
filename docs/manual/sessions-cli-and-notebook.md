# Sessions, CLI, And The Notebook

## The Session Contract

A session owns one reusable kernel evaluation context. It preserves definitions
and returns structured results. Current operations include evaluation,
simplification, full form, inspection, pack discovery, completion, and
diagnostics.

This is shared infrastructure for the CLI and future graphical products.

## CLI Workflow

```text
:help
:mode
:inspect f[x + 1]
:packs
:complete Pol
:quit
```

The symbolic REPL preserves assignments and definitions. One-shot commands
start fresh. On supported terminals Tab completes commands and symbols;
`:complete` is the deterministic, pipe-friendly fallback.

For stateful batch work, a script contains one expression per non-empty line:

```text
aleph3_cli script calculations.aleph3
aleph3_cli script --json calculations.aleph3
```

One session is shared across the file, failures do not stop later lines, and
the process exits with `2` if any expression fails. JSON mode emits one compact
object per submitted line with its line number, source, status, canonical
output, and unchanged session diagnostics. Scripts are limited to 8 MiB and
individual lines to 1 MiB; comments and multiline expressions are unsupported.

## Headless Notebook Foundation

The current build includes an experimental `aleph3_notebook_core` library, but
not a graphical notebook executable. The library models ordered input and text
cells with stable document-local identifiers. Its `Run All` operation starts a
fresh session, skips text cells, evaluates every input in order, and replaces
the previous generated results.

Definitions flow to later cells during a run. Repeating `Run All` starts clean,
and one failed input records its session diagnostics without preventing later
inputs from running. Generated results retain canonical plain text and current
diagnostic codes/messages. The core saves and loads bounded UTF-8 JSON v1
documents, including optional cached results marked with their producer
version. Loading never evaluates source.

Saves validate first, write beside the destination, and atomically replace the
old file using the supported platform API. A failed validation, write, or
replacement leaves the previous valid destination intact. Autosave, recovery
journals, and migrations are not implemented.

## Planned Notebook Application

The first graphical Aleph3 product should be a thin session consumer, not a new
semantic layer. A useful v0.1 feedback release needs:

- ordered input and output cells
- persistent session state and run-cell/run-all actions
- inline structured diagnostics
- registry- and session-backed completion
- expression inspection and full form
- copyable plain-text input and output
- local save/open with a documented, versioned notebook format
- a small examples gallery using only verified supported syntax

The GUI owns presentation and documents. The session owns execution state, the
kernel owns meaning, and packs own domain mathematics.

No notebook application is included in the current build. Until it exists,
`aleph3_cli repl` is the runnable local interactive surface. Code generation,
PDF/HTML export, rich Markdown, and paid packs are roadmap features, not
current capabilities.

The first notebook should evaluate currently supported examples such as exact
arithmetic, `Refine[Sqrt[x^2], x >= 0]`, rewriting, and polynomial operations.
Examples such as `D[x^3 + 2*x, x]`, trigonometric identity simplification, and
C++ code generation must wait until their kernel or pack contracts are
implemented and tested.

The planned document, evaluation, persistence, and display behavior is defined
in the [Notebook MVP Design](../notebook_mvp_design.md).

## Plot And Graph Capabilities

Graphs are important to a compelling notebook, but plotting should enter as an
explicit reusable contract:

1. evaluate a supported expression over a bounded numeric domain
2. return structured series data and diagnostics
3. render axes, lines, points, labels, and interaction in the GUI

The first slice should provide deterministic two-dimensional function plots
with explicit sample budgets. Discontinuities, non-finite values, domain
errors, and partial series need deliberate representations. The renderer must
not contain a private evaluator.

Later work can add multiple series, parametric and list plots, export, and
richer interaction. Three-dimensional graphics and a broad visualization
grammar are not v0.1 requirements.

## A Shareable v0.1

The first public release needs a coherent loop more than broad CAS parity:

```text
write -> complete -> evaluate -> inspect result/diagnostic
      -> optionally visualize -> edit and run again
```

That loop can expose exact arithmetic, assumptions, rewrites, and the algebra
pack honestly while gathering feedback about syntax, discovery, performance,
and desired mathematical workflows.
