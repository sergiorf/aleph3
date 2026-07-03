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

## Notebook-Like Workbench

The first graphical Aleph3 product should be a thin session consumer, not a new
semantic layer. A useful v0.1 feedback release needs:

- ordered input and output cells
- persistent session state and run-cell/run-all actions
- inline structured diagnostics
- registry- and session-backed completion
- expression inspection and full form
- a small versioned save/open format
- copyable plain-text input and output

The GUI owns presentation and documents. The session owns execution state, the
kernel owns meaning, and packs own domain mathematics.

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

