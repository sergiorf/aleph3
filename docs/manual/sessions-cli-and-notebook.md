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
:reset
:quit
```

The symbolic REPL preserves assignments and definitions. One-shot commands
start fresh. On supported terminals Tab completes commands and symbols;
`:complete` is the deterministic, pipe-friendly fallback.

Use `:reset` when an interactive symbolic session should start over without
leaving the REPL. It discards session-local assignments and user function
definitions, but it does not unload builtins or registered packs and does not
change the current `:mode`:

```text
a = 2
a                         -> 2
:reset
a                         -> a
Factor[x^2 - 1]           -> (x - 1) * (x + 1)
```

Use `Clear[symbol]` to remove a session-local own value and user function
definition. Use `Unset[symbol]` when only the own value should be removed:

```text
a = 10
Clear[a]
a

f[x_] := x + 1
Unset[f]
f[2]
```

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
the previous generated results. Documents can also clear cached generated
results without changing cells or source.

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

## Graphical Notebook Status

No graphical notebook application is included in the current build. The
delivered headless core and JSON format are product foundations, not a claim
that a desktop UI has shipped. Until it does, `aleph3_cli repl` is the runnable
local interactive surface.

The planned application remains a thin consumer: the GUI owns cells,
presentation, and file interaction; the session owns interactive state; the
kernel and packs own semantics. The first verified gallery should begin with
current behavior such as:

```text
1/2 + 1/3
Refine[Sqrt[x^2], x >= 0]
Replace[f[x], f[a_] -> g[a]]
Factor[(1/2)*x^2 + x + 1/2]
PolynomialQuotient[x^2*y + x*y^2 + y, x*y, {x, y}]
D[x^2 + 3*x, x]
Det[{{1, 2}, {3, 4}}]
```

Plotting, code generation, export, rich Markdown, and other roadmap
capabilities must not appear as current examples until their contracts and
tests ship. The [Notebook MVP Design](../notebook_mvp_design.md)
owns the planned document and UI contract; the
[Unified Plan](../aleph3_unified_plan.md) owns sequencing.
